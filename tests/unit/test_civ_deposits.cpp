// Test de identidad de civilización + depósitos desde el mapa (Sprint 1.6B,
// pieza K1, SPEC-004 Parte III §15.3/§16/§17/§20). Autor: sonnet-5 (brief
// docs/briefs/SONNET_K1_CIV_DEPOSITOS_SPRINT_1.6B.md).
//
// Cubre: civ_id tipado (las 2 civs reales del golden resuelven + fixture
// propio civ_id inválido rechaza el catálogo) · resource_spawns del mapa
// (fixture propio: posiciones/cantidades exactas, conversión mt→raw, +
// rechazo si excede ECO_MAX_DEPOSITS, + rechazo de kind/id fuera de A/B/Me)
// · fallback legacy bit-idéntico (sin catálogo/sin spawns) · época inicial
// por jugador distinta entre dos civs (gs_init_epoch_from_catalog_per_player,
// con control de que la variante catálogo-ancha original sigue intacta) ·
// gate de civilización en PLACE_BUILDING/TRAIN_UNIT/RESEARCH_TECH (rechaza
// cruzado, NO rechaza con INVALID_CIV_ID) · save v12 round-trip con
// player_civ.
//
// NOTA: GameState SIEMPRE en heap (make_unique) — un GameState en pila
// segfaultea bajo ctest (lección K1 de Sprint 1.2, ver game_state.hpp).
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
#error "CHUNSA_GOLDEN_CHDB_PATH debe definirse via CMake (ver CMakeLists.txt: chunsa_test_civ_deposits)"
#endif

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

static RejectReason last_result(const ReceiptMailbox& m) {
    return m.ring[(m.head + m.count - 1) % MAILBOX_CAP].result;
}

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

