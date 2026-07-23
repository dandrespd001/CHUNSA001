// Test del kernel data-driven (Sprint 0.4, SPEC-002): loader CHDB v1 sobre el
// golden versionado y SPAWN_UNIT/SPAWN_CITIZEN copiando stats SOLO del
// catálogo, nunca del payload. Autor: sonnet-5 (brief
// docs/briefs/SONNET_KERNEL_DATOS_SPEC002.md, alcance ajustado — ver
// docs/briefs/SONNET_KERNEL_DATOS_SPEC002_RESULT.md).
//
// NOTA: GameState pesa varios MB (arrays dimensionados a ENTITY_HARD_CAP) —
// SIEMPRE se asigna con `new` (heap), nunca como variable local; declararlo
// en el stack desborda la pila del hilo (ver el resto del kernel: test_state,
// cli_run.hpp, driver.hpp usan el mismo patrón).
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <vector>

#include "chunsa/data_catalog.hpp"
#include "chunsa/game_state.hpp"
#include "chunsa/step.hpp"
#include "chunsa/checksum.hpp"

#ifndef CHUNSA_GOLDEN_CHDB_PATH
#error "CHUNSA_GOLDEN_CHDB_PATH debe definirse via CMake (ver CMakeLists.txt: chunsa_test_data_blob)"
#endif

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

