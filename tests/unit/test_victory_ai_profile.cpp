// Test de condición de victoria/derrota + perfil de IA tipado (Sprint 1.4 K1,
// SPEC-005 §3/§6/§7). Autor: sonnet-5 (brief
// docs/briefs/SONNET_K1_VICTORIA_PERFIL_SPRINT_1.4.md).
//
// Cubre §8.4 (subconjunto K1): perfil de IA — carga real de `base:demo_normal`
// (pesos correctos) + rechazo de un perfil con rango inválido (fixture) +
// rechazo de un perfil sin tactical_behaviors (índice [0] inexistente) ·
// victoria — jugador sin edificios ni ciudadanos pierde, el otro gana ·
// empate simultáneo · partida en curso (edificio O ciudadano, cualquiera de
// los dos basta) · congelado tras game_over (no se reabre) · jugador nunca
// poblado (participants_mask) no fuerza ganador/empate espurio (mismo patrón
// 2-jugadores-pero-1-real que test_production_tech.cpp) · escenario de un
// solo jugador activo (>=2 gate) nunca alcanza game_over — la trayectoria
// previa (SPAWN_DEBUG, sin edificios/ciudadanos) no cambia · save v11
// round-trip de game_over/winner/participants_mask.
//
// NOTA: GameState SIEMPRE en heap (make_unique) — un GameState en pila
// segfaultea bajo ctest (lección K1, ver game_state.hpp).
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "chunsa/data_catalog.hpp"
#include "chunsa/game_state.hpp"
#include "chunsa/step.hpp"
#include "chunsa/checksum.hpp"
#include "chunsa/serialize.hpp"
#include "chunsa/save_io.hpp"
#include "chunsa/ai_stub.hpp"

#ifndef CHUNSA_GOLDEN_CHDB_PATH
#error "CHUNSA_GOLDEN_CHDB_PATH debe definirse via CMake (ver CMakeLists.txt)"
#endif

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

// ============================================================================
// PARTE 1 — perfil de IA tipado (SPEC-005 §3).
// ============================================================================

// Mini-constructor de CHDB byte a byte (mismo patrón que
// tests/unit/test_data_blob.cpp::mini_chdb — no se reutiliza directamente
// porque vive en un TU/namespace anónimo distinto; se replica minimal aquí
// solo lo necesario para fabricar un manifest + UNA sección ai-profile).
namespace mini_chdb_ai {

inline void w_u8(std::vector<uint8_t>& b, uint8_t v) { b.push_back(v); }
inline void w_u16(std::vector<uint8_t>& b, uint16_t v) {
    b.push_back(static_cast<uint8_t>(v & 0xFFu));
    b.push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
}
inline void w_u32(std::vector<uint8_t>& b, uint32_t v) {
    for (int i = 0; i < 4; ++i) b.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFFu));
}
inline void w_u64(std::vector<uint8_t>& b, uint64_t v) {
    for (int i = 0; i < 8; ++i) b.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFFu));
}
inline void w_bytes(std::vector<uint8_t>& b, const void* p, size_t n) {
    const uint8_t* s = static_cast<const uint8_t*>(p);
    for (size_t i = 0; i < n; ++i) b.push_back(s[i]);
}
inline void append(std::vector<uint8_t>& b, const std::vector<uint8_t>& tail) {
    b.insert(b.end(), tail.begin(), tail.end());
}
inline std::vector<uint8_t> cve_int(int64_t v) {
    std::vector<uint8_t> b;
    w_u8(b, 0x10u);
    w_u64(b, static_cast<uint64_t>(v));
    return b;
}
inline std::vector<uint8_t> cve_str(const std::string& s) {
    std::vector<uint8_t> b;
    w_u8(b, 0x20u);
    w_u32(b, static_cast<uint32_t>(s.size()));
    w_bytes(b, s.data(), s.size());
    return b;
}
inline std::vector<uint8_t> cve_str_arr(const std::vector<std::string>& items) {
    std::vector<uint8_t> b;
    w_u8(b, 0x30u);
    w_u32(b, static_cast<uint32_t>(items.size()));
    for (const auto& s : items) append(b, cve_str(s));
    return b;
}
inline std::vector<uint8_t> cve_arr(const std::vector<std::vector<uint8_t>>& items) {
    std::vector<uint8_t> b;
    w_u8(b, 0x30u);
    w_u32(b, static_cast<uint32_t>(items.size()));
    for (const auto& it : items) append(b, it);
    return b;
}
// `kvs` DEBE venir ya en orden ascendente estricto por clave.
inline std::vector<uint8_t> cve_obj(const std::vector<std::pair<std::string, std::vector<uint8_t>>>& kvs) {
    std::vector<uint8_t> b;
    w_u8(b, 0x40u);
    w_u32(b, static_cast<uint32_t>(kvs.size()));
    for (const auto& kv : kvs) {
        w_u32(b, static_cast<uint32_t>(kv.first.size()));
        w_bytes(b, kv.first.data(), kv.first.size());
        append(b, kv.second);
    }
    return b;
}

