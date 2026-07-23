// Test del kernel de edificios y construcción (Sprint 1.1, SPEC-004 Parte I).
// Autor: sonnet-5 (brief docs/briefs/SONNET_KERNEL_EDIFICIOS_SPRINT_1.1.md).
//
// Cubre §9.3 del SPEC: colocación válida/inválida (cada rechazo de §4.1,
// incluida la exención de escenario de la enmienda del Arquitecto), ASSIGN_BUILD
// (§4.2), construcción completa por 1 y por N ciudadanos, dropoff a edificio
// vs fallback legacy (§6), combate contra edificios (§7: targeting + RPS),
// destrucción libera cost_grid, y save/load v8 round-trip con un edificio a
// medio construir. También ejercita, con un escenario propio (no se toca
// driver.hpp/cli_run.hpp — código sensible a la trayectoria golden), el
// equivalente de los gates G3 (savetest)/G4 (savetest+IA no aplica aquí, se
// usa determinismo puro)/G5 (record+replay) incluyendo PLACE_BUILDING/
// ASSIGN_BUILD, tal como exige SPEC-004 §9.2.
//
// NOTA sobre G5-equivalente: el formato de replay v2 (replay.hpp) NO
// serializa CmdPayload::unit_id (gap heredado de Sprint 0.4, documentado en
// SONNET_KERNEL_DATOS_SPEC002_RESULT.md §2.4/§2.6 — ningún escenario previo
// usaba SPAWN_UNIT/CITIZEN con unit_id real, así que nunca se ejercitó).
// SPEC-004 §8 exige "replay v2 sin cambios de formato"; en vez de tocar ese
// formato compartido (fuera de alcance de este sprint), el escenario de abajo
// usa DELIBERADAMENTE BuildingId=0 y UnitId=0 (citizen) como únicos valores de
// catálogo referenciados por PLACE_BUILDING/SPAWN_CITIZEN: la truncación a 0
// del campo no serializado coincide por diseño con el valor real, así que la
// reproducción sigue siendo bit-exacta sin parchear replay.hpp. Ver RESULT.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#include "chunsa/data_catalog.hpp"
#include "chunsa/game_state.hpp"
#include "chunsa/step.hpp"
#include "chunsa/checksum.hpp"
#include "chunsa/serialize.hpp"
#include "chunsa/save_io.hpp"
#include "chunsa/replay.hpp"
#include "chunsa/ai_stub.hpp"

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

// El mailbox es un ring buffer (head/count): el receipt del ÚLTIMO comando
// emitido está en ring[(head+count-1) % CAP], no en ring[0] (que sería el
// MÁS ANTIGUO si ya hubo varios comandos previos para el mismo emisor).
static RejectReason last_result(const ReceiptMailbox& m) {
    return m.ring[(m.head + m.count - 1) % MAILBOX_CAP].result;
}

// ============================================================================
// Catálogo de prueba en memoria (sin pasar por el loader CHDB — mismo patrón
// que el catálogo "mini" de test_data_blob.cpp para Siege/NavalLight).
//
// units[0]    = citizen (UnitId 0): usado por SPAWN_CITIZEN data-driven.
// buildings[0]= "hut" (BuildingId 0): constructible, footprint 2x2, T=5,
//               cost A=10, dropoff A. Es el edificio "normal" de la mayoría
//               de los tests (y el único referenciado en el escenario
//               G3/G4/G5-equivalente, ver nota de arriba).
// buildings[1]= "center" (BuildingId 1): constructible:false, T=0 (nace
//               completo), sin coste, dropoff A|B|Me — para la exención de
//               escenario (enmienda del Arquitecto) y el dropoff-a-edificio.
// ============================================================================
namespace fixture {

inline UnitDefinitionV1 make_citizen_def() {
    UnitDefinitionV1 d{};
    d.id = 0;
    d.unit_class = UnitClassV1::Citizen;
    d.tags_mask = 0;
    d.hp = 20;
    d.attack = 0;
    d.range_millitiles = 0;
    d.speed_millitile_tick = 800;
    d.morale = 100;
    d.build_time_ticks = 1;
    for (int k = 0; k < 6; ++k) d.bonus_vs_bp[k] = 0;
    return d;
}

inline BuildingDefinitionV1 make_hut_def() {
    BuildingDefinitionV1 d{};
    d.id = 0;
    d.hp = 300;
    d.footprint_w = 2; d.footprint_h = 2;
    d.build_time_ticks = 5;
    d.cost_a = 10; d.cost_b = 0; d.cost_me = 0;
    d.dropoff_mask = 0x1u;  // A
    d.constructible = 1;
    return d;
}

inline BuildingDefinitionV1 make_center_def() {
    BuildingDefinitionV1 d{};
    d.id = 1;
    d.hp = 1000;
    d.footprint_w = 2; d.footprint_h = 2;
    d.build_time_ticks = 0;   // nace completo (enmienda §4.1.2/§4.3)
    d.cost_a = 0; d.cost_b = 0; d.cost_me = 0;
    d.dropoff_mask = 0x7u;    // A|B|Me
    d.constructible = 0;
    return d;
}

static UnitDefinitionV1 g_units[1] = { make_citizen_def() };
static BuildingDefinitionV1 g_buildings[2] = { make_hut_def(), make_center_def() };

inline DataCatalogV1 make_catalog() {
    DataCatalogV1 c{};
    c.unit_count = 1;
    c.units = g_units;
    c.unit_names = nullptr;  // no se usa catalog_find_unit en estos tests
    c.building_count = 2;
    c.buildings = g_buildings;
    c.building_names = nullptr;
    return c;
}

}  // namespace fixture