// ============================================================================
// civ_chdb: constructor de CHDB v1 hecho a mano (mismo patrón/estilo que
// mini_chdb de test_data_blob.cpp y mini_chdb_ai de test_victory_ai_profile.cpp
// — cada test file mantiene su PROPIO builder mínimo, sin compartir header;
// ver el comentario de esos archivos). NO depende de la rama de datos
// paralela de MiniMax (mm/datos-apertura-1.6b): tipifica el FORMATO del
// schema (map.schema.json/civ.schema.json) con datos sintéticos propios.
// ============================================================================
namespace civ_chdb {

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
inline std::vector<uint8_t> cve_arr_int(const std::vector<int64_t>& items) {
    std::vector<uint8_t> b;
    w_u8(b, 0x30u);
    w_u32(b, static_cast<uint32_t>(items.size()));
    for (int64_t v : items) append(b, cve_int(v));
    return b;
}
inline std::vector<uint8_t> cve_arr_raw(const std::vector<std::vector<uint8_t>>& items) {
    std::vector<uint8_t> b;
    w_u8(b, 0x30u);
    w_u32(b, static_cast<uint32_t>(items.size()));
    for (const auto& it : items) append(b, it);
    return b;
}
// `kvs` DEBE venir ya en orden ascendente estricto por clave (responsabilidad
// del caller, igual que exige cve_parse al leerlo).
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

// Objeto unit MÍNIMO (SPEC-004 §15.3): solo las claves que build_unit_definition
// exige (civ_id/class/epoch_window/id/stats{attack,build_time_ticks,hp,morale,
// range_millitiles,speed_millitile_tick}/tags) — is_known_unit_key acepta
// (no exige) el resto (display_name_key/description_key/.../provenance).
inline std::vector<uint8_t> unit_obj(const std::string& id, const std::string& civ_id,
                                     const std::string& cls, int64_t epoch_min, int64_t epoch_max,
                                     int64_t hp, int64_t attack, int64_t range_mt,
                                     int64_t speed, int64_t morale, int64_t build_time) {
    // Sprint 1.18: armadura y tipo de arma son OBLIGATORIOS en stats. Las
    // claves van en orden canonico (ascendente), como el resto del CVE.
    auto armor = cve_obj({
        {"cut", cve_int(0)},
        {"impact", cve_int(0)},
        {"pierce", cve_int(0)},
    });
    auto stats = cve_obj({
        {"armor", armor},
        {"attack", cve_int(attack)},
        {"attack_damage_type", cve_str("cut")},
        {"build_time_ticks", cve_int(build_time)},
        {"hp", cve_int(hp)},
        {"morale", cve_int(morale)},
        {"range_millitiles", cve_int(range_mt)},
        {"speed_millitile_tick", cve_int(speed)},
    });
    auto epoch_window = cve_arr_int({epoch_min, epoch_max});
    return cve_obj({
        {"civ_id", cve_str(civ_id)},
        {"class", cve_str(cls)},
        {"epoch_window", epoch_window},
        {"id", cve_str(id)},
        {"stats", stats},
        {"tags", cve_str_arr({})},
    });
}

// Objeto civ MÍNIMO (Sprint 1.6B): el loader solo reconstruye la tabla
// nombre-índice (CivNameIndexV1) — ningún otro campo del schema civ real se
// tipa (ver comentario de CivNameIndexV1 en data_catalog.hpp), así que un
// record con SOLO "id" basta para ejercitar la resolución.
inline std::vector<uint8_t> civ_obj_min(const std::string& id) {
    return cve_obj({ {"id", cve_str(id)} });
}

// Sprint 1.6B (SPEC-004 §16): un ítem de `resource_spawns` (map.schema.json).
struct SpawnSpec {
    std::string kind = "resource";
    std::string id;
    int64_t x_mt = 0, y_mt = 0;
    int64_t amount = 1;
};
inline std::vector<uint8_t> spawn_item_obj(const SpawnSpec& s) {
    return cve_obj({
        {"amount", cve_int(s.amount)},
        {"id", cve_str(s.id)},
        {"kind", cve_str(s.kind)},
        {"x_millitiles", cve_int(s.x_mt)},
        {"y_millitiles", cve_int(s.y_mt)},
    });
}
// Objeto map MÍNIMO (Sprint 1.6B): el loader solo tipa `resource_spawns`
// del primer record map ("mapa activo") — el resto de campos del schema
// (terrain_rle/cost_rle/starting_positions/...) sigue sin exigirse (sin
// gate is_known_key, misma disciplina que building/tech).
inline std::vector<uint8_t> map_obj(const std::string& id, const std::vector<SpawnSpec>& spawns) {
    std::vector<std::vector<uint8_t>> items;
    items.reserve(spawns.size());
    for (const auto& s : spawns) items.push_back(spawn_item_obj(s));
    return cve_obj({
        {"id", cve_str(id)},
        {"resource_spawns", cve_arr_raw(items)},
    });
}

// Ensambla un blob CHDB v1 completo. `unit_objs`/`civ_objs` ya en orden
// ascendente por record_id (responsabilidad del caller, mismo contrato que
// el resto de este builder); `map_obj_bytes`/`has_map` = 0 o 1 record map.
inline std::vector<uint8_t> build(const std::vector<std::vector<uint8_t>>& unit_objs,
                                  const std::vector<std::vector<uint8_t>>& civ_objs,
                                  const std::vector<uint8_t>& map_obj_bytes, bool has_map) {
    auto frame = [](const std::vector<uint8_t>& obj) {
        std::vector<uint8_t> rec;
        w_u32(rec, static_cast<uint32_t>(obj.size()));
        append(rec, obj);
        return rec;
    };
    auto manifest_obj = cve_obj({
        {"declared_capabilities", cve_str_arr({})},
        {"package_id", cve_str(std::string("civ.fixture"))},
    });
    std::vector<uint8_t> manifest_record = frame(manifest_obj);

    std::vector<uint8_t> unit_section;
    for (const auto& u : unit_objs) append(unit_section, frame(u));
    std::vector<uint8_t> civ_section;
    for (const auto& c : civ_objs) append(civ_section, frame(c));
    std::vector<uint8_t> map_section;
    if (has_map) append(map_section, frame(map_obj_bytes));

    // Orden kKindTable (data_catalog.hpp): manifest, unit, building, tech,
    // civ, map, ai-profile.
    std::vector<uint8_t> sections[7];
    sections[0] = manifest_record;
    sections[1] = unit_section;
    sections[4] = civ_section;
    sections[5] = map_section;

    struct KindRow { uint16_t kind, version; uint32_t count; };
    const KindRow rows[7] = {
        {1, 1, 1},
        {2, 2, static_cast<uint32_t>(unit_objs.size())},
        {3, 1, 0},
        {4, 1, 0},
        {5, 1, static_cast<uint32_t>(civ_objs.size())},
        {6, 1, has_map ? 1u : 0u},
        {7, 1, 0},
    };

    const uint64_t directory_end = 40 + 7u * 24u;
    uint64_t cursor = directory_end;
    uint64_t offsets[7];
    for (int k = 0; k < 7; ++k) { offsets[k] = cursor; cursor += sections[k].size(); }
    const uint64_t file_size = cursor;

    std::vector<uint8_t> blob;
    static constexpr char kMagic[8] = {'C', 'H', 'N', 'S', 'D', 'B', '1', '\0'};
    w_bytes(blob, kMagic, 8);
    w_u16(blob, 1u); w_u16(blob, 0u);   // fmt_major/minor
    w_u32(blob, 1u);                     // schema_set_version
    w_u32(blob, 0u);                     // flags
    w_u32(blob, 7u);                     // section_count
    w_u32(blob, 24u);                    // entry_size
    w_u32(blob, 0u);                     // reserved
    w_u64(blob, file_size);
    for (int k = 0; k < 7; ++k) {
        w_u16(blob, rows[k].kind);
        w_u16(blob, rows[k].version);
        w_u32(blob, rows[k].count);
        w_u64(blob, offsets[k]);
        w_u64(blob, sections[k].size());
    }
    for (int k = 0; k < 7; ++k) append(blob, sections[k]);
    return blob;
}

}  // namespace civ_chdb