// Construye un ai-profile CVE (claves ASCENDENTE estricto). `economy_bp` y
// `n_tactical` son los dos ejes que las pruebas de rechazo perturban.
inline std::vector<uint8_t> ai_profile_obj(const std::string& id, int64_t economy_bp,
                                           int n_tactical) {
    auto sw = cve_obj({
        {"diplomacy_openness_bp", cve_int(5000)},
        {"economy_focus_bp", cve_int(economy_bp)},
        {"expansion_aggressiveness_bp", cve_int(5000)},
        {"military_focus_bp", cve_int(5000)},
        {"risk_tolerance_bp", cve_int(5000)},
        {"tech_focus_bp", cve_int(5000)},
    });
    auto dp = cve_obj({
        {"build_order_variance_bp", cve_int(1500)},
        {"counter_reaction_delay_ticks", cve_int(8)},
        {"decision_period_ticks", cve_int(20)},
        {"micro_quality_bp", cve_int(5000)},
        {"reaction_latency_ticks", cve_int(4)},
        {"scouting_thoroughness_bp", cve_int(5000)},
    });
    std::vector<std::vector<uint8_t>> behaviors;
    for (int k = 0; k < n_tactical; ++k) {
        behaviors.push_back(cve_obj({
            {"behavior_id", cve_str("base:melee_line_v1")},
            {"formation_preference", cve_str("line")},
            {"group_type", cve_str("melee_line")},
            {"retreat_hp_threshold_bp", cve_int(2500)},
            {"retreat_morale_threshold_bp", cve_int(3000)},
            {"seek_cover", {0x02u}},  // true
            {"suppression_response", cve_str("hold")},
        }));
    }
    auto pp = cve_obj({
        {"cache_ttl_ticks", cve_int(40)},
        {"min_decision_period_ticks", cve_int(20)},
    });
    auto prov = cve_obj({
        {"status", cve_str("promoted")},
    });
    // Claves raíz ASCENDENTE estricto: difficulty < difficulty_params <
    // display_name_key < id < performance_lod < personality <
    // provenance < schema_version < strategic_weights_bp < tactical_behaviors.
    return cve_obj({
        {"difficulty", cve_str("normal")},
        {"difficulty_params", dp},
        {"display_name_key", cve_str("base:ai.test")},
        {"id", cve_str(id)},
        {"performance_lod", pp},
        {"personality", cve_str("balanced")},
        {"provenance", prov},
        {"schema_version", cve_int(1)},
        {"strategic_weights_bp", sw},
        {"tactical_behaviors", cve_arr(behaviors)},
    });
}

