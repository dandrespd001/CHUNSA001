// Test del comando GATHER + agotamiento/reasignación determinista (Sprint
// 1.6B, pieza K2, SPEC-004 §18). Autor: sonnet-5 (brief
// docs/briefs/SONNET_K2_GATHER_APERTURA_SPRINT_1.6B.md).
//
// Cubre: GATHER camino feliz + CADA rechazo EN ORDEN (INVALID_ENTITY handle
// muerto, NOT_OWNER, ILLEGAL_STATE unit_class!=3, INVALID_ENTITY sin depósito
// en radio) · GATHER cancela build_target · GATHER redirige un ciudadano que
// YA está recolectando otro recurso · agotamiento -> reasignación al MISMO
// recurso (economy.hpp::eco_find_nearest_deposit, nivel unitario) -> a
// cualquiera si no queda ninguno de ese recurso -> ocioso (ECO_NO_DEPOSIT) si
// no queda ninguno.
//
// NOTA: GameState SIEMPRE en heap (make_unique) — un GameState en pila
// segfaultea bajo ctest (lección K1, ver game_state.hpp).
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>

#include "chunsa/data_catalog.hpp"
#include "chunsa/game_state.hpp"
#include "chunsa/step.hpp"
#include "chunsa/economy.hpp"

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

static RejectReason last_result(const ReceiptMailbox& m) {
    return m.ring[(m.head + m.count - 1) % MAILBOX_CAP].result;
}

// ============================================================================
// Fixture: units[0]="citizen" (Citizen); units[1]="soldier" (Infantry, para
// ejercitar el rechazo ILLEGAL_STATE de GATHER sobre una entidad no-ciudadana
// que SÍ es un handle vivo y propio). buildings[0]="center" (nace completo,
// footprint 2x2, trains=[citizen]).
// ============================================================================
namespace gather_fixture {

inline UnitDefinitionV1 make_citizen() {
    UnitDefinitionV1 d{};
    d.id = 0; d.unit_class = UnitClassV1::Citizen; d.tags_mask = 0;
    d.hp = 20; d.attack = 0; d.range_millitiles = 0;
    d.speed_millitile_tick = 400; d.morale = 100; d.build_time_ticks = 1;
    for (int k = 0; k < 6; ++k) d.bonus_vs_bp[k] = 0;
    d.cost_a = 5; d.cost_b = 0; d.cost_me = 0; d.pop_cost = 1;
    d.epoch_min = 1; d.epoch_max = 15;
    return d;
}
inline UnitDefinitionV1 make_soldier() {
    UnitDefinitionV1 d{};
    d.id = 1; d.unit_class = UnitClassV1::Infantry; d.tags_mask = 0;
    d.hp = 50; d.attack = 10; d.range_millitiles = 1000;
    d.speed_millitile_tick = 400; d.morale = 100; d.build_time_ticks = 2;
    for (int k = 0; k < 6; ++k) d.bonus_vs_bp[k] = 0;
    d.cost_a = 10; d.cost_b = 0; d.cost_me = 0; d.pop_cost = 1;
    d.epoch_min = 1; d.epoch_max = 15;
    return d;
}
inline BuildingDefinitionV1 make_center() {
    BuildingDefinitionV1 d{};
    d.id = 0; d.hp = 500; d.footprint_w = 2; d.footprint_h = 2;
    d.build_time_ticks = 0;
    d.cost_a = 0; d.cost_b = 0; d.cost_me = 0;
    d.dropoff_mask = 0; d.constructible = 0;
    d.epoch_min = 1; d.epoch_max = 15;
    d.trains[0] = 0 /*citizen*/; d.train_count = 1;
    for (uint32_t k = 1; k < PROD_TRAINS_MAX; ++k) d.trains[k] = INVALID_UNIT_ID;
    for (uint32_t k = 0; k < PROD_TECHS_MAX; ++k) d.researches[k] = INVALID_TECH_ID;
    d.research_count = 0;
    for (uint32_t k = 0; k < BUILDING_REQCAP_MAX; ++k) d.required_capabilities[k] = INVALID_CAPABILITY_ID;
    d.required_capabilities_count = 0;
    return d;
}
// Sitio construible con build_time_ticks > 0 (nace INCOMPLETO) — dedicado a
// test_gather_cancels_build_target: ASSIGN_BUILD exige un edificio con
// build_progress < build_time_ticks (§4.1), lo que "center" (nace completo,
// build_time_ticks=0) nunca satisface.
inline BuildingDefinitionV1 make_site() {
    BuildingDefinitionV1 d{};
    d.id = 1; d.hp = 300; d.footprint_w = 2; d.footprint_h = 2;
    d.build_time_ticks = 50;
    d.cost_a = 0; d.cost_b = 0; d.cost_me = 0;
    d.dropoff_mask = 0; d.constructible = 1;
    d.epoch_min = 1; d.epoch_max = 15;
    for (uint32_t k = 0; k < PROD_TRAINS_MAX; ++k) d.trains[k] = INVALID_UNIT_ID;
    d.train_count = 0;
    for (uint32_t k = 0; k < PROD_TECHS_MAX; ++k) d.researches[k] = INVALID_TECH_ID;
    d.research_count = 0;
    for (uint32_t k = 0; k < BUILDING_REQCAP_MAX; ++k) d.required_capabilities[k] = INVALID_CAPABILITY_ID;
    d.required_capabilities_count = 0;
    return d;
}

static UnitDefinitionV1 g_units[2] = { make_citizen(), make_soldier() };
static BuildingDefinitionV1 g_buildings[2] = { make_center(), make_site() };

inline DataCatalogV1 make_catalog() {
    DataCatalogV1 c{};
    c.unit_count = 2; c.units = g_units; c.unit_names = nullptr;
    c.building_count = 2; c.buildings = g_buildings; c.building_names = nullptr;
    c.tech_count = 0; c.techs = nullptr; c.tech_names = nullptr;
    c.capability_count = 0; c.capability_names = nullptr;
    return c;
}

}  // namespace gather_fixture