// ============================================================================
// 1) civ_id tipado: las 2 civs REALES del golden resuelven (SPEC-004 §15.3).
// ============================================================================
static void test_civ_id_real_golden() {
    DataCatalogStorageV1 store;
    const auto code = catalog_load_file_v1(CHUNSA_GOLDEN_CHDB_PATH, CatalogLoadProfile::Verified, store);
    CHECK(code == CatalogLoadCode::Ok);
    if (!store.valid()) { std::printf("civ_id_real_golden: catálogo inválido, abortando subtest\n"); return; }
    const DataCatalogV1& cat = store.catalog();
    CHECK(cat.civ_count == 2);

    const CivId egipto = catalog_find_civ(cat, "egipto:dynastic_nile", std::strlen("egipto:dynastic_nile"));
    const CivId rome = catalog_find_civ(cat, "rome:republic_imperial", std::strlen("rome:republic_imperial"));
    CHECK(egipto != INVALID_CIV_ID);
    CHECK(rome != INVALID_CIV_ID);
    CHECK(egipto != rome);
    CHECK(catalog_find_civ(cat, "nope:nope", std::strlen("nope:nope")) == INVALID_CIV_ID);

    const UnitId legionary = catalog_find_unit(cat, "rome:legionary", std::strlen("rome:legionary"));
    const UnitId chariot = catalog_find_unit(cat, "egipto:chariot_warrior", std::strlen("egipto:chariot_warrior"));
    CHECK(legionary != INVALID_UNIT_ID);
    CHECK(chariot != INVALID_UNIT_ID);
    if (legionary != INVALID_UNIT_ID) CHECK(cat.units[legionary].civ_id == rome);
    if (chariot != INVALID_UNIT_ID) CHECK(cat.units[chariot].civ_id == egipto);

    const BuildingId forum = catalog_find_building(cat, "rome:forum_center", std::strlen("rome:forum_center"));
    const BuildingId settlement = catalog_find_building(cat, "egipto:settlement_center", std::strlen("egipto:settlement_center"));
    CHECK(forum != INVALID_BUILDING_ID);
    CHECK(settlement != INVALID_BUILDING_ID);
    if (forum != INVALID_BUILDING_ID) CHECK(cat.buildings[forum].civ_id == rome);
    if (settlement != INVALID_BUILDING_ID) CHECK(cat.buildings[settlement].civ_id == egipto);

    // Techs: civ_id derivado de `available_to` (desviación D1, tamaño==1 en
    // las 4 techs reales del repo).
    const TechId road = catalog_find_tech(cat, "rome:road_engineering", std::strlen("rome:road_engineering"));
    const TechId corvee = catalog_find_tech(cat, "egipto:corvee_logistics", std::strlen("egipto:corvee_logistics"));
    CHECK(road != INVALID_TECH_ID);
    CHECK(corvee != INVALID_TECH_ID);
    if (road != INVALID_TECH_ID) CHECK(cat.techs[road].civ_id == rome);
    if (corvee != INVALID_TECH_ID) CHECK(cat.techs[corvee].civ_id == egipto);
}

// ============================================================================
// 2) civ_id tipado: fixture propio resuelve + referencia civ inválida
//    rechaza el catálogo entero (SPEC-004 §15.3).
// ============================================================================
static void test_civ_id_fixture() {
    // 2a) Resuelve.
    {
        auto unit = civ_chdb::unit_obj("test:u0", "test:civA", "infantry", 1, 15, 50, 10, 1000, 400, 100, 5);
        auto civ = civ_chdb::civ_obj_min("test:civA");
        auto blob = civ_chdb::build({unit}, {civ}, {}, false);
        DataCatalogStorageV1 s;
        const auto c = catalog_load_bytes_v1(blob.data(), blob.size(), CatalogLoadProfile::Verified, s);
        CHECK(c == CatalogLoadCode::Ok);
        if (c == CatalogLoadCode::Ok && s.valid()) {
            const DataCatalogV1& cat = s.catalog();
            CHECK(cat.civ_count == 1);
            const CivId civ_a = catalog_find_civ(cat, "test:civA", std::strlen("test:civA"));
            CHECK(civ_a != INVALID_CIV_ID);
            const UnitId u0 = catalog_find_unit(cat, "test:u0", std::strlen("test:u0"));
            CHECK(u0 != INVALID_UNIT_ID);
            if (u0 != INVALID_UNIT_ID) CHECK(cat.units[u0].civ_id == civ_a);
        }
    }
    // 2b) Rechazo: civ_id referencia una civ que NO existe en el catálogo.
    {
        auto unit = civ_chdb::unit_obj("test:u0", "test:missing", "infantry", 1, 15, 50, 10, 1000, 400, 100, 5);
        auto blob = civ_chdb::build({unit}, {}, {}, false);
        DataCatalogStorageV1 s;
        const auto c = catalog_load_bytes_v1(blob.data(), blob.size(), CatalogLoadProfile::Verified, s);
        CHECK(c == CatalogLoadCode::InvalidUnit);
        CHECK(!s.valid());
    }
}