// Blob mínimo: manifest(1) + ai-profile(1), resto de secciones vacías (el
// loader no exige mínimo salvo manifest==1 — mismo patrón que
// test_data_blob.cpp::mini_chdb::build, adaptado a la sección kind=7).
inline std::vector<uint8_t> build(const std::vector<uint8_t>& ai_profile_obj_bytes) {
    auto manifest_obj = cve_obj({
        {"declared_capabilities", cve_str_arr({})},
        {"package_id", cve_str(std::string("test.fixture.ai"))},
    });
    std::vector<uint8_t> manifest_record;
    w_u32(manifest_record, static_cast<uint32_t>(manifest_obj.size()));
    append(manifest_record, manifest_obj);

    std::vector<uint8_t> ai_record;
    w_u32(ai_record, static_cast<uint32_t>(ai_profile_obj_bytes.size()));
    append(ai_record, ai_profile_obj_bytes);

    // Orden kKindTable: manifest, unit, building, tech, civ, map, ai-profile.
    std::vector<uint8_t> sections[7];
    sections[0] = manifest_record;
    sections[6] = ai_record;

    struct KindRow { uint16_t kind, version; uint32_t count; };
    static constexpr KindRow ROWS[7] = {
        {1, 1, 1}, {2, 2, 0}, {3, 1, 0}, {4, 1, 0}, {5, 1, 0}, {6, 1, 0}, {7, 1, 1},
    };

    const uint64_t directory_end = 40 + 7u * 24u;
    uint64_t cursor = directory_end;
    uint64_t offsets[7];
    for (int k = 0; k < 7; ++k) { offsets[k] = cursor; cursor += sections[k].size(); }
    const uint64_t file_size = cursor;

    std::vector<uint8_t> blob;
    static constexpr char kMagic[8] = {'C', 'H', 'N', 'S', 'D', 'B', '1', '\0'};
    w_bytes(blob, kMagic, 8);
    w_u16(blob, 1u); w_u16(blob, 0u);
    w_u32(blob, 1u);
    w_u32(blob, 0u);
    w_u32(blob, 7u);
    w_u32(blob, 24u);
    w_u32(blob, 0u);
    w_u64(blob, file_size);
    for (int k = 0; k < 7; ++k) {
        w_u16(blob, ROWS[k].kind);
        w_u16(blob, ROWS[k].version);
        w_u32(blob, ROWS[k].count);
        w_u64(blob, offsets[k]);
        w_u64(blob, sections[k].size());
    }
    for (int k = 0; k < 7; ++k) append(blob, sections[k]);
    return blob;
}

}  // namespace mini_chdb_ai

static void test_ai_profile_real_golden() {
    DataCatalogStorageV1 store;
    const auto code = catalog_load_file_v1(CHUNSA_GOLDEN_CHDB_PATH, CatalogLoadProfile::Verified, store);
    CHECK(code == CatalogLoadCode::Ok);
    if (!store.valid()) { std::printf("ai_profile_real_golden: catálogo inválido, abortando subtest\n"); return; }
    const DataCatalogV1& cat = store.catalog();
    CHECK(cat.ai_profile_count >= 1);

    const AiProfileId id = catalog_find_ai_profile(cat, "base:demo_normal", std::strlen("base:demo_normal"));
    CHECK(id != INVALID_AI_PROFILE_ID);
    if (id == INVALID_AI_PROFILE_ID) return;
    const AiProfileV1& p = cat.ai_profiles[id];
    // Valores literales de data/ai_profiles/base_demo_normal.yaml.
    CHECK(p.economy_focus_bp == 5000);
    CHECK(p.military_focus_bp == 5000);
    CHECK(p.tech_focus_bp == 5000);
    CHECK(p.expansion_aggressiveness_bp == 5000);
    CHECK(p.risk_tolerance_bp == 5000);
    CHECK(p.decision_period_ticks == 20u);
    CHECK(p.reaction_latency_ticks == 4u);
    CHECK(p.retreat_hp_threshold_bp == 2500);
    CHECK(p.retreat_morale_threshold_bp == 3000);

    CHECK(catalog_find_ai_profile(cat, "nope:nope", 9) == INVALID_AI_PROFILE_ID);
}

static void test_ai_profile_rejects_invalid_range() {
    // economy_focus_bp = 99999 (> 10000, fuera de rango del schema) ->
    // catálogo entero rechazado con InvalidAiProfile.
    const auto obj = mini_chdb_ai::ai_profile_obj("base:bad_range", 99999, 1);
    const auto blob = mini_chdb_ai::build(obj);
    DataCatalogStorageV1 store;
    const auto code = catalog_load_bytes_v1(blob.data(), blob.size(), CatalogLoadProfile::Verified, store);
    CHECK(code == CatalogLoadCode::InvalidAiProfile);
    CHECK(!store.valid());
}

