// Test de la IA jugando la apertura económica (Sprint 1.6B, pieza K2,
// SPEC-004 §19). Autor: sonnet-5 (brief
// docs/briefs/SONNET_K2_GATHER_APERTURA_SPRINT_1.6B.md).
//
// Cubre: ai_find_trainer_type respeta civ_id (regresión del bug real que este
// sprint corrige: con DOS civs reales en el mismo catálogo, un barrido
// civ-agnóstico resolvía SIEMPRE el primer edificio del catálogo sin importar
// la civ del jugador IA) · la capa económica adaptativa emite GATHER
// redirigiendo un ocioso (prioridad) o un excedente cuando un recurso está
// bajo el umbral · la capa económica NO reasigna dos veces al mismo
// ciudadano que ASSIGN_BUILD (paso 1) ya consumió este ciclo · TRAIN_UNIT
// repone ciudadanos cuando faltan (con civ asignada) · determinismo de
// ai_execute con la capa económica activa (dos llamadas, mismo estado
// congelado -> result[] byte-idéntico).
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
#include "chunsa/ai_stub.hpp"

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

// ============================================================================
// Fixture "2 civs" (regresión del bug de ai_find_trainer_type): CIV_A(=10) va
// PRIMERO en el catálogo (índices bajos), CIV_B(=20) después — el orden
// deliberadamente adversarial: un barrido civ-agnóstico SIEMPRE encontraría
// primero los edificios/unidades de CIV_A, sin importar de qué civ es el
// jugador que pregunta.
// units[0]=citizenA(civ A, Citizen) units[1]=soldierA(civ A, Infantry)
// units[2]=citizenB(civ B, Citizen) units[3]=soldierB(civ B, Infantry)
// buildings[0]=centerA(civ A, nace completo, trains=[citizenA])
// buildings[1]=barracksA(civ A, constructible, trains=[soldierA])
// buildings[2]=centerB(civ B, nace completo, trains=[citizenB])
// buildings[3]=barracksB(civ B, constructible, trains=[soldierB])
// ============================================================================
namespace civ2_fixture {
inline constexpr CivId CIV_A = 10;
inline constexpr CivId CIV_B = 20;

inline UnitDefinitionV1 make_citizen(UnitId id, CivId civ) {
    UnitDefinitionV1 d{};
    d.id = id; d.civ_id = civ; d.unit_class = UnitClassV1::Citizen; d.tags_mask = 0;
    d.hp = 20; d.attack = 0; d.range_millitiles = 0;
    d.speed_millitile_tick = 400; d.morale = 100; d.build_time_ticks = 1;
    for (int k = 0; k < 6; ++k) d.bonus_vs_bp[k] = 0;
    d.cost[0] = 5; d.cost[1] = 0; d.cost[2] = 0; d.pop_cost = 1;
    d.epoch_min = 1; d.epoch_max = 15;
    return d;
}
inline UnitDefinitionV1 make_soldier(UnitId id, CivId civ) {
    UnitDefinitionV1 d{};
    d.id = id; d.civ_id = civ; d.unit_class = UnitClassV1::Infantry; d.tags_mask = 0;
    d.hp = 50; d.attack = 10; d.range_millitiles = 1000;
    d.speed_millitile_tick = 400; d.morale = 100; d.build_time_ticks = 2;
    for (int k = 0; k < 6; ++k) d.bonus_vs_bp[k] = 0;
    d.cost[0] = 10; d.cost[1] = 0; d.cost[2] = 0; d.pop_cost = 1;
    d.epoch_min = 1; d.epoch_max = 15;
    return d;
}
inline BuildingDefinitionV1 make_center(BuildingId id, CivId civ, UnitId trains) {
    BuildingDefinitionV1 d{};
    d.id = id; d.civ_id = civ; d.hp = 500; d.footprint_w = 2; d.footprint_h = 2;
    d.build_time_ticks = 0;
    d.cost[0] = 0; d.cost[1] = 0; d.cost[2] = 0;
    d.dropoff_mask = 0; d.constructible = 0;
    d.epoch_min = 1; d.epoch_max = 15;
    d.trains[0] = trains; d.train_count = 1;
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
inline BuildingDefinitionV1 make_barracks(BuildingId id, CivId civ, UnitId trains) {
    BuildingDefinitionV1 d{};
    d.id = id; d.civ_id = civ; d.hp = 300; d.footprint_w = 2; d.footprint_h = 2;
    d.build_time_ticks = 50;
    d.cost[0] = 0; d.cost[1] = 20; d.cost[2] = 0;
    d.dropoff_mask = 0; d.constructible = 1;
    d.epoch_min = 1; d.epoch_max = 15;
    d.trains[0] = trains; d.train_count = 1;
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

static UnitDefinitionV1 g_units[4] = {
    make_citizen(0, CIV_A), make_soldier(1, CIV_A),
    make_citizen(2, CIV_B), make_soldier(3, CIV_B),
};
static BuildingDefinitionV1 g_buildings[4] = {
    make_center(0, CIV_A, 0 /*citizenA*/), make_barracks(1, CIV_A, 1 /*soldierA*/),
    make_center(2, CIV_B, 2 /*citizenB*/), make_barracks(3, CIV_B, 3 /*soldierB*/),
};

inline DataCatalogV1 make_catalog() {
    DataCatalogV1 c{};
    c.unit_count = 4; c.units = g_units; c.unit_names = nullptr;
    c.building_count = 4; c.buildings = g_buildings; c.building_names = nullptr;
    c.tech_count = 0; c.techs = nullptr; c.tech_names = nullptr;
    c.capability_count = 0; c.capability_names = nullptr;
    return c;
}
}  // namespace civ2_fixture

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

static void run_ai_once(const GameState& g, uint8_t ai_player, uint32_t source_tick,
                        uint64_t ai_sequence, AiJobBox& box) {
    ai_box_init(box, ai_player);
    box.state = AiJobState::DISPATCHED;
    box.source_tick = source_tick;
    box.runtime_before = AiRuntimeV1{0u, ai_sequence};
    ai_execute(box, g);
}

// ============================================================================
// 1) ai_find_trainer_type respeta civ_id: el jugador de CIV_B (índices de
//    catálogo MÁS ALTOS) entrena SU PROPIO ciudadano (citizenB) desde SU
//    PROPIO centro (centerB), pese a que centerA/citizenA (CIV_A) resuelven
//    ANTES en un barrido civ-agnóstico. Sin el filtro por civ, la IA de
//    CIV_B jamás encontraría un edificio suyo (ai_find_owned_building_of_type
//    sobre centerA, que no posee) y quedaría económicamente paralizada.
// ============================================================================
static void test_trainer_type_respects_civ() {
    static DataCatalogV1 cat = civ2_fixture::make_catalog();
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg());
    gs_bind_catalog(*g, cat);
    gs_init_epoch_from_catalog(*g);
    gs_set_player_civ(*g, 1, civ2_fixture::CIV_B);

    RawCommand setup = place_building(0, 1, 1, 2 /*centerB*/, 40, 40);
    const StepResult r0 = step(*g, &setup, 1);
    CHECK(r0.accepted == 1);
    g->player_stock[1][0] = 100;  // A: paga citizenB (cost[0]=5)

    AiJobBox box{};
    run_ai_once(*g, 1, g->tick, /*ai_sequence=*/1, box);
    bool found_train = false;
    for (uint32_t i = 0; i < box.result_count; ++i) {
        if (box.result[i].type == CommandType::TRAIN_UNIT) {
            CHECK(box.result[i].p.unit_id == 2u /*citizenB*/);
            found_train = true;
        }
    }
    CHECK(found_train);

    // Aplicarlo: el kernel lo acepta (civ correcta) y produce un citizenB real.
    RawCommand applied[AI_MAX_COMMANDS];
    for (uint32_t i = 0; i < box.result_count; ++i) applied[i] = box.result[i];
    while (g->tick < box.result[0].target_tick) step(*g, nullptr, 0);
    const StepResult r1 = step(*g, applied, box.result_count);
    CHECK(r1.accepted == box.result_count);

    uint32_t new_citizens_b = 0;
    for (uint32_t i = 0; i < g->entities.capacity; ++i) {
        if (g->entities.alive[i] && g->owner[i] == 1 && g->unit_class[i] == 3u
            && g->unit_id[i] == 2u /*citizenB*/) {
            ++new_citizens_b;
        }
    }
    CHECK(new_citizens_b >= 1u);
}

// ============================================================================
// 2) La capa económica emite GATHER redirigiendo el ciudadano OCIOSO
//    (prioridad sobre excedente) cuando un recurso (B) está bajo el umbral.
// ============================================================================
static void test_economic_layer_redirects_idle_citizen() {
    static DataCatalogV1 cat = civ2_fixture::make_catalog();
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg());
    gs_bind_catalog(*g, cat);
    gs_init_epoch_from_catalog(*g);

