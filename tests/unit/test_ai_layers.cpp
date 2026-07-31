// Test de las 3 capas de la IA v1 (Sprint 1.4 K2, SPEC-005 §4/§5/§8.4).
// Autor: sonnet-5 (brief docs/briefs/SONNET_K2_IA_EXECUTE_SPRINT_1.4.md).
//
// Cubre §8.4 (subconjunto K2): cada capa en un fixture pequeño — la IA
// construye un cuartel (PLACE_BUILDING válido, después ASSIGN_BUILD del
// ciudadano ocioso al sitio incompleto), entrena (TRAIN_UNIT de un ciudadano
// desde el centro completo), ataca (MOVE_TO del ejército hacia el edificio
// enemigo más cercano), defiende (capa reactiva: enemigo de combate cerca
// del ancla -> MOVE_TO al ancla en vez de atacar), presupuesto de comandos
// respetado (nunca > AI_MAX_COMMANDS) y determinismo de ai_execute (mismo
// (g, source_tick, runtime) -> result[] idéntico byte a byte).
//
// Fixture sintético en memoria (mismo patrón que test_production_tech.cpp/
// test_victory_ai_profile.cpp — sin pasar por el loader CHDB real): NO usa
// CHUNSA_GOLDEN_CHDB_PATH.
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
// Fixture: units[0]="citizen" (Citizen, barato); units[1]="soldier"
// (Infantry, combate). buildings[0]="center" (no construible, nace
// completo, trains=[citizen]); buildings[1]="barracks" (construible,
// trains=[soldier], build_time_ticks=50 > 0 para poder observar el estado
// "incompleto" entre PLACE_BUILDING y ASSIGN_BUILD).
// ============================================================================
namespace ai_fixture {

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
    d.build_time_ticks = 0;  // nace completo (pre-colocado por escenario)
    d.cost[0] = 0; d.cost[1] = 0; d.cost[2] = 0;
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
inline BuildingDefinitionV1 make_barracks() {
    BuildingDefinitionV1 d{};
    d.id = 1; d.hp = 300; d.footprint_w = 2; d.footprint_h = 2;
    d.build_time_ticks = 50;  // > 0: observable "incompleto" entre PLACE y ASSIGN
    d.cost[0] = 0; d.cost[1] = 20; d.cost[2] = 0;
    d.dropoff_mask = 0; d.constructible = 1;
    d.epoch_min = 1; d.epoch_max = 15;
    d.trains[0] = 1 /*soldier*/; d.train_count = 1;
    for (uint32_t k = 1; k < PROD_TRAINS_MAX; ++k) d.trains[k] = INVALID_UNIT_ID;
    for (uint32_t k = 0; k < PROD_TECHS_MAX; ++k) d.researches[k] = INVALID_TECH_ID;
    d.research_count = 0;
    for (uint32_t k = 0; k < BUILDING_REQCAP_MAX; ++k) d.required_capabilities[k] = INVALID_CAPABILITY_ID;
    d.required_capabilities_count = 0;
    return d;
}

static UnitDefinitionV1 g_units[2] = { make_citizen(), make_soldier() };
static BuildingDefinitionV1 g_buildings[2] = { make_center(), make_barracks() };

inline DataCatalogV1 make_catalog() {
    DataCatalogV1 c{};
    c.unit_count = 2; c.units = g_units; c.unit_names = nullptr;
    c.building_count = 2; c.buildings = g_buildings; c.building_names = nullptr;
    c.tech_count = 0; c.techs = nullptr; c.tech_names = nullptr;
    c.capability_count = 0; c.capability_names = nullptr;
    c.ai_profile_count = 0; c.ai_profiles = nullptr; c.ai_profile_names = nullptr;
    return c;
}

}  // namespace ai_fixture

static MatchConfig01A make_cfg(uint32_t max_entities = 256) {
    MatchConfig01A cfg{};
    cfg.max_entities = max_entities;
    cfg.player_count = 2;
    cfg.human_input_delay_ticks = 0;
    cfg.max_future_command_ticks = 20;
    cfg.checksum_every_ticks = 1;
    cfg.map_tiles_x = 256; cfg.map_tiles_y = 256;
    cfg.seed = 20260724ull;
    cfg.allow_debug_stat_payload = 0;
    return cfg;
}

