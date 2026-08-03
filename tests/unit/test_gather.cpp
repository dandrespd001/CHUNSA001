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
// no queda ninguno · zona aliada de auto-recolección (§23): filtro a 32 tiles
// de edificios completos, desempates, GATHER remoto explícito, retorno tras
// agotamiento y expansión de zona al completar un edificio.
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
#include "chunsa/ai_stub.hpp"
#include "chunsa/replay.hpp"
#include "chunsa/save_io.hpp"

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
    d.cost[0] = 5; d.cost[1] = 0; d.cost[2] = 0; d.pop_cost = 1;
    d.epoch_min = 1; d.epoch_max = 15;
    return d;
}
inline UnitDefinitionV1 make_soldier() {
    UnitDefinitionV1 d{};
    d.id = 1; d.unit_class = UnitClassV1::Infantry; d.tags_mask = 0;
    d.hp = 50; d.attack = 10; d.range_millitiles = 1000;
    d.speed_millitile_tick = 400; d.morale = 100; d.build_time_ticks = 2;
    for (int k = 0; k < 6; ++k) d.bonus_vs_bp[k] = 0;
    d.cost[0] = 10; d.cost[1] = 0; d.cost[2] = 0; d.pop_cost = 1;
    d.epoch_min = 1; d.epoch_max = 15;
    return d;
}
inline BuildingDefinitionV1 make_center() {
    BuildingDefinitionV1 d{};
    d.id = 0; d.hp = 500; d.footprint_w = 2; d.footprint_h = 2;
    d.build_time_ticks = 0;
    d.cost[0] = 0; d.cost[1] = 0; d.cost[2] = 0;
    d.dropoff_mask = 0; d.constructible = 0;
    d.epoch_min = 1; d.epoch_max = 15;
    d.trains[0] = 0 /*citizen*/; d.train_count = 1;
    // Sprint 1.14: el tope de poblacion pasa a salir de los edificios. Este
    // fixture es sintetico y no va de poblacion, asi que declara el tope
    // entero para seguir midiendo lo que media.
    d.population_provided = static_cast<int32_t>(POP_CAP_V1);
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
    d.cost[0] = 0; d.cost[1] = 0; d.cost[2] = 0;
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

namespace allied_zone_fixture {

inline constexpr int64_t CENTER_X = 40 * FX_ONE_RAW;
inline constexpr int64_t CENTER_Y = 40 * FX_ONE_RAW;

static EntityHandle spawn_building(GameState& g, BuildingId bid,
                                   int64_t x_raw, int64_t y_raw,
                                   uint32_t progress, uint8_t owner = 0u) {
    const EntityHandle h = et_spawn(g.entities);
    CHECK(!handle_eq(h, NULL_HANDLE));
    if (handle_eq(h, NULL_HANDLE)) return h;
    g.entity_kind[h.index] = 1u;
    g.building_id[h.index] = bid;
    g.build_progress[h.index] = progress;
    g.owner[h.index] = owner;
    g.pos_x[h.index] = x_raw;
    g.pos_y[h.index] = y_raw;
    g.unit_class[h.index] = 255u;
    g.hp[h.index] = g.max_hp[h.index] = 500;
    return h;
}

static EntityHandle spawn_citizen(GameState& g, int64_t x_raw, int64_t y_raw,
                                  uint8_t owner = 0u) {
    const EntityHandle h = et_spawn(g.entities);
    CHECK(!handle_eq(h, NULL_HANDLE));
    if (handle_eq(h, NULL_HANDLE)) return h;
    g.owner[h.index] = owner;
    g.unit_id[h.index] = 0u;
    g.unit_class[h.index] = 3u;
    g.hp[h.index] = g.max_hp[h.index] = 20;
    g.speed_mtpt[h.index] = 1000;
    g.pos_x[h.index] = x_raw;
    g.pos_y[h.index] = y_raw;
    g.eco_state[h.index] = EcoState::SEEK;
    g.eco_assigned_deposit[h.index] = ECO_NO_DEPOSIT;
    g.eco_carry[h.index] = 0;
    g.eco_carry_resource[h.index] = 0u;
    g.build_target[h.index] = BUILD_NO_TARGET;
    g.citizen_task[h.index] = CITIZEN_TASK_GATHER;
    return h;
}

static std::unique_ptr<GameState> make_state(uint32_t n_deposits) {
    static DataCatalogV1 cat = gather_fixture::make_catalog();
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg());
    gs_bind_catalog(*g, cat);
    gs_init_epoch_from_catalog(*g);
    g->n_deposits = n_deposits;
    for (uint32_t d = 0; d < n_deposits; ++d) g->deposits[d] = EcoDeposit{};

    const EntityHandle center =
        spawn_building(*g, 0u, CENTER_X, CENTER_Y, 0u);
    CHECK(center.index == 0u);
    const EntityHandle citizen =
        spawn_citizen(*g, CENTER_X, CENTER_Y);
    CHECK(citizen.index == 1u);
    return g;
}

}  // namespace allied_zone_fixture

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
        {0, 0, /*A*/0u, 0, 0, 0, 0, 0, 0},
        {3 * T, 0, /*B*/1u, 50, 0, 0, 0, 0, 0},
        {10 * T, 0, /*A*/0u, 50, 0, 0, 0, 0, 0},
    };

    EcoCitizenIn in{};
    in.pos_x = 0; in.pos_y = 0;
    in.state = EcoState::SEEK;
    in.assigned_deposit = 0u;  // el A agotado
    in.carry = 0; in.carry_resource_idx = 0;
    in.speed_mtpt = 100;

    // a) Reasigna al A lejano (índice 2), NO al B cercano (índice 1).
    EcoCitizenOut out_a = eco_step_citizen(
        in, deposits, 3, ECO_ALL_DEPOSITS_MASK, 20 * T, 20 * T, fatal);
    CHECK(fatal == FatalReason::NONE);
    CHECK(out_a.assigned_deposit == 2u);

    // b) Ese A también se agota -> reasigna al más cercano de CUALQUIERA (B, índice 1).
    deposits[2].remaining = 0;
    EcoCitizenIn in_b = in;
    in_b.assigned_deposit = 2u;
    EcoCitizenOut out_b = eco_step_citizen(
        in_b, deposits, 3, ECO_ALL_DEPOSITS_MASK, 20 * T, 20 * T, fatal);
    CHECK(fatal == FatalReason::NONE);
    CHECK(out_b.assigned_deposit == 1u);

    // c) B también se agota -> ocioso (ECO_NO_DEPOSIT), sin más movimiento.
    deposits[1].remaining = 0;
    EcoCitizenIn in_c = in;
    in_c.assigned_deposit = 1u;
    EcoCitizenOut out_c = eco_step_citizen(
        in_c, deposits, 3, ECO_ALL_DEPOSITS_MASK, 20 * T, 20 * T, fatal);
    CHECK(fatal == FatalReason::NONE);
    CHECK(out_c.assigned_deposit == ECO_NO_DEPOSIT);
    CHECK(out_c.vel_x == 0 && out_c.vel_y == 0);

    // Control directo de eco_find_nearest_deposit con preferencia explícita:
    // preferir A cuando SOLO el B cercano vive -> cae a "cualquiera" (B).
    deposits[1].remaining = 50; deposits[2].remaining = 0;
    const uint32_t idx = eco_find_nearest_deposit(
        deposits, 3, 0, 0, /*preferred=*/0u, ECO_ALL_DEPOSITS_MASK, fatal);
    CHECK(idx == 1u);  // no queda A vivo -> el más cercano de cualquiera (B)
    // Con preferencia ECO_ANY_RESOURCE, el resultado es el legacy (más cercano, cualquiera).
    const uint32_t idx_any = eco_find_nearest_deposit(
        deposits, 3, 0, 0, ECO_ANY_RESOURCE, ECO_ALL_DEPOSITS_MASK, fatal);
    CHECK(idx_any == 1u);
}