static MatchConfig01A make_cfg() {
    MatchConfig01A cfg{};
    cfg.max_entities = 64;
    cfg.player_count = 2;
    cfg.human_input_delay_ticks = 0;
    cfg.max_future_command_ticks = 20;
    cfg.checksum_every_ticks = 1;
    cfg.map_tiles_x = 256; cfg.map_tiles_y = 256;
    cfg.seed = 20260724ull;
    cfg.allow_debug_stat_payload = 0;
    return cfg;
}

static RawCommand place_building(uint32_t tick, uint16_t emitter, uint64_t seq,
                                 BuildingId bid, int64_t tx, int64_t ty) {
    RawCommand c{};
    std::memset(&c, 0, sizeof(c));
    c.target_tick = tick; c.emitter = emitter; c.type = CommandType::PLACE_BUILDING;
    c.sequence = seq; c.p.unit_id = bid; c.p.x_raw = tx; c.p.y_raw = ty;
    return c;
}
static RawCommand spawn_citizen(uint32_t tick, uint16_t emitter, uint64_t seq,
                                int64_t x_raw, int64_t y_raw) {
    RawCommand c{};
    std::memset(&c, 0, sizeof(c));
    c.target_tick = tick; c.emitter = emitter; c.type = CommandType::SPAWN_CITIZEN;
    c.sequence = seq; c.p.unit_id = 0 /*citizen*/; c.p.x_raw = x_raw; c.p.y_raw = y_raw;
    return c;
}
static RawCommand spawn_soldier(uint32_t tick, uint16_t emitter, uint64_t seq,
                                int64_t x_raw, int64_t y_raw) {
    RawCommand c{};
    std::memset(&c, 0, sizeof(c));
    c.target_tick = tick; c.emitter = emitter; c.type = CommandType::SPAWN_UNIT;
    c.sequence = seq; c.p.unit_id = 1 /*soldier*/; c.p.x_raw = x_raw; c.p.y_raw = y_raw;
    return c;
}
static RawCommand assign_build(uint32_t tick, uint16_t emitter, uint64_t seq,
                               EntityHandle citizen, int64_t tile_x, int64_t tile_y) {
    RawCommand c{};
    std::memset(&c, 0, sizeof(c));
    c.target_tick = tick; c.emitter = emitter; c.type = CommandType::ASSIGN_BUILD;
    c.sequence = seq; c.p.handle = citizen; c.p.x_raw = tile_x; c.p.y_raw = tile_y;
    return c;
}
static RawCommand gather(uint32_t tick, uint16_t emitter, uint64_t seq,
                         EntityHandle citizen, int64_t x_raw, int64_t y_raw) {
    RawCommand c{};
    std::memset(&c, 0, sizeof(c));
    c.target_tick = tick; c.emitter = emitter; c.type = CommandType::GATHER;
    c.sequence = seq; c.p.handle = citizen; c.p.x_raw = x_raw; c.p.y_raw = y_raw;
    return c;
}