// ---- Helpers de comandos (mismo patrón que test_production_tech.cpp) ------
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

// Ejecuta un ciclo de decisión de la IA de forma aislada (sin pasar por
// ai_dispatch/ai_should_dispatch — control total del test sobre
// source_tick/ai_sequence, mismo efecto que congela ai_dispatch).
static void run_ai_once(const GameState& g, uint8_t ai_player, uint32_t source_tick,
                        uint64_t ai_sequence, AiJobBox& box) {
    ai_box_init(box, ai_player);
    box.state = AiJobState::DISPATCHED;
    box.source_tick = source_tick;
    box.runtime_before = AiRuntimeV1{0u, ai_sequence};
    ai_execute(box, g);
}

// Avanza g hasta que g.tick == until (ticks vacíos, sin comandos).
static void advance_to(GameState& g, uint32_t until) {
    while (g.tick < until) step(g, nullptr, 0);
}

static uint32_t find_owned_building(const GameState& g, uint8_t owner, BuildingId type_id) {
    for (uint32_t i = 0; i < g.entities.capacity; ++i) {
        if (g.entities.alive[i] && g.owner[i] == owner && g.entity_kind[i] == 1u
            && g.building_id[i] == type_id) {
            return i;
        }
    }
    return g.entities.capacity;
}

// ============================================================================
// A) La IA construye un cuartel: PLACE_BUILDING válido (aceptado por el
//    kernel), y en el ciclo siguiente ASSIGN_BUILD del ciudadano ocioso al
//    sitio incompleto (también aceptado).
// ============================================================================
static void test_layer_build_and_assign() {
    static DataCatalogV1 cat = ai_fixture::make_catalog();
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg());
    gs_bind_catalog(*g, cat);
    gs_init_epoch_from_catalog(*g);

    // t=0: centro del jugador IA (owner=1) + 2 ciudadanos ociosos cerca.
    RawCommand setup[3] = {
        place_building(0, 1, 1, 0 /*center*/, 40, 40),
        spawn_citizen(0, 1, 2, 42 * 65536, 42 * 65536),
        spawn_citizen(0, 1, 3, 43 * 65536, 43 * 65536),
    };
    const StepResult r0 = step(*g, setup, 3);
    CHECK(r0.accepted == 3);

    // Solo B: paga el cuartel (cost[1]=20) pero NO el ciudadano (cost[0]=5,
    // A=0) — aísla la intención "construir" de "economía" en este ciclo.
    g->player_stock[1][1] = 100;

    AiJobBox box{};
    // ai_sequence arranca en 3: el batch de t=0 ya consumió sequences 1..3
    // bajo emitter=1 (mismo patrón que drive_fresh: AiRuntimeV1 se siembra
    // con el nº de comandos de setup ya emitidos por ese emisor).
    run_ai_once(*g, 1, g->tick, /*ai_sequence=*/3, box);
    CHECK(box.state == AiJobState::COMPLETED);
    CHECK(box.result_count >= 1);
    bool found_place = false;
    for (uint32_t i = 0; i < box.result_count; ++i) {
        if (box.result[i].type == CommandType::PLACE_BUILDING) {
            CHECK(box.result[i].p.unit_id == 1u);  // barracks
            found_place = true;
        }
    }
    CHECK(found_place);

    RawCommand applied1[AI_MAX_COMMANDS];
    for (uint32_t i = 0; i < box.result_count; ++i) applied1[i] = box.result[i];
    const uint32_t target_tick1 = box.result[0].target_tick;
    advance_to(*g, target_tick1);
    const StepResult r1 = step(*g, applied1, box.result_count);
    CHECK(r1.accepted == box.result_count);  // el kernel acepta lo que la IA emitió

    const uint32_t barracks_idx = find_owned_building(*g, 1, 1 /*barracks*/);
    CHECK(barracks_idx != g->entities.capacity);
    CHECK(g->build_progress[barracks_idx] < 50u);  // incompleto

    // Siguiente ciclo: ASSIGN_BUILD del ciudadano ocioso al sitio.
    AiJobBox box2{};
    run_ai_once(*g, 1, g->tick, /*ai_sequence=*/3 + box.result_count, box2);
    bool found_assign = false;
    for (uint32_t i = 0; i < box2.result_count; ++i) {
        if (box2.result[i].type == CommandType::ASSIGN_BUILD) found_assign = true;
    }
    CHECK(found_assign);
    CHECK(box2.result_count >= 1);

    RawCommand applied2[AI_MAX_COMMANDS];
    for (uint32_t i = 0; i < box2.result_count; ++i) applied2[i] = box2.result[i];
    const uint32_t target_tick2 = box2.result[0].target_tick;
    advance_to(*g, target_tick2);
    const StepResult r2 = step(*g, applied2, box2.result_count);
    CHECK(r2.accepted == box2.result_count);

    bool any_assigned = false;
    for (uint32_t i = 0; i < g->entities.capacity; ++i) {
        if (g->entities.alive[i] && g->owner[i] == 1 && g->unit_class[i] == 3u
            && g->build_target[i] == barracks_idx) {
            any_assigned = true;
        }
    }
    CHECK(any_assigned);
}