// ============================================================================
// Helpers de construcción de comandos.
// ============================================================================
static RawCommand place_building(uint32_t tick, uint16_t emitter, uint64_t seq,
                                 BuildingId bid, int64_t tile_x, int64_t tile_y) {
    RawCommand c{};
    std::memset(&c, 0, sizeof(c));
    c.target_tick = tick; c.emitter = emitter; c.type = CommandType::PLACE_BUILDING;
    c.sequence = seq;
    c.p.unit_id = bid;
    c.p.x_raw = tile_x; c.p.y_raw = tile_y;
    return c;
}
static RawCommand assign_build(uint32_t tick, uint16_t emitter, uint64_t seq,
                               EntityHandle citizen, int64_t tile_x, int64_t tile_y) {
    RawCommand c{};
    std::memset(&c, 0, sizeof(c));
    c.target_tick = tick; c.emitter = emitter; c.type = CommandType::ASSIGN_BUILD;
    c.sequence = seq;
    c.p.handle = citizen;
    c.p.x_raw = tile_x; c.p.y_raw = tile_y;
    return c;
}
static RawCommand spawn_citizen(uint32_t tick, uint16_t emitter, uint64_t seq,
                                uint32_t idx, int64_t x_raw, int64_t y_raw, int32_t speed = 800) {
    RawCommand c{};
    std::memset(&c, 0, sizeof(c));
    c.target_tick = tick; c.emitter = emitter; c.type = CommandType::SPAWN_CITIZEN;
    c.sequence = seq;
    c.p.handle = EntityHandle{idx, 1u};
    c.p.x_raw = x_raw; c.p.y_raw = y_raw;
    c.p.unit_id = 0;  // catálogo: citizen (UnitId 0)
    (void)speed;  // el path data-driven ignora speed_mtpt del payload (debe ser 0)
    return c;
}

static MatchConfig01A make_cfg(uint32_t max_entities = 64, uint8_t players = 2) {
    MatchConfig01A cfg{};
    cfg.max_entities = max_entities;
    cfg.player_count = players;
    cfg.human_input_delay_ticks = 0;   // comandos surten efecto en su propio target_tick
    cfg.max_future_command_ticks = 20;
    cfg.checksum_every_ticks = 1;
    cfg.map_tiles_x = 256; cfg.map_tiles_y = 256;
    cfg.seed = 777ull;
    cfg.allow_debug_stat_payload = 0;
    return cfg;
}