// ============================================================================
// 6) Auditoría multimodelo 2026-07-27, F-01: una redirección que cambia el
//    recurso de una carga parcial debe descargar primero el recurso anterior.
// ============================================================================
static std::unique_ptr<GameState> make_loaded_gatherer(uint8_t owner,
                                                       int32_t carry,
                                                       uint8_t resource_idx) {
    static DataCatalogV1 cat = gather_fixture::make_catalog();
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg());
    gs_bind_catalog(*g, cat);
    gs_init_epoch_from_catalog(*g);

    const EntityHandle h = et_spawn(g->entities);
    CHECK(h.index == 0u);
    g->owner[h.index] = owner;
    g->unit_id[h.index] = 0u;
    g->unit_class[h.index] = 3u;
    g->hp[h.index] = 20;
    g->speed_mtpt[h.index] = 400;
    g->pos_x[h.index] = 100 * FX_ONE_RAW;
    g->pos_y[h.index] = 100 * FX_ONE_RAW;
    g->build_target[h.index] = BUILD_NO_TARGET;
    g->eco_assigned_deposit[h.index] = 0u;  // depósito A
    g->eco_state[h.index] = EcoState::HARVEST;
    g->eco_carry[h.index] = carry;
    g->eco_carry_resource[h.index] = resource_idx;
    return g;
}