// ============================================================================
// B) La IA entrena: TRAIN_UNIT válido de un ciudadano desde el centro ya
//    completo (aceptado por el kernel, prod_count incrementa).
// ============================================================================
static void test_layer_train() {
    static DataCatalogV1 cat = ai_fixture::make_catalog();
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg());
    gs_bind_catalog(*g, cat);
    gs_init_epoch_from_catalog(*g);

    RawCommand setup = place_building(0, 1, 1, 0 /*center*/, 40, 40);
    const StepResult r0 = step(*g, &setup, 1);
    CHECK(r0.accepted == 1);

    g->player_stock[1][0] = 100;  // A: paga el ciudadano (cost[0]=5)

    AiJobBox box{};
    run_ai_once(*g, 1, g->tick, /*ai_sequence=*/1, box);
    CHECK(box.result_count >= 1);
    bool found_train = false;
    for (uint32_t i = 0; i < box.result_count; ++i) {
        if (box.result[i].type == CommandType::TRAIN_UNIT) {
            CHECK(box.result[i].p.unit_id == 0u /*citizen*/);
            found_train = true;
        }
    }
    CHECK(found_train);

    const uint32_t center_idx = find_owned_building(*g, 1, 0 /*center*/);
    CHECK(center_idx != g->entities.capacity);
    CHECK(g->prod_count[center_idx] == 0u);  // aún no aplicado

    RawCommand applied[AI_MAX_COMMANDS];
    for (uint32_t i = 0; i < box.result_count; ++i) applied[i] = box.result[i];
    const uint32_t target_tick = box.result[0].target_tick;
    advance_to(*g, target_tick);
    const StepResult r1 = step(*g, applied, box.result_count);
    CHECK(r1.accepted == box.result_count);  // el kernel acepta el TRAIN_UNIT emitido
    // citizen.build_time_ticks==1: production_system lo completa en el MISMO
    // tick en que se encola (prod_count vuelve a 0 de inmediato) — la
    // evidencia observable de "entrenó" es que aparece un ciudadano nuevo.
    uint32_t new_citizens = 0;
    for (uint32_t i = 0; i < g->entities.capacity; ++i) {
        if (g->entities.alive[i] && g->owner[i] == 1 && g->unit_class[i] == 3u) ++new_citizens;
    }
    CHECK(new_citizens >= 1u);
}

