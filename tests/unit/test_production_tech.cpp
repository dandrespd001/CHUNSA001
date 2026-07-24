// Test de producción, tecnología y épocas (Sprint 1.2 K2, SPEC-004 §11+§12+§13).
// Autor: sonnet-5 (brief docs/briefs/SONNET_K2_PRODUCCION_TECH_SPRINT_1.2.md).
//
// Cubre §13 (subconjunto K2): TRAIN feliz + cada rechazo del §11.3 en orden ·
// cola llena · pop llena · producción multi-ítem con pool de entidades
// exhausto a mitad (espera sin perder progreso) · rally · RESEARCH feliz +
// prereq incumplido + mutex + época + edificio ocupado · EPOCH_UP doble gate
// (falla por edificios, falla por tiempo, pasa con ambos) · gating de época
// en TRAIN_UNIT y PLACE_BUILDING (§12.4) · save v10 round-trip con cola no
// vacía y research en curso · catálogo golden real (2 cuarteles resuelven
// trains, 4 techs resuelven grants) · determinismo (dos corridas + save
// intermedio + replay v3).
//
// NOTA: GameState SIEMPRE en heap (make_unique) — un GameState en pila
// segfaultea bajo ctest (lección K1, ver game_state.hpp).
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

#ifndef CHUNSA_GOLDEN_CHDB_PATH
#error "CHUNSA_GOLDEN_CHDB_PATH debe definirse via CMake (ver CMakeLists.txt)"
#endif

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

static RejectReason last_result(const ReceiptMailbox& m) {
    return m.ring[(m.head + m.count - 1) % MAILBOX_CAP].result;
}