// ============================================================================
// 3) Depósitos desde el mapa (SPEC-004 §16): fixture propio con spawns
//    (posiciones/cantidades exactas, conversión mt→raw) + rechazos.
// ============================================================================
static void test_map_resource_spawns() {
    // 3a) 3 spawns (A/B/Me), conversión exacta mt->raw.
    {
        const std::vector<civ_chdb::SpawnSpec> spawns = {
            {"resource", "A", 40000, 40000, 500},
            {"resource", "B", 100500, 20250, 300},
            {"resource", "Me", 128000, 216000, 750},
        };
        auto map = civ_chdb::map_obj("test:map0", spawns);
        auto blob = civ_chdb::build({}, {}, map, true);
        DataCatalogStorageV1 s;
        const auto c = catalog_load_bytes_v1(blob.data(), blob.size(), CatalogLoadProfile::Verified, s);
        CHECK(c == CatalogLoadCode::Ok);
        if (c == CatalogLoadCode::Ok && s.valid()) {
            const DataCatalogV1& cat = s.catalog();
            CHECK(cat.map_resource_spawn_count == 3);
            CHECK(cat.map_resource_spawns[0].resource_idx == 0u);
            CHECK(cat.map_resource_spawns[0].x_raw == (40000ll * static_cast<int64_t>(FX_ONE_RAW)) / 1000);
            CHECK(cat.map_resource_spawns[0].y_raw == (40000ll * static_cast<int64_t>(FX_ONE_RAW)) / 1000);
            CHECK(cat.map_resource_spawns[0].amount == 500);
            CHECK(cat.map_resource_spawns[1].resource_idx == 1u);
            CHECK(cat.map_resource_spawns[1].x_raw == (100500ll * static_cast<int64_t>(FX_ONE_RAW)) / 1000);
            CHECK(cat.map_resource_spawns[1].y_raw == (20250ll * static_cast<int64_t>(FX_ONE_RAW)) / 1000);
            CHECK(cat.map_resource_spawns[1].amount == 300);
            CHECK(cat.map_resource_spawns[2].resource_idx == 2u);
            CHECK(cat.map_resource_spawns[2].amount == 750);
        }
    }
    // 3b) Rechazo: excede ECO_MAX_DEPOSITS (33 spawns válidos, uno de más).
    {
        std::vector<civ_chdb::SpawnSpec> spawns;
        for (uint32_t i = 0; i < ECO_MAX_DEPOSITS + 1; ++i) {
            civ_chdb::SpawnSpec sp;
            sp.kind = "resource"; sp.id = "A";
            sp.x_mt = 1000 * static_cast<int64_t>(i + 1); sp.y_mt = 1000; sp.amount = 10;
            spawns.push_back(sp);
        }
        auto map = civ_chdb::map_obj("test:map1", spawns);
        auto blob = civ_chdb::build({}, {}, map, true);
        DataCatalogStorageV1 s;
        const auto c = catalog_load_bytes_v1(blob.data(), blob.size(), CatalogLoadProfile::Verified, s);
        CHECK(c == CatalogLoadCode::InvalidMap);
        CHECK(!s.valid());
    }
    // 3c) Control positivo: EXACTAMENTE ECO_MAX_DEPOSITS spawns SÍ carga.
    {
        std::vector<civ_chdb::SpawnSpec> spawns;
        for (uint32_t i = 0; i < ECO_MAX_DEPOSITS; ++i) {
            civ_chdb::SpawnSpec sp;
            sp.kind = "resource"; sp.id = "A";
            sp.x_mt = 1000 * static_cast<int64_t>(i + 1); sp.y_mt = 1000; sp.amount = 10;
            spawns.push_back(sp);
        }
        auto map = civ_chdb::map_obj("test:map1b", spawns);
        auto blob = civ_chdb::build({}, {}, map, true);
        DataCatalogStorageV1 s;
        const auto c = catalog_load_bytes_v1(blob.data(), blob.size(), CatalogLoadProfile::Verified, s);
        CHECK(c == CatalogLoadCode::Ok);
        if (c == CatalogLoadCode::Ok && s.valid()) {
            CHECK(s.catalog().map_resource_spawn_count == ECO_MAX_DEPOSITS);
        }
    }
    // 3d) Rechazo: id fuera de A/B/Me.
    {
        const std::vector<civ_chdb::SpawnSpec> spawns = { {"resource", "Zzz", 1000, 1000, 10} };
        auto map = civ_chdb::map_obj("test:map2", spawns);
        auto blob = civ_chdb::build({}, {}, map, true);
        DataCatalogStorageV1 s;
        const auto c = catalog_load_bytes_v1(blob.data(), blob.size(), CatalogLoadProfile::Verified, s);
        CHECK(c == CatalogLoadCode::InvalidMap);
    }
    // 3e) `kind == "material"` se IGNORA, NO rechaza (endurecimiento del
    //     Arquitecto tras la auditoría Opus, P3 del Sprint 1.6B): el enum del
    //     schema admite {resource, material} y los materiales son contenido
    //     LEGÍTIMO de Fase 2 (recetas), excluidos del alcance de este sprint.
    //     Rechazar el catálogo entero por un valor válido del schema sería una
    //     bomba de compatibilidad. El spawn se salta: el catálogo carga y no
    //     aporta depósitos.
    {
        const std::vector<civ_chdb::SpawnSpec> spawns = { {"material", "A", 1000, 1000, 10} };
        auto map = civ_chdb::map_obj("test:map3", spawns);
        auto blob = civ_chdb::build({}, {}, map, true);
        DataCatalogStorageV1 s;
        const auto c = catalog_load_bytes_v1(blob.data(), blob.size(), CatalogLoadProfile::Verified, s);
        CHECK(c == CatalogLoadCode::Ok);
        CHECK(s.valid());
        if (s.valid()) CHECK(s.catalog().map_resource_spawn_count == 0u);
    }
    // 3f) Rechazo: depósito FUERA de la cota del mundo (P1 de la auditoría
    //     Opus). El schema permite hasta 2^31-1 mili-tiles (~262x el mundo);
    //     un blob así congelaba el kernel (FatalReason::WORLD_BOUNDS en el
    //     primer tick). Entrada no confiable ⇒ rechazo del catálogo entero.
    {
        const std::vector<civ_chdb::SpawnSpec> spawns = { {"resource", "A", 2147483647, 1000, 10} };
        auto map = civ_chdb::map_obj("test:map4", spawns);
        auto blob = civ_chdb::build({}, {}, map, true);
        DataCatalogStorageV1 s;
        const auto c = catalog_load_bytes_v1(blob.data(), blob.size(), CatalogLoadProfile::Verified, s);
        CHECK(c == CatalogLoadCode::InvalidMap);
    }
}