static void move_loaded_gatherer_to_dropoff(GameState& g, uint32_t ci) {
    g.pos_x[ci] = g.dropoff_x[g.owner[ci]];
    g.pos_y[ci] = g.dropoff_y[g.owner[ci]];
}

static void test_partial_load_redirect_different_resource_preserves_type() {
    auto g = make_loaded_gatherer(0u, 10, 0u);
    const int64_t stock_a = g->player_stock[0][0];
    const int64_t stock_b = g->player_stock[0][1];
    const EntityHandle h{0u, g->entities.generation[0]};

    RawCommand redirect_b = gather(g->tick, 0u, 1u, h,
                                   g->deposits[2].x_raw, g->deposits[2].y_raw);
    CHECK(g->deposits[2].resource_idx == 1u);
    const StepResult redirected = step(*g, &redirect_b, 1u);
    CHECK(redirected.accepted == 1u);
    CHECK(g->eco_assigned_deposit[0] == 2u);
    CHECK(g->eco_state[0] == EcoState::RETURN);
    CHECK(g->eco_carry[0] == 10);
    CHECK(g->eco_carry_resource[0] == 0u);

    move_loaded_gatherer_to_dropoff(*g, 0u);
    step(*g, nullptr, 0u);
    CHECK(g->player_stock[0][0] == stock_a + 10);
    CHECK(g->player_stock[0][1] == stock_b);
    CHECK(g->eco_carry[0] == 0);
    CHECK(g->eco_state[0] == EcoState::SEEK);
    CHECK(g->eco_assigned_deposit[0] == 2u);
}

static void test_partial_load_redirect_same_resource_keeps_gathering() {
    auto g = make_loaded_gatherer(0u, 10, 0u);
    const EntityHandle h{0u, g->entities.generation[0]};
    RawCommand redirect_a = gather(g->tick, 0u, 1u, h,
                                   g->deposits[1].x_raw, g->deposits[1].y_raw);
    CHECK(g->deposits[1].resource_idx == 0u);
    const StepResult redirected = step(*g, &redirect_a, 1u);
    CHECK(redirected.accepted == 1u);
    CHECK(g->eco_assigned_deposit[0] == 1u);
    CHECK(g->eco_state[0] == EcoState::SEEK);
    CHECK(g->eco_carry[0] == 10);
    CHECK(g->eco_carry_resource[0] == 0u);

    g->pos_x[0] = g->deposits[1].x_raw;
    g->pos_y[0] = g->deposits[1].y_raw;
    step(*g, nullptr, 0u);
    CHECK(g->eco_state[0] == EcoState::HARVEST);
    const int32_t remaining_before = g->deposits[1].remaining;
    step(*g, nullptr, 0u);
    CHECK(g->eco_carry[0] == 10 + ECO_HARVEST_PER_TICK);
    CHECK(g->eco_carry_resource[0] == 0u);
    CHECK(g->deposits[1].remaining == remaining_before - ECO_HARVEST_PER_TICK);
}