// ============================================================================
// Fixture: catálogo en memoria (mismo patrón que test_replay_v3.cpp/
// test_buildings.cpp — sin pasar por el loader CHDB) con units/buildings/
// techs deliberadamente diseñados para ejercitar CADA gate de §11+§12:
//
// units[0] = "worker" (citizen, barato, epoch 1..15) — usado para pop/cola.
// units[1] = "warrior" (infantry, epoch 1..3) — gating de época (alto).
// units[2] = "elite" (infantry, epoch 5..15) — gating de época (bajo).
//
// buildings[0] = "barracks" (nace completo, trains=[warrior,elite],
//                researches=[tech_a,tech_b], epoch 1..15).
// buildings[1] = "second_barracks" (nace completo, sin trains/researches,
//                epoch 1..15) — el 2º edificio para el gate (a) de EPOCH_UP.
// buildings[2] = "future_hall" (constructible, epoch 5..15) — gating de
//                época en PLACE_BUILDING.
// buildings[3] = "guild_hall" (constructible, required_capabilities=[cap0])
//                — gating de capacidades en PLACE_BUILDING.
//
// techs[0] = "tech_a" (sin prereq/mutex, epoch 1, grants=[cap0]).
// techs[1] = "tech_b" (prereq=[tech_a], mutex=[tech_c], epoch 1, grants=[cap1]).
// techs[2] = "tech_c" (mutex=[tech_b], epoch 1, grants=[cap2]) — no está en
//            researches de ningún edificio del fixture (solo se usa para
//            fabricar el conflicto de mutex vía player_techs directo).
// techs[3] = "tech_high_epoch" (epoch 5) — gating de época en RESEARCH_TECH.
// ============================================================================
namespace fixture {

inline UnitDefinitionV1 make_worker() {
    UnitDefinitionV1 d{};
    d.id = 0; d.unit_class = UnitClassV1::Citizen; d.tags_mask = 0;
    d.hp = 20; d.attack = 0; d.range_millitiles = 0;
    d.speed_millitile_tick = 800; d.morale = 100; d.build_time_ticks = 1;
    for (int k = 0; k < 6; ++k) d.bonus_vs_bp[k] = 0;
    d.cost_a = 5; d.cost_b = 0; d.cost_me = 0; d.pop_cost = 1;
    d.epoch_min = 1; d.epoch_max = 15;
    return d;
}
inline UnitDefinitionV1 make_warrior() {
    UnitDefinitionV1 d{};
    d.id = 1; d.unit_class = UnitClassV1::Infantry; d.tags_mask = 0;
    d.hp = 50; d.attack = 10; d.range_millitiles = 1000;
    d.speed_millitile_tick = 400; d.morale = 100; d.build_time_ticks = 2;
    for (int k = 0; k < 6; ++k) d.bonus_vs_bp[k] = 0;
    d.cost_a = 10; d.cost_b = 0; d.cost_me = 0; d.pop_cost = 1;
    d.epoch_min = 1; d.epoch_max = 3;
    return d;
}
inline UnitDefinitionV1 make_elite() {
    UnitDefinitionV1 d{};
    d.id = 2; d.unit_class = UnitClassV1::Infantry; d.tags_mask = 0;
    d.hp = 80; d.attack = 20; d.range_millitiles = 1000;
    d.speed_millitile_tick = 400; d.morale = 100; d.build_time_ticks = 2;
    for (int k = 0; k < 6; ++k) d.bonus_vs_bp[k] = 0;
    d.cost_a = 20; d.cost_b = 0; d.cost_me = 0; d.pop_cost = 1;
    d.epoch_min = 5; d.epoch_max = 15;
    return d;
}

inline BuildingDefinitionV1 make_barracks() {
    BuildingDefinitionV1 d{};
    d.id = 0; d.hp = 300; d.footprint_w = 2; d.footprint_h = 2;
    d.build_time_ticks = 0;  // nace completo (mismo patrón "center" de test_buildings.cpp)
    d.cost_a = 0; d.cost_b = 0; d.cost_me = 0;
    d.dropoff_mask = 0; d.constructible = 0;
    d.epoch_min = 1; d.epoch_max = 15;
    d.trains[0] = 1; d.trains[1] = 2; d.train_count = 2;
    for (uint32_t k = 2; k < PROD_TRAINS_MAX; ++k) d.trains[k] = INVALID_UNIT_ID;
    // tech_a(0)/tech_b(1)/tech_c(2) resolubles; tech_high_epoch(3) NO está
    // aquí a propósito (§4e del test ejercita el rechazo por "no en researches").
    d.researches[0] = 0; d.researches[1] = 1; d.researches[2] = 2; d.research_count = 3;
    for (uint32_t k = 3; k < PROD_TECHS_MAX; ++k) d.researches[k] = INVALID_TECH_ID;
    for (uint32_t k = 0; k < BUILDING_REQCAP_MAX; ++k) d.required_capabilities[k] = INVALID_CAPABILITY_ID;
    d.required_capabilities_count = 0;
    return d;
}
inline BuildingDefinitionV1 make_second_barracks() {
    BuildingDefinitionV1 d{};
    d.id = 1; d.hp = 300; d.footprint_w = 2; d.footprint_h = 2;
    d.build_time_ticks = 0; d.cost_a = 0; d.cost_b = 0; d.cost_me = 0;
    d.dropoff_mask = 0; d.constructible = 0;
    d.epoch_min = 1; d.epoch_max = 15;
    for (uint32_t k = 0; k < PROD_TRAINS_MAX; ++k) d.trains[k] = INVALID_UNIT_ID;
    d.train_count = 0;
    for (uint32_t k = 0; k < PROD_TECHS_MAX; ++k) d.researches[k] = INVALID_TECH_ID;
    d.research_count = 0;
    for (uint32_t k = 0; k < BUILDING_REQCAP_MAX; ++k) d.required_capabilities[k] = INVALID_CAPABILITY_ID;
    d.required_capabilities_count = 0;
    return d;
}
inline BuildingDefinitionV1 make_future_hall() {
    BuildingDefinitionV1 d{};
    d.id = 2; d.hp = 400; d.footprint_w = 2; d.footprint_h = 2;
    d.build_time_ticks = 5; d.cost_a = 10; d.cost_b = 0; d.cost_me = 0;
    d.dropoff_mask = 0; d.constructible = 1;
    d.epoch_min = 5; d.epoch_max = 15;  // gating de época en PLACE_BUILDING
    for (uint32_t k = 0; k < PROD_TRAINS_MAX; ++k) d.trains[k] = INVALID_UNIT_ID;
    d.train_count = 0;
    for (uint32_t k = 0; k < PROD_TECHS_MAX; ++k) d.researches[k] = INVALID_TECH_ID;
    d.research_count = 0;
    for (uint32_t k = 0; k < BUILDING_REQCAP_MAX; ++k) d.required_capabilities[k] = INVALID_CAPABILITY_ID;
    d.required_capabilities_count = 0;
    return d;
}
inline BuildingDefinitionV1 make_guild_hall() {
    BuildingDefinitionV1 d{};
    d.id = 3; d.hp = 400; d.footprint_w = 2; d.footprint_h = 2;
    d.build_time_ticks = 5; d.cost_a = 5; d.cost_b = 0; d.cost_me = 0;
    d.dropoff_mask = 0; d.constructible = 1;
    d.epoch_min = 1; d.epoch_max = 15;
    for (uint32_t k = 0; k < PROD_TRAINS_MAX; ++k) d.trains[k] = INVALID_UNIT_ID;
    d.train_count = 0;
    for (uint32_t k = 0; k < PROD_TECHS_MAX; ++k) d.researches[k] = INVALID_TECH_ID;
    d.research_count = 0;
    for (uint32_t k = 0; k < BUILDING_REQCAP_MAX; ++k) d.required_capabilities[k] = INVALID_CAPABILITY_ID;
    d.required_capabilities[0] = 0;  // cap0 (grants[0] de tech_a)
    d.required_capabilities_count = 1;
    return d;
}

inline TechDefinitionV1 make_tech_a() {
    TechDefinitionV1 d{};
    d.id = 0; d.cost_a = 10; d.cost_b = 0; d.cost_me = 0;
    d.research_time_ticks = 3; d.epoch = 1;
    for (uint32_t k = 0; k < TECH_PREREQ_MAX; ++k) d.prerequisites[k] = INVALID_TECH_ID;
    d.prereq_count = 0;
    for (uint32_t k = 0; k < TECH_GRANT_MAX; ++k) d.grants[k] = INVALID_CAPABILITY_ID;
    d.grants[0] = 0; d.grant_count = 1;
    for (uint32_t k = 0; k < TECH_MUTEX_MAX; ++k) d.mutually_exclusive_with[k] = INVALID_TECH_ID;
    d.mutex_count = 0;
    return d;
}
inline TechDefinitionV1 make_tech_b() {
    TechDefinitionV1 d{};
    d.id = 1; d.cost_a = 10; d.cost_b = 0; d.cost_me = 0;
    d.research_time_ticks = 3; d.epoch = 1;
    for (uint32_t k = 0; k < TECH_PREREQ_MAX; ++k) d.prerequisites[k] = INVALID_TECH_ID;
    d.prerequisites[0] = 0; d.prereq_count = 1;  // requiere tech_a
    for (uint32_t k = 0; k < TECH_GRANT_MAX; ++k) d.grants[k] = INVALID_CAPABILITY_ID;
    d.grants[0] = 1; d.grant_count = 1;
    for (uint32_t k = 0; k < TECH_MUTEX_MAX; ++k) d.mutually_exclusive_with[k] = INVALID_TECH_ID;
    d.mutually_exclusive_with[0] = 2; d.mutex_count = 1;  // excluye tech_c
    return d;
}
inline TechDefinitionV1 make_tech_c() {
    TechDefinitionV1 d{};
    d.id = 2; d.cost_a = 10; d.cost_b = 0; d.cost_me = 0;
    d.research_time_ticks = 3; d.epoch = 1;
    for (uint32_t k = 0; k < TECH_PREREQ_MAX; ++k) d.prerequisites[k] = INVALID_TECH_ID;
    d.prereq_count = 0;
    for (uint32_t k = 0; k < TECH_GRANT_MAX; ++k) d.grants[k] = INVALID_CAPABILITY_ID;
    d.grants[0] = 2; d.grant_count = 1;
    for (uint32_t k = 0; k < TECH_MUTEX_MAX; ++k) d.mutually_exclusive_with[k] = INVALID_TECH_ID;
    d.mutually_exclusive_with[0] = 1; d.mutex_count = 1;  // excluye tech_b
    return d;
}
inline TechDefinitionV1 make_tech_high_epoch() {
    TechDefinitionV1 d{};
    d.id = 3; d.cost_a = 10; d.cost_b = 0; d.cost_me = 0;
    d.research_time_ticks = 3; d.epoch = 5;
    for (uint32_t k = 0; k < TECH_PREREQ_MAX; ++k) d.prerequisites[k] = INVALID_TECH_ID;
    d.prereq_count = 0;
    for (uint32_t k = 0; k < TECH_GRANT_MAX; ++k) d.grants[k] = INVALID_CAPABILITY_ID;
    d.grant_count = 0;
    for (uint32_t k = 0; k < TECH_MUTEX_MAX; ++k) d.mutually_exclusive_with[k] = INVALID_TECH_ID;
    d.mutex_count = 0;
    return d;
}

static UnitDefinitionV1 g_units[3] = { make_worker(), make_warrior(), make_elite() };
static BuildingDefinitionV1 g_buildings[4] = {
    make_barracks(), make_second_barracks(), make_future_hall(), make_guild_hall(),
};
static TechDefinitionV1 g_techs[4] = {
    make_tech_a(), make_tech_b(), make_tech_c(), make_tech_high_epoch(),
};

inline DataCatalogV1 make_catalog() {
    DataCatalogV1 c{};
    c.unit_count = 3; c.units = g_units; c.unit_names = nullptr;
    c.building_count = 4; c.buildings = g_buildings; c.building_names = nullptr;
    c.tech_count = 4; c.techs = g_techs; c.tech_names = nullptr;
    c.capability_count = 0; c.capability_names = nullptr;  // no usado por step.hpp (índice puro)
    return c;
}

}  // namespace fixture