// ============================================================================
// 1) PLACE_BUILDING: validación §4.1, orden exacto (contrato).
// ============================================================================
static void test_place_building_validation() {
    static DataCatalogV1 cat = fixture::make_catalog();

    // 1a) Camino feliz: coloca "hut" (BuildingId 0) en (10,10), footprint 2x2.
    // A target_tick=1 (NO 0): con eff>=1 se ejercita el camino NORMAL completo
    // (constructible + stock + deducción), fuera de la exención de escenario.
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        gs_bind_catalog(*g, cat);
        g->player_stock[0][0] = 100;  // A suficiente

        RawCommand cmd = place_building(1, 0, 1, 0, 10, 10);
        step(*g, &cmd, 1);                           // t=0: agenda (eff=1)
        const StepResult r = step(*g, nullptr, 0);   // t=1: aplica
        CHECK(r.accepted == 1 && r.rejected == 0);
        CHECK(g->entities.alive[0] == 1);
        CHECK(g->entity_kind[0] == 1);
        CHECK(g->building_id[0] == 0);
        CHECK(g->hp[0] == 300 && g->max_hp[0] == 300);
        CHECK(g->unit_class[0] == 255);
        CHECK(g->bld_anchor_tx[0] == 10 && g->bld_anchor_ty[0] == 10);
        CHECK(g->build_progress[0] == 0);
        // Centro geométrico: anchor*T + (w*T)/2 = 10*65536 + (2*65536)/2 = 720896
        CHECK(g->pos_x[0] == 10 * 65536 + 65536);
        CHECK(g->pos_y[0] == 10 * 65536 + 65536);
        CHECK(g->player_stock[0][0] == 90);  // 100 - cost_a(10)
        CHECK(g->cost_grid[10 * FF_AXIS + 10] == FF_WALL);
        CHECK(g->cost_grid[11 * FF_AXIS + 11] == FF_WALL);
        CHECK(g->cost_grid[9 * FF_AXIS + 10] != FF_WALL);   // fuera del footprint
        CHECK(g->flow_dirty == 1);
    }

    // 1b) Paso 1: catalog==nullptr -> MALFORMED.
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        // sin gs_bind_catalog: g->catalog queda nullptr.
        RawCommand cmd = place_building(0, 0, 1, 0, 10, 10);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::MALFORMED);
    }
    // 1b') Paso 1: unit_id >= building_count -> MALFORMED.
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        gs_bind_catalog(*g, cat);
        RawCommand cmd = place_building(0, 0, 1, 99, 10, 10);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::MALFORMED);
    }

    // 1c) Paso 2: constructible==0 en tick > 0 -> ILLEGAL_STATE (fuera de la
    // exención de escenario, ver test_scenario_exemption más abajo). Se
    // avanza un tick vacío primero para que target_tick=1 caiga en t=1 (no en
    // la ventana exenta t==0), con delay=0 (default de make_cfg).
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        gs_bind_catalog(*g, cat);
        step(*g, nullptr, 0);  // t=0 -> t=1, sin comandos
        RawCommand cmd = place_building(1, 0, 1, 1 /*center, no-constructible*/, 10, 10);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::ILLEGAL_STATE);
    }

    // 1d) Paso 3: payload contaminado (hp!=0) -> MALFORMED.
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        gs_bind_catalog(*g, cat);
        g->player_stock[0][0] = 100;
        RawCommand cmd = place_building(0, 0, 1, 0, 10, 10);
        cmd.p.hp = 5;
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::MALFORMED);
    }
    // 1d') handle != {0,0} -> MALFORMED.
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        gs_bind_catalog(*g, cat);
        g->player_stock[0][0] = 100;
        RawCommand cmd = place_building(0, 0, 1, 0, 10, 10);
        cmd.p.handle = EntityHandle{5, 1};
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::MALFORMED);
    }

    // 1e) Paso 4: footprint fuera del mapa -> MALFORMED.
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        gs_bind_catalog(*g, cat);
        g->player_stock[0][0] = 100;
        RawCommand cmd = place_building(0, 0, 1, 0, 255, 255);  // 255+2 > 256
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::MALFORMED);
    }
    // 1e') Coordenada negativa -> MALFORMED.
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        gs_bind_catalog(*g, cat);
        g->player_stock[0][0] = 100;
        RawCommand cmd = place_building(0, 0, 1, 0, -1, 10);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::MALFORMED);
    }

    // 1f) Paso 5: celda no transitable (muro) -> ILLEGAL_STATE.
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        gs_bind_catalog(*g, cat);
        g->player_stock[0][0] = 100;
        g->cost_grid[10 * FF_AXIS + 11] = FF_WALL;  // dentro del footprint 2x2 en (10,10)
        RawCommand cmd = place_building(0, 0, 1, 0, 10, 10);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::ILLEGAL_STATE);
    }
    // 1f') Solape con edificio vivo -> ILLEGAL_STATE (corolario del mismo
    // chequeo: la 1ra colocación ya walló sus celdas).
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        gs_bind_catalog(*g, cat);
        g->player_stock[0][0] = 100;
        RawCommand first = place_building(0, 0, 1, 0, 10, 10);
        step(*g, &first, 1);
        CHECK(g->entities.alive[0] == 1);
        RawCommand overlap = place_building(1, 0, 2, 0, 11, 11);  // solapa 1 celda
        const StepResult r = step(*g, &overlap, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::ILLEGAL_STATE);
    }

    // 1g) Paso 6: stock insuficiente -> ILLEGAL_STATE. Se avanza a t=1 (fuera
    // de la exención de escenario de tick 0, que omitiría este chequeo).
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        gs_bind_catalog(*g, cat);
        g->player_stock[0][0] = 5;  // cost_a=10
        step(*g, nullptr, 0);  // t=0 -> t=1
        RawCommand cmd = place_building(1, 0, 1, 0, 10, 10);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::ILLEGAL_STATE);
    }

    // 1h) Paso 7: pool agotado -> POOL_EXHAUSTED.
    {
        auto g = std::make_unique<GameState>();
        MatchConfig01A cfg = make_cfg();
        cfg.max_entities = 1;
        gs_init(*g, cfg);
        gs_bind_catalog(*g, cat);
        g->player_stock[0][0] = 1000;
        RawCommand fill = place_building(0, 0, 1, 0, 10, 10);
        step(*g, &fill, 1);
        CHECK(g->entities.alive_count == 1);
        RawCommand cmd = place_building(1, 0, 2, 0, 20, 20);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::POOL_EXHAUSTED);
    }
}