    RawCommand setup = place_building(0, 1, 1, 0 /*centerA, nace completo*/, 40, 40);
    const StepResult r0 = step(*g, &setup, 1);
    CHECK(r0.accepted == 1);

    // 3 depósitos propios de prueba: A cerca, B cerca, Me lejos — todos vivos.
    const int64_t T = FX_ONE_RAW;
    g->n_deposits = 3;
    g->deposits[0] = EcoDeposit{100 * T, 100 * T, 0u /*A*/, 500};
    g->deposits[1] = EcoDeposit{105 * T, 100 * T, 1u /*B*/, 500};
    g->deposits[2] = EcoDeposit{200 * T, 200 * T, 2u /*Me*/, 500};

    // 3 ciudadanos propios (owner=1): 2 ya recolectando A, 1 OCIOSO (sin
    // depósito válido). Ninguno con build_target.
    for (int k = 0; k < 3; ++k) {
        const EntityHandle h = et_spawn(g->entities);
        const uint32_t i = h.index;
        g->owner[i] = 1;
        g->unit_class[i] = 3u;
        g->pos_x[i] = 101 * T; g->pos_y[i] = 101 * T;
        g->build_target[i] = BUILD_NO_TARGET;
        if (k < 2) {
            g->eco_assigned_deposit[i] = 0u;  // A
            g->eco_state[i] = EcoState::SEEK;
        } else {
            g->eco_assigned_deposit[i] = ECO_NO_DEPOSIT;  // ocioso
            g->eco_state[i] = EcoState::SEEK;
        }
    }