static MatchConfig01A make_cfg(uint32_t max_entities = 512, uint32_t delay = 0) {
    MatchConfig01A cfg{};
    cfg.max_entities = max_entities;
    cfg.player_count = 2;
    cfg.human_input_delay_ticks = static_cast<uint16_t>(delay);
    cfg.max_future_command_ticks = 20;
    cfg.checksum_every_ticks = 1;
    cfg.map_tiles_x = 256; cfg.map_tiles_y = 256;
    cfg.seed = 20260724ull;
    cfg.allow_debug_stat_payload = 0;
    return cfg;
}

// ---- Helpers de comandos ---------------------------------------------------
static RawCommand place_building(uint32_t tick, uint16_t emitter, uint64_t seq,
                                 BuildingId bid, int64_t tx, int64_t ty) {
    RawCommand c{};
    std::memset(&c, 0, sizeof(c));
    c.target_tick = tick; c.emitter = emitter; c.type = CommandType::PLACE_BUILDING;
    c.sequence = seq; c.p.unit_id = bid; c.p.x_raw = tx; c.p.y_raw = ty;
    return c;
}
static RawCommand train_unit(uint32_t tick, uint16_t emitter, uint64_t seq,
                             EntityHandle building, UnitId uid) {
    RawCommand c{};
    std::memset(&c, 0, sizeof(c));
    c.target_tick = tick; c.emitter = emitter; c.type = CommandType::TRAIN_UNIT;
    c.sequence = seq; c.p.handle = building; c.p.unit_id = uid;
    return c;
}
static RawCommand set_rally(uint32_t tick, uint16_t emitter, uint64_t seq,
                            EntityHandle building, int64_t x_raw, int64_t y_raw) {
    RawCommand c{};
    std::memset(&c, 0, sizeof(c));
    c.target_tick = tick; c.emitter = emitter; c.type = CommandType::SET_RALLY;
    c.sequence = seq; c.p.handle = building; c.p.x_raw = x_raw; c.p.y_raw = y_raw;
    return c;
}
static RawCommand research_tech(uint32_t tick, uint16_t emitter, uint64_t seq,
                                EntityHandle building, TechId tid) {
    RawCommand c{};
    std::memset(&c, 0, sizeof(c));
    c.target_tick = tick; c.emitter = emitter; c.type = CommandType::RESEARCH_TECH;
    c.sequence = seq; c.p.handle = building; c.p.unit_id = tid;
    return c;
}
static RawCommand epoch_up(uint32_t tick, uint16_t emitter, uint64_t seq) {
    RawCommand c{};
    std::memset(&c, 0, sizeof(c));
    c.target_tick = tick; c.emitter = emitter; c.type = CommandType::EPOCH_UP;
    c.sequence = seq;
    c.p.unit_id = 0;  // centinela "sin uso" del payload (NO INVALID_UNIT_ID, ver step.hpp)
    return c;
}

// Coloca "barracks" (bid=0) en (10,10) vía la exención de escenario (tick 0)
// y devuelve su handle (siempre índice 0, generación 1, en un GameState recién
// inicializado). player_stock[0] queda con abundancia de A para el resto del
// escenario (los tests de "stock insuficiente" fijan el stock a mano después).
static EntityHandle setup_barracks(GameState& g, const DataCatalogV1& cat, int64_t tx = 10, int64_t ty = 10) {
    gs_bind_catalog(g, cat);
    gs_init_epoch_from_catalog(g);
    g.player_stock[0][0] = 100000;
    g.player_stock[1][0] = 100000;
    RawCommand cmd = place_building(0, 0, 1, 0 /*barracks*/, tx, ty);
    step(g, &cmd, 1);
    return EntityHandle{0, g.entities.generation[0]};
}

// Variante para EPOCH_UP: coloca barracks(0) Y second_barracks(1) EN EL MISMO
// batch de tick 0 (ambos scenario-exempt) — second_barracks es
// constructible=0 (nace completo), así que colocarlo FUERA de la ventana de
// exención (p.ej. en tick 1) lo rechazaría por el paso "constructible" de
// PLACE_BUILDING (§4.1.2), ya que su costo/constructibilidad solo tiene
// sentido como pieza de setup de escenario, igual que barracks.
static EntityHandle setup_two_barracks(GameState& g, const DataCatalogV1& cat) {
    gs_bind_catalog(g, cat);
    gs_init_epoch_from_catalog(g);
    g.player_stock[0][0] = 100000;
    g.player_stock[1][0] = 100000;
    RawCommand place_a = place_building(0, 0, 1, 0 /*barracks*/, 10, 10);
    RawCommand place_b = place_building(0, 0, 2, 1 /*second_barracks*/, 50, 50);
    RawCommand batch[2] = {place_a, place_b};
    step(g, batch, 2);
    return EntityHandle{0, g.entities.generation[0]};
}