static void test_full_load_redirect_different_resource_drops_first() {
    auto g = make_loaded_gatherer(0u, ECO_CARRY_CAP, 0u);
    const int64_t stock_a = g->player_stock[0][0];
    const int64_t stock_b = g->player_stock[0][1];
    const EntityHandle h{0u, g->entities.generation[0]};
    RawCommand redirect_b = gather(g->tick, 0u, 1u, h,
                                   g->deposits[2].x_raw, g->deposits[2].y_raw);
    CHECK(step(*g, &redirect_b, 1u).accepted == 1u);
    CHECK(g->eco_state[0] == EcoState::RETURN);
    CHECK(g->eco_carry[0] == ECO_CARRY_CAP);
    CHECK(g->eco_assigned_deposit[0] == 2u);

    move_loaded_gatherer_to_dropoff(*g, 0u);
    step(*g, nullptr, 0u);
    CHECK(g->player_stock[0][0] == stock_a + ECO_CARRY_CAP);
    CHECK(g->player_stock[0][1] == stock_b);
    CHECK(g->eco_state[0] == EcoState::SEEK);
    CHECK(g->eco_assigned_deposit[0] == 2u);
}

static void test_ai_redirect_loaded_donor_preserves_type() {
    auto g = make_loaded_gatherer(1u, 10, 0u);
    g->eco_state[0] = EcoState::SEEK;
    const EntityHandle donor2 = et_spawn(g->entities);
    CHECK(donor2.index == 1u);
    g->owner[1] = 1u;
    g->unit_id[1] = 0u;
    g->unit_class[1] = 3u;
    g->hp[1] = 20;
    g->speed_mtpt[1] = 400;
    g->pos_x[1] = 101 * FX_ONE_RAW;
    g->pos_y[1] = 100 * FX_ONE_RAW;
    g->build_target[1] = BUILD_NO_TARGET;
    g->eco_assigned_deposit[1] = 0u;
    g->eco_state[1] = EcoState::SEEK;
    g->eco_carry[1] = 0;
    g->eco_carry_resource[1] = 0u;
    g->player_stock[1][0] = 1000;
    g->player_stock[1][1] = 0;
    g->player_stock[1][2] = 1000;
    const int64_t stock_a = g->player_stock[1][0];
    const int64_t stock_b = g->player_stock[1][1];

    AiJobBox box{};
    ai_box_init(box, 1u);
    box.state = AiJobState::DISPATCHED;
    box.source_tick = g->tick;
    box.runtime_before = AiRuntimeV1{0u, 0u};
    ai_execute(box, *g);

    const RawCommand* redirect = nullptr;
    for (uint32_t i = 0; i < box.result_count; ++i) {
        if (box.result[i].type == CommandType::GATHER) {
            redirect = &box.result[i];
            break;
        }
    }
    CHECK(redirect != nullptr);
    if (redirect == nullptr) return;
    CHECK(redirect->p.handle.index == 0u);
    step(*g, redirect, 1u);
    while (g->tick <= redirect->target_tick) step(*g, nullptr, 0u);
    CHECK(last_result(g->mailbox[1]) == RejectReason::ACCEPTED);
    CHECK(g->eco_state[0] == EcoState::RETURN);
    CHECK(g->eco_carry[0] == 10);
    CHECK(g->eco_carry_resource[0] == 0u);
    CHECK(g->deposits[g->eco_assigned_deposit[0]].resource_idx == 1u);

    move_loaded_gatherer_to_dropoff(*g, 0u);
    step(*g, nullptr, 0u);
    CHECK(g->player_stock[1][0] == stock_a + 10);
    CHECK(g->player_stock[1][1] == stock_b);
}