static void test_ai_profile_rejects_empty_tactical_behaviors() {
    // tactical_behaviors=[] -> v1 usa el índice [0], que no existe ->
    // referencia no resoluble, catálogo entero rechazado.
    const auto obj = mini_chdb_ai::ai_profile_obj("base:no_tactical", 5000, 0);
    const auto blob = mini_chdb_ai::build(obj);
    DataCatalogStorageV1 store;
    const auto code = catalog_load_bytes_v1(blob.data(), blob.size(), CatalogLoadProfile::Verified, store);
    CHECK(code == CatalogLoadCode::InvalidAiProfile);
    CHECK(!store.valid());
}

static void test_ai_profile_accepts_valid_fixture() {
    // Control positivo: el mismo constructor con valores válidos SÍ carga.
    const auto obj = mini_chdb_ai::ai_profile_obj("base:ok", 5000, 1);
    const auto blob = mini_chdb_ai::build(obj);
    DataCatalogStorageV1 store;
    const auto code = catalog_load_bytes_v1(blob.data(), blob.size(), CatalogLoadProfile::Verified, store);
    CHECK(code == CatalogLoadCode::Ok);
    if (!store.valid()) return;
    const DataCatalogV1& cat = store.catalog();
    CHECK(cat.ai_profile_count == 1u);
    const AiProfileId id = catalog_find_ai_profile(cat, "base:ok", 7);
    CHECK(id != INVALID_AI_PROFILE_ID);
    if (id != INVALID_AI_PROFILE_ID) {
        CHECK(cat.ai_profiles[id].economy_focus_bp == 5000);
        CHECK(cat.ai_profiles[id].retreat_hp_threshold_bp == 2500);
    }
}

// ============================================================================
// PARTE 2 — condición de victoria/derrota (SPEC-005 §6).
// ============================================================================