    // Stock: A y Me sobrados, B en cero (bajo el umbral) -> la IA debe
    // redirigir hacia B.
    g->player_stock[1][0] = 1000;  // A
    g->player_stock[1][1] = 0;     // B (falta)
    g->player_stock[1][2] = 1000;  // Me

    AiJobBox box{};
    run_ai_once(*g, 1, g->tick, /*ai_sequence=*/1, box);

    bool found_gather = false;
    uint32_t gather_target_idx = 0;
    for (uint32_t i = 0; i < box.result_count; ++i) {
        if (box.result[i].type == CommandType::GATHER) {
            found_gather = true;
            gather_target_idx = box.result[i].p.handle.index;
            CHECK(box.result[i].p.x_raw == g->deposits[1].x_raw);
            CHECK(box.result[i].p.y_raw == g->deposits[1].y_raw);
        }
    }
    CHECK(found_gather);
    // El ocioso (índice 3, tercer spawn tras el centro en índice 0..2 —
    // et_spawn asigna 1,2,3 tras el centro en 0) es el candidato PRIORITARIO,
    // no uno de los que ya recolectaban A.
    CHECK(g->eco_assigned_deposit[gather_target_idx] == ECO_NO_DEPOSIT);  // AÚN no aplicado
    bool target_was_idle = true;
    for (int k = 0; k < 2; ++k) {
        // Los índices 1,2 (los que YA recolectaban A) no deben ser el objetivo.
        if (gather_target_idx == static_cast<uint32_t>(1 + k)) target_was_idle = false;
    }
    CHECK(target_was_idle);
}