// ============================================================================
// 4) Fallback legacy BIT-IDÉNTICO (SPEC-004 §16): sin catálogo/sin spawns,
//    los 6 depósitos fijos de siempre; con spawns, gs_init_economy_from_catalog
//    SÍ sobreescribe (opt-in explícito).
// ============================================================================
static void test_legacy_fallback_bit_identical() {
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg());

    const int64_t T = FX_ONE_RAW;
    struct { int64_t tx, ty; uint8_t res; } expected[6] = {
        {40, 40, 0}, {216, 216, 0}, {40, 216, 1}, {216, 40, 1}, {128, 40, 2}, {128, 216, 2},
    };
    CHECK(g->n_deposits == 6u);
    for (uint32_t i = 0; i < 6; ++i) {
        CHECK(g->deposits[i].x_raw == expected[i].tx * T + T / 2);
        CHECK(g->deposits[i].y_raw == expected[i].ty * T + T / 2);
        CHECK(g->deposits[i].resource_idx == expected[i].res);
        CHECK(g->deposits[i].remaining == 500);
    }
    for (uint32_t e = 0; e < MAX_EMITTERS; ++e) {
        const int64_t dtx = 20 + static_cast<int64_t>(e) * 28;
        CHECK(g->dropoff_x[e] == dtx * T + T / 2);
        CHECK(g->dropoff_y[e] == 128 * T + T / 2);
    }

    // Sin catálogo enlazado: NO-OP.
    gs_init_economy_from_catalog(*g);
    CHECK(g->n_deposits == 6u);
    CHECK(g->deposits[0].x_raw == expected[0].tx * T + T / 2);

    // Catálogo enlazado SIN spawns de mapa (map_resource_spawn_count==0):
    // TAMBIÉN NO-OP (fallback legacy exacto).
    DataCatalogV1 empty_cat{};
    gs_bind_catalog(*g, empty_cat);
    gs_init_economy_from_catalog(*g);
    CHECK(g->n_deposits == 6u);
    CHECK(g->deposits[5].y_raw == expected[5].ty * T + T / 2);
    CHECK(g->deposits[5].resource_idx == expected[5].res);

    // Catálogo CON spawns de mapa: SÍ sobreescribe (control positivo).
    const std::vector<civ_chdb::SpawnSpec> spawns = {
        {"resource", "A", 40000, 40000, 500},
        {"resource", "B", 100500, 20250, 300},
    };
    auto map = civ_chdb::map_obj("test:map_ov", spawns);
    auto blob = civ_chdb::build({}, {}, map, true);
    DataCatalogStorageV1 s;
    const auto c = catalog_load_bytes_v1(blob.data(), blob.size(), CatalogLoadProfile::Verified, s);
    CHECK(c == CatalogLoadCode::Ok);
    if (c == CatalogLoadCode::Ok && s.valid()) {
        gs_bind_catalog(*g, s.catalog());
        gs_init_economy_from_catalog(*g);
        CHECK(g->n_deposits == 2u);
        CHECK(g->deposits[0].resource_idx == 0u);
        CHECK(g->deposits[0].remaining == 500);
        CHECK(g->deposits[1].resource_idx == 1u);
        CHECK(g->deposits[1].remaining == 300);
        // Slots [2, ECO_MAX_DEPOSITS) limpios: sin basura del patrón legacy.
        CHECK(g->deposits[2].x_raw == 0 && g->deposits[2].remaining == 0);
        CHECK(g->deposits[5].x_raw == 0 && g->deposits[5].remaining == 0);
        // El dropoff NO cambia (§6/§16: sigue siendo el fallback fijo).
        CHECK(g->dropoff_x[0] == 20 * T + T / 2);
    }
}

// ============================================================================
// 5) Época inicial por jugador distinta entre dos civs (SPEC-004 §17, cierra
//    la deuda catálogo-ancha del 1.2) — con control de que la variante
//    catálogo-ancha ORIGINAL (gs_init_epoch_from_catalog) sigue intacta.
// ============================================================================
namespace epoch_fixture {
inline constexpr CivId CIV_A = 0;
inline constexpr CivId CIV_B = 1;

inline BuildingDefinitionV1 make_probe(BuildingId id, CivId civ, uint8_t epoch_min) {
    BuildingDefinitionV1 d{};
    d.id = id; d.civ_id = civ; d.hp = 100; d.footprint_w = 1; d.footprint_h = 1;
    d.build_time_ticks = 0; d.cost[0] = 0; d.cost[1] = 0; d.cost[2] = 0;
    d.dropoff_mask = 0; d.constructible = 0;
    d.epoch_min = epoch_min; d.epoch_max = 15;
    for (uint32_t k = 0; k < PROD_TRAINS_MAX; ++k) d.trains[k] = INVALID_UNIT_ID;
    d.train_count = 0;
    for (uint32_t k = 0; k < PROD_TECHS_MAX; ++k) d.researches[k] = INVALID_TECH_ID;
    d.research_count = 0;
    for (uint32_t k = 0; k < BUILDING_REQCAP_MAX; ++k) d.required_capabilities[k] = INVALID_CAPABILITY_ID;
    d.required_capabilities_count = 0;
    return d;
}
}  // namespace epoch_fixture