// Fixture mínimo: units[0] = worker (Citizen); buildings[0] = "post" (nace
// completo, sin trains/researches — no se entrena/investiga nada en estos
// tests, solo se ejercita presencia/ausencia de edificio y ciudadano).
namespace victory_fixture {

inline UnitDefinitionV1 make_worker() {
    UnitDefinitionV1 d{};
    d.id = 0; d.unit_class = UnitClassV1::Citizen; d.tags_mask = 0;
    d.hp = 20; d.attack = 0; d.range_millitiles = 0;
    d.speed_millitile_tick = 800; d.morale = 100; d.build_time_ticks = 1;
    for (int k = 0; k < 6; ++k) d.bonus_vs_bp[k] = 0;
    d.cost_a = 0; d.cost_b = 0; d.cost_me = 0; d.pop_cost = 1;
    d.epoch_min = 1; d.epoch_max = 15;
    return d;
}
inline BuildingDefinitionV1 make_post() {
    BuildingDefinitionV1 d{};
    d.id = 0; d.hp = 100; d.footprint_w = 2; d.footprint_h = 2;
    d.build_time_ticks = 0;  // nace completo (mismo patrón "center"/"barracks")
    d.cost_a = 0; d.cost_b = 0; d.cost_me = 0;
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

static UnitDefinitionV1 g_units[1] = { make_worker() };
static BuildingDefinitionV1 g_buildings[1] = { make_post() };

inline DataCatalogV1 make_catalog() {
    DataCatalogV1 c{};
    c.unit_count = 1; c.units = g_units; c.unit_names = nullptr;
    c.building_count = 1; c.buildings = g_buildings; c.building_names = nullptr;
    c.tech_count = 0; c.techs = nullptr; c.tech_names = nullptr;
    c.capability_count = 0; c.capability_names = nullptr;
    c.ai_profile_count = 0; c.ai_profiles = nullptr; c.ai_profile_names = nullptr;
    return c;
}

}  // namespace victory_fixture

static MatchConfig01A make_cfg(uint8_t players = 2) {
    MatchConfig01A cfg{};
    cfg.max_entities = 64;
    cfg.player_count = players;
    cfg.human_input_delay_ticks = 0;
    cfg.max_future_command_ticks = 20;
    cfg.checksum_every_ticks = 1;
    cfg.map_tiles_x = 256; cfg.map_tiles_y = 256;
    cfg.seed = 20260724ull;
    cfg.allow_debug_stat_payload = 0;
    return cfg;
}

static RawCommand place_building(uint32_t tick, uint16_t emitter, uint64_t seq,
                                 int64_t tx, int64_t ty) {
    RawCommand c{};
    std::memset(&c, 0, sizeof(c));
    c.target_tick = tick; c.emitter = emitter; c.type = CommandType::PLACE_BUILDING;
    c.sequence = seq; c.p.unit_id = 0 /*post*/; c.p.x_raw = tx; c.p.y_raw = ty;
    return c;
}
static RawCommand spawn_citizen(uint32_t tick, uint16_t emitter, uint64_t seq,
                                int64_t x_raw, int64_t y_raw) {
    RawCommand c{};
    std::memset(&c, 0, sizeof(c));
    c.target_tick = tick; c.emitter = emitter; c.type = CommandType::SPAWN_CITIZEN;
    c.sequence = seq; c.p.unit_id = 0 /*worker*/; c.p.x_raw = x_raw; c.p.y_raw = y_raw;
    return c;
}
static RawCommand destroy_debug(uint32_t tick, uint16_t emitter, uint64_t seq, EntityHandle h) {
    RawCommand c{};
    std::memset(&c, 0, sizeof(c));
    c.target_tick = tick; c.emitter = emitter; c.type = CommandType::DESTROY_DEBUG;
    c.sequence = seq; c.p.handle = h;
    return c;
}

// 2a) Jugador owner=1 se queda sin edificios ni ciudadanos -> game_over==1,
//     winner==0 (owner 0 conserva su edificio).
static void test_victory_single_winner() {
    static DataCatalogV1 cat = victory_fixture::make_catalog();
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg());
    gs_bind_catalog(*g, cat);
    gs_init_epoch_from_catalog(*g);

    // t=0: PLACE_BUILDING de owner0 (10,10) y owner1 (50,50), ambos scenario-exempt.
    RawCommand setup[2] = {
        place_building(0, 0, 1, 10, 10),
        place_building(0, 1, 1, 50, 50),
    };
    const StepResult r0 = step(*g, setup, 2);
    CHECK(r0.accepted == 2);
    CHECK(g->entities.alive[0] == 1 && g->entity_kind[0] == 1 && g->owner[0] == 0);
    CHECK(g->entities.alive[1] == 1 && g->entity_kind[1] == 1 && g->owner[1] == 1);
    CHECK(g->game_over == 0u);  // ambos activos, ninguno derrotado
    CHECK((g->participants_mask & 0x3u) == 0x3u);  // bits 0 y 1 ya marcados activos

    // Destruye el edificio de owner1 (único, sin ciudadano) -> derrotado.
    RawCommand kill = destroy_debug(g->tick, 1, 2, EntityHandle{1, g->entities.generation[1]});
    const StepResult r1 = step(*g, &kill, 1);
    CHECK(r1.accepted == 1);
    CHECK(g->entities.alive[1] == 0);
    CHECK(g->game_over == 1u);
    CHECK(g->winner == 0u);
}

// 2b) Empate simultáneo: ambos edificios destruidos en el MISMO batch/step.
static void test_victory_simultaneous_tie() {
    static DataCatalogV1 cat = victory_fixture::make_catalog();
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg());
    gs_bind_catalog(*g, cat);
    gs_init_epoch_from_catalog(*g);

    RawCommand setup[2] = {
        place_building(0, 0, 1, 10, 10),
        place_building(0, 1, 1, 50, 50),
    };
    step(*g, setup, 2);
    CHECK(g->game_over == 0u);

    RawCommand kill_both[2] = {
        destroy_debug(g->tick, 0, 2, EntityHandle{0, g->entities.generation[0]}),
        destroy_debug(g->tick, 1, 2, EntityHandle{1, g->entities.generation[1]}),
    };
    const StepResult r = step(*g, kill_both, 2);
    CHECK(r.accepted == 2);
    CHECK(g->entities.alive_count == 0u);
    CHECK(g->game_over == 1u);
    CHECK(g->winner == 0xFFu);
}