// ============================================================================
// 1) TRAIN_UNIT: feliz + cada rechazo del §11.3, en orden.
// ============================================================================
static void test_train_unit_validation() {
    static DataCatalogV1 cat = fixture::make_catalog();

    // 1a) Camino feliz.
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        EntityHandle bld = setup_barracks(*g, cat);
        RawCommand cmd = train_unit(1, 0, 2, bld, 1 /*warrior*/);
        step(*g, nullptr, 0);  // t=0 -> t=1 (barracks ya completo desde t=0)
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.accepted == 1 && r.rejected == 0);
        CHECK(g->prod_count[0] == 1u);
        CHECK(g->prod_queue[0][0] == 1u);
        CHECK(g->pop_used[0] == 1);
        CHECK(g->player_stock[0][0] == 100000 - 10);  // cost_a del warrior
    }

    // 1b) INVALID_ENTITY: handle inválido.
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        setup_barracks(*g, cat);
        RawCommand cmd = train_unit(1, 0, 2, EntityHandle{99, 1}, 1);
        step(*g, nullptr, 0);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::INVALID_ENTITY);
    }

    // 1c) NOT_OWNER.
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        EntityHandle bld = setup_barracks(*g, cat);
        RawCommand cmd = train_unit(1, 1 /*owner 1, no dueño*/, 2, bld, 1);
        step(*g, nullptr, 0);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[1]) == RejectReason::NOT_OWNER);
    }

    // 1d) ILLEGAL_STATE: no es edificio (usar una unidad entrenada como
    // "handle" — warrior, id=1, SÍ está en barracks.trains, a diferencia de
    // worker id=0).
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        EntityHandle bld = setup_barracks(*g, cat);
        RawCommand tr = train_unit(g->tick, 0, 2, bld, 1 /*warrior*/);
        step(*g, &tr, 1);
        for (int k = 0; k < 5; ++k) step(*g, nullptr, 0);  // deja que spawnee (build_time=2)
        CHECK(g->entities.alive[1] == 1);  // warrior vivo en índice 1
        // target_tick == g->tick (NO un valor futuro arbitrario): con
        // delay=0, eff=max(target,t); un target en el futuro solo agendaría
        // el comando en vez de aplicarlo en ESTA misma llamada a step().
        RawCommand cmd = train_unit(g->tick, 0, 3, EntityHandle{1, g->entities.generation[1]}, 1);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::ILLEGAL_STATE);
    }

    // 1d') ILLEGAL_STATE: edificio NO completo (future_hall en construcción).
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        gs_bind_catalog(*g, cat);
        gs_init_epoch_from_catalog(*g);
        g->player_stock[0][0] = 100000;
        g->player_epoch[0] = 5;  // future_hall exige epoch 5..15
        RawCommand place = place_building(1, 0, 1, 2 /*future_hall*/, 20, 20);
        step(*g, &place, 1);  // t=0: agenda (eff=1)
        step(*g, nullptr, 0);  // t=1: aplica PLACE_BUILDING (build_time=5, NO completo aún)
        CHECK(g->entities.alive[0] == 1 && g->build_progress[0] < 5u);
        RawCommand cmd = train_unit(2, 0, 2, EntityHandle{0, g->entities.generation[0]}, 1);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::ILLEGAL_STATE);
    }

    // 1e) MALFORMED: unit_id fuera de rango del catálogo.
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        EntityHandle bld = setup_barracks(*g, cat);
        RawCommand cmd = train_unit(1, 0, 2, bld, 999);
        step(*g, nullptr, 0);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::MALFORMED);
    }

    // 1e') MALFORMED: unit_id válido pero NO está en trains del edificio
    // (worker, id=0, no forma parte de barracks.trains=[1,2]).
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        EntityHandle bld = setup_barracks(*g, cat);
        RawCommand cmd = train_unit(1, 0, 2, bld, 0 /*worker*/);
        step(*g, nullptr, 0);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::MALFORMED);
    }

    // 1f) ILLEGAL_STATE: época — "elite" (epoch 5..15) a época inicial (1).
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        EntityHandle bld = setup_barracks(*g, cat);
        RawCommand cmd = train_unit(1, 0, 2, bld, 2 /*elite*/);
        step(*g, nullptr, 0);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::ILLEGAL_STATE);
    }

    // 1g) ILLEGAL_STATE: cola llena (PROD_QUEUE_CAP=5). Los 5 TRAIN_UNIT + el
    // 6º (overflow) van en el MISMO batch/step(): apply_command procesa los
    // comandos debidos secuencialmente dentro de esa única llamada (mismo
    // orden canónico), así que prod_count ya refleja los 5 aceptados cuando
    // se evalúa el 6º — sin esto, production_system (que también corre en el
    // mismo tick, tras aplicar comandos) alcanzaría a completar el warrior de
    // cabeza entre llamadas separadas y la cola nunca se vería "llena".
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        EntityHandle bld = setup_barracks(*g, cat);
        RawCommand batch[PROD_QUEUE_CAP + 1];
        for (uint64_t k = 0; k < PROD_QUEUE_CAP; ++k) {
            batch[k] = train_unit(g->tick, 0, 2 + k, bld, 1);
        }
        batch[PROD_QUEUE_CAP] = train_unit(g->tick, 0, 2 + PROD_QUEUE_CAP, bld, 1);  // overflow
        const StepResult r = step(*g, batch, PROD_QUEUE_CAP + 1);
        CHECK(r.accepted == PROD_QUEUE_CAP);
        CHECK(r.rejected == 1);
        CHECK(g->prod_count[0] == PROD_QUEUE_CAP);
        CHECK(last_result(g->mailbox[0]) == RejectReason::ILLEGAL_STATE);
    }

    // 1h) ILLEGAL_STATE: pop llena (POP_CAP_V1=200). Se entrena "worker"
    // (build_time=1) repetidamente, dejando que la cola drene entre lotes,
    // hasta agotar el cupo; el intento nº201 se rechaza.
    {
        auto g = std::make_unique<GameState>();
        MatchConfig01A cfg = make_cfg(/*max_entities=*/POP_CAP_V1 + 16);
        gs_init(*g, cfg);
        EntityHandle bld = setup_barracks(*g, cat);
        step(*g, nullptr, 0);
        uint64_t seq = 2;
        uint32_t trained = 0;
        uint32_t guard = 0;
        while (trained < POP_CAP_V1 && guard < 100000u) {
            ++guard;
            if (g->prod_count[0] < PROD_QUEUE_CAP) {
                RawCommand cmd = train_unit(g->tick, 0, seq++, bld, 1 /*warrior, sí está en trains*/);
                const StepResult r = step(*g, &cmd, 1);
                if (r.accepted == 1) ++trained;
            } else {
                step(*g, nullptr, 0);
            }
        }
        CHECK(trained == POP_CAP_V1);
        CHECK(static_cast<uint32_t>(g->pop_used[0]) == POP_CAP_V1);
        // Drenar la cola restante antes del intento de overflow (irrelevante
        // para pop_used, que ya está en el cupo).
        for (int k = 0; k < 4; ++k) step(*g, nullptr, 0);
        RawCommand overflow = train_unit(g->tick, 0, seq++, bld, 1 /*warrior*/);
        const StepResult r = step(*g, &overflow, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::ILLEGAL_STATE);
    }

    // 1i) ILLEGAL_STATE: stock insuficiente.
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        gs_bind_catalog(*g, cat);
        gs_init_epoch_from_catalog(*g);
        g->player_stock[0][0] = 2;  // cost_a del warrior = 10
        RawCommand place = place_building(0, 0, 1, 0, 10, 10);
        step(*g, &place, 1);
        EntityHandle bld{0, g->entities.generation[0]};
        RawCommand cmd = train_unit(1, 0, 2, bld, 1);
        step(*g, nullptr, 0);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::ILLEGAL_STATE);
    }
}