static void test_epoch_per_player_civ() {
    static BuildingDefinitionV1 g_buildings[2] = {
        epoch_fixture::make_probe(0, epoch_fixture::CIV_A, 3),
        epoch_fixture::make_probe(1, epoch_fixture::CIV_B, 1),
    };
    DataCatalogV1 cat{};
    cat.building_count = 2; cat.buildings = g_buildings; cat.building_names = nullptr;

    // 5a) Dos jugadores CON civ asignada: cada uno ve SOLO su propio mínimo.
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg());
    gs_bind_catalog(*g, cat);
    gs_set_player_civ(*g, 0, epoch_fixture::CIV_A);
    gs_set_player_civ(*g, 1, epoch_fixture::CIV_B);
    gs_init_epoch_from_catalog_per_player(*g);
    CHECK(g->player_epoch[0] == 3u);
    CHECK(g->player_epoch[1] == 1u);
    CHECK(g->epoch_initial[0] == 3u);
    CHECK(g->epoch_initial[1] == 1u);
    CHECK(g->player_epoch[0] != g->player_epoch[1]);

    // 5b) Jugador SIN civ asignada (INVALID_CIV_ID, el default de gs_init):
    // usa la variante catálogo-ancha (mínimo de TODO el catálogo) — min(3,1)=1.
    auto g2 = std::make_unique<GameState>();
    gs_init(*g2, make_cfg());
    gs_bind_catalog(*g2, cat);
    gs_init_epoch_from_catalog_per_player(*g2);
    CHECK(g2->player_epoch[0] == 1u);
    CHECK(g2->player_epoch[2] == 1u);  // cualquier jugador, mismo resultado

    // 5c) Control: la función ORIGINAL gs_init_epoch_from_catalog (catálogo-
    // ancha, SIN TOCAR por este sprint) sigue dando el mismo resultado — no
    // se fusionaron los dos entry points.
    auto g3 = std::make_unique<GameState>();
    gs_init(*g3, make_cfg());
    gs_bind_catalog(*g3, cat);
    gs_init_epoch_from_catalog(*g3);
    CHECK(g3->player_epoch[0] == 1u);
}