// 2c) Partida en curso: game_over==0 mientras ambos tengan edificio O
//     ciudadano (cualquiera de los dos basta — se ejercita con owner1 solo
//     con ciudadano, sin edificio, para probar que el ciudadano también
//     cuenta como "no derrotado").
static void test_victory_in_progress_citizen_alone() {
    static DataCatalogV1 cat = victory_fixture::make_catalog();
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg());
    gs_bind_catalog(*g, cat);
    gs_init_epoch_from_catalog(*g);

    RawCommand setup[2] = {
        place_building(0, 0, 1, 10, 10),
        spawn_citizen(0, 1, 1, 50 * 65536, 50 * 65536),
    };
    const StepResult r0 = step(*g, setup, 2);
    CHECK(r0.accepted == 2);
    CHECK(g->entity_kind[0] == 1u);           // owner0: edificio
    CHECK(g->unit_class[1] == 3u);            // owner1: ciudadano (sin edificio)
    CHECK(g->game_over == 0u);

    for (int k = 0; k < 20; ++k) {
        step(*g, nullptr, 0);
        CHECK(g->game_over == 0u);            // sigue en curso: owner1 solo tiene ciudadano, basta
    }
}

// 2d) Congelado: tras game_over==1, un cambio de estado posterior NO reabre
//     la partida (ni cambia el winner a empate).
static void test_victory_frozen_after_game_over() {
    static DataCatalogV1 cat = victory_fixture::make_catalog();
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg());
    gs_bind_catalog(*g, cat);
    gs_init_epoch_from_catalog(*g);

    RawCommand setup[2] = {
        place_building(0, 0, 1, 10, 10),
        place_building(0, 1, 1, 50, 50),
    };
    step(*g, setup, 2);
    RawCommand kill = destroy_debug(g->tick, 1, 2, EntityHandle{1, g->entities.generation[1]});
    step(*g, &kill, 1);
    CHECK(g->game_over == 1u && g->winner == 0u);

    // Destruye TAMBIÉN el edificio de owner0 (el "ganador"): si la condición
    // se reevaluara, esto produciría un empate (0xFF). Debe quedar CONGELADO.
    RawCommand kill_winner = destroy_debug(g->tick, 0, 3, EntityHandle{0, g->entities.generation[0]});
    const StepResult r = step(*g, &kill_winner, 1);
    CHECK(r.accepted == 1);
    CHECK(g->entities.alive_count == 0u);
    CHECK(g->game_over == 1u);   // sigue en 1 (obvio, ya lo estaba)
    CHECK(g->winner == 0u);      // NO cambia a 0xFF: congelado, no se reevalúa
}

// 2e) Jugador nunca poblado (player_count==2 pero owner1 nunca recibe
//     ninguna entidad, mismo patrón que test_production_tech.cpp): NO se
//     declara ganador espurio a owner0 ni empate — la partida sigue "en
//     curso" indefinidamente (active_count < 2, evaluación nunca se
//     dispara). Es la salvaguarda literal de SPEC-005 §6: "no marques
//     ganador a un emisor que nunca jugó".
static void test_victory_never_populated_player_no_spurious_result() {
    static DataCatalogV1 cat = victory_fixture::make_catalog();
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg(/*players=*/2));
    gs_bind_catalog(*g, cat);
    gs_init_epoch_from_catalog(*g);

    RawCommand setup = place_building(0, 0, 1, 10, 10);  // SOLO owner0
    step(*g, &setup, 1);
    CHECK(g->game_over == 0u);
    CHECK((g->participants_mask & 0x1u) == 0x1u);   // owner0: activo
    CHECK((g->participants_mask & 0x2u) == 0u);     // owner1: NUNCA activo

    for (int k = 0; k < 50; ++k) {
        step(*g, nullptr, 0);
        CHECK(g->game_over == 0u);  // NUNCA termina: <2 jugadores activos
    }
}