static void test_save_load_and_replay_preserve_redirect_transition() {
    static DataCatalogV1 cat = gather_fixture::make_catalog();
    const char* save_path = "test_gather_redirect_v12.sav";
    const char* replay_path = "test_gather_redirect_v3.curp";
    auto direct = make_loaded_gatherer(0u, 10, 0u);
    const EntityHandle h{0u, direct->entities.generation[0]};
    RawCommand redirect_b = gather(direct->tick, 0u, 1u, h,
                                   direct->deposits[2].x_raw, direct->deposits[2].y_raw);

    ReplayWriter writer;
    writer.begin(make_cfg().seed, 1u, 1u, 1u, 0u, 20u);
    writer.tick_batch(&redirect_b, 1u, direct->tick);
    CHECK(writer.finish(0u, replay_path) == 0);

    CHECK(step(*direct, &redirect_b, 1u).accepted == 1u);
    CHECK(direct->eco_state[0] == EcoState::RETURN);
    AiJobBox box{};
    ai_box_init(box, 1u);
    AiRuntimeV1 rt{};
    CHECK(save_game(*direct, box, rt, save_path) == 0);

    auto loaded = std::make_unique<GameState>();
    AiJobBox loaded_box{};
    AiRuntimeV1 loaded_rt{};
    CHECK(load_game(*loaded, loaded_box, loaded_rt, save_path) == 0);
    gs_bind_catalog(*loaded, cat);
    CHECK(loaded->eco_carry[0] == direct->eco_carry[0]);
    CHECK(loaded->eco_carry_resource[0] == direct->eco_carry_resource[0]);
    CHECK(loaded->eco_state[0] == direct->eco_state[0]);
    CHECK(loaded->eco_assigned_deposit[0] == direct->eco_assigned_deposit[0]);
    CHECK(state_checksum_v1(*loaded) == state_checksum_v1(*direct));

    ReplayData replay;
    CHECK(replay_load(replay_path, replay) == 0);
    CHECK(replay.batches.size() == 1u && replay.batches[0].size() == 1u);
    auto replayed = make_loaded_gatherer(0u, 10, 0u);
    CHECK(step(*replayed, replay.batches[0].data(),
               static_cast<uint32_t>(replay.batches[0].size())).accepted == 1u);
    CHECK(replayed->eco_carry[0] == direct->eco_carry[0]);
    CHECK(replayed->eco_carry_resource[0] == direct->eco_carry_resource[0]);
    CHECK(replayed->eco_state[0] == direct->eco_state[0]);
    CHECK(replayed->eco_assigned_deposit[0] == direct->eco_assigned_deposit[0]);
    CHECK(state_checksum_v1(*replayed) == state_checksum_v1(*direct));

    move_loaded_gatherer_to_dropoff(*direct, 0u);
    move_loaded_gatherer_to_dropoff(*loaded, 0u);
    move_loaded_gatherer_to_dropoff(*replayed, 0u);
    step(*direct, nullptr, 0u);
    step(*loaded, nullptr, 0u);
    step(*replayed, nullptr, 0u);
    CHECK(direct->player_stock[0][0] == loaded->player_stock[0][0]);
    CHECK(direct->player_stock[0][0] == replayed->player_stock[0][0]);
    CHECK(direct->player_stock[0][1] == loaded->player_stock[0][1]);
    CHECK(direct->player_stock[0][1] == replayed->player_stock[0][1]);
    CHECK(state_checksum_v1(*direct) == state_checksum_v1(*loaded));
    CHECK(state_checksum_v1(*direct) == state_checksum_v1(*replayed));

    std::remove(save_path);
    std::remove(replay_path);
}

// ============================================================================
// 7) Reproducción del defecto del Director (SPEC-004 §23.1): al agotarse A,
//    otro A remoto queda fuera de zona y un B cercano queda dentro. La
//    auto-asignación debe elegir B, no marchar 100+ tiles por preferencia.
// ============================================================================
static void test_auto_gather_prefers_in_zone_over_remote_same_resource() {
    using namespace allied_zone_fixture;
    auto g = make_state(3u);
    g->deposits[0] = EcoDeposit{CENTER_X, CENTER_Y, 0u, 0, 0, 0, 0, 0, 0};
    g->deposits[1] = EcoDeposit{100 * FX_ONE_RAW, CENTER_Y, 0u, 100, 0, 0, 0, 0, 0};
    g->deposits[2] = EcoDeposit{50 * FX_ONE_RAW, CENTER_Y, 1u, 100, 0, 0, 0, 0, 0};
    g->eco_assigned_deposit[1] = 0u;

    step(*g, nullptr, 0u);

    CHECK(g->eco_assigned_deposit[1] == 2u);
    CHECK(g->citizen_task[1] == CITIZEN_TASK_GATHER);
}