// ============================================================================
// 6) Gate de civilización en PLACE_BUILDING/TRAIN_UNIT/RESEARCH_TECH
//    (SPEC-004 §17): rechaza cruzado, NO rechaza con INVALID_CIV_ID.
// ============================================================================
namespace gate_fixture {
inline constexpr CivId CIV_A = 10;
inline constexpr CivId CIV_B = 20;

inline UnitDefinitionV1 make_warrior(UnitId id, CivId civ) {
    UnitDefinitionV1 d{};
    d.id = id; d.civ_id = civ; d.unit_class = UnitClassV1::Infantry; d.tags_mask = 0;
    d.hp = 50; d.attack = 10; d.range_millitiles = 1000;
    d.speed_millitile_tick = 400; d.morale = 100; d.build_time_ticks = 2;
    for (int k = 0; k < 6; ++k) d.bonus_vs_bp[k] = 0;
    d.cost[0] = 10; d.cost[1] = 0; d.cost[2] = 0; d.pop_cost = 1;
    d.epoch_min = 1; d.epoch_max = 15;
    return d;
}
// barracks_a (civ A): nace completo, trains/researches incluyen AMBAS civs a
// propósito — aísla el gate de civilización del rechazo previo "no está en
// trains/researches" (MALFORMED), mismo recurso que ya usa
// test_production_tech.cpp con tech_high_epoch/tech_c.
inline BuildingDefinitionV1 make_barracks_a() {
    BuildingDefinitionV1 d{};
    d.id = 0; d.civ_id = CIV_A; d.hp = 300; d.footprint_w = 1; d.footprint_h = 1;
    d.build_time_ticks = 0; d.cost[0] = 0; d.cost[1] = 0; d.cost[2] = 0;
    d.dropoff_mask = 0; d.constructible = 0;
    d.epoch_min = 1; d.epoch_max = 15;
    d.trains[0] = 0; d.trains[1] = 1; d.train_count = 2;  // warrior_a(0), warrior_b(1)
    // Sprint 1.14: fixture sintetico, no va de poblacion. Declara el tope
    // entero para seguir midiendo lo que media.
    d.population_provided = static_cast<int32_t>(POP_CAP_V1);
    for (uint32_t k = 2; k < PROD_TRAINS_MAX; ++k) d.trains[k] = INVALID_UNIT_ID;
    d.researches[0] = 0; d.researches[1] = 1; d.research_count = 2;  // tech_a(0), tech_b(1)
    for (uint32_t k = 2; k < PROD_TECHS_MAX; ++k) d.researches[k] = INVALID_TECH_ID;
    for (uint32_t k = 0; k < BUILDING_REQCAP_MAX; ++k) d.required_capabilities[k] = INVALID_CAPABILITY_ID;
    d.required_capabilities_count = 0;
    return d;
}
// outpost_b (civ B): constructible normal, para ejercitar el gate en
// PLACE_BUILDING (fuera de la ventana de setup).
inline BuildingDefinitionV1 make_outpost_b() {
    BuildingDefinitionV1 d{};
    d.id = 1; d.civ_id = CIV_B; d.hp = 200; d.footprint_w = 2; d.footprint_h = 2;
    d.build_time_ticks = 5; d.cost[0] = 5; d.cost[1] = 0; d.cost[2] = 0;
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
inline TechDefinitionV1 make_tech(TechId id, CivId civ) {
    TechDefinitionV1 d{};
    d.id = id; d.civ_id = civ; d.cost[0] = 5; d.cost[1] = 0; d.cost[2] = 0;
    d.research_time_ticks = 3; d.epoch = 1;
    for (uint32_t k = 0; k < TECH_PREREQ_MAX; ++k) d.prerequisites[k] = INVALID_TECH_ID;
    d.prereq_count = 0;
    for (uint32_t k = 0; k < TECH_GRANT_MAX; ++k) d.grants[k] = INVALID_CAPABILITY_ID;
    d.grant_count = 0;
    for (uint32_t k = 0; k < TECH_MUTEX_MAX; ++k) d.mutually_exclusive_with[k] = INVALID_TECH_ID;
    d.mutex_count = 0;
    return d;
}

static UnitDefinitionV1 g_units[2] = { make_warrior(0, CIV_A), make_warrior(1, CIV_B) };
static BuildingDefinitionV1 g_buildings[2] = { make_barracks_a(), make_outpost_b() };
static TechDefinitionV1 g_techs[2] = { make_tech(0, CIV_A), make_tech(1, CIV_B) };

inline DataCatalogV1 make_catalog() {
    DataCatalogV1 c{};
    c.unit_count = 2; c.units = g_units; c.unit_names = nullptr;
    c.building_count = 2; c.buildings = g_buildings; c.building_names = nullptr;
    c.tech_count = 2; c.techs = g_techs; c.tech_names = nullptr;
    c.capability_count = 0; c.capability_names = nullptr;
    return c;
}
}  // namespace gate_fixture

static RawCommand gf_place_building(uint32_t tick, uint16_t emitter, uint64_t seq,
                                    BuildingId bid, int64_t tx, int64_t ty) {
    RawCommand c{};
    std::memset(&c, 0, sizeof(c));
    c.target_tick = tick; c.emitter = emitter; c.type = CommandType::PLACE_BUILDING;
    c.sequence = seq; c.p.unit_id = bid; c.p.x_raw = tx; c.p.y_raw = ty;
    return c;
}
static RawCommand gf_train_unit(uint32_t tick, uint16_t emitter, uint64_t seq,
                                EntityHandle building, UnitId uid) {
    RawCommand c{};
    std::memset(&c, 0, sizeof(c));
    c.target_tick = tick; c.emitter = emitter; c.type = CommandType::TRAIN_UNIT;
    c.sequence = seq; c.p.handle = building; c.p.unit_id = uid;
    return c;
}
static RawCommand gf_research_tech(uint32_t tick, uint16_t emitter, uint64_t seq,
                                   EntityHandle building, TechId tid) {
    RawCommand c{};
    std::memset(&c, 0, sizeof(c));
    c.target_tick = tick; c.emitter = emitter; c.type = CommandType::RESEARCH_TECH;
    c.sequence = seq; c.p.handle = building; c.p.unit_id = tid;
    return c;
}

// Coloca barracks_a (bid=0, civ A) vía la exención de escenario (tick 0) —
// el gate de civ NO aplica en esa ventana, igual que el resto de gates de
// §4.1.2, así que esto funciona idéntico con o sin player_civ asignada.
static EntityHandle gf_setup(GameState& g, const DataCatalogV1& cat) {
    gs_bind_catalog(g, cat);
    g.player_epoch[0] = 1; g.player_epoch[1] = 1;  // evita interferencia del gate de época
    g.player_stock[0][0] = 100000; g.player_stock[1][0] = 100000;
    RawCommand cmd = gf_place_building(0, 0, 1, 0 /*barracks_a*/, 10, 10);
    step(g, &cmd, 1);
    return EntityHandle{0, g.entities.generation[0]};
}

static void test_civ_gate_reject_cross_civ() {
    static DataCatalogV1 cat = gate_fixture::make_catalog();

    // 6a) player_civ[0]=CIV_A asignada: TRAIN de warrior_b (civ B) rechazado.
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        EntityHandle bld = gf_setup(*g, cat);
        gs_set_player_civ(*g, 0, gate_fixture::CIV_A);
        RawCommand cmd = gf_train_unit(g->tick, 0, 2, bld, 1 /*warrior_b*/);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::ILLEGAL_STATE);
    }
    // 6b) ... mismo escenario, warrior_a (civ A, propia) SÍ se acepta.
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        EntityHandle bld = gf_setup(*g, cat);
        gs_set_player_civ(*g, 0, gate_fixture::CIV_A);
        RawCommand cmd = gf_train_unit(g->tick, 0, 2, bld, 0 /*warrior_a*/);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.accepted == 1);
    }
    // 6c) RESEARCH de tech_b (civ B) rechazado; tech_a (civ A) aceptado.
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        EntityHandle bld = gf_setup(*g, cat);
        gs_set_player_civ(*g, 0, gate_fixture::CIV_A);
        RawCommand cmd = gf_research_tech(g->tick, 0, 2, bld, 1 /*tech_b*/);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::ILLEGAL_STATE);
    }
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        EntityHandle bld = gf_setup(*g, cat);
        gs_set_player_civ(*g, 0, gate_fixture::CIV_A);
        RawCommand cmd = gf_research_tech(g->tick, 0, 2, bld, 0 /*tech_a*/);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.accepted == 1);
    }
    // 6d) PLACE_BUILDING de outpost_b (civ B) fuera de la ventana de setup,
    // con player_civ[0]=CIV_A -> rechazado.
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        gf_setup(*g, cat);  // barracks_a ya colocado en tick 0; g->tick==1
        gs_set_player_civ(*g, 0, gate_fixture::CIV_A);
        RawCommand place = gf_place_building(g->tick, 0, 2, 1 /*outpost_b*/, 50, 50);
        const StepResult r = step(*g, &place, 1);
        CHECK(r.rejected == 1);
        CHECK(last_result(g->mailbox[0]) == RejectReason::ILLEGAL_STATE);
        CHECK(g->entities.alive_count == 1u);  // solo barracks_a, outpost_b NO se coloca
    }
}