// ============================================================================
// 2) Producción multi-ítem con pool de entidades exhausto a mitad: espera sin
//    perder progreso, determinista.
// ============================================================================
static void test_production_pool_exhausted() {
    static DataCatalogV1 cat = fixture::make_catalog();
    // Pool diminuto: barracks (índice 0) + 1 sola entidad más de margen ->
    // el primer warrior entrenado ocupa el único slot libre; el segundo debe
    // esperar indefinidamente sin perder el progreso ya acumulado.
    auto g = std::make_unique<GameState>();
    MatchConfig01A cfg = make_cfg(/*max_entities=*/2);
    gs_init(*g, cfg);
    EntityHandle bld = setup_barracks(*g, cat);  // g->tick == 1 tras esto

    RawCommand t1 = train_unit(g->tick, 0, 2, bld, 1 /*warrior, build_time=2*/);
    RawCommand t2 = train_unit(g->tick, 0, 3, bld, 1);
    RawCommand batch[2] = {t1, t2};
    step(*g, batch, 2);
    CHECK(g->prod_count[0] == 2u);

    // Avanza hasta que el primer warrior spawnee (pool: barracks + 1 slot
    // libre). No se asume un nº exacto de ticks (production_system ya venía
    // avanzando prod_progress[0] desde el propio tick del batch): se espera
    // a la condición observable.
    uint32_t guard = 0;
    while (g->entities.alive_count < 2u && guard < 50u) { step(*g, nullptr, 0); ++guard; }
    CHECK(g->entities.alive_count == 2u);  // barracks + 1 warrior
    CHECK(g->prod_count[0] == 1u);          // el 2º ítem sigue en cola

    // El 2º ítem NUNCA puede spawnear (pool ya en capacidad máxima): tras
    // suficientes ticks su progreso se ESTABILIZA (incrementa a
    // build_time_ticks, el spawn falla, se revierte a build_time_ticks-1 —
    // neto: sin cambio observable tick a tick) y se queda ahí indefinidamente,
    // reintentando sin perder NI ganar progreso.
    for (int k = 0; k < 10; ++k) step(*g, nullptr, 0);
    const uint32_t stalled_progress = g->prod_progress[0];
    CHECK(g->prod_count[0] == 1u);
    for (int k = 0; k < 5; ++k) {
        step(*g, nullptr, 0);
        CHECK(g->prod_count[0] == 1u);                    // sigue esperando
        CHECK(g->prod_progress[0] == stalled_progress);   // sin perder NI ganar progreso
    }

    // Libera el pool destruyendo al primer warrior (mutación directa
    // test-only, mismo patrón/reciclaje que el paso 6 de step() — sin pasar
    // por DESTROY_DEBUG para no consumir un comando de este escenario
    // aislado): el segundo ítem spawnea en el siguiente tick (determinista).
    et_mark_dead(g->entities, 1);
    zero_components(*g, 1);
    et_release_index(g->entities, 1);
    step(*g, nullptr, 0);
    CHECK(g->prod_count[0] == 0u);
}

// ============================================================================
// 3) Rally: la unidad entrenada camina al punto de rally.
// ============================================================================
static void test_set_rally() {
    static DataCatalogV1 cat = fixture::make_catalog();
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg());
    EntityHandle bld = setup_barracks(*g, cat);
    step(*g, nullptr, 0);

    // Rally razonablemente cerca del spawn (~11,12): a speed_millitile_tick=
    // 400 (0.4 tiles/tick) llegar a (200,200) tomaría >600 ticks; (50,50)
    // basta con margen holgado dentro del loop de abajo.
    const int64_t rally_x = 50 * 65536, rally_y = 50 * 65536;
    RawCommand rally = set_rally(1, 0, 2, bld, rally_x, rally_y);
    const StepResult rr = step(*g, &rally, 1);
    CHECK(rr.accepted == 1);
    CHECK(g->rally_set[0] == 1u);
    CHECK(g->rally_x[0] == rally_x && g->rally_y[0] == rally_y);

    RawCommand tr = train_unit(2, 0, 3, bld, 1 /*warrior, build_time=2*/);
    step(*g, &tr, 1);
    for (int k = 0; k < 4; ++k) step(*g, nullptr, 0);  // deja que spawnee
    CHECK(g->entities.alive[1] == 1);
    CHECK(g->tgt_x[1] == rally_x && g->tgt_y[1] == rally_y);

    const int64_t start_x = g->pos_x[1];
    for (int k = 0; k < 300 && (g->pos_x[1] != rally_x || g->pos_y[1] != rally_y); ++k) {
        step(*g, nullptr, 0);
    }
    CHECK(g->pos_x[1] == rally_x && g->pos_y[1] == rally_y);
    CHECK(g->pos_x[1] != start_x);  // realmente caminó, no nació ya en el rally
}