// ============================================================================
// 2) Exención de escenario en effective_tick==0 (enmienda del Arquitecto,
//    SPEC-004 §4.1.2/§4.3): un edificio constructible:false + T=0 se coloca
//    SIN validar constructible ni costes si effective_tick==0; con
//    effective_tick>=1 el mismo comando es ILLEGAL_STATE.
// ============================================================================
static void test_scenario_exemption() {
    static DataCatalogV1 cat = fixture::make_catalog();

    // 2a) effective_tick==0 (delay=0, target_tick=0) -> ACCEPTED, nace completo.
    {
        auto g = std::make_unique<GameState>();
        MatchConfig01A cfg = make_cfg();
        cfg.human_input_delay_ticks = 0;
        gs_init(*g, cfg);
        gs_bind_catalog(*g, cat);
        CHECK(g->player_stock[0][0] == 0);  // sin stock: si el paso 6 NO se omitiera, fallaría

        RawCommand cmd = place_building(0, 0, 1, 1 /*center*/, 10, 10);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.accepted == 1 && r.rejected == 0);
        CHECK(g->entities.alive[0] == 1);
        CHECK(g->build_progress[0] >= g->catalog->buildings[1].build_time_ticks);  // completo
    }

    // 2a-bis) Exención con edificio CON COSTE (hut, A=10) y stock 0: aceptado
    // Y el stock NO queda en negativo — la exención exime también la DEDUCCIÓN,
    // no solo el chequeo (endurecimiento del Arquitecto en revisión).
    {
        auto g = std::make_unique<GameState>();
        MatchConfig01A cfg = make_cfg();
        cfg.human_input_delay_ticks = 0;
        gs_init(*g, cfg);
        gs_bind_catalog(*g, cat);
        CHECK(g->player_stock[0][0] == 0);

        RawCommand cmd = place_building(0, 0, 1, 0 /*hut, cost A=10*/, 30, 30);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.accepted == 1 && r.rejected == 0);
        CHECK(g->entities.alive[0] == 1);
        CHECK(g->player_stock[0][0] == 0);  // sin deducción bajo exención
    }

    // 2b') Sprint 1.2 (SPEC-004 §10.3): mismo comando (target_tick=0) pero con
    // delay=1 (valor de PRODUCCIÓN, ya no delay=0) ingerido en el PRIMER Step
    // (t=0) -> effective_tick SIGUE siendo 0 (ventana de setup, no aplica el
    // delay) -> ACCEPTED bajo la exención de escenario, exactamente igual que
    // 2a. Esto es lo que permite que la demo (chunsa_sim_node.cpp) vuelva a
    // delay=1 sin dejar de colocar sus centros iniciales.
    {
        auto g = std::make_unique<GameState>();
        MatchConfig01A cfg = make_cfg();
        cfg.human_input_delay_ticks = 1;
        gs_init(*g, cfg);
        gs_bind_catalog(*g, cat);
        CHECK(g->player_stock[0][0] == 0);  // sin stock: la exención también exime costes

        RawCommand cmd = place_building(0, 0, 1, 1 /*center*/, 10, 10);
        // target_tick==0 && t==0 -> eff=0, SIN sumar delay (§10.3).
        CHECK(command_effective_tick(cmd.target_tick, 0u, cfg.human_input_delay_ticks) == 0u);
        const StepResult r = step(*g, &cmd, 1);  // t=0: eff==0 -> debido en el MISMO step
        CHECK(r.accepted == 1 && r.rejected == 0);
        CHECK(g->entities.alive[0] == 1);
        CHECK(g->build_progress[0] >= g->catalog->buildings[1].build_time_ticks);
    }

    // 2b'') Fuera de la ventana de setup con delay=1: target_tick=1 (NO 0) en
    // t=0 -> eff=max(1, 0+1)=1 != 0 (la regla nueva NO aplica si target!=0) ->
    // NO exento -> ILLEGAL_STATE al aplicarse en t=1 (mismo comportamiento de
    // "fuera de la exención" que antes de este sprint, solo que ahora se
    // alcanza con target_tick=1 en vez de con target_tick=0+delay=1).
    {
        auto g = std::make_unique<GameState>();
        MatchConfig01A cfg = make_cfg();
        cfg.human_input_delay_ticks = 1;
        gs_init(*g, cfg);
        gs_bind_catalog(*g, cat);

        RawCommand cmd = place_building(1, 0, 1, 1 /*center*/, 10, 10);
        CHECK(command_effective_tick(cmd.target_tick, 0u, cfg.human_input_delay_ticks) == 1u);
        step(*g, &cmd, 1);                           // t=0: agenda, no debido aún
        const StepResult r2 = step(*g, nullptr, 0);  // t=1: debido, eff=1 != 0 -> no exento
        CHECK(r2.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::ILLEGAL_STATE);
        CHECK(g->entities.alive_count == 0);
    }
}