// ============================================================================
// C) La IA ataca: con el ejército por encima del umbral y sin amenaza cerca
//    de la base, MOVE_TO de cada unidad de combate propia hacia el edificio
//    enemigo (aceptado, tgt_x/tgt_y actualizado).
// ============================================================================
static void test_layer_attack() {
    static DataCatalogV1 cat = ai_fixture::make_catalog();
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg());
    gs_bind_catalog(*g, cat);
    gs_init_epoch_from_catalog(*g);

    // Centro IA (owner=1) en (40,40); 5 soldados propios cerca (umbral con
    // expansion_aggressiveness_bp=5000 del perfil default -> 10-5=5).
    RawCommand setup[7];
    setup[0] = place_building(0, 1, 1, 0 /*center*/, 40, 40);
    for (int i = 0; i < 5; ++i) {
        setup[1 + i] = spawn_soldier(0, 1, 2u + static_cast<uint64_t>(i),
                                     (44 + i) * 65536, 44 * 65536);
    }
    // Edificio enemigo (owner=0), lejos de la base IA -> sin amenaza local.
    setup[6] = place_building(0, 0, 1, 0 /*center*/, 200, 200);
    const StepResult r0 = step(*g, setup, 7);
    CHECK(r0.accepted == 7);

    const uint32_t enemy_idx = find_owned_building(*g, 0, 0);
    CHECK(enemy_idx != g->entities.capacity);
    const int64_t enemy_x = g->pos_x[enemy_idx];
    const int64_t enemy_y = g->pos_y[enemy_idx];

    AiJobBox box{};
    run_ai_once(*g, 1, g->tick, /*ai_sequence=*/6, box);
    // Sprint 1.19 fase B: la intencion es la MISMA —los 5 soldados reciben
    // orden de atacar al objetivo elegido— pero ya no es MOVE_TO a sus
    // coordenadas, sino ATTACK sobre ESA entidad (fuego focalizado). El
    // objetivo viaja en unit_id, como fija SPEC-004 §24.2.
    (void)enemy_x; (void)enemy_y;
    uint32_t moves = 0;
    for (uint32_t i = 0; i < box.result_count; ++i) {
        if (box.result[i].type == CommandType::ATTACK) {
            CHECK(box.result[i].p.unit_id == enemy_idx);
            ++moves;
        }
    }
    CHECK(moves == 5u);  // los 5 soldados propios reciben orden de ataque

    RawCommand applied[AI_MAX_COMMANDS];
    for (uint32_t i = 0; i < box.result_count; ++i) applied[i] = box.result[i];
    const uint32_t target_tick = box.result[0].target_tick;
    advance_to(*g, target_tick);
    const StepResult r1 = step(*g, applied, box.result_count);
    CHECK(r1.accepted == box.result_count);
    for (uint32_t i = 0; i < g->entities.capacity; ++i) {
        if (g->entities.alive[i] && g->owner[i] == 1 && g->unit_class[i] <= 2u) {
            CHECK(g->tgt_x[i] == enemy_x);
            CHECK(g->tgt_y[i] == enemy_y);
        }
    }
}

// ============================================================================
// D) La IA defiende (capa reactiva): un soldado enemigo cerca del ancla de
//    la base IA hace que el ejército reciba MOVE_TO al ancla en vez de
//    atacar (prioridad reactiva > táctica, SPEC-005 §4.3).
// ============================================================================
static void test_layer_defend_reactive_priority() {
    static DataCatalogV1 cat = ai_fixture::make_catalog();
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg());
    gs_bind_catalog(*g, cat);
    gs_init_epoch_from_catalog(*g);

    RawCommand setup[8];
    setup[0] = place_building(0, 1, 1, 0 /*center*/, 40, 40);
    for (int i = 0; i < 5; ++i) {
        setup[1 + i] = spawn_soldier(0, 1, 2u + static_cast<uint64_t>(i),
                                     (44 + i) * 65536, 44 * 65536);
    }
    setup[6] = place_building(0, 0, 1, 0 /*center*/, 200, 200);
    // Soldado enemigo A 5 tiles del ancla (40,40) -> dentro de
    // AI_REACTIVE_RADIUS_MT (20 tiles).
    setup[7] = spawn_soldier(0, 0, 2, 45 * 65536, 40 * 65536);
    const StepResult r0 = step(*g, setup, 8);
    CHECK(r0.accepted == 8);

    const uint32_t anchor_idx = find_owned_building(*g, 1, 0);
    CHECK(anchor_idx != g->entities.capacity);
    const int64_t anchor_x = g->pos_x[anchor_idx];
    const int64_t anchor_y = g->pos_y[anchor_idx];

    AiJobBox box{};
    run_ai_once(*g, 1, g->tick, /*ai_sequence=*/6, box);
    uint32_t moves = 0;
    // Defendiendo: ATTACK_MOVE a la base — vuelve PELEANDO con lo que se
    // encuentre en vez de atravesarlo. El destino sigue siendo el ancla.
    for (uint32_t i = 0; i < box.result_count; ++i) {
        if (box.result[i].type == CommandType::ATTACK_MOVE) {
            CHECK(box.result[i].p.x_raw == anchor_x);
            CHECK(box.result[i].p.y_raw == anchor_y);
            ++moves;
        }
    }
    CHECK(moves == 5u);
}