// ============================================================================
// 8) Dos preferidos dentro de zona: proximidad primero; con distancia exacta
//    igual, el recorrido ascendente conserva el índice menor.
// ============================================================================
static void test_auto_gather_nearest_and_low_index_tiebreak() {
    using namespace allied_zone_fixture;
    {
        auto g = make_state(3u);
        g->deposits[0] = EcoDeposit{CENTER_X, CENTER_Y, 0u, 0, 0, 0, 0, 0, 0};
        g->deposits[1] = EcoDeposit{55 * FX_ONE_RAW, CENTER_Y, 0u, 100, 0, 0, 0, 0, 0};
        g->deposits[2] = EcoDeposit{45 * FX_ONE_RAW, CENTER_Y, 0u, 100, 0, 0, 0, 0, 0};
        g->eco_assigned_deposit[1] = 0u;

        step(*g, nullptr, 0u);
        CHECK(g->eco_assigned_deposit[1] == 2u);
    }
    {
        auto g = make_state(3u);
        g->deposits[0] = EcoDeposit{CENTER_X, CENTER_Y, 0u, 0, 0, 0, 0, 0, 0};
        g->deposits[1] = EcoDeposit{35 * FX_ONE_RAW, CENTER_Y, 0u, 100, 0, 0, 0, 0, 0};
        g->deposits[2] = EcoDeposit{45 * FX_ONE_RAW, CENTER_Y, 0u, 100, 0, 0, 0, 0, 0};
        g->eco_assigned_deposit[1] = 0u;

        step(*g, nullptr, 0u);
        CHECK(g->eco_assigned_deposit[1] == 1u);
    }
}

// ============================================================================
// 9) Sin depósitos vivos dentro de zona, SEEK termina en IDLE y no conserva
//    una asignación remota ni velocidad residual.
// ============================================================================
static void test_auto_gather_without_in_zone_deposit_becomes_idle() {
    using namespace allied_zone_fixture;
    auto g = make_state(2u);
    g->deposits[0] = EcoDeposit{CENTER_X, CENTER_Y, 0u, 0, 0, 0, 0, 0, 0};
    g->deposits[1] = EcoDeposit{100 * FX_ONE_RAW, CENTER_Y, 1u, 100, 0, 0, 0, 0, 0};
    g->eco_assigned_deposit[1] = 0u;
    g->vel_x[1] = FX_ONE_RAW;

    step(*g, nullptr, 0u);

    CHECK(g->eco_assigned_deposit[1] == ECO_NO_DEPOSIT);
    CHECK(g->eco_state[1] == EcoState::SEEK);
    CHECK(g->citizen_task[1] == CITIZEN_TASK_IDLE);
    CHECK(g->vel_x[1] == 0 && g->vel_y[1] == 0);
}

// ============================================================================
// 10) Agencia del jugador (§23.3): GATHER directo a un depósito remoto se
//     acepta y la locomoción comienza hacia él aunque la máscara automática
//     esté vacía para ese depósito.
// ============================================================================
static void test_player_gather_outside_allied_zone_is_accepted() {
    using namespace allied_zone_fixture;
    auto g = make_state(1u);
    g->deposits[0] = EcoDeposit{100 * FX_ONE_RAW, CENTER_Y, 0u, 100, 0, 0, 0, 0, 0};
    const EntityHandle citizen{1u, g->entities.generation[1]};
    const int64_t x_before = g->pos_x[1];
    RawCommand cmd = gather(g->tick, 0u, 1u, citizen,
                            g->deposits[0].x_raw, g->deposits[0].y_raw);

    const StepResult result = step(*g, &cmd, 1u);

    CHECK(result.accepted == 1u);
    CHECK(last_result(g->mailbox[0]) == RejectReason::ACCEPTED);
    CHECK(g->eco_assigned_deposit[1] == 0u);
    CHECK(g->citizen_task[1] == CITIZEN_TASK_GATHER);
    CHECK(g->pos_x[1] > x_before);
}