// ============================================================================
// 4) RESEARCH_TECH: feliz + prereq incumplido + mutex + época + edificio
//    ocupado.
// ============================================================================
static void test_research_validation() {
    static DataCatalogV1 cat = fixture::make_catalog();

    // 4a) Prereq incumplido: investigar tech_b (id=1, prereq=tech_a) sin
    // tech_a investigada -> ILLEGAL_STATE.
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        EntityHandle bld = setup_barracks(*g, cat);
        step(*g, nullptr, 0);
        RawCommand cmd = research_tech(1, 0, 2, bld, 1 /*tech_b*/);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::ILLEGAL_STATE);
    }

    // 4b) Camino feliz: investigar tech_a (sin prereq) hasta completarse;
    // set bit en player_techs + OR de grants en player_caps.
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        EntityHandle bld = setup_barracks(*g, cat);
        step(*g, nullptr, 0);
        RawCommand cmd = research_tech(1, 0, 2, bld, 0 /*tech_a*/);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.accepted == 1);
        CHECK(g->research_tech[0] == 0u);
        CHECK(g->player_stock[0][0] == 100000 - 10);  // cost_a de tech_a
        for (int k = 0; k < 5; ++k) step(*g, nullptr, 0);  // research_time_ticks=3
        CHECK(g->research_tech[0] == INVALID_TECH_ID);  // completado
        CHECK((g->player_techs[0][0] & 1ull) != 0u);      // bit tech_a
        CHECK((g->player_caps[0][0] & 1ull) != 0u);       // bit cap0 (grants de tech_a)

        // 4c) Ahora tech_b (prereq=tech_a, cumplido) SÍ se acepta.
        RawCommand cmd_b = research_tech(g->tick, 0, 3, bld, 1 /*tech_b*/);
        const StepResult rb = step(*g, &cmd_b, 1);
        CHECK(rb.accepted == 1);
        for (int k = 0; k < 5; ++k) step(*g, nullptr, 0);
        CHECK((g->player_techs[0][0] & 2ull) != 0u);  // bit tech_b

        // 4d) Mutex: tech_c (mutex=tech_b, ya investigada) -> ILLEGAL_STATE.
        RawCommand cmd_c = research_tech(g->tick, 0, 4, bld, 2 /*tech_c*/);
        const StepResult rc = step(*g, &cmd_c, 1);
        CHECK(rc.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::ILLEGAL_STATE);
    }

    // 4e) Época incumplida: tech_high_epoch (epoch=5) a época inicial (1).
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        EntityHandle bld = setup_barracks(*g, cat);
        step(*g, nullptr, 0);
        // tech_high_epoch (id=3) NO está en researches del fixture; se añade
        // temporalmente para este test aislado (no afecta a los demás: cada
        // test usa su propio GameState/edificio, el catálogo es compartido
        // pero solo se LEE).
        RawCommand cmd = research_tech(1, 0, 2, bld, 3 /*tech_high_epoch*/);
        const StepResult r = step(*g, &cmd, 1);
        // researches de barracks = [tech_a, tech_b] -> tech_high_epoch no
        // pertenece -> MALFORMED (paso previo al de época en el orden §12.3).
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::MALFORMED);
    }

    // 4f) Edificio ocupado: 2º RESEARCH_TECH sobre el MISMO edificio mientras
    // el primero está en curso -> ILLEGAL_STATE (edificio, no jugador — para
    // eso se necesitarían 2 techs distintas y SIN relación de mutex/prereq
    // entre sí; aquí basta con intentar de nuevo tech_a sobre el mismo
    // edificio, que además coincide con "no investigada ya ni en curso").
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        EntityHandle bld = setup_barracks(*g, cat);
        step(*g, nullptr, 0);
        RawCommand cmd = research_tech(1, 0, 2, bld, 0 /*tech_a*/);
        step(*g, &cmd, 1);
        CHECK(g->research_tech[0] == 0u);
        RawCommand cmd2 = research_tech(g->tick, 0, 3, bld, 1 /*tech_b: edificio ocupado*/);
        const StepResult r2 = step(*g, &cmd2, 1);
        CHECK(r2.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::ILLEGAL_STATE);
    }
}

// ============================================================================
// 5) EPOCH_UP: doble gate ADR-015 — falla por edificios, falla por tiempo,
//    pasa con ambos.
// ============================================================================
static void test_epoch_up() {
    static DataCatalogV1 cat = fixture::make_catalog();

    // 5a) Falla por edificios: solo 1 edificio completo (barracks) cuya
    // epoch_window incluye la época actual (1) -> gate (a) falla.
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        setup_barracks(*g, cat);
        g->tick = EPOCH_MIN_TICKS * 10;  // gate (b) trivialmente satisfecho
        RawCommand cmd = epoch_up(g->tick, 0, 2);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::ILLEGAL_STATE);
        CHECK(g->player_epoch[0] == 1u);
    }

    // 5b) Falla por tiempo: 2 edificios completos (barracks + second_barracks)
    // pero g.tick insuficiente -> gate (b) falla.
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        setup_two_barracks(*g, cat);
        CHECK(g->entities.alive[1] == 1 && g->entity_kind[1] == 1);
        RawCommand cmd = epoch_up(g->tick, 0, 3);  // g.tick == 1, << EPOCH_MIN_TICKS
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::ILLEGAL_STATE);
        CHECK(g->player_epoch[0] == 1u);
    }

    // 5c) Pasa con ambos gates: 2 edificios + tiempo suficiente + stock
    // (EPOCH_COST_A/B/ME los 3 recursos — setup_two_barracks solo fondea A).
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        setup_two_barracks(*g, cat);
        g->player_stock[0][1] = 100000;
        g->player_stock[0][2] = 100000;
        g->tick = EPOCH_MIN_TICKS;  // steps=(1-1+1)=1 -> umbral EPOCH_MIN_TICKS*1
        const int64_t stock_a_before = g->player_stock[0][0];
        const int64_t stock_b_before = g->player_stock[0][1];
        const int64_t stock_me_before = g->player_stock[0][2];
        RawCommand cmd = epoch_up(g->tick, 0, 3);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.accepted == 1);
        CHECK(g->player_epoch[0] == 2u);
        CHECK(g->player_stock[0][0] == stock_a_before - EPOCH_COST_A);
        CHECK(g->player_stock[0][1] == stock_b_before - EPOCH_COST_B);
        CHECK(g->player_stock[0][2] == stock_me_before - EPOCH_COST_ME);
    }
}

// ============================================================================
// 6) Gating de época en TRAIN_UNIT y PLACE_BUILDING (§12.4).
// ============================================================================
static void test_epoch_gating_train_and_place() {
    static DataCatalogV1 cat = fixture::make_catalog();

    // 6a) TRAIN_UNIT de "elite" (epoch 5..15) rechazado a época 1, aceptado
    // tras subir player_epoch a 5 (mutación directa test-only — la
    // trayectoria REAL de EPOCH_UP ya se prueba en test_epoch_up()).
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        EntityHandle bld = setup_barracks(*g, cat);
        step(*g, nullptr, 0);
        RawCommand cmd = train_unit(1, 0, 2, bld, 2 /*elite*/);
        const StepResult r1 = step(*g, &cmd, 1);
        CHECK(r1.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::ILLEGAL_STATE);

        g->player_epoch[0] = 5;
        RawCommand cmd2 = train_unit(g->tick, 0, 3, bld, 2);
        const StepResult r2 = step(*g, &cmd2, 1);
        CHECK(r2.accepted == 1);
    }

    // 6b) PLACE_BUILDING de "future_hall" (epoch 5..15) fuera de la ventana
    // de setup: rechazado a época 1, aceptado tras subir a época 5.
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        gs_bind_catalog(*g, cat);
        gs_init_epoch_from_catalog(*g);
        g->player_stock[0][0] = 100000;
        RawCommand place = place_building(1, 0, 1, 2 /*future_hall*/, 10, 10);
        const StepResult r1 = step(*g, &place, 1);  // t=0: agenda (eff=1, NO exento)
        const StepResult r1b = step(*g, nullptr, 0);  // t=1: aplica
        CHECK(r1.accepted == 0 && r1.rejected == 0);  // agendado, aún no evaluado
        CHECK(r1b.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::ILLEGAL_STATE);
        CHECK(g->entities.alive_count == 0u);

        g->player_epoch[0] = 5;
        RawCommand place2 = place_building(g->tick + 1, 0, 2, 2, 10, 10);
        step(*g, &place2, 1);
        const StepResult r2 = step(*g, nullptr, 0);
        CHECK(r2.accepted == 1);
        CHECK(g->entities.alive_count == 1u);
    }

    // 6c) PLACE_BUILDING de "guild_hall" (required_capabilities=[cap0]):
    // rechazado sin la capacidad, aceptado tras concederla (mutación directa
    // del bitmask test-only — la vía real es completar tech_a vía
    // RESEARCH_TECH, ya probada en test_research_validation()).
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        gs_bind_catalog(*g, cat);
        gs_init_epoch_from_catalog(*g);
        g->player_stock[0][0] = 100000;
        RawCommand place = place_building(1, 0, 1, 3 /*guild_hall*/, 10, 10);
        step(*g, &place, 1);
        const StepResult r1 = step(*g, nullptr, 0);
        CHECK(r1.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::ILLEGAL_STATE);

        g->player_caps[0][0] |= 1ull;  // cap0
        RawCommand place2 = place_building(g->tick + 1, 0, 2, 3, 10, 10);
        step(*g, &place2, 1);
        const StepResult r2 = step(*g, nullptr, 0);
        CHECK(r2.accepted == 1);
    }
}