// ============================================================================
// 1) GATHER camino feliz: ciudadano propio + punto sobre un depósito vivo ->
//    ACCEPTED, eco_assigned_deposit fijado, eco_state=SEEK.
// ============================================================================
static void test_gather_happy_path() {
    static DataCatalogV1 cat = gather_fixture::make_catalog();
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg());
    gs_bind_catalog(*g, cat);
    gs_init_epoch_from_catalog(*g);

    // gs_init ya deja el patrón legacy de 6 depósitos (gs_init_economy):
    // deposits[0] = A en (40,40).
    const int64_t T = FX_ONE_RAW;
    RawCommand setup = spawn_citizen(0, 0, 1, 100 * T, 100 * T);
    const StepResult r0 = step(*g, &setup, 1);
    CHECK(r0.accepted == 1);
    const EntityHandle citizen{0, g->entities.generation[0]};
    // NOTA: NO se comprueba eco_assigned_deposit==ECO_NO_DEPOSIT aquí — el
    // mismo step() que aplica el SPAWN_CITIZEN ya corrió economy_system
    // sobre la entidad recién nacida (economía autónoma de Sprint 0.3, sin
    // relación con GATHER): al terminar r0 el ciudadano YA tiene un depósito
    // auto-asignado (el más cercano de cualquier recurso). Lo que este test
    // ejercita es que un GATHER explícito puede REDIRIGIRLO al depósito 0.

    // Punto EXACTO del depósito 0 (40.5, 40.5 tiles en raw).
    const int64_t dep_x = g->deposits[0].x_raw;
    const int64_t dep_y = g->deposits[0].y_raw;
    RawCommand cmd = gather(g->tick, 0, 2, citizen, dep_x, dep_y);
    const StepResult r1 = step(*g, &cmd, 1);
    CHECK(r1.accepted == 1);
    CHECK(last_result(g->mailbox[0]) == RejectReason::ACCEPTED);
    CHECK(g->eco_assigned_deposit[0] == 0u);
    CHECK(g->eco_state[0] == EcoState::SEEK);
}

// ============================================================================
// 2) Rechazos EN ORDEN (contrato §18): INVALID_ENTITY (handle muerto) ·
//    NOT_OWNER · ILLEGAL_STATE (unit_class != 3) · INVALID_ENTITY (ningún
//    depósito con remaining>0 dentro de GATHER_PICK_RADIUS_RAW del punto).
// ============================================================================
static void test_gather_rejections_in_order() {
    static DataCatalogV1 cat = gather_fixture::make_catalog();
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg());
    gs_bind_catalog(*g, cat);
    gs_init_epoch_from_catalog(*g);
    const int64_t T = FX_ONE_RAW;

    // 2a) INVALID_ENTITY: handle que nunca existió.
    {
        RawCommand cmd = gather(g->tick, 0, 1, EntityHandle{5, 1}, 0, 0);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::INVALID_ENTITY);
    }

    // Setup: ciudadano del jugador 0, soldado del jugador 1.
    RawCommand setup[2] = {
        spawn_citizen(0, 0, 2, 100 * T, 100 * T),
        spawn_soldier(0, 1, 1, 110 * T, 110 * T),
    };
    const StepResult r0 = step(*g, setup, 2);
    CHECK(r0.accepted == 2);
    const EntityHandle citizen0{0, g->entities.generation[0]};
    const EntityHandle soldier1{1, g->entities.generation[1]};

    // 2b) NOT_OWNER: jugador 1 intenta GATHER sobre el ciudadano del jugador 0.
    {
        RawCommand cmd = gather(g->tick, 1, 2, citizen0, 0, 0);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[1]) == RejectReason::NOT_OWNER);
    }

    // 2c) ILLEGAL_STATE: unit_class != 3 (el soldado del jugador 1, su propio dueño).
    {
        RawCommand cmd = gather(g->tick, 1, 3, soldier1, 0, 0);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[1]) == RejectReason::ILLEGAL_STATE);
    }

    // 2d) INVALID_ENTITY: punto lejos de TODO depósito vivo (esquina vacía
    // del mapa, > GATHER_PICK_RADIUS_RAW de cualquiera de los 6 fijos).
    {
        RawCommand cmd = gather(g->tick, 0, 3, citizen0, 250 * T, 2 * T);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::INVALID_ENTITY);
    }

    // 2e) INVALID_ENTITY: depósito EN RADIO pero agotado (remaining<=0).
    {
        g->deposits[0].remaining = 0;  // agota el depósito 0 (A, en 40,40)
        RawCommand cmd = gather(g->tick, 0, 4, citizen0, g->deposits[0].x_raw, g->deposits[0].y_raw);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::INVALID_ENTITY);
    }
}