// ============================================================================
// 3) ASSIGN_BUILD: validación §4.2.
// ============================================================================
static void test_assign_build_validation() {
    static DataCatalogV1 cat = fixture::make_catalog();

    auto setup_with_hut = [&](GameState& g) {
        gs_init(g, make_cfg());
        gs_bind_catalog(g, cat);
        g.player_stock[0][0] = 100;
        RawCommand place = place_building(0, 0, 1, 0, 10, 10);
        step(g, &place, 1);
    };

    // 3a) handle inválido -> INVALID_ENTITY.
    {
        auto g = std::make_unique<GameState>();
        setup_with_hut(*g);
        RawCommand cmd = assign_build(1, 0, 2, EntityHandle{99, 1}, 10, 10);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::INVALID_ENTITY);
    }

    // 3b) NOT_OWNER: ciudadano de otro emisor.
    {
        auto g = std::make_unique<GameState>();
        setup_with_hut(*g);
        RawCommand spawn = spawn_citizen(1, 1 /*owner 1*/, 1, 1, 10 * 65536, 5 * 65536);
        step(*g, &spawn, 1);
        CHECK(g->entities.alive[1] == 1);
        RawCommand cmd = assign_build(2, 0 /*emitter 0, no dueño del citizen*/, 3,
                                      EntityHandle{1, g->entities.generation[1]}, 10, 10);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::NOT_OWNER);
    }

    // 3c) unit_class != 3 -> ILLEGAL_STATE (usar el propio edificio como "handle").
    {
        auto g = std::make_unique<GameState>();
        setup_with_hut(*g);
        RawCommand cmd = assign_build(1, 0, 2, EntityHandle{0, g->entities.generation[0]}, 10, 10);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::ILLEGAL_STATE);
    }

    // 3d) Tile fuera de cualquier footprint -> INVALID_ENTITY.
    {
        auto g = std::make_unique<GameState>();
        setup_with_hut(*g);
        RawCommand spawn = spawn_citizen(1, 0, 2, 1, 10 * 65536, 5 * 65536);
        step(*g, &spawn, 1);
        RawCommand cmd = assign_build(2, 0, 3, EntityHandle{1, g->entities.generation[1]}, 50, 50);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::INVALID_ENTITY);
    }

    // 3e) Éxito: build_target queda apuntando al edificio.
    {
        auto g = std::make_unique<GameState>();
        setup_with_hut(*g);
        RawCommand spawn = spawn_citizen(1, 0, 2, 1, 10 * 65536, 5 * 65536);
        step(*g, &spawn, 1);
        RawCommand cmd = assign_build(2, 0, 3, EntityHandle{1, g->entities.generation[1]}, 10, 10);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.accepted == 1);
        CHECK(g->build_target[1] == 0u);
    }
}

// ============================================================================
// 4) Construcción completa: 1 ciudadano y N ciudadanos (progreso suma).
// ============================================================================
static void test_construction_progress() {
    static DataCatalogV1 cat = fixture::make_catalog();

    // 4a) 1 ciudadano completa un T=5 en, como mucho, unos pocos ticks tras
    // llegar (con margen de ticks de viaje incluido).
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        gs_bind_catalog(*g, cat);
        g->player_stock[0][0] = 100;
        RawCommand place = place_building(0, 0, 1, 0, 10, 10);
        step(*g, &place, 1);
        // Ciudadano ya DENTRO del footprint (10..12,10..12 raw): sin viaje.
        RawCommand spawn = spawn_citizen(1, 0, 2, 1,
                                         10 * 65536 + 32768, 10 * 65536 + 32768);
        step(*g, &spawn, 1);
        RawCommand assign = assign_build(2, 0, 3, EntityHandle{1, g->entities.generation[1]}, 10, 10);
        step(*g, &assign, 1);
        // construction_system corre en la MISMA fase 5 del tick del assign
        // (después de aplicar comandos, igual que economy/combat/aggro): el
        // ciudadano ya estaba en el sitio, así que el progreso ya avanzó 1
        // punto antes de que step() devuelva el control (mismo patrón que
        // morale_system con SPAWN_UNIT en test_data_blob.cpp).
        CHECK(g->build_progress[0] == 1u);
        for (int k = 0; k < 10; ++k) step(*g, nullptr, 0);
        CHECK(g->build_progress[0] == 5u);  // == T, funcional (saturado, no sigue subiendo)
        CHECK(g->build_progress[0] >= g->catalog->buildings[0].build_time_ticks);
        // build_target se limpia solo (el edificio ya es funcional).
        CHECK(g->build_target[1] == BUILD_NO_TARGET);
    }

    // 4b) N=3 ciudadanos EN el sitio suman +3/tick.
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        gs_bind_catalog(*g, cat);
        g->player_stock[0][0] = 100;
        RawCommand place = place_building(0, 0, 1, 0, 10, 10);
        step(*g, &place, 1);

        RawCommand batch[3];
        for (uint32_t k = 0; k < 3; ++k) {
            batch[k] = spawn_citizen(1, 0, 2 + k, 1 + k,
                                     10 * 65536 + 32768, 10 * 65536 + 32768);
        }
        step(*g, batch, 3);
        RawCommand assigns[3];
        for (uint32_t k = 0; k < 3; ++k) {
            assigns[k] = assign_build(2, 0, 5 + k,
                                      EntityHandle{1 + k, g->entities.generation[1 + k]}, 10, 10);
        }
        step(*g, assigns, 3);
        // Mismo tick del assign: los 3 ciudadanos ya estaban en el sitio, así
        // que construction_system suma +1 por cada uno EN ESTE mismo tick
        // (recorrido ascendente): progreso += 3 de una vez ("progreso suma").
        CHECK(g->build_progress[0] == 3u);
        step(*g, nullptr, 0);  // +3 más, satura en T=5
        CHECK(g->build_progress[0] == 5u);
    }
}