// 2f) Escenario de un único jugador CONFIGURADO (player_count==1, sin
//     edificios/ciudadanos — mismo patrón que cli_run.hpp::run_synthetic,
//     SPAWN_DEBUG): jamás alcanza game_over (>=2 gate). Es la prueba
//     concreta de que la trayectoria previa (escenarios sintéticos
//     existentes) NO cambia: game_over permanece en 0 durante TODA la
//     corrida, exactamente como antes de este sprint (game_over no existía).
static void test_victory_single_configured_player_never_ends() {
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg(/*players=*/1));
    RawCommand debug_cmd{};
    std::memset(&debug_cmd, 0, sizeof(debug_cmd));
    debug_cmd.target_tick = 0; debug_cmd.emitter = 0;
    debug_cmd.type = CommandType::SPAWN_DEBUG; debug_cmd.sequence = 1;
    debug_cmd.p.handle = EntityHandle{0, 1};
    debug_cmd.p.x_raw = 10 * 65536; debug_cmd.p.y_raw = 10 * 65536;
    debug_cmd.p.speed_mtpt = 100;
    step(*g, &debug_cmd, 1);
    CHECK(g->entities.alive[0] == 1);
    CHECK(g->game_over == 0u);
    for (int k = 0; k < 100; ++k) {
        step(*g, nullptr, 0);
        CHECK(g->game_over == 0u);
    }
}

// 2g) Save v11 round-trip: game_over/winner/participants_mask sobreviven.
static void test_victory_save_v11_roundtrip() {
    static DataCatalogV1 cat = victory_fixture::make_catalog();
    auto g1 = std::make_unique<GameState>();
    gs_init(*g1, make_cfg());
    gs_bind_catalog(*g1, cat);
    gs_init_epoch_from_catalog(*g1);

    RawCommand setup[2] = {
        place_building(0, 0, 1, 10, 10),
        place_building(0, 1, 1, 50, 50),
    };
    step(*g1, setup, 2);
    RawCommand kill = destroy_debug(g1->tick, 1, 2, EntityHandle{1, g1->entities.generation[1]});
    step(*g1, &kill, 1);
    CHECK(g1->game_over == 1u && g1->winner == 0u);
    CHECK((g1->participants_mask & 0x3u) == 0x3u);

    AiJobBox box{}; ai_box_init(box, 1);
    AiRuntimeV1 rt{};
    CHECK(save_game(*g1, box, rt, "test_victory_v11.sav") == 0);

    auto g2 = std::make_unique<GameState>();
    AiJobBox box2{}; AiRuntimeV1 rt2{};
    CHECK(load_game(*g2, box2, rt2, "test_victory_v11.sav") == 0);
    CHECK(g2->game_over == 1u);
    CHECK(g2->winner == 0u);
    CHECK(g2->participants_mask == g1->participants_mask);
    CHECK(state_checksum_v1(*g1) == state_checksum_v1(*g2));

    // Congelado sobrevive el load: un step posterior no reabre la partida.
    step(*g2, nullptr, 0);
    CHECK(g2->game_over == 1u && g2->winner == 0u);

    std::remove("test_victory_v11.sav");
}

int main() {
    test_ai_profile_real_golden();
    test_ai_profile_rejects_invalid_range();
    test_ai_profile_rejects_empty_tactical_behaviors();
    test_ai_profile_accepts_valid_fixture();

    test_victory_single_winner();
    test_victory_simultaneous_tie();
    test_victory_in_progress_citizen_alone();
    test_victory_frozen_after_game_over();
    test_victory_never_populated_player_no_spurious_result();
    test_victory_single_configured_player_never_ends();
    test_victory_save_v11_roundtrip();

    if (g_fails == 0) { std::printf("victory_ai_profile: OK\n"); return 0; }
    std::printf("victory_ai_profile: %d fallos\n", g_fails);
    return 1;
}