static void test_civ_gate_invalid_does_not_reject() {
    static DataCatalogV1 cat = gate_fixture::make_catalog();

    // 7a) player_civ[0] queda INVALID_CIV_ID (nunca se llama gs_set_player_civ):
    // TRAIN de warrior_b (civ B) por el jugador 0 (dueño de barracks_a, civ A)
    // SÍ se acepta — el gate no aplica.
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        EntityHandle bld = gf_setup(*g, cat);
        CHECK(g->player_civ[0] == INVALID_CIV_ID);  // control: nunca asignada
        RawCommand cmd = gf_train_unit(g->tick, 0, 2, bld, 1 /*warrior_b, otra civ*/);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.accepted == 1);
    }
    // 7b) RESEARCH de tech_b (civ B) SÍ se acepta con INVALID_CIV_ID.
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        EntityHandle bld = gf_setup(*g, cat);
        RawCommand cmd = gf_research_tech(g->tick, 0, 2, bld, 1 /*tech_b*/);
        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.accepted == 1);
    }
    // 7c) PLACE_BUILDING de outpost_b (civ B) fuera de setup SÍ se acepta
    // con INVALID_CIV_ID.
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, make_cfg());
        gf_setup(*g, cat);
        RawCommand place = gf_place_building(g->tick, 0, 2, 1 /*outpost_b*/, 50, 50);
        const StepResult r = step(*g, &place, 1);
        CHECK(r.accepted == 1);
        CHECK(g->entities.alive_count == 2u);  // barracks_a + outpost_b
    }
}

// ============================================================================
// 8) Save v12 round-trip con player_civ (SPEC-004 §20).
// ============================================================================
static void test_save_v12_player_civ_roundtrip() {
    auto g1 = std::make_unique<GameState>();
    gs_init(*g1, make_cfg());
    gs_set_player_civ(*g1, 0, 7u);
    // g1->player_civ[1] queda INVALID_CIV_ID (default) a propósito: cubre
    // ambos casos (asignada / sin asignar) en el mismo round-trip.
    step(*g1, nullptr, 0);  // un tick cualquiera, sin comandos

    AiJobBox box{}; ai_box_init(box, 2);
    AiRuntimeV1 rt{};
    CHECK(save_game(*g1, box, rt, "test_civ_deposits_v12.sav") == 0);

    auto g2 = std::make_unique<GameState>();
    AiJobBox box2{}; AiRuntimeV1 rt2{};
    CHECK(load_game(*g2, box2, rt2, "test_civ_deposits_v12.sav") == 0);

    CHECK(g2->player_civ[0] == 7u);
    CHECK(g2->player_civ[1] == INVALID_CIV_ID);
    for (uint32_t e = 2; e < MAX_EMITTERS; ++e) CHECK(g2->player_civ[e] == INVALID_CIV_ID);
    CHECK(state_checksum_v1(*g1) == state_checksum_v1(*g2));

    std::remove("test_civ_deposits_v12.sav");
}

int main() {
    test_civ_id_real_golden();
    test_civ_id_fixture();
    test_map_resource_spawns();
    test_legacy_fallback_bit_identical();
    test_epoch_per_player_civ();
    test_civ_gate_reject_cross_civ();
    test_civ_gate_invalid_does_not_reject();
    test_save_v12_player_civ_roundtrip();

    if (g_fails == 0) { std::printf("civ_deposits: OK\n"); return 0; }
    std::printf("civ_deposits: %d fallos\n", g_fails);
    return 1;
}