// ============================================================================
// 5) Dropoff a edificio vs fallback legacy (§6).
// ============================================================================
static void test_dropoff_building_vs_fallback() {
    static DataCatalogV1 cat = fixture::make_catalog();

    // 5a) Sin edificios: fallback legacy — el ciudadano entrega en
    // dropoff_x/y[owner] (comportamiento histórico, escenarios golden).
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        gs_bind_catalog(*g, cat);
        // Ciudadano YA cargando A, en RETURN, lejos del dropoff legacy.
        RawCommand spawn = spawn_citizen(0, 0, 1, 0, 100 * 65536, 100 * 65536);
        step(*g, &spawn, 1);
        g->eco_state[0] = EcoState::RETURN;
        g->eco_carry[0] = 50;
        g->eco_carry_resource[0] = 0;  // A
        const int64_t legacy_x = g->dropoff_x[0], legacy_y = g->dropoff_y[0];
        for (int k = 0; k < 400 && g->eco_state[0] != EcoState::SEEK; ++k) step(*g, nullptr, 0);
        CHECK(g->eco_state[0] == EcoState::SEEK);
        CHECK(g->player_stock[0][0] == 50);
        (void)legacy_x; (void)legacy_y;
    }

    // 5b) Con un "center" (BuildingId 1, dropoff A|B|Me, nace completo por la
    // exención de escenario) MÁS CERCA que el fallback legacy, el ciudadano
    // entrega ahí en vez de en dropoff_x/y[owner].
    {
        auto g = std::make_unique<GameState>();
        MatchConfig01A cfg = make_cfg();
        gs_init(*g, cfg);
        gs_bind_catalog(*g, cat);
        // Coloca el center en (100,100) — lejos del dropoff legacy (tile ~20).
        RawCommand place = place_building(0, 0, 1, 1 /*center*/, 100, 100);
        step(*g, &place, 1);
        CHECK(g->entities.alive[0] == 1 && g->entity_kind[0] == 1);
        CHECK(g->build_progress[0] >= g->catalog->buildings[1].build_time_ticks);

        RawCommand spawn = spawn_citizen(1, 0, 2, 1, 100 * 65536, 96 * 65536);
        step(*g, &spawn, 1);
        g->eco_state[1] = EcoState::RETURN;
        g->eco_carry[1] = 30;
        g->eco_carry_resource[1] = 0;  // A

        for (int k = 0; k < 100 && g->eco_state[1] != EcoState::SEEK; ++k) step(*g, nullptr, 0);
        CHECK(g->eco_state[1] == EcoState::SEEK);
        CHECK(g->player_stock[0][0] == 30);
        // El citizen NUNCA se alejó hacia el dropoff legacy (tile ~20): su
        // posición final quedó pegada al footprint del center (tiles 100-101).
        CHECK(g->pos_x[1] >= 99 * 65536 && g->pos_x[1] <= 103 * 65536);
    }
}

// ============================================================================
// 6) Destrucción libera cost_grid + combate contra edificios (targeting/RPS).
// ============================================================================
static void test_destruction_and_combat() {
    static DataCatalogV1 cat = fixture::make_catalog();

    // 6a) Combate: infantry (debug path) ataca y destruye un edificio;
    // targeting válido, muerte recicla el slot y restaura cost_grid=1.
    auto g = std::make_unique<GameState>();
    MatchConfig01A cfg = make_cfg();
    cfg.allow_debug_stat_payload = 1;  // debug path para el atacante (fuera de catálogo)
    gs_init(*g, cfg);
    gs_bind_catalog(*g, cat);
    g->player_stock[0][0] = 100;

    RawCommand place = place_building(0, 1 /*owner 1, enemigo*/, 1, 0, 10, 10);
    step(*g, &place, 1);
    CHECK(g->entities.alive[0] == 1 && g->hp[0] == 300);
    CHECK(g->cost_grid[10 * FF_AXIS + 10] == FF_WALL);

    // Artillery (unit_class=2) del owner 0, HP alto de daño, junto al edificio
    // (dentro del rango de ataque) para forzar varios impactos deterministas.
    RawCommand atk{};
    std::memset(&atk, 0, sizeof(atk));
    atk.target_tick = 1; atk.emitter = 0; atk.type = CommandType::SPAWN_UNIT; atk.sequence = 2;
    atk.p.handle = EntityHandle{1, 1};
    atk.p.x_raw = 9 * 65536; atk.p.y_raw = 9 * 65536;  // junto al footprint 10..12
    atk.p.hp = 999; atk.p.attack = 400; atk.p.range_mt = 3000; atk.p.unit_class = 2;  // artillery
    atk.p.speed_mtpt = 0;
    atk.p.unit_id = INVALID_UNIT_ID;
    step(*g, &atk, 1);
    CHECK(g->entities.alive[1] == 1);

    // RPS artillery vs edificio = x2.0 (20000bp): daño/tick = 400*2 = 800 > hp(300).
    const StepResult r = step(*g, nullptr, 0);
    (void)r;
    CHECK(g->entities.alive[0] == 0);              // el edificio murió y se recicló
    CHECK(g->cost_grid[10 * FF_AXIS + 10] == 1u);   // footprint restaurado a transitable
    CHECK(g->cost_grid[11 * FF_AXIS + 11] == 1u);

    // 6b) Una unidad atraviesa el tile donde estaba el edificio: MOVE_TO a
    // través de esa zona no queda bloqueada (cost_grid ya no es FF_WALL). El
    // atacante nació con speed_mtpt=0 (a propósito, para quedarse disparando
    // sin perseguir); se le da velocidad AHORA para poder verificar el
    // tránsito (mutación de estado directa, test-only — no pasa por comando).
    g->speed_mtpt[1] = 500;
    RawCommand mv{};
    std::memset(&mv, 0, sizeof(mv));
    mv.target_tick = 3; mv.emitter = 0; mv.type = CommandType::MOVE_TO; mv.sequence = 3;
    mv.p.handle = EntityHandle{1, g->entities.generation[1]};
    mv.p.x_raw = 11 * 65536 + 32768; mv.p.y_raw = 11 * 65536 + 32768;  // centro del ex-footprint
    const StepResult rmv = step(*g, &mv, 1);
    CHECK(rmv.accepted == 1);
    for (int k = 0; k < 5; ++k) step(*g, nullptr, 0);
    // Llegó (o está cerca) del punto que antes era un muro — sin quedar
    // "atascada": basta con que la posición avanzó desde su origen.
    CHECK(g->pos_x[1] != 9 * 65536 || g->pos_y[1] != 9 * 65536);
}