// ============================================================================
// 11) Un GATHER remoto conserva su objetivo hasta agotarlo. Después, el
//     siguiente retarget vuelve a la búsqueda automática acotada y trae al
//     ciudadano a un recurso local, aunque sea de tipo distinto.
// ============================================================================
static void test_remote_player_deposit_exhaustion_returns_to_allied_zone() {
    using namespace allied_zone_fixture;
    auto g = make_state(2u);
    g->deposits[0] = EcoDeposit{50 * FX_ONE_RAW, CENTER_Y, 1u, 100, 0, 0, 0, 0, 0};
    g->deposits[1] = EcoDeposit{100 * FX_ONE_RAW, CENTER_Y, 0u, 100, 0, 0, 0, 0, 0};
    const EntityHandle citizen{1u, g->entities.generation[1]};
    RawCommand cmd = gather(g->tick, 0u, 1u, citizen,
                            g->deposits[1].x_raw, g->deposits[1].y_raw);
    CHECK(step(*g, &cmd, 1u).accepted == 1u);
    CHECK(g->eco_assigned_deposit[1] == 1u);

    g->deposits[1].remaining = 0;
    g->eco_state[1] = EcoState::SEEK;
    g->eco_carry[1] = 0;
    step(*g, nullptr, 0u);

    CHECK(g->eco_assigned_deposit[1] == 0u);
    CHECK(g->citizen_task[1] == CITIZEN_TASK_GATHER);
}

// ============================================================================
// 12) La completitud se evalúa dinámicamente con el helper compartido de
//     dropoff/zona: un sitio incompleto no habilita el depósito; al alcanzar
//     build_time_ticks, el mismo depósito entra en la máscara y se asigna.
// ============================================================================
static void test_completed_expansion_building_extends_allied_zone() {
    using namespace allied_zone_fixture;
    auto g = make_state(1u);
    g->deposits[0] = EcoDeposit{100 * FX_ONE_RAW, CENTER_Y, 2u, 100, 0, 0, 0, 0, 0};
    const EntityHandle expansion =
        spawn_building(*g, 1u, 100 * FX_ONE_RAW, CENTER_Y, 49u);
    CHECK(expansion.index == 2u);
    CHECK((detail::allied_auto_gather_deposit_mask(*g, 0u) & 1u) == 0u);

    g->build_progress[expansion.index] = 50u;
    CHECK((detail::allied_auto_gather_deposit_mask(*g, 0u) & 1u) != 0u);
    step(*g, nullptr, 0u);

    CHECK(g->eco_assigned_deposit[1] == 0u);
    CHECK(g->citizen_task[1] == CITIZEN_TASK_GATHER);
}

int main() {
    test_gather_happy_path();
    test_gather_rejections_in_order();
    test_gather_cancels_build_target();
    test_gather_redirects_active_gatherer();
    test_exhaustion_prefers_same_resource_then_any_then_idle();
    test_partial_load_redirect_different_resource_preserves_type();
    test_partial_load_redirect_same_resource_keeps_gathering();
    test_full_load_redirect_different_resource_drops_first();
    test_ai_redirect_loaded_donor_preserves_type();
    test_save_load_and_replay_preserve_redirect_transition();
    test_auto_gather_prefers_in_zone_over_remote_same_resource();
    test_auto_gather_nearest_and_low_index_tiebreak();
    test_auto_gather_without_in_zone_deposit_becomes_idle();
    test_player_gather_outside_allied_zone_is_accepted();
    test_remote_player_deposit_exhaustion_returns_to_allied_zone();
    test_completed_expansion_building_extends_allied_zone();

    if (g_fails == 0) { std::printf("gather: OK\n"); return 0; }
    std::printf("gather: %d fallos\n", g_fails);
    return 1;
}