static std::vector<uint8_t> read_all(const char* path) {
    std::ifstream f(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

static void expect_reject(const std::vector<uint8_t>& bytes, CatalogLoadProfile prof, const char* label) {
    DataCatalogStorageV1 s;
    const auto c = catalog_load_bytes_v1(bytes.data(), bytes.size(), prof, s);
    if (c == CatalogLoadCode::Ok) {
        ++g_fails;
        std::printf("CHECK (corrupt corpus) %s: esperado rechazo, aceptado\n", label);
    }
}

// Heap-allocated GameState inicializado, para no repetir `new/gs_init/delete`.
static std::unique_ptr<GameState> make_state(const MatchConfig01A& cfg) {
    std::unique_ptr<GameState> g(new GameState());
    gs_init(*g, cfg);
    return g;
}

// ============================================================================
// Sprint 1.1 (SPEC-004 §2/§9.3): constructor de un CHDB v1 MÍNIMO hecho a mano
// (no generado por chunsa_data_compiler.py) para ejercitar el rechazo del
// loader ante un building inválido (hp=0, footprint=9). El brief pide
// extender "el fixture de test_data_blob" sin tocar los datos reales de
// data/ (de eso se ocupa la tarea paralela de datos) — como no existe un
// pipeline Python que genere un fixture C++ aparte del golden real, se
// construye aquí el blob byte a byte (mismo formato CVE1/directorio que
// valida data_catalog.hpp), con manifest(1) + building(1) + el resto de
// secciones vacías (0 records; el loader no exige mínimo salvo manifest==1).
// ============================================================================
namespace mini_chdb {

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
inline std::vector<uint8_t> cve_bool(bool v) { return { static_cast<uint8_t>(v ? 0x02u : 0x01u) }; }
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

struct BuildingSpec {
    std::string id = "test:bldg";
    int64_t hp = 500;
    int64_t width = 3, height = 2;
    int64_t build_time = 30;
    bool constructible = true;
    int64_t cost_a = 100, cost_b = 0, cost_me = 20;
};

inline std::vector<uint8_t> build(const BuildingSpec& spec) {
    // Objeto building — claves en orden ASCENDENTE estricto (build_time_ticks
    // < constructible < dropoff_resources < footprint < id < resource_costs < stats).
    auto footprint = cve_obj({
        {"height_cells", cve_int(spec.height)},
        {"width_cells", cve_int(spec.width)},
    });
    auto stats = cve_obj({ {"hp", cve_int(spec.hp)} });
    auto costs = cve_obj({
        {"A", cve_int(spec.cost_a)},
        {"B", cve_int(spec.cost_b)},
        {"Me", cve_int(spec.cost_me)},
    });
    auto dropoff = cve_str_arr({"A"});
    auto building_obj = cve_obj({
        {"build_time_ticks", cve_int(spec.build_time)},
        {"constructible", cve_bool(spec.constructible)},
        {"dropoff_resources", dropoff},
        {"footprint", footprint},
        {"id", cve_str(spec.id)},
        {"resource_costs", costs},
        {"stats", stats},
    });
    auto manifest_obj = cve_obj({ {"package_id", cve_str(std::string("test.fixture"))} });

    std::vector<uint8_t> manifest_record;
    w_u32(manifest_record, static_cast<uint32_t>(manifest_obj.size()));
    append(manifest_record, manifest_obj);

    std::vector<uint8_t> building_record;
    w_u32(building_record, static_cast<uint32_t>(building_obj.size()));
    append(building_record, building_obj);

    // Orden kKindTable (data_catalog.hpp): manifest, unit, building, tech,
    // civ, map, ai-profile. Solo manifest(1) y building(1) llevan contenido;
    // el resto quedan vacías (0 records — el loader no exige mínimo salvo
    // manifest==1).
    std::vector<uint8_t> sections[7];
    sections[0] = manifest_record;
    sections[2] = building_record;

    struct KindRow { uint16_t kind, version; uint32_t count; };
    static constexpr KindRow ROWS[7] = {
        {1, 1, 1}, {2, 2, 0}, {3, 1, 1}, {4, 1, 0}, {5, 1, 0}, {6, 1, 0}, {7, 1, 0},
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
    w_u32(blob, 0u);                     // flags (release: sin UNVERIFIED/HAS_PATCHES)
    w_u32(blob, 7u);                     // section_count
    w_u32(blob, 24u);                    // entry_size
    w_u32(blob, 0u);                     // reserved
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

}  // namespace mini_chdb

int main() {
    // 1) Loader: golden aceptado, content_hash coincide con el sidecar
    //    publicado (data/compiled/chunsa_base.chdb.content.json). El hash NO
    //    se hardcodea aparte de este vector de prueba versionado junto al
    //    propio golden — si el Arquitecto regenera el blob (corrección de
    //    procedencia en curso), este test y su hash cambian JUNTOS.
    DataCatalogStorageV1 store;
    const auto code = catalog_load_file_v1(CHUNSA_GOLDEN_CHDB_PATH, CatalogLoadProfile::Verified, store);
    CHECK(code == CatalogLoadCode::Ok);
    CHECK(store.valid());
    if (!store.valid()) {
        std::printf("data_blob: %d fallos (loader no produjo catálogo válido)\n", g_fails);
        return 1;
    }

    const DataCatalogV1& cat = store.catalog();
    CHECK(cat.unit_count == 5);
    static constexpr uint8_t kExpectedHash[32] = {
        0xe1, 0x73, 0xbe, 0x0a, 0x3c, 0xb1, 0xc6, 0x8d, 0xac, 0xc0, 0x13, 0xc7, 0x81, 0x17, 0x81, 0x73,
        0x6d, 0xaf, 0xe5, 0x5b, 0x3c, 0x4d, 0x77, 0x64, 0x42, 0xf1, 0xd6, 0xef, 0x97, 0x6f, 0x0f, 0xb1,
    };
    CHECK(std::memcmp(cat.content_hash.bytes, kExpectedHash, 32) == 0);
    CHECK(cat.blob_format_major == 1 && cat.blob_format_minor == 0);
    CHECK(cat.schema_set_version == 1);
    CHECK(cat.catalog_flags == 0);  // fixture release (no UNVERIFIED/HAS_PATCHES)
    CHECK(cat.base_package_id_bytes == std::strlen("chunsa.base"));
    CHECK(std::memcmp(cat.base_package_id_utf8, "chunsa.base", cat.base_package_id_bytes) == 0);

    // 2) Resolución de record_id → UnitId (setup, fuera de Step()).
    const UnitId legionary = catalog_find_unit(cat, "rome:legionary", 14);
    CHECK(legionary != INVALID_UNIT_ID);
    CHECK(legionary < cat.unit_count);
    CHECK(cat.units[legionary].unit_class == UnitClassV1::Infantry);
    CHECK(cat.units[legionary].hp == 90);
    CHECK(cat.units[legionary].attack == 10);
    CHECK(cat.units[legionary].range_millitiles == 100);
    CHECK(cat.units[legionary].speed_millitile_tick == 50);
    CHECK(cat.units[legionary].morale == 70);

    const UnitId citizen = catalog_find_unit(cat, "egipto:work_crew", 16);
    CHECK(citizen != INVALID_UNIT_ID);
    CHECK(cat.units[citizen].unit_class == UnitClassV1::Citizen);
    CHECK(cat.units[citizen].hp == 45);

    CHECK(catalog_find_unit(cat, "nope:nope", 9) == INVALID_UNIT_ID);

    // 3) SPAWN_UNIT data-driven: stats vienen del CATÁLOGO, no del payload.
    //    El test central que exige el brief: "spawnea una unidad desde el
    //    blob y comprueba que sus stats vienen del dato, no del payload".
    MatchConfig01A cfg{};
    cfg.max_entities = 16; cfg.player_count = 1;
    // human_input_delay_ticks=0: cada test llama step() UNA sola vez y espera
    // que el comando surta efecto en ESE mismo tick (command_effective_tick =
    // max(target_tick, t+delay); con delay>0 el comando quedaría agendado
    // para el tick siguiente, que este test no ejecuta — ver step.hpp §6.2).
    cfg.human_input_delay_ticks = 0; cfg.max_future_command_ticks = 20;
    cfg.checksum_every_ticks = 1; cfg.map_tiles_x = 256; cfg.map_tiles_y = 256;
    cfg.seed = 1;
    cfg.allow_debug_stat_payload = 0;  // producción: SOLO camino data-driven

    {
        auto g = make_state(cfg);
        gs_bind_catalog(*g, cat);

        RawCommand cmd{};
        cmd.target_tick = 0; cmd.emitter = 0; cmd.type = CommandType::SPAWN_UNIT; cmd.sequence = 1;
        cmd.p.handle = EntityHandle{0, 1};
        cmd.p.x_raw = 100 * 65536; cmd.p.y_raw = 100 * 65536;
        cmd.p.unit_id = legionary;
        // hp/attack/range_mt/unit_class/speed_mtpt quedan en 0 (aggregate
        // init) — exactamente lo que exige el camino normal (payload limpio).

        const StepResult r = step(*g, &cmd, 1);
        CHECK(r.accepted == 1);
        CHECK(r.rejected == 0);
        CHECK(g->entities.alive[0] == 1);
        CHECK(g->unit_id[0] == legionary);
        CHECK(g->hp[0] == 90);      // catálogo (rome:legionary), NO el payload (0)
        CHECK(g->max_hp[0] == 90);
        CHECK(g->attack[0] == 10);
        CHECK(g->range_mt[0] == 100);
        CHECK(g->speed_mtpt[0] == 50);
        // El SPAWN_UNIT asigna catalog.units[legionary].morale == 70, pero
        // morale_system corre en la MISMA fase 5 del tick (sin enemigos
        // cerca ⇒ MORALE_REGEN) antes de que step() devuelva el control —
        // el valor observable tras step() es 70+MORALE_REGEN, no el crudo
        // del catálogo. Esto sigue probando que la moral SALIÓ del dato
        // (70), no del payload legado (que hubiera sido MORALE_MAX=100).
        CHECK(g->morale[0] == 70 + MORALE_REGEN);
        CHECK(g->unit_class[0] == 0);  // Infantry
        CHECK(g->fleeing[0] == 0);
    }

    // 4) Payload contaminado (stat != 0) con unit_id válido ⇒ MALFORMED. El
    //    kernel nunca mezcla dato de catálogo con stats del payload.
    {
        auto g = make_state(cfg);
        gs_bind_catalog(*g, cat);
        RawCommand bad{};
        bad.target_tick = 0; bad.emitter = 0; bad.type = CommandType::SPAWN_UNIT; bad.sequence = 1;
        bad.p.handle = EntityHandle{0, 1};
        bad.p.x_raw = 100 * 65536; bad.p.y_raw = 100 * 65536;
        bad.p.unit_id = legionary;
        bad.p.hp = 999;  // contaminado
        const StepResult rb = step(*g, &bad, 1);
        CHECK(rb.rejected == 1);
        CHECK(g->entities.alive[0] == 0);
    }

    // 5) unit_id fuera de bounds del catálogo enlazado ⇒ MALFORMED.
    {
        auto g = make_state(cfg);
        gs_bind_catalog(*g, cat);
        RawCommand oob{};
        oob.target_tick = 0; oob.emitter = 0; oob.type = CommandType::SPAWN_UNIT; oob.sequence = 1;
        oob.p.handle = EntityHandle{0, 1};
        oob.p.x_raw = 100 * 65536; oob.p.y_raw = 100 * 65536;
        oob.p.unit_id = cat.unit_count + 1000u;
        const StepResult ro = step(*g, &oob, 1);
        CHECK(ro.rejected == 1);
        CHECK(g->entities.alive[0] == 0);
    }

    // 6) Camino debug legado: existe pero SOLO si el match lo activa
    //    explícitamente (allow_debug_stat_payload=1); si no, MALFORMED.
    {
        RawCommand debug_cmd{};
        debug_cmd.target_tick = 0; debug_cmd.emitter = 0; debug_cmd.type = CommandType::SPAWN_UNIT;
        debug_cmd.sequence = 1;
        debug_cmd.p.handle = EntityHandle{0, 1};
        debug_cmd.p.x_raw = 50 * 65536; debug_cmd.p.y_raw = 50 * 65536;
        debug_cmd.p.hp = 33; debug_cmd.p.attack = 4; debug_cmd.p.range_mt = 1000; debug_cmd.p.unit_class = 1;
        debug_cmd.p.unit_id = INVALID_UNIT_ID;

        MatchConfig01A dbg_cfg = cfg;
        dbg_cfg.allow_debug_stat_payload = 1;
        auto g3 = make_state(dbg_cfg);  // sin catálogo enlazado: el camino debug no lo necesita
        const StepResult rd = step(*g3, &debug_cmd, 1);
        CHECK(rd.accepted == 1);
        CHECK(g3->hp[0] == 33);
        CHECK(g3->unit_id[0] == INVALID_UNIT_ID);

        auto g4 = make_state(cfg);  // allow_debug_stat_payload = 0 (default de este test)
        const StepResult rd2 = step(*g4, &debug_cmd, 1);
        CHECK(rd2.rejected == 1);
        CHECK(g4->entities.alive[0] == 0);
    }

    // 7) Siege/NavalLight: compilados/cargables pero el spawn es ILLEGAL_STATE
    //    (SPEC-002 §8.4). Catálogo mínimo fabricado en memoria (no hay unidad
    //    de esas clases en el fixture D1 promovido).
    {
        UnitDefinitionV1 siege_def{};
        siege_def.id = 0; siege_def.unit_class = UnitClassV1::Siege; siege_def.tags_mask = 0;
        siege_def.hp = 50; siege_def.attack = 5; siege_def.range_millitiles = 500;
        siege_def.speed_millitile_tick = 10; siege_def.morale = 50; siege_def.build_time_ticks = 100;
        for (int k = 0; k < 6; ++k) siege_def.bonus_vs_bp[k] = 0;
        DataCatalogV1 mini{};
        mini.unit_count = 1;
        mini.units = &siege_def;
        mini.unit_names = nullptr;

        auto g5 = make_state(cfg);
        gs_bind_catalog(*g5, mini);
        RawCommand siege_cmd{};
        siege_cmd.target_tick = 0; siege_cmd.emitter = 0; siege_cmd.type = CommandType::SPAWN_UNIT;
        siege_cmd.sequence = 1;
        siege_cmd.p.handle = EntityHandle{0, 1};
        siege_cmd.p.x_raw = 50 * 65536; siege_cmd.p.y_raw = 50 * 65536;
        siege_cmd.p.unit_id = 0;
        const StepResult rs = step(*g5, &siege_cmd, 1);
        CHECK(rs.rejected == 1);
        CHECK(g5->entities.alive[0] == 0);
    }

    // 8) SPAWN_CITIZEN data-driven: exige clase Citizen; sobre una unidad de
    //    combate (rome:legionary) es ILLEGAL_STATE.
    {
        auto g = make_state(cfg);
        gs_bind_catalog(*g, cat);
        RawCommand cz{};
        cz.target_tick = 0; cz.emitter = 0; cz.type = CommandType::SPAWN_CITIZEN; cz.sequence = 1;
        cz.p.handle = EntityHandle{0, 1};
        cz.p.x_raw = 100 * 65536; cz.p.y_raw = 100 * 65536;
        cz.p.unit_id = citizen;
        const StepResult rc = step(*g, &cz, 1);
        CHECK(rc.accepted == 1);
        CHECK(g->unit_class[0] == 3);
        CHECK(g->hp[0] == 45);
        CHECK(g->attack[0] == 0);
        CHECK(g->range_mt[0] == 0);

        auto g2 = make_state(cfg);
        gs_bind_catalog(*g2, cat);
        RawCommand cz_bad{};
        cz_bad.target_tick = 0; cz_bad.emitter = 0; cz_bad.type = CommandType::SPAWN_CITIZEN; cz_bad.sequence = 1;
        cz_bad.p.handle = EntityHandle{0, 1};
        cz_bad.p.x_raw = 100 * 65536; cz_bad.p.y_raw = 100 * 65536;
        cz_bad.p.unit_id = legionary;  // Infantry, no Citizen
        const StepResult rc2 = step(*g2, &cz_bad, 1);
        CHECK(rc2.rejected == 1);
        CHECK(g2->entities.alive[0] == 0);
    }

    // 9) Determinismo: checksum v2 (con unit_id) es estable entre dos
    //    corridas idénticas, y difiere si unit_id difiere (aunque el resto
    //    del estado sea igual) — invariante §8.5 del brief.
    {
        auto ga = make_state(cfg); gs_bind_catalog(*ga, cat);
        RawCommand cmd{};
        cmd.target_tick = 0; cmd.emitter = 0; cmd.type = CommandType::SPAWN_UNIT; cmd.sequence = 1;
        cmd.p.handle = EntityHandle{0, 1};
        cmd.p.x_raw = 100 * 65536; cmd.p.y_raw = 100 * 65536;
        cmd.p.unit_id = legionary;
        step(*ga, &cmd, 1);
        const uint64_t ck_a1 = state_checksum_v1(*ga);

        auto gb = make_state(cfg); gs_bind_catalog(*gb, cat);
        step(*gb, &cmd, 1);
        const uint64_t ck_a2 = state_checksum_v1(*gb);
        CHECK(ck_a1 == ck_a2);  // determinismo: misma entrada, mismo checksum

        // Mismo comando pero con OTRO unit_id (citizen en vez de legionary,
        // ambos válidos en el catálogo) debe dar checksum DISTINTO.
        auto gc = make_state(cfg); gs_bind_catalog(*gc, cat);
        RawCommand cmd2 = cmd;
        cmd2.p.unit_id = citizen;
        step(*gc, &cmd2, 1);
        const uint64_t ck_b = state_checksum_v1(*gc);
        CHECK(ck_a1 != ck_b);
    }

    // 10) Corpus de rechazo del loader (SPEC-002 §5/§6): truncación, magic
    //     incorrecto, flags desconocidos/HAS_PATCHES, UNVERIFIED bajo
    //     Verified (con control positivo bajo Development), section_count
    //     corrupto, offset de sección solapado, file_size mentiroso.
    {
        std::vector<uint8_t> golden = read_all(CHUNSA_GOLDEN_CHDB_PATH);
        CHECK(!golden.empty());

        for (size_t cut : {size_t(0), size_t(39), size_t(100), size_t(207),
                           golden.size() / 2, golden.size() - 1}) {
            auto v = golden; v.resize(cut);
            expect_reject(v, CatalogLoadProfile::Verified, "truncated");
        }
        { auto v = golden; v[0] ^= 0xFF; expect_reject(v, CatalogLoadProfile::Verified, "bad_magic"); }
        { auto v = golden; v[0x10] |= 0x80; expect_reject(v, CatalogLoadProfile::Verified, "unknown_flags"); }
        { auto v = golden; v[0x10] |= 0x02; expect_reject(v, CatalogLoadProfile::Verified, "has_patches"); }
        { auto v = golden; v[0x10] |= 0x01; expect_reject(v, CatalogLoadProfile::Verified, "unverified_under_verified"); }
        {
            auto v = golden; v[0x10] |= 0x01;
            DataCatalogStorageV1 s;
            const auto c = catalog_load_bytes_v1(v.data(), v.size(), CatalogLoadProfile::Development, s);
            CHECK(c == CatalogLoadCode::Ok);  // control positivo: Development SÍ admite UNVERIFIED
        }
        { auto v = golden; v[0x14] = 8; expect_reject(v, CatalogLoadProfile::Verified, "section_count"); }
        {
            auto v = golden;
            const size_t off_pos = 40 + 24 + 8;  // directory[1].offset (kind=unit)
            for (int i = 0; i < 8; ++i) v[off_pos + i] = 0;
            expect_reject(v, CatalogLoadProfile::Verified, "offset_overlap_zero");
        }
        { auto v = golden; v[0x20] ^= 0xFF; expect_reject(v, CatalogLoadProfile::Verified, "file_size_lie"); }
        {
            std::vector<uint8_t> empty_input;
            expect_reject(empty_input, CatalogLoadProfile::Verified, "empty_input");
        }
    }

    // 11) Auditoría de seguridad post-integración (P1-A/P1-B, ver RESULT).
    {
        // P1-B: NFC parcial. El chequeo interno `utf8_nfc_safe_no_nul` NO es
        // parte de la API literal del brief, pero vive en el mismo header y
        // es el punto exacto que decide NonCanonical para strings CVE — se
        // ejercita directamente porque construir un CHDB completo con un
        // string en forma NFD a mano (reempaquetando offsets/tamaños) es
        // desproporcionado frente a probar la función que realmente decide.
        using data_catalog_detail::utf8_nfc_safe_no_nul;
        // ASCII puro: siempre válido.
        CHECK(utf8_nfc_safe_no_nul("caballeria"));
        // Precompuesto NFC real del fixture (í = U+00ED, 0xC3 0xAD): válido —
        // el golden real usa exactamente esta forma ("caballería" en
        // egipto_chariot_warrior.yaml/provenance.balance_design.rationale) y
        // DEBE seguir cargando tras este parche.
        CHECK(utf8_nfc_safe_no_nul(std::string("caballer\xC3\xAD" "a")));
        // Forma NFD equivalente: 'i' + COMBINING ACUTE ACCENT (U+0301, 0xCC
        // 0x81) — el productor jamás la emite (unicodedata.normalize la
        // recompondría); el loader debe rechazarla como NO canónica.
        CHECK(!utf8_nfc_safe_no_nul(std::string("caballeri\xCC\x81" "a")));
        // Marca combinante suelta, sin letra base.
        CHECK(!utf8_nfc_safe_no_nul(std::string("\xCC\x81")));

        // El golden real (que SÍ contiene texto acentuado en `provenance`)
        // sigue cargando y con el MISMO content_hash que antes del parche —
        // la regla NFC parcial no rompe el fixture legítimo.
        std::vector<uint8_t> golden2 = read_all(CHUNSA_GOLDEN_CHDB_PATH);
        DataCatalogStorageV1 s_nfc;
        const auto c_nfc = catalog_load_bytes_v1(golden2.data(), golden2.size(),
                                                 CatalogLoadProfile::Verified, s_nfc);
        CHECK(c_nfc == CatalogLoadCode::Ok);
        if (s_nfc.valid()) {
            CHECK(std::memcmp(s_nfc.catalog().content_hash.bytes, kExpectedHash, 32) == 0);
        }

        // P1-A: fuga de memoria en camino de fallo. Corromper `hp` de la
        // ÚLTIMA unidad (rome:legionary, 90 -> fuera de rango) hace fallar
        // `build_unit_definition` DESPUÉS de que ya se hayan insertado 4
        // unidades completas en impl->units/unit_ids/unit_names — exactamente
        // el escenario "blob corrupto no trivial" que describe la auditoría.
        // No hay aserción de "no leak" observable en un test funcional (se
        // verificó con AddressSanitizer+LeakSanitizer fuera de este target,
        // ver RESULT); aquí se deja como regresión de comportamiento: la
        // carga falla con el código correcto y de forma repetible.
        {
            auto u64_at = [&](size_t off) {
                uint64_t v = 0;
                for (int i = 0; i < 8; ++i) v |= static_cast<uint64_t>(golden2[off + i]) << (8 * i);
                return v;
            };
            const size_t dir1 = 40 + 24;  // directory[1] (unit) entry
            const uint64_t unit_off = u64_at(dir1 + 8);
            const uint64_t unit_sz = u64_at(dir1 + 16);
            uint8_t pattern[9] = {0x10, 90, 0, 0, 0, 0, 0, 0, 0};  // CVE int64 tag + LE64(90)
            size_t hit = static_cast<size_t>(-1);
            int hit_count = 0;
            for (size_t i = unit_off; i + 9 <= unit_off + unit_sz; ++i) {
                if (std::memcmp(golden2.data() + i, pattern, 9) == 0) { hit = i; ++hit_count; }
            }
            CHECK(hit_count == 1);  // el patrón debe ser único en la sección unit
            if (hit_count == 1) {
                auto bad = golden2;
                const uint64_t newval = 2000000ull;  // fuera de [1, 1000000]
                for (int i = 0; i < 8; ++i) {
                    bad[hit + 1 + i] = static_cast<uint8_t>((newval >> (8 * i)) & 0xFFu);
                }
                for (int rep = 0; rep < 200; ++rep) {  // repetido: amplifica cualquier fuga
                    DataCatalogStorageV1 s_leak;
                    const auto c_leak = catalog_load_bytes_v1(bad.data(), bad.size(),
                                                              CatalogLoadProfile::Verified, s_leak);
                    CHECK(c_leak == CatalogLoadCode::InvalidUnit);
                    CHECK(!s_leak.valid());
                }
            }
        }
    }

    // NOTA (deviación documentada, ver RESULT del sprint): el invariante
    // "recompilar un YAML con un stat cambiado produce blob/hash distinto,
    // sin recompilar C++" ya lo prueba
    // tools/data_compile/test_data_compiler.py::test_determinism_reordering_and_semantic_change
    // (gate `data_compile`, registrado en CMakeLists.txt). No se duplicó
    // aquí para no acoplar este target C++ a invocar un intérprete Python en
    // tiempo de ctest.

    // 12) Sprint 1.1 (SPEC-004 §2): catálogo tipado de edificios.
    {
        // 12a) Golden REAL (data/buildings/, ya compilado por MiniMax + cierre
        // del Arquitecto): catalog_find_building resuelve los record_id del
        // Sprint 1.1 y los campos tipados coinciden con los YAML — incluido
        // egipto:settlement_center/rome:forum_center, que son
        // constructible:false + build_time_ticks:0 (enmienda del Arquitecto
        // 2026-07-23, SPEC-004 §4.1.2/§4.3: el loader debe ACEPTAR T==0).
        // Sprint 1.2 añade los cuarteles: el conteo crece con los datos (>= 4,
        // exacto 6 desde mm/datos-tech-1.2); los checks por record_id de abajo
        // son los que fijan el contrato, no el conteo.
        CHECK(cat.building_count == 6);

        auto find = [&](const char* name) {
            return catalog_find_building(cat, name, std::strlen(name));
        };
        const BuildingId settlement = find("egipto:settlement_center");
        const BuildingId shena = find("egipto:shena_granary");
        const BuildingId forum = find("rome:forum_center");
        const BuildingId horreum = find("rome:horreum");
        CHECK(settlement != INVALID_BUILDING_ID);
        CHECK(shena != INVALID_BUILDING_ID);
        CHECK(forum != INVALID_BUILDING_ID);
        CHECK(horreum != INVALID_BUILDING_ID);
        CHECK(find("nope:nope") == INVALID_BUILDING_ID);

        if (settlement != INVALID_BUILDING_ID) {
            const BuildingDefinitionV1& d = cat.buildings[settlement];
            CHECK(d.hp == 1500);
            CHECK(d.footprint_w == 3 && d.footprint_h == 3);
            CHECK(d.build_time_ticks == 0);   // nace completo (progress 0 >= T 0)
            CHECK(d.constructible == 0);
            CHECK(d.cost_a == 0 && d.cost_b == 0 && d.cost_me == 0);
            CHECK(d.dropoff_mask == 0x7u);    // A|B|Me
        }
        if (shena != INVALID_BUILDING_ID) {
            const BuildingDefinitionV1& d = cat.buildings[shena];
            CHECK(d.hp == 600);
            CHECK(d.footprint_w == 2 && d.footprint_h == 2);
            CHECK(d.build_time_ticks == 500);
            CHECK(d.constructible == 1);
            CHECK(d.cost_a == 0 && d.cost_b == 60 && d.cost_me == 0);
            CHECK(d.dropoff_mask == 0x1u);    // A
        }

        // 12b) Fixture propio (SPEC-004 §9.3): edificio válido carga y sus
        // campos tipados coinciden exactamente con el spec de entrada.
        {
            mini_chdb::BuildingSpec spec;
            const auto blob = mini_chdb::build(spec);
            DataCatalogStorageV1 s;
            const auto c = catalog_load_bytes_v1(blob.data(), blob.size(), CatalogLoadProfile::Verified, s);
            CHECK(c == CatalogLoadCode::Ok);
            if (c == CatalogLoadCode::Ok && s.valid()) {
                const DataCatalogV1& mc = s.catalog();
                CHECK(mc.building_count == 1);
                const BuildingId bid = catalog_find_building(mc, "test:bldg", std::strlen("test:bldg"));
                CHECK(bid == 0);
                if (bid != INVALID_BUILDING_ID) {
                    const BuildingDefinitionV1& d = mc.buildings[bid];
                    CHECK(d.hp == 500);
                    CHECK(d.footprint_w == 3 && d.footprint_h == 2);
                    CHECK(d.build_time_ticks == 30);
                    CHECK(d.constructible == 1);
                    CHECK(d.cost_a == 100 && d.cost_b == 0 && d.cost_me == 20);
                    CHECK(d.dropoff_mask == 0x1u);  // solo A
                }
            }
        }

        // 12c) Rechazo (SPEC-004 §9.3): hp==0 → InvalidBuilding.
        {
            mini_chdb::BuildingSpec spec;
            spec.hp = 0;
            const auto blob = mini_chdb::build(spec);
            DataCatalogStorageV1 s;
            const auto c = catalog_load_bytes_v1(blob.data(), blob.size(), CatalogLoadProfile::Verified, s);
            CHECK(c == CatalogLoadCode::InvalidBuilding);
            CHECK(!s.valid());
        }

        // 12d) Rechazo (SPEC-004 §9.3): footprint width=9 (fuera de 1..8) →
        // InvalidBuilding.
        {
            mini_chdb::BuildingSpec spec;
            spec.width = 9;
            const auto blob = mini_chdb::build(spec);
            DataCatalogStorageV1 s;
            const auto c = catalog_load_bytes_v1(blob.data(), blob.size(), CatalogLoadProfile::Verified, s);
            CHECK(c == CatalogLoadCode::InvalidBuilding);
            CHECK(!s.valid());
        }

        // 12e) Control positivo del build_time_ticks==0 vía el fixture propio
        // (no solo el golden real): constructible:false + T==0 debe ACEPTARSE
        // (enmienda §4.1.2), no confundirse con un rechazo de hp/footprint.
        {
            mini_chdb::BuildingSpec spec;
            spec.constructible = false;
            spec.build_time = 0;
            spec.cost_a = 0; spec.cost_b = 0; spec.cost_me = 0;
            const auto blob = mini_chdb::build(spec);
            DataCatalogStorageV1 s;
            const auto c = catalog_load_bytes_v1(blob.data(), blob.size(), CatalogLoadProfile::Verified, s);
            CHECK(c == CatalogLoadCode::Ok);
            if (c == CatalogLoadCode::Ok && s.valid()) {
                const BuildingId bid = catalog_find_building(s.catalog(), "test:bldg", std::strlen("test:bldg"));
                CHECK(bid != INVALID_BUILDING_ID);
                if (bid != INVALID_BUILDING_ID) {
                    CHECK(s.catalog().buildings[bid].build_time_ticks == 0);
                    CHECK(s.catalog().buildings[bid].constructible == 0);
                }
            }
        }
    }

    if (g_fails == 0) { std::printf("data_blob: OK\n"); return 0; }
    std::printf("data_blob: %d fallos\n", g_fails);
    return 1;
}