// ============================================================================
// 7) Save/load v8 round-trip con un edificio a medio construir.
// ============================================================================
static void test_save_load_v8_mid_construction() {
    static DataCatalogV1 cat = fixture::make_catalog();

    auto g1 = std::make_unique<GameState>();
    gs_init(*g1, make_cfg());
    gs_bind_catalog(*g1, cat);
    g1->player_stock[0][0] = 100;
    RawCommand place = place_building(0, 0, 1, 0, 10, 10);
    step(*g1, &place, 1);
    RawCommand spawn = spawn_citizen(1, 0, 2, 1, 10 * 65536 + 32768, 10 * 65536 + 32768);
    step(*g1, &spawn, 1);
    RawCommand assign = assign_build(2, 0, 3, EntityHandle{1, g1->entities.generation[1]}, 10, 10);
    step(*g1, &assign, 1);
    // El tick del assign ya sumó +1 (ciudadano ya en sitio); 1 tick más deja
    // el progreso a medias (2 de 5).
    step(*g1, nullptr, 0);
    CHECK(g1->build_progress[0] == 2u);
    CHECK(g1->build_progress[0] < g1->catalog->buildings[0].build_time_ticks);

    AiJobBox box{}; ai_box_init(box, 1);
    AiRuntimeV1 rt{};
    CHECK(save_game(*g1, box, rt, "test_buildings_v8.sav") == 0);

    auto g2 = std::make_unique<GameState>();
    AiJobBox box2{}; AiRuntimeV1 rt2{};
    CHECK(load_game(*g2, box2, rt2, "test_buildings_v8.sav") == 0);
    gs_bind_catalog(*g2, cat);  // el binding runtime se re-enlaza aparte (no se serializa)

    CHECK(g2->entity_kind[0] == 1);
    CHECK(g2->building_id[0] == 0);
    CHECK(g2->build_progress[0] == 2u);
    CHECK(g2->bld_anchor_tx[0] == 10 && g2->bld_anchor_ty[0] == 10);
    CHECK(g2->build_target[1] == 0u);
    CHECK(state_checksum_v1(*g1) == state_checksum_v1(*g2));

    // Continuar 3 ticks más en AMBOS produce el mismo resultado (completo).
    for (int k = 0; k < 3; ++k) step(*g1, nullptr, 0);
    for (int k = 0; k < 3; ++k) step(*g2, nullptr, 0);
    CHECK(g1->build_progress[0] == 5u && g2->build_progress[0] == 5u);
    CHECK(state_checksum_v1(*g1) == state_checksum_v1(*g2));

    std::remove("test_buildings_v8.sav");
}

// ============================================================================
// 8) G3/G4/G5-equivalente (SPEC-004 §9.2): determinismo + save/continue +
// record/replay-equivalente para un escenario que incluye PLACE_BUILDING y
// ASSIGN_BUILD. Ver nota de cabecera sobre el truco BuildingId=0/UnitId=0.
// ============================================================================
static uint32_t build_scenario_batch(RawCommand* out, uint32_t t) {
    uint32_t n = 0;
    if (t == 0u) {
        out[n++] = place_building(0, 0, 1, 0 /*hut*/, 10, 10);
        out[n++] = spawn_citizen(0, 0, 2, 1, 10 * 65536 + 32768, 10 * 65536 + 32768);
    } else if (t == 1u) {
        out[n++] = assign_build(1, 0, 3, EntityHandle{1, 1}, 10, 10);
    }
    return n;
}

static uint64_t run_scenario(uint32_t ticks, ReplayWriter* rec) {
    static DataCatalogV1 cat = fixture::make_catalog();
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg());
    gs_bind_catalog(*g, cat);
    g->player_stock[0][0] = 100;

    RawCommand batch[4];
    uint64_t last_ck = 0;
    for (uint32_t t = 0; t < ticks; ++t) {
        const uint32_t n = build_scenario_batch(batch, t);
        if (rec != nullptr) rec->tick_batch(batch, n, t);
        const StepResult r = step(*g, batch, n);
        if (r.checksum_computed) last_ck = r.checksum;
    }
    return last_ck;
}