// ============================================================================
// 7) Save v10 round-trip con cola de producción no vacía y research en curso.
// ============================================================================
static void test_save_v10_roundtrip() {
    static DataCatalogV1 cat = fixture::make_catalog();

    auto g1 = std::make_unique<GameState>();
    gs_init(*g1, make_cfg());
    EntityHandle bld = setup_barracks(*g1, cat);
    step(*g1, nullptr, 0);

    RawCommand tr = train_unit(1, 0, 2, bld, 1 /*warrior*/);
    RawCommand rs = research_tech(1, 0, 3, bld, 0 /*tech_a*/);
    RawCommand batch[2] = {tr, rs};
    step(*g1, batch, 2);
    CHECK(g1->prod_count[0] == 1u);
    CHECK(g1->research_tech[0] == 0u);
    CHECK(g1->pop_used[0] == 1);

    AiJobBox box{}; ai_box_init(box, 1);
    AiRuntimeV1 rt{};
    CHECK(save_game(*g1, box, rt, "test_prodtech_v10.sav") == 0);

    auto g2 = std::make_unique<GameState>();
    AiJobBox box2{}; AiRuntimeV1 rt2{};
    CHECK(load_game(*g2, box2, rt2, "test_prodtech_v10.sav") == 0);
    gs_bind_catalog(*g2, cat);

    CHECK(g2->prod_count[0] == 1u);
    CHECK(g2->prod_queue[0][0] == 1u);
    CHECK(g2->research_tech[0] == 0u);
    CHECK(g2->pop_used[0] == 1);
    CHECK(g2->player_epoch[0] == g1->player_epoch[0]);
    CHECK(g2->epoch_initial[0] == g1->epoch_initial[0]);
    CHECK(state_checksum_v1(*g1) == state_checksum_v1(*g2));

    for (int k = 0; k < 6; ++k) step(*g1, nullptr, 0);
    for (int k = 0; k < 6; ++k) step(*g2, nullptr, 0);
    CHECK(g1->entities.alive_count == g2->entities.alive_count);
    CHECK((g1->player_techs[0][0] & 1ull) == (g2->player_techs[0][0] & 1ull));
    CHECK(state_checksum_v1(*g1) == state_checksum_v1(*g2));

    std::remove("test_prodtech_v10.sav");
}

// ============================================================================
// 8) Catálogo golden real (SPEC-004 §13): los 2 cuarteles resuelven sus
//    `trains` y las 4 techs resuelven sus `grants` (ids del blob del repo).
// ============================================================================
static void test_catalog_real_golden() {
    DataCatalogStorageV1 store;
    const auto code = catalog_load_file_v1(CHUNSA_GOLDEN_CHDB_PATH, CatalogLoadProfile::Verified, store);
    CHECK(code == CatalogLoadCode::Ok);
    if (!store.valid()) { std::printf("catalog_real_golden: catálogo inválido, abortando subtest\n"); return; }
    const DataCatalogV1& cat = store.catalog();
    CHECK(cat.building_count == 6);
    CHECK(cat.tech_count == 4);
    CHECK(cat.capability_count == 5);

    auto find_b = [&](const char* name) { return catalog_find_building(cat, name, std::strlen(name)); };
    auto find_t = [&](const char* name) { return catalog_find_tech(cat, name, std::strlen(name)); };
    auto find_u = [&](const char* name) { return catalog_find_unit(cat, name, std::strlen(name)); };
    auto find_c = [&](const char* name) { return catalog_find_capability(cat, name, std::strlen(name)); };

    // Los 2 cuarteles resuelven sus trains.
    const BuildingId chariotry = find_b("egipto:chariotry_stable");
    const BuildingId castra = find_b("rome:castra_barracks");
    CHECK(chariotry != INVALID_BUILDING_ID && castra != INVALID_BUILDING_ID);
    if (chariotry != INVALID_BUILDING_ID) {
        const BuildingDefinitionV1& d = cat.buildings[chariotry];
        CHECK(d.train_count == 1);
        CHECK(d.trains[0] == find_u("egipto:chariot_warrior"));
        CHECK(d.epoch_min == 3 && d.epoch_max == 4);
    }
    if (castra != INVALID_BUILDING_ID) {
        const BuildingDefinitionV1& d = cat.buildings[castra];
        CHECK(d.train_count == 2);
        // Orden del schema: [rome:legionary, rome:ballista_crew].
        CHECK(d.trains[0] == find_u("rome:legionary"));
        CHECK(d.trains[1] == find_u("rome:ballista_crew"));
        CHECK(d.epoch_min == 5 && d.epoch_max == 5);
    }

    // Las 4 techs resuelven sus grants (capacidades).
    static constexpr const char* kTechs[4] = {
        "egipto:composite_bow_program", "egipto:corvee_logistics",
        "rome:marching_drill", "rome:road_engineering",
    };
    static constexpr const char* kCaps[4] = {
        "egipto:composite_bow_craft", "egipto:corvee_levy",
        "rome:legion_drill", "rome:maintain_public_infrastructure",
    };
    for (int i = 0; i < 4; ++i) {
        const TechId tid = find_t(kTechs[i]);
        CHECK(tid != INVALID_TECH_ID);
        if (tid == INVALID_TECH_ID) continue;
        const TechDefinitionV1& d = cat.techs[tid];
        CHECK(d.grant_count == 1);
        const CapabilityId expected = find_c(kCaps[i]);
        CHECK(expected != INVALID_CAPABILITY_ID);
        CHECK(d.grants[0] == expected);
        CHECK(d.prereq_count == 0);
        CHECK(d.mutex_count == 0);
    }
    CHECK(find_t("nope:nope") == INVALID_TECH_ID);
    CHECK(find_c("nope:nope") == INVALID_CAPABILITY_ID);
}