// ============================================================================
// 3) GATHER cancela build_target (SPEC-004 §18: "recolectar cancela
//    construir" — decisión explícita del contrato).
// ============================================================================
static void test_gather_cancels_build_target() {
    static DataCatalogV1 cat = gather_fixture::make_catalog();
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg());
    gs_bind_catalog(*g, cat);
    gs_init_epoch_from_catalog(*g);
    const int64_t T = FX_ONE_RAW;

    RawCommand setup[2] = {
        place_building(0, 0, 1, 1 /*site, incompleto*/, 10, 10),
        spawn_citizen(0, 0, 2, 12 * T, 12 * T),
    };
    const StepResult r0 = step(*g, setup, 2);
    CHECK(r0.accepted == 2);
    const EntityHandle citizen0{1, g->entities.generation[1]};

    RawCommand assign = assign_build(g->tick, 0, 3, citizen0, 10, 10);
    const StepResult r1 = step(*g, &assign, 1);
    CHECK(r1.accepted == 1);
    CHECK(g->build_target[1] == 0u);  // apunta al sitio (índice 0)

    RawCommand cmd = gather(g->tick, 0, 4, citizen0, g->deposits[0].x_raw, g->deposits[0].y_raw);
    const StepResult r2 = step(*g, &cmd, 1);
    CHECK(r2.accepted == 1);
    CHECK(g->build_target[1] == BUILD_NO_TARGET);  // GATHER canceló la orden de construir
    CHECK(g->eco_assigned_deposit[1] == 0u);
    CHECK(g->eco_state[1] == EcoState::SEEK);
}

// ============================================================================
// 4) GATHER redirige: un ciudadano YA recolectando el depósito 0 (A) recibe
//    una orden explícita hacia el depósito 4 (Me, índice 4 en el patrón
//    legacy de gs_init_economy: {40,40,A} {216,216,A} {40,216,B} {216,40,B}
//    {128,40,Me} {128,216,Me}) — el comando SIEMPRE gana sobre lo que el
//    ciudadano estuviera haciendo antes.
// ============================================================================
static void test_gather_redirects_active_gatherer() {
    static DataCatalogV1 cat = gather_fixture::make_catalog();
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg());
    gs_bind_catalog(*g, cat);
    gs_init_epoch_from_catalog(*g);
    const int64_t T = FX_ONE_RAW;

    RawCommand setup = spawn_citizen(0, 0, 1, 100 * T, 100 * T);
    const StepResult r0 = step(*g, &setup, 1);
    CHECK(r0.accepted == 1);
    const EntityHandle citizen0{0, g->entities.generation[0]};

    RawCommand gather_a = gather(g->tick, 0, 2, citizen0, g->deposits[0].x_raw, g->deposits[0].y_raw);
    const StepResult r1 = step(*g, &gather_a, 1);
    CHECK(r1.accepted == 1);
    CHECK(g->eco_assigned_deposit[0] == 0u);

    // Redirección explícita al depósito 4 (Me, en (128,40) del patrón legacy).
    RawCommand gather_me = gather(g->tick, 0, 3, citizen0, g->deposits[4].x_raw, g->deposits[4].y_raw);
    const StepResult r2 = step(*g, &gather_me, 1);
    CHECK(r2.accepted == 1);
    CHECK(g->eco_assigned_deposit[0] == 4u);
    CHECK(g->deposits[4].resource_idx == 2u);  // Me
}