static void test_gate_equivalents() {
    // 8a) G1-estilo: determinismo puro — dos corridas idénticas, mismo checksum.
    const uint64_t ck1 = run_scenario(20, nullptr);
    const uint64_t ck2 = run_scenario(20, nullptr);
    CHECK(ck1 == ck2);

    // 8b) G3-estilo: guardar a mitad de escenario y continuar == corrida continua.
    {
        static DataCatalogV1 cat = fixture::make_catalog();
        auto ga = std::make_unique<GameState>();
        gs_init(*ga, make_cfg());
        gs_bind_catalog(*ga, cat);
        ga->player_stock[0][0] = 100;
        RawCommand batch[4];
        AiJobBox box{}; ai_box_init(box, 1); AiRuntimeV1 rt{};
        for (uint32_t t = 0; t < 20u; ++t) {
            if (t == 8u) CHECK(save_game(*ga, box, rt, "test_buildings_gate.sav") == 0);
            const uint32_t n = build_scenario_batch(batch, t);
            step(*ga, batch, n);
        }
        const uint64_t continuous_ck = state_checksum_v1(*ga);

        auto gb = std::make_unique<GameState>();
        AiJobBox box2{}; AiRuntimeV1 rt2{};
        CHECK(load_game(*gb, box2, rt2, "test_buildings_gate.sav") == 0);
        gs_bind_catalog(*gb, cat);
        for (uint32_t t = gb->tick; t < 20u; ++t) {
            RawCommand batch2[4];
            const uint32_t n = build_scenario_batch(batch2, t);
            step(*gb, batch2, n);
        }
        CHECK(continuous_ck == state_checksum_v1(*gb));
        std::remove("test_buildings_gate.sav");
    }

    // 8c) G5-estilo: grabar con ReplayWriter y reproducir con replay_load,
    // incluyendo PLACE_BUILDING+ASSIGN_BUILD. Nota histórica (Sprint 1.1): el
    // escenario usa deliberadamente BuildingId=0 y UnitId=0 para que la
    // NO-serialización de CmdPayload::unit_id en replay v2 no afectara el
    // resultado (gap heredado, no tocado en ESE sprint). Sprint 1.2
    // (SPEC-004 §10.1) cierra ese gap: ReplayWriter graba SIEMPRE v3 desde
    // ahora, así que el truco de índice 0 ya no es necesario para que este
    // test pase — se conserva solo porque sigue siendo un escenario válido
    // (ver test_replay_v3.cpp para la cobertura dedicada de un BuildingId/
    // UnitId != 0 bajo v3, incluida la comparación explícita contra v2).
    {
        ReplayWriter rec;
        rec.begin(777ull, 0u, 20u, 1u, 0u, 20u);
        const uint64_t rec_ck = run_scenario(20, &rec);
        CHECK(rec.finish(rec_ck, "test_buildings_gate.curp") == 0);

        ReplayData data;
        CHECK(replay_load("test_buildings_gate.curp", data) == 0);
        CHECK(data.version == 3u);
        CHECK(data.legacy_payload_loss == 0u);

        static DataCatalogV1 cat = fixture::make_catalog();
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        gs_bind_catalog(*g, cat);
        g->player_stock[0][0] = 100;
        uint64_t replay_ck = 0;
        uint32_t schedule_mismatches = 0;
        for (uint32_t t = 0; t < data.ticks; ++t) {
            const auto& b = data.batches[t];
            for (size_t i = 0; i < b.size(); ++i) {
                const uint32_t recomputed = command_effective_tick(
                        b[i].target_tick, t, g->cfg.human_input_delay_ticks);
                if (t < data.eff_ticks.size() && i < data.eff_ticks[t].size()
                    && recomputed != data.eff_ticks[t][i]) {
                    ++schedule_mismatches;
                }
            }
            const StepResult r = step(*g, b.data(), static_cast<uint32_t>(b.size()));
            if (r.checksum_computed) replay_ck = r.checksum;
        }
        CHECK(schedule_mismatches == 0);
        CHECK(replay_ck == data.final_checksum);
        CHECK(replay_ck == rec_ck);
        // Confirma que el edificio/ciudadano quedaron como se pretendía (el
        // truco de índice 0 no enmascaró un fallo real de aplicación).
        CHECK(g->entity_kind[0] == 1 && g->building_id[0] == 0);
        CHECK(g->build_target[1] == BUILD_NO_TARGET || g->build_target[1] == 0u);
        std::remove("test_buildings_gate.curp");
    }
}

int main() {
    test_place_building_validation();
    test_scenario_exemption();
    test_assign_build_validation();
    test_construction_progress();
    test_dropoff_building_vs_fallback();
    test_destruction_and_combat();
    test_save_load_v8_mid_construction();
    test_gate_equivalents();

    if (g_fails == 0) { std::printf("buildings: OK\n"); return 0; }
    std::printf("buildings: %d fallos\n", g_fails);
    return 1;
}