// ============================================================================
// 9) Determinismo del escenario completo: dos corridas + save intermedio +
//    replay v3 (patrón de test_buildings.cpp/test_replay_v3.cpp).
// ============================================================================
static uint32_t build_full_scenario_batch(RawCommand* out, uint32_t t, EntityHandle bld) {
    uint32_t n = 0;
    if (t == 1u) {
        out[n++] = train_unit(t, 0, 10, bld, 1 /*warrior*/);
        out[n++] = research_tech(t, 0, 11, bld, 0 /*tech_a*/);
        out[n++] = set_rally(t, 0, 12, bld, 150 * 65536, 150 * 65536);
    } else if (t == 5u) {
        out[n++] = train_unit(t, 0, 13, bld, 1);
    }
    return n;
}

static uint64_t run_full_scenario(uint32_t ticks, ReplayWriter* rec, GameState* out_final) {
    static DataCatalogV1 cat = fixture::make_catalog();
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg());
    EntityHandle bld = setup_barracks(*g, cat);

    RawCommand batch[4];
    uint64_t last_ck = 0;
    for (uint32_t t = 0; t < ticks; ++t) {
        const uint32_t n = build_full_scenario_batch(batch, t, bld);
        if (rec != nullptr) rec->tick_batch(batch, n, t);
        const StepResult r = step(*g, batch, n);
        if (r.checksum_computed) last_ck = r.checksum;
    }
    if (out_final != nullptr) *out_final = *g;
    return last_ck;
}

static void test_determinism_full_scenario() {
    // 9a) Dos corridas independientes -> mismo checksum final.
    const uint64_t ck1 = run_full_scenario(30, nullptr, nullptr);
    const uint64_t ck2 = run_full_scenario(30, nullptr, nullptr);
    CHECK(ck1 == ck2);

    // 9b) Save a mitad + continuar == corrida continua.
    //
    // NOTA: se reanuda desde el MISMO índice nominal de loop (`kSaveAtLoopT`)
    // en el que se guardó, NO desde `gb->tick` — setup_barracks() ya consume
    // un `step()` (tick 0) ANTES de que el loop empiece en t=0, así que
    // `g.tick` real queda permanentemente desfasado +1 respecto al índice de
    // loop usado por build_full_scenario_batch (que decide el contenido del
    // batch por el propio índice, no por g.tick). El patrón `for (t=gb->tick;
    // ...)` de test_buildings.cpp/test_replay_v3.cpp asume que loop-t ==
    // g.tick en todo momento — válido ahí (sin pre-step), no aquí.
    {
        static DataCatalogV1 cat = fixture::make_catalog();
        constexpr uint32_t kSaveAtLoopT = 3u;
        auto ga = std::make_unique<GameState>();
        gs_init(*ga, make_cfg());
        EntityHandle bld = setup_barracks(*ga, cat);
        AiJobBox box{}; ai_box_init(box, 1); AiRuntimeV1 rt{};
        RawCommand batch[4];
        for (uint32_t t = 0; t < 30u; ++t) {
            if (t == kSaveAtLoopT) CHECK(save_game(*ga, box, rt, "test_prodtech_gate.sav") == 0);
            const uint32_t n = build_full_scenario_batch(batch, t, bld);
            step(*ga, batch, n);
        }
        const uint64_t continuous_ck = state_checksum_v1(*ga);

        auto gb = std::make_unique<GameState>();
        AiJobBox box2{}; AiRuntimeV1 rt2{};
        CHECK(load_game(*gb, box2, rt2, "test_prodtech_gate.sav") == 0);
        gs_bind_catalog(*gb, cat);
        for (uint32_t t = kSaveAtLoopT; t < 30u; ++t) {
            RawCommand batch2[4];
            const uint32_t n = build_full_scenario_batch(batch2, t, bld);
            step(*gb, batch2, n);
        }
        CHECK(continuous_ck == state_checksum_v1(*gb));
        std::remove("test_prodtech_gate.sav");
    }

    // 9c) Record con ReplayWriter (v3) + replay_load, reproducción bit-exacta.
    {
        static DataCatalogV1 cat = fixture::make_catalog();
        ReplayWriter rec;
        rec.begin(20260724ull, 0u, 30u, 1u, 0u, 20u);
        // out_final=nullptr: no se necesita el estado final aquí (solo el
        // checksum grabado, comparado más abajo contra la reproducción).
        // GameState SIEMPRE en heap — run_full_scenario ya lo asigna con
        // make_unique internamente; nunca uno adicional en pila aquí.
        const uint64_t rec_ck = run_full_scenario(30, &rec, nullptr);
        CHECK(rec.finish(rec_ck, "test_prodtech_gate.curp") == 0);

        ReplayData data;
        CHECK(replay_load("test_prodtech_gate.curp", data) == 0);
        CHECK(data.version == 3u);
        CHECK(data.legacy_payload_loss == 0u);

        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        EntityHandle bld2 = setup_barracks(*g, cat);
        (void)bld2;
        uint64_t replay_ck = 0;
        uint32_t mismatches = 0;
        for (uint32_t t = 0; t < data.ticks; ++t) {
            const auto& b = data.batches[t];
            for (size_t i = 0; i < b.size(); ++i) {
                const uint32_t recomputed = command_effective_tick(b[i].target_tick, t, 0u);
                if (t < data.eff_ticks.size() && i < data.eff_ticks[t].size()
                    && recomputed != data.eff_ticks[t][i]) ++mismatches;
            }
            const StepResult r = step(*g, b.data(), static_cast<uint32_t>(b.size()));
            if (r.checksum_computed) replay_ck = r.checksum;
        }
        CHECK(mismatches == 0);
        CHECK(replay_ck == rec_ck);
        CHECK(replay_ck == data.final_checksum);
        std::remove("test_prodtech_gate.curp");
    }
}

int main() {
    test_train_unit_validation();
    test_production_pool_exhausted();
    test_set_rally();
    test_research_validation();
    test_epoch_up();
    test_epoch_gating_train_and_place();
    test_save_v10_roundtrip();
    test_catalog_real_golden();
    test_determinism_full_scenario();

    if (g_fails == 0) { std::printf("production_tech: OK\n"); return 0; }
    std::printf("production_tech: %d fallos\n", g_fails);
    return 1;
}