// ============================================================================
// 5) Agotamiento -> reasignación determinista (SPEC-004 §18, nivel unitario
//    de economy.hpp, mismo patrón que test_economy.cpp):
//    a) el depósito asignado (A, cercano) se agota, pero OTRO depósito del
//       MISMO recurso (A, más lejos) sigue vivo -> reasigna a ESE, aunque un
//       depósito de OTRO recurso esté más cerca (la preferencia de recurso
//       gana sobre la proximidad — el contrato explícito de §18).
//    b) si el segundo A TAMBIÉN se agota, reasigna al más cercano de
//       CUALQUIER recurso vivo (B, en este fixture).
//    c) si B también se agota, queda ocioso (ECO_NO_DEPOSIT).
// ============================================================================
static void test_exhaustion_prefers_same_resource_then_any_then_idle() {
    constexpr int64_t T = FX_ONE_RAW;
    FatalReason fatal = FatalReason::NONE;

    // Depósito 0: A, en el origen, YA agotado (remaining=0).
    // Depósito 1: B, a 3 tiles — MÁS CERCA que el depósito 2, pero recurso distinto.
    // Depósito 2: A, a 10 tiles — MISMO recurso que el agotado, más lejos que B.
    EcoDeposit deposits[3] = {
        {0, 0, /*A*/0u, 0},
        {3 * T, 0, /*B*/1u, 50},
        {10 * T, 0, /*A*/0u, 50},
    };

    EcoCitizenIn in{};
    in.pos_x = 0; in.pos_y = 0;
    in.state = EcoState::SEEK;
    in.assigned_deposit = 0u;  // el A agotado
    in.carry = 0; in.carry_resource_idx = 0;
    in.speed_mtpt = 100;

    // a) Reasigna al A lejano (índice 2), NO al B cercano (índice 1).
    EcoCitizenOut out_a = eco_step_citizen(in, deposits, 3, 20 * T, 20 * T, fatal);
    CHECK(fatal == FatalReason::NONE);
    CHECK(out_a.assigned_deposit == 2u);

    // b) Ese A también se agota -> reasigna al más cercano de CUALQUIERA (B, índice 1).
    deposits[2].remaining = 0;
    EcoCitizenIn in_b = in;
    in_b.assigned_deposit = 2u;
    EcoCitizenOut out_b = eco_step_citizen(in_b, deposits, 3, 20 * T, 20 * T, fatal);
    CHECK(fatal == FatalReason::NONE);
    CHECK(out_b.assigned_deposit == 1u);

    // c) B también se agota -> ocioso (ECO_NO_DEPOSIT), sin más movimiento.
    deposits[1].remaining = 0;
    EcoCitizenIn in_c = in;
    in_c.assigned_deposit = 1u;
    EcoCitizenOut out_c = eco_step_citizen(in_c, deposits, 3, 20 * T, 20 * T, fatal);
    CHECK(fatal == FatalReason::NONE);
    CHECK(out_c.assigned_deposit == ECO_NO_DEPOSIT);
    CHECK(out_c.vel_x == 0 && out_c.vel_y == 0);

    // Control directo de eco_find_nearest_deposit con preferencia explícita:
    // preferir A cuando SOLO el B cercano vive -> cae a "cualquiera" (B).
    deposits[1].remaining = 50; deposits[2].remaining = 0;
    const uint32_t idx = eco_find_nearest_deposit(deposits, 3, 0, 0, /*preferred=*/0u, fatal);
    CHECK(idx == 1u);  // no queda A vivo -> el más cercano de cualquiera (B)
    // Con preferencia ECO_ANY_RESOURCE, el resultado es el legacy (más cercano, cualquiera).
    const uint32_t idx_any = eco_find_nearest_deposit(deposits, 3, 0, 0, ECO_ANY_RESOURCE, fatal);
    CHECK(idx_any == 1u);
}

int main() {
    test_gather_happy_path();
    test_gather_rejections_in_order();
    test_gather_cancels_build_target();
    test_gather_redirects_active_gatherer();
    test_exhaustion_prefers_same_resource_then_any_then_idle();

    if (g_fails == 0) { std::printf("gather: OK\n"); return 0; }
    std::printf("gather: %d fallos\n", g_fails);
    return 1;
}