// ============================================================================
// 3) Sin ocioso disponible: la capa económica redirige un EXCEDENTE (el
//    ciudadano del recurso con MÁS asignados, aquí A con 2 contra Me con 1).
// ============================================================================
static void test_economic_layer_redirects_surplus_when_no_idle() {
    static DataCatalogV1 cat = civ2_fixture::make_catalog();
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg());
    gs_bind_catalog(*g, cat);
    gs_init_epoch_from_catalog(*g);

    RawCommand setup = place_building(0, 1, 1, 0 /*centerA*/, 40, 40);
    const StepResult r0 = step(*g, &setup, 1);
    CHECK(r0.accepted == 1);

    const int64_t T = FX_ONE_RAW;
    g->n_deposits = 3;
    g->deposits[0] = EcoDeposit{100 * T, 100 * T, 0u /*A*/, 500};
    g->deposits[1] = EcoDeposit{105 * T, 100 * T, 1u /*B*/, 500};
    g->deposits[2] = EcoDeposit{200 * T, 200 * T, 2u /*Me*/, 500};

    // 3 ciudadanos: 2 en A (índices 1,2), 1 en Me (índice 3) — NINGUNO ocioso.
    for (int k = 0; k < 3; ++k) {
        const EntityHandle h = et_spawn(g->entities);
        const uint32_t i = h.index;
        g->owner[i] = 1;
        g->unit_class[i] = 3u;
        g->pos_x[i] = 101 * T; g->pos_y[i] = 101 * T;
        g->build_target[i] = BUILD_NO_TARGET;
        g->eco_assigned_deposit[i] = (k < 2) ? 0u : 2u;
        g->eco_state[i] = EcoState::SEEK;
    }

    g->player_stock[1][0] = 1000;  // A
    g->player_stock[1][1] = 0;     // B (falta)
    g->player_stock[1][2] = 1000;  // Me

    AiJobBox box{};
    run_ai_once(*g, 1, g->tick, /*ai_sequence=*/1, box);

    bool found_gather = false;
    uint32_t gather_target_idx = 0;
    for (uint32_t i = 0; i < box.result_count; ++i) {
        if (box.result[i].type == CommandType::GATHER) {
            found_gather = true;
            gather_target_idx = box.result[i].p.handle.index;
        }
    }
    CHECK(found_gather);
    // Donante = A (2 asignados, > el 1 de Me): el candidato es el PRIMERO
    // (menor índice) de los que recolectan A -> índice 1.
    CHECK(gather_target_idx == 1u);
}

// ============================================================================
// 4) La capa económica NO reasigna al mismo ciudadano que ASSIGN_BUILD
//    (paso 1) ya consumió este ciclo — sin otro candidato disponible, no
//    emite ningún GATHER (evita órdenes contradictorias en el mismo tick).
// ============================================================================
static void test_economic_layer_skips_citizen_used_by_assign_build() {
    static DataCatalogV1 cat = civ2_fixture::make_catalog();
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg());
    gs_bind_catalog(*g, cat);
    gs_init_epoch_from_catalog(*g);

    // Centro (ancla) + un sitio INCOMPLETO (barracksA) propio sin ciudadano
    // asignado -> dispara ASSIGN_BUILD en el paso 1.
    RawCommand setup[2] = {
        place_building(0, 1, 1, 0 /*centerA*/, 40, 40),
        place_building(0, 1, 2, 1 /*barracksA, incompleto*/, 60, 60),
    };
    const StepResult r0 = step(*g, setup, 2);
    CHECK(r0.accepted == 2);

    const int64_t T = FX_ONE_RAW;
    g->n_deposits = 1;
    g->deposits[0] = EcoDeposit{100 * T, 100 * T, 1u /*B*/, 500};

    // ÚNICO ciudadano propio: ocioso (sin build_target, sin depósito) — el
    // ÚNICO candidato tanto para ASSIGN_BUILD como para la capa económica.
    const EntityHandle h = et_spawn(g->entities);
    const uint32_t ci = h.index;
    g->owner[ci] = 1;
    g->unit_class[ci] = 3u;
    g->pos_x[ci] = 61 * T; g->pos_y[ci] = 61 * T;
    g->build_target[ci] = BUILD_NO_TARGET;
    g->eco_assigned_deposit[ci] = ECO_NO_DEPOSIT;
    g->eco_state[ci] = EcoState::SEEK;

    g->player_stock[1][1] = 0;  // B falta (umbral bajo) -> tentaría a GATHER

    AiJobBox box{};
    run_ai_once(*g, 1, g->tick, /*ai_sequence=*/2, box);

    bool found_assign = false, found_gather = false;
    for (uint32_t i = 0; i < box.result_count; ++i) {
        if (box.result[i].type == CommandType::ASSIGN_BUILD) found_assign = true;
        if (box.result[i].type == CommandType::GATHER) found_gather = true;
    }
    CHECK(found_assign);
    CHECK(!found_gather);  // el único ciudadano ya fue consumido por ASSIGN_BUILD
}