// ============================================================================
// E) Presupuesto de comandos SIEMPRE <= AI_MAX_COMMANDS, incluso con un
//    ejército mayor que el presupuesto.
// ============================================================================
static void test_budget_never_exceeds_cap() {
    static DataCatalogV1 cat = ai_fixture::make_catalog();
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg(/*max_entities=*/256));
    gs_bind_catalog(*g, cat);
    gs_init_epoch_from_catalog(*g);

    RawCommand setup[91];
    setup[0] = place_building(0, 1, 1, 0 /*center*/, 40, 40);
    for (int i = 0; i < 80; ++i) {
        setup[1 + i] = spawn_soldier(0, 1, 2u + static_cast<uint64_t>(i),
                                     (44 + (i % 60)) * 65536, (44 + i / 60) * 65536);
    }
    setup[81] = place_building(0, 0, 1, 0 /*center*/, 200, 200);
    for (int i = 0; i < 9; ++i) {
        setup[82 + i] = spawn_soldier(0, 0, 2u + static_cast<uint64_t>(i),
                                      (200 + i) * 65536, 200 * 65536);
    }
    const StepResult r0 = step(*g, setup, 91);
    CHECK(r0.accepted == 91);

    AiJobBox box{};
    run_ai_once(*g, 1, g->tick, /*ai_sequence=*/81, box);
    CHECK(box.result_count <= AI_MAX_COMMANDS);
    CHECK(box.result_count == AI_MAX_COMMANDS);  // 80 soldados > 64: se corta el ciclo
}

// ============================================================================
// F) Determinismo de ai_execute: mismo (g, source_tick, runtime) -> result[]
//    idéntico byte a byte en dos ejecuciones independientes.
// ============================================================================
static void test_ai_execute_deterministic() {
    static DataCatalogV1 cat = ai_fixture::make_catalog();
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg());
    gs_bind_catalog(*g, cat);
    gs_init_epoch_from_catalog(*g);

    RawCommand setup[7];
    setup[0] = place_building(0, 1, 1, 0 /*center*/, 40, 40);
    for (int i = 0; i < 5; ++i) {
        setup[1 + i] = spawn_soldier(0, 1, 2u + static_cast<uint64_t>(i),
                                     (44 + i) * 65536, 44 * 65536);
    }
    setup[6] = place_building(0, 0, 1, 0 /*center*/, 200, 200);
    const StepResult r0 = step(*g, setup, 7);
    CHECK(r0.accepted == 7);
    g->player_stock[1][0] = 100;
    g->player_stock[1][1] = 100;

    AiJobBox box_a{}, box_b{};
    run_ai_once(*g, 1, g->tick, /*ai_sequence=*/6, box_a);
    run_ai_once(*g, 1, g->tick, /*ai_sequence=*/6, box_b);

    CHECK(box_a.result_count == box_b.result_count);
    CHECK(box_a.result_count > 0);
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
    }
}

int main() {
    test_layer_build_and_assign();
    test_layer_train();
    test_layer_attack();
    test_layer_defend_reactive_priority();
    test_budget_never_exceeds_cap();
    test_ai_execute_deterministic();

    if (g_fails == 0) { std::printf("ai_layers: OK\n"); return 0; }
    std::printf("ai_layers: %d fallos\n", g_fails);
    return 1;
}