// ============================================================================
// 5) Determinismo de ai_execute con la capa económica activa: mismo
//    (g, source_tick, runtime) -> result[] byte-idéntico en dos llamadas.
// ============================================================================
static void test_ai_execute_deterministic_with_economic_layer() {
    static DataCatalogV1 cat = civ2_fixture::make_catalog();
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg());
    gs_bind_catalog(*g, cat);
    gs_init_epoch_from_catalog(*g);

    RawCommand setup = place_building(0, 1, 1, 0 /*centerA*/, 40, 40);
    const StepResult r0 = step(*g, &setup, 1);
    CHECK(r0.accepted == 1);

    const int64_t T = FX_ONE_RAW;
    g->n_deposits = 2;
    g->deposits[0] = EcoDeposit{100 * T, 100 * T, 0u /*A*/, 500};
    g->deposits[1] = EcoDeposit{105 * T, 100 * T, 1u /*B*/, 500};
    for (int k = 0; k < 2; ++k) {
        const EntityHandle h = et_spawn(g->entities);
        const uint32_t i = h.index;
        g->owner[i] = 1;
        g->unit_class[i] = 3u;
        g->pos_x[i] = 101 * T; g->pos_y[i] = 101 * T;
        g->build_target[i] = BUILD_NO_TARGET;
        g->eco_assigned_deposit[i] = 0u;  // ambos en A
        g->eco_state[i] = EcoState::SEEK;
    }
    g->player_stock[1][0] = 1000;
    g->player_stock[1][1] = 0;  // B falta -> dispara la capa económica

    AiJobBox box_a{}, box_b{};
    run_ai_once(*g, 1, g->tick, /*ai_sequence=*/1, box_a);
    run_ai_once(*g, 1, g->tick, /*ai_sequence=*/1, box_b);

    CHECK(box_a.result_count == box_b.result_count);
    CHECK(box_a.result_count > 0);
    bool any_gather = false;
    for (uint32_t i = 0; i < box_a.result_count && i < box_b.result_count; ++i) {
        const RawCommand& ca = box_a.result[i];
        const RawCommand& cb = box_b.result[i];
        CHECK(ca.target_tick == cb.target_tick);
        CHECK(ca.emitter == cb.emitter);
        CHECK(ca.type == cb.type);
        CHECK(ca.sequence == cb.sequence);
        CHECK(ca.p.handle.index == cb.p.handle.index);
        CHECK(ca.p.handle.generation == cb.p.handle.generation);
        CHECK(ca.p.x_raw == cb.p.x_raw);
        CHECK(ca.p.y_raw == cb.p.y_raw);
        CHECK(ca.p.unit_id == cb.p.unit_id);
        if (ca.type == CommandType::GATHER) any_gather = true;
    }
    CHECK(any_gather);
}

int main() {
    test_trainer_type_respects_civ();
    test_economic_layer_redirects_idle_citizen();
    test_economic_layer_redirects_surplus_when_no_idle();
    test_economic_layer_skips_citizen_used_by_assign_build();
    test_ai_execute_deterministic_with_economic_layer();

    if (g_fails == 0) { std::printf("ai_apertura: OK\n"); return 0; }
    std::printf("ai_apertura: %d fallos\n", g_fails);
    return 1;
}
