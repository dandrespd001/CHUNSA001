#pragma once
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <new>

#include "chunsa/sha256.hpp"

// chunsa_sim_core — data_catalog: loader CHDB v1 NO CONFIABLE (SPEC-002 §§6-8).
// Sprint 0.4 (Sonnet 5, brief docs/briefs/SONNET_KERNEL_DATOS_SPEC002.md §2/§5).
//
// Contrato: el blob es entrada hostil. Antes de reservar memoria dependiente
// del archivo se validan tamaño, header, directorio (offsets/tamaños/orden,
// aritmética checked, sin solapamiento/gaps/trailing) y, por cada record, el
// CVE1 (Canonical Value Encoding v1) con caps de profundidad/nodos/colección/
// string ANTES de descender. Solo tras superar todo eso se reconstruyen los
// `UnitDefinitionV1` tipados y se recomputa el content hash. Ninguna excepción
// cruza la API pública: internamente se usa `LoadFail` para abortar temprano
// y `catalog_load_bytes_v1`/`catalog_load_file_v1` la atrapan en el borde,
// igual que cualquier bad_alloc (convertido a Bounds).
//
// Simplificaciones documentadas frente a SPEC-002 (ver RESULT del sprint):
//  - El loader valida estructuralmente TODAS las secciones (header, directorio,
//    CVE, orden de record_id) pero solo reconstruye tipado semántico completo
//    para UNIT (kind=2); building/tech/civ/map/ai-profile solo se validan
//    estructuralmente + su "id"/"package_id" para el orden canónico. La
//    validación semántica completa de esos kinds (referencias, ventanas de
//    época, etc.) ya la ejerce `chunsa_data_compiler.py` (gate `data_compile`);
//    duplicarla en C++ para kinds que el kernel 0.4 no consume es está fuera
//    de alcance de este incremento.
//  - NFC (revisado tras auditoría de seguridad, P1-B): se valida UTF-8 bien
//    formado, ausencia de NUL, y un chequeo NFC PARCIAL — se rechaza toda
//    marca diacrítica combinante suelta (U+0300–U+036F), que es la forma
//    NFD que el productor (Python `unicodedata.normalize`) jamás emite para
//    el texto real de este fixture (español/inglés con acentos precompuestos,
//    p.ej. "caballería"). NFC completo (tablas Unicode de descomposición/
//    composición canónica) no se implementa; ver el comentario de
//    `utf8_nfc_safe_no_nul` para el detalle de qué SÍ y qué NO cubre esta
//    verificación y por qué "rechazar todo no-ASCII" habría roto el golden
//    real (que tiene texto no-ASCII legítimo fuera de los campos que el
//    kernel tipa).
//  - Minimalidad CVE: al no existir en CVE1 dos codificaciones distintas para
//    el mismo valor (int64 de ancho fijo, strings con longitud explícita,
//    claves en orden estricto ya validado), la validación campo-a-campo basta;
//    no se implementó un segundo paso de re-encode/byte-compare.
//  - Fuga de memoria en fallo (P1-A, corregida): `load_impl` posee su
//    `Impl` con `std::unique_ptr` durante toda la validación y solo hace
//    `release()` en el único punto de éxito; cualquier `fail()` (incluidos
//    los de `build_unit_definition`/`cve_parse`) libera `impl` vía
//    unwinding en vez de fugarlo.

namespace chunsa {

// ============================================================================
// §2 del brief — API literal (tipos y firmas NO renombrables).
// ============================================================================

using UnitId = uint32_t;
inline constexpr UnitId INVALID_UNIT_ID = 0xFFFFFFFFu;

struct ContentHashV1 {
    uint8_t bytes[32];
};

enum class ContentHashAlgorithmId : uint16_t { Sha256 = 1 };

enum class UnitClassV1 : uint8_t {
    Infantry = 0,
    Cavalry = 1,
    Artillery = 2,
    Citizen = 3,
    Siege = 4,
    NavalLight = 5,
};

struct UnitDefinitionV1 {
    UnitId id;
    UnitClassV1 unit_class;
    uint8_t tags_mask;
    int32_t hp;
    int32_t attack;
    int32_t range_millitiles;
    int32_t speed_millitile_tick;
    int32_t morale;
    int32_t build_time_ticks;
    int32_t bonus_vs_bp[6];
    // Sprint 1.2 (SPEC-004 §11.1): costes de entrenamiento (de resource_costs,
    // ausente=0; solo A/B/Me — misma política que building Parte I) y
    // pop_cost (constante v1=1, NO viene de datos). §12.4: epoch_window
    // (mismo campo del schema que building; unit.schema.json NO declara
    // required_capabilities — el gate correspondiente pasa trivialmente
    // sobre el conjunto vacío, ver step.hpp/TRAIN_UNIT y RESULT del sprint).
    int32_t cost_a, cost_b, cost_me;
    int32_t pop_cost;
    uint8_t epoch_min, epoch_max;  // 1..15, epoch_min <= epoch_max
};

struct UnitNameIndexV1 {
    const char* record_id_utf8;
    uint16_t record_id_bytes;
    UnitId id;
};

// Sprint 1.1 (SPEC-004 §2): tabla tipada de edificios, API espejo de la de
// unidades. `id` == índice en `DataCatalogV1::buildings[]`.
using BuildingId = uint32_t;
inline constexpr BuildingId INVALID_BUILDING_ID = 0xFFFFFFFFu;

// Sprint 1.2 (SPEC-004 §11.1/§12.1): tipos de producción/tech. TechId/CapabilityId
// == índice en DataCatalogV1::techs[]/capability_names[] (mismo patrón que
// UnitId/BuildingId). Caps del kernel (PROD_*_MAX, TECH_*_MAX, TECH_HARD_CAP,
// CAP_HARD_CAP) son MÁS ESTRICTOS que los del schema/blob (65535) — mismo
// espíritu que el footprint 1..8 de BuildingDefinitionV1 (Parte I): un dato
// real que excediera estos caps se rechaza (catálogo entero), no se trunca.
using TechId = uint32_t;
inline constexpr TechId INVALID_TECH_ID = 0xFFFFFFFFu;
using CapabilityId = uint32_t;
inline constexpr CapabilityId INVALID_CAPABILITY_ID = 0xFFFFFFFFu;

inline constexpr uint32_t PROD_TRAINS_MAX = 8;
inline constexpr uint32_t PROD_TECHS_MAX = 8;
inline constexpr uint32_t BUILDING_REQCAP_MAX = 8;
inline constexpr uint32_t TECH_PREREQ_MAX = 4;
inline constexpr uint32_t TECH_GRANT_MAX = 4;
inline constexpr uint32_t TECH_MUTEX_MAX = 4;
// Caps duros del kernel (múltiplos de 64 a propósito: GameState::player_techs/
// player_caps los dimensiona en TECH_WORDS/CAP_WORDS palabras de 64 bits).
inline constexpr uint32_t TECH_HARD_CAP = 256;
inline constexpr uint32_t CAP_HARD_CAP = 256;

struct BuildingDefinitionV1 {
    BuildingId id;                 // == índice
    int32_t  hp;                   // > 0
    uint8_t  footprint_w;          // tiles, 1..8
    uint8_t  footprint_h;          // tiles, 1..8
    uint32_t build_time_ticks;     // >= 0 (enmienda del Arquitecto 2026-07-23,
                                   // SPEC-004 §4.1.2/§4.3: 0 = nace completo,
                                   // reservado a `constructible:false` de escenario)
    int32_t  cost_a, cost_b, cost_me;  // >= 0 (deducidos al aceptar PLACE_BUILDING)
    uint8_t  dropoff_mask;         // bit0=A bit1=B bit2=Me (de dropoff_resources)
    uint8_t  constructible;        // 0/1 (schema `constructible`)
    // Sprint 1.2 (SPEC-004 §11.1/§12.1/§12.4): epoch_window (mismo patrón que
    // unit); trains/researches resueltos desde los record_id del schema
    // (referencia no resoluble ⇒ catálogo rechazado); required_capabilities
    // resuelto contra la tabla de capacidades del blob (manifest.declared_
    // capabilities, ver DataCatalogV1::capability_names).
    uint8_t  epoch_min, epoch_max;
    UnitId   trains[PROD_TRAINS_MAX];
    uint8_t  train_count;
    TechId   researches[PROD_TECHS_MAX];
    uint8_t  research_count;
    CapabilityId required_capabilities[BUILDING_REQCAP_MAX];
    uint8_t  required_capabilities_count;
    // resto de campos del schema (recipes/grants_capabilities/...) NO tipados
};

struct BuildingNameIndexV1 {
    const char* record_id_utf8;
    uint16_t record_id_bytes;
    BuildingId id;
};

// Sprint 1.2 (SPEC-004 §12.1): tabla tipada de tecnologías. Las techs son
// PAQUETES DE CAPACIDAD en Parte II (base v1.1): sin efectos de stats — solo
// gatean contenido vía `grants` (CapabilityId) y el epoch-up (ADR-015).
struct TechDefinitionV1 {
    TechId id;
    int32_t cost_a, cost_b, cost_me;
    uint32_t research_time_ticks;  // >= 1
    uint8_t epoch;                 // 1..15
    TechId prerequisites[TECH_PREREQ_MAX];
    uint8_t prereq_count;
    CapabilityId grants[TECH_GRANT_MAX];
    uint8_t grant_count;
    TechId mutually_exclusive_with[TECH_MUTEX_MAX];
    uint8_t mutex_count;
    // required_buildings del schema NO se tipa aquí: el kernel gatea
    // RESEARCH_TECH por `tech ∈ BuildingDefinitionV1::researches` (relación
    // inversa, ver §11.1), no por este campo (deviación documentada en el
    // RESULT — el compilador Python sí valida required_buildings, el kernel
    // C++ no lo consume en Parte II).
};

struct TechNameIndexV1 {
    const char* record_id_utf8;
    uint16_t record_id_bytes;
    TechId id;
};

// Sprint 1.2 (SPEC-004 §12.1): tabla de capacidades declaradas del blob —
// espejo textual de manifest.declared_capabilities, reordenada bytewise
// ascendente POR EL LOADER (el orden de entrada en el blob no es bytewise —
// ver el comentario en `load_impl` sobre el criterio real de `_normalize`).
// CapabilityId == índice en esta tabla ya ordenada.
struct CapabilityNameIndexV1 {
    const char* record_id_utf8;
    uint16_t record_id_bytes;
    CapabilityId id;
};

struct DataCatalogV1 {
    ContentHashV1 content_hash;
    ContentHashAlgorithmId hash_algorithm;
    uint16_t hash_algorithm_version;
    uint16_t blob_format_major;
    uint16_t blob_format_minor;
    uint32_t schema_set_version;
    uint32_t catalog_flags;
    const char* base_package_id_utf8;
    uint16_t base_package_id_bytes;
    const uint8_t* content_binding_bytes;
    uint32_t content_binding_size;
    uint32_t unit_count;
    const UnitDefinitionV1* units;
    const UnitNameIndexV1* unit_names;
    // Sprint 1.1 (SPEC-004 §2): espejo de unit_count/units/unit_names.
    uint32_t building_count;
    const BuildingDefinitionV1* buildings;
    const BuildingNameIndexV1* building_names;
    // Sprint 1.2 (SPEC-004 §12.1): espejo de unit_count/units/unit_names, y
    // tabla de capacidades (manifest.declared_capabilities, sin definición
    // propia — solo nombre + índice).
    uint32_t tech_count;
    const TechDefinitionV1* techs;
    const TechNameIndexV1* tech_names;
    uint32_t capability_count;
    const CapabilityNameIndexV1* capability_names;
};

enum class CatalogLoadProfile : uint8_t { Verified = 0, Development = 1 };

enum class MatchLaunchPolicy : uint8_t {
    VerifiedRelease = 0,
    DeterministicModded = 1,
    Development = 2,
};

enum class CatalogLoadCode : uint8_t {
    Ok = 0, Io, TooLarge, BadMagic, UnsupportedVersion, UnknownFlags,
    UnverifiedForbidden, Bounds, NonCanonical, SchemaMismatch,
    InvalidUnit,
    InvalidBuilding,  // Sprint 1.1 (SPEC-004 §2); append-only, no renumerar.
    InvalidTech,      // Sprint 1.2 (SPEC-004 §12.1); append-only, no renumerar.
};

class DataCatalogStorageV1 {
public:
    DataCatalogStorageV1() noexcept;
    ~DataCatalogStorageV1() noexcept;
    DataCatalogStorageV1(const DataCatalogStorageV1&) = delete;
    DataCatalogStorageV1& operator=(const DataCatalogStorageV1&) = delete;
    DataCatalogStorageV1(DataCatalogStorageV1&&) noexcept;
    DataCatalogStorageV1& operator=(DataCatalogStorageV1&&) noexcept;
    bool valid() const noexcept;
    const DataCatalogV1& catalog() const noexcept;
    // Nested-type forward declaration expuesta (no privada) únicamente para que
    // la factoría interna `data_catalog_detail::load_impl` pueda nombrar el
    // tipo de retorno; su DEFINICIÓN completa y el puntero `impl_` que la
    // posee siguen siendo un detalle de implementación no accesible desde
    // fuera de este header (nadie más incluye/usa `Impl`).
    struct Impl;
private:
    Impl* impl_ = nullptr;
    friend CatalogLoadCode catalog_load_bytes_v1(
        const uint8_t*, size_t, CatalogLoadProfile, DataCatalogStorageV1&) noexcept;
    friend CatalogLoadCode catalog_load_file_v1(
        const char*, CatalogLoadProfile, DataCatalogStorageV1&) noexcept;
};

CatalogLoadCode catalog_load_bytes_v1(const uint8_t* bytes, size_t size,
                                      CatalogLoadProfile profile,
                                      DataCatalogStorageV1& out) noexcept;

CatalogLoadCode catalog_load_file_v1(const char* path,
                                     CatalogLoadProfile profile,
                                     DataCatalogStorageV1& out) noexcept;

// ============================================================================
// Detalle de implementación (no literal del brief).
// ============================================================================

namespace data_catalog_detail {

// Caps duros SPEC-002 §6.1, independientes de lo declarado por el archivo.
inline constexpr uint64_t HARD_MAX_CHDB_FILE_BYTES = 64ull * 1024 * 1024;
inline constexpr uint32_t HARD_MAX_CVE_DEPTH = 16;
inline constexpr uint32_t HARD_MAX_CVE_NODES_PER_RECORD = 262144;
inline constexpr uint32_t HARD_MAX_CVE_COLLECTION_ITEMS = 65535;
inline constexpr uint32_t HARD_MAX_CVE_STRING_BYTES = 65535;
inline constexpr uint32_t RECORD_PAYLOAD_CAP = 1u << 20;       // 1 MiB
inline constexpr uint32_t RECORD_PAYLOAD_CAP_MAP = 16u << 20;  // 16 MiB
inline constexpr size_t DIRECTORY_ENTRY_SIZE = 24;
inline constexpr size_t HEADER_SIZE = 40;
inline constexpr uint32_t SECTION_COUNT_D1 = 7;

// Control de flujo interno: nunca cruza la API pública (atrapado en el borde).
struct LoadFail { CatalogLoadCode code; };
[[noreturn]] inline void fail(CatalogLoadCode c) { throw LoadFail{c}; }

// Validación UTF-8 bien formada + ausencia de NUL + NFC PARCIAL (auditoría
// de seguridad post-integración, P1-B).
//
// El productor (`chunsa_data_compiler.py`, líneas ~503/537) rechaza
// cualquier string donde `unicodedata.normalize("NFC", s) != s`. Implementar
// NFC completo en C++ exigiría las tablas Unicode de descomposición/
// composición canónica + orden de combinación + composición algorítmica de
// Hangul — miles de code points, fuera de alcance de este ciclo de parche.
//
// Descarté "rechazar todo no-ASCII": el fixture D1 real SÍ tiene texto
// no-ASCII legítimo (p.ej. `rationale` de `egipto_chariot_warrior.yaml`:
// "la clase cavalry aproxima movilidad de carro, no CABALLERÍA montada" —
// el CVE genérico recorre TODOS los campos de TODAS las secciones, no solo
// `stats`/`class`/`tags`, así que un rechazo por-no-ASCII habría roto la
// carga del golden real con el MISMO content_hash que antes).
//
// Decisión: verificación NFC PARCIAL pero real, no un sucedáneo. La
// inmensa mayoría de las strings que NFC transformaría de verdad son
// secuencias DESCOMPUESTAS de letra base + marca diacrítica combinante
// (p.ej. 'e' U+0065 + COMBINING ACUTE ACCENT U+0301, que NFC compone en 'é'
// U+00E9). El productor (Python) SIEMPRE emite la forma precompuesta para
// este tipo de texto (español/inglés/latín transliterado); ninguna marca
// combinante suelta del bloque U+0300–U+036F (Combining Diacritical Marks)
// puede aparecer en salida NFC de este productor para estos scripts. Por
// eso: cualquier code point en U+0300–U+036F se rechaza como NO canónico
// (NonCanonical) — detecta exactamente el vector de ataque/divergencia
// realista (inyectar la forma NFD de un acento) sin romper el fixture real
// (que usa 'é'/'í'/'ñ' precompuestos, fuera de ese bloque).
//
// Gaps residuales documentados (aceptados para este ciclo, no en el
// fixture actual): (a) los ~12 "singleton" canónicos de Unicode (p.ej.
// U+2126 OHM SIGN → U+03A9) no se detectan — no son marcas combinantes y
// no aparecen en este dataset; (b) bloques de marcas combinantes fuera de
// U+0300–U+036F (Combining Diacritical Marks Supplement/Extended, marcas
// para símbolos, medias-marcas) no se cubren — relevantes para scripts que
// este fixture no usa; (c) Hangul descompuesto en jamos (algorítmico, no
// basado en marcas combinantes) no se detecta — irrelevante sin texto
// coreano. Sesgo deliberado: sobre-rechazar (falso rechazo de un NFC
// técnicamente válido pero exótico) es seguro; el riesgo que cierra esta
// auditoría es el opuesto (aceptar de más), y ese SÍ queda cerrado para el
// vector realista.
inline bool utf8_nfc_safe_no_nul(const std::string& s) noexcept {
    size_t i = 0, n = s.size();
    while (i < n) {
        const uint8_t c = static_cast<uint8_t>(s[i]);
        if (c == 0x00u) return false;
        if (c < 0x80u) { ++i; continue; }
        size_t extra;
        uint32_t cp;
        if ((c & 0xE0u) == 0xC0u) {
            if (c < 0xC2u) return false;  // overlong 2-byte
            extra = 1; cp = c & 0x1Fu;
        } else if ((c & 0xF0u) == 0xE0u) {
            extra = 2; cp = c & 0x0Fu;
        } else if ((c & 0xF8u) == 0xF0u) {
            if (c > 0xF4u) return false;
            extra = 3; cp = c & 0x07u;
        } else {
            return false;
        }
        if (i + extra >= n) return false;
        for (size_t k = 1; k <= extra; ++k) {
            const uint8_t cc = static_cast<uint8_t>(s[i + k]);
            if ((cc & 0xC0u) != 0x80u) return false;
            cp = (cp << 6) | (cc & 0x3Fu);
        }
        if (extra == 1 && cp < 0x80u) return false;
        if (extra == 2 && cp < 0x800u) return false;
        if (extra == 3 && cp < 0x10000u) return false;
        if (cp >= 0xD800u && cp <= 0xDFFFu) return false;
        if (cp > 0x10FFFFu) return false;
        // NFC parcial (P1-B, ver comentario arriba): rechaza marcas
        // diacríticas combinantes sueltas — la forma NFD de un acento que
        // el productor jamás emitiría.
        if (cp >= 0x0300u && cp <= 0x036Fu) return false;
        i += extra + 1;
    }
    return true;
}

// ---------------------------------------------------------------------------
// CveValue — árbol genérico mínimo para decodificar CVE1 (SPEC-002 §6.2).
// ---------------------------------------------------------------------------
struct CveValue {
    uint8_t tag = 0;              // 0x01 false, 0x02 true, 0x10 int, 0x20 str, 0x30 arr, 0x40 obj
    int64_t i = 0;
    std::string s;
    std::vector<CveValue> arr;
    std::vector<std::pair<std::string, CveValue>> obj;

    const CveValue* find(const char* key) const noexcept {
        for (const auto& kv : obj) if (kv.first == key) return &kv.second;
        return nullptr;
    }
    bool is_int() const noexcept { return tag == 0x10u; }
    bool is_str() const noexcept { return tag == 0x20u; }
    bool is_arr() const noexcept { return tag == 0x30u; }
    bool is_obj() const noexcept { return tag == 0x40u; }
};

// Cursor de lectura acotado; cualquier desbordamiento aborta vía LoadFail.
struct RawCursor {
    const uint8_t* p;
    size_t len;
    size_t pos = 0;

    size_t remaining() const noexcept { return len - pos; }

    uint8_t u8() {
        if (pos + 1 > len) fail(CatalogLoadCode::Bounds);
        return p[pos++];
    }
    uint16_t u16() {
        if (pos + 2 > len) fail(CatalogLoadCode::Bounds);
        uint16_t v = static_cast<uint16_t>(p[pos]) | (static_cast<uint16_t>(p[pos + 1]) << 8);
        pos += 2;
        return v;
    }
    uint32_t u32() {
        if (pos + 4 > len) fail(CatalogLoadCode::Bounds);
        uint32_t v = 0;
        for (int k = 0; k < 4; ++k) v |= static_cast<uint32_t>(p[pos + k]) << (8 * k);
        pos += 4;
        return v;
    }
    uint64_t u64() {
        if (pos + 8 > len) fail(CatalogLoadCode::Bounds);
        uint64_t v = 0;
        for (int k = 0; k < 8; ++k) v |= static_cast<uint64_t>(p[pos + k]) << (8 * k);
        pos += 8;
        return v;
    }
    int64_t i64() { return static_cast<int64_t>(u64()); }
    const uint8_t* take(size_t n) {
        if (n > remaining()) fail(CatalogLoadCode::Bounds);
        const uint8_t* r = p + pos;
        pos += n;
        return r;
    }
};

// Decodifica UN valor CVE1, con caps aplicados ANTES de reservar/iterar
// (SPEC-002 §6.2/§6.3, orden de validación del loader). `nodes` es el
// contador POR RECORD (se resetea en cada record, no es global al archivo).
inline CveValue cve_parse(RawCursor& c, uint32_t depth, uint32_t& nodes) {
    if (depth > HARD_MAX_CVE_DEPTH) fail(CatalogLoadCode::Bounds);
    if (++nodes > HARD_MAX_CVE_NODES_PER_RECORD) fail(CatalogLoadCode::Bounds);
    const uint8_t tag = c.u8();
    CveValue v;
    v.tag = tag;
    switch (tag) {
        case 0x01u: v.i = 0; return v;
        case 0x02u: v.i = 1; return v;
        case 0x10u: v.i = c.i64(); return v;
        case 0x20u: {
            const uint32_t n = c.u32();
            if (n > HARD_MAX_CVE_STRING_BYTES) fail(CatalogLoadCode::Bounds);
            const uint8_t* sp = c.take(n);
            v.s.assign(reinterpret_cast<const char*>(sp), n);
            if (!utf8_nfc_safe_no_nul(v.s)) fail(CatalogLoadCode::NonCanonical);
            return v;
        }
        case 0x30u: {
            const uint32_t n = c.u32();
            if (n > HARD_MAX_CVE_COLLECTION_ITEMS) fail(CatalogLoadCode::Bounds);
            // Mínimo 1 byte por elemento (tag false/true): valida contra los
            // bytes restantes ANTES de reservar el vector.
            if (static_cast<uint64_t>(n) > c.remaining()) fail(CatalogLoadCode::Bounds);
            v.arr.reserve(n);
            for (uint32_t k = 0; k < n; ++k) v.arr.push_back(cve_parse(c, depth + 1, nodes));
            return v;
        }
        case 0x40u: {
            const uint32_t n = c.u32();
            if (n > HARD_MAX_CVE_COLLECTION_ITEMS) fail(CatalogLoadCode::Bounds);
            // Mínimo por par: u32 key_len + 1 byte de valor = 5 bytes.
            if (n > 0 && static_cast<uint64_t>(n) > c.remaining() / 5u) fail(CatalogLoadCode::Bounds);
            v.obj.reserve(n);
            std::string last;
            bool has_last = false;
            for (uint32_t k = 0; k < n; ++k) {
                const uint32_t kl = c.u32();
                if (kl > HARD_MAX_CVE_STRING_BYTES) fail(CatalogLoadCode::Bounds);
                const uint8_t* kp = c.take(kl);
                std::string key(reinterpret_cast<const char*>(kp), kl);
                if (!utf8_nfc_safe_no_nul(key)) fail(CatalogLoadCode::NonCanonical);
                if (has_last && !(last < key)) fail(CatalogLoadCode::NonCanonical);
                last = key;
                has_last = true;
                CveValue val = cve_parse(c, depth + 1, nodes);
                v.obj.emplace_back(std::move(key), std::move(val));
            }
            return v;
        }
        default:
            fail(CatalogLoadCode::SchemaMismatch);
    }
    return v;  // inalcanzable; silencia -Wreturn-type
}

// ---------------------------------------------------------------------------
// Tablas de mapeo string→ordinal (SPEC-002 unit.schema.json, orden congelado).
// ---------------------------------------------------------------------------
inline bool unit_class_from_string(const std::string& s, UnitClassV1& out) noexcept {
    struct Entry { const char* name; UnitClassV1 v; };
    static constexpr Entry T[] = {
        {"infantry", UnitClassV1::Infantry}, {"cavalry", UnitClassV1::Cavalry},
        {"artillery", UnitClassV1::Artillery}, {"citizen", UnitClassV1::Citizen},
        {"siege", UnitClassV1::Siege}, {"naval_light", UnitClassV1::NavalLight},
    };
    for (const auto& e : T) if (s == e.name) { out = e.v; return true; }
    return false;
}

inline bool tag_bit_from_string(const std::string& s, uint8_t& bit) noexcept {
    static constexpr const char* T[] = {
        "can_take_cover", "formation_capable", "suppression_resist_low",
        "suppression_resist_high", "drop_off_carrier",
    };
    for (uint8_t k = 0; k < 5; ++k) if (s == T[k]) { bit = k; return true; }
    return false;
}

inline bool bonus_index_from_string(const std::string& s, size_t& idx) noexcept {
    static constexpr const char* T[] = {
        "infantry", "cavalry", "artillery", "citizen", "siege", "naval_light",
    };
    for (size_t k = 0; k < 6; ++k) if (s == T[k]) { idx = k; return true; }
    return false;
}

// P2 (auditoría de seguridad post-integración): el productor aplica
// `additionalProperties:false` (data/schemas/unit.schema.json); antes de
// este fix el loader ignoraba en silencio cualquier clave desconocida en
// vez de rechazarla como el productor. Estas dos listas son exactamente las
// claves declaradas en el schema para el objeto unit raíz y para `stats`.
inline bool is_known_unit_key(const std::string& k) noexcept {
    static constexpr const char* T[] = {
        "schema_version", "id", "display_name_key", "description_key", "civ_id",
        "epoch_window", "class", "tags", "resource_costs", "material_costs",
        "playable_period_ids", "availability_mode", "counterfactual_label_key",
        "stats", "bonus_vs_bp", "provenance",
    };
    for (const char* t : T) if (k == t) return true;
    return false;
}

inline bool is_known_stats_key(const std::string& k) noexcept {
    static constexpr const char* T[] = {
        "hp", "attack", "range_millitiles", "speed_millitile_tick", "morale",
        "build_time_ticks",
    };
    for (const char* t : T) if (k == t) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Sprint 1.2 (SPEC-004 §11.1/§12.1): helpers compartidos unit/building/tech.
// ---------------------------------------------------------------------------

// `epoch_window` (common.schema.json): array [min,max], ambos 1..15, min<=max.
// Compartido por unit y building (mismo `$ref` del schema).
inline void parse_epoch_window(const CveValue& obj, uint8_t& emin, uint8_t& emax,
                               CatalogLoadCode range_fail) {
    const CveValue* ew = obj.find("epoch_window");
    if (!ew || !ew->is_arr() || ew->arr.size() != 2) fail(CatalogLoadCode::SchemaMismatch);
    const CveValue& lo = ew->arr[0];
    const CveValue& hi = ew->arr[1];
    if (!lo.is_int() || !hi.is_int()) fail(CatalogLoadCode::SchemaMismatch);
    if (lo.i < 1 || lo.i > 15 || hi.i < 1 || hi.i > 15 || lo.i > hi.i) fail(range_fail);
    emin = static_cast<uint8_t>(lo.i);
    emax = static_cast<uint8_t>(hi.i);
}

// `resource_costs` (common.schema.json): objeto con hasta 8 claves de recurso;
// el kernel v1 solo rastrea A/B/Me (misma política que building Parte I —
// P/W/F/I/El se aceptan sin tipar). Ausencia de la clave ⇒ costes en 0
// (defensivo: el schema exige la clave del objeto en unit/building/tech, pero
// el loader no impone esa completitud — ese rigor ya lo ejerce el compilador).
inline void parse_resource_costs(const CveValue& obj, int32_t& cost_a, int32_t& cost_b,
                                 int32_t& cost_me, CatalogLoadCode range_fail) {
    cost_a = 0; cost_b = 0; cost_me = 0;
    const CveValue* costs = obj.find("resource_costs");
    if (!costs) return;
    if (!costs->is_obj()) fail(CatalogLoadCode::SchemaMismatch);
    for (const auto& kv : costs->obj) {
        if (!kv.second.is_int()) fail(CatalogLoadCode::SchemaMismatch);
        if (kv.second.i < 0 || kv.second.i > 1000000) fail(range_fail);
        if (kv.first == "A") cost_a = static_cast<int32_t>(kv.second.i);
        else if (kv.first == "B") cost_b = static_cast<int32_t>(kv.second.i);
        else if (kv.first == "Me") cost_me = static_cast<int32_t>(kv.second.i);
        // Otros recursos (P/W/F/I/El): fuera de alcance del kernel v1.
    }
}

// `record_id_set` (common.schema.json): array de strings de record_id. Se
// devuelven SIN resolver (el caller decide contra qué tabla y con qué cap —
// building.trains/researches/required_capabilities necesitan tablas que aún
// no existen en el momento en que se parsea el record building, ver el
// comentario de `load_impl` sobre resolución diferida de referencias).
inline std::vector<std::string> parse_string_array(const CveValue& obj, const char* key) {
    const CveValue* arr = obj.find(key);
    if (!arr || !arr->is_arr()) fail(CatalogLoadCode::SchemaMismatch);
    std::vector<std::string> out;
    out.reserve(arr->arr.size());
    for (const auto& item : arr->arr) {
        if (!item.is_str()) fail(CatalogLoadCode::SchemaMismatch);
        out.push_back(item.s);
    }
    return out;
}

// Búsqueda binaria bytewise sobre una tabla de record_id YA ascendente
// estricta (invariante que el loader ya verificó al parsear esa sección —
// std::string::operator< es equivalente a memcmp lexicográfico de los bytes
// UTF-8, mismo criterio de orden que `catalog_find_unit`/`catalog_find_building`).
inline bool resolve_id(const std::vector<std::string>& ids, const std::string& target,
                       uint32_t& out_index) noexcept {
    uint32_t lo = 0, hi = static_cast<uint32_t>(ids.size());
    while (lo < hi) {
        const uint32_t mid = lo + (hi - lo) / 2;
        if (ids[mid] == target) { out_index = mid; return true; }
        if (ids[mid] < target) lo = mid + 1; else hi = mid;
    }
    return false;
}

// Reconstruye y valida un UnitDefinitionV1 desde su objeto CVE ya parseado
// (SPEC-002 §8.1: rangos exactos del schema; ver data/schemas/unit.schema.json).
inline UnitDefinitionV1 build_unit_definition(const CveValue& obj, UnitId id) {
    if (!obj.is_obj()) fail(CatalogLoadCode::SchemaMismatch);
    for (const auto& kv : obj.obj) {
        if (!is_known_unit_key(kv.first)) fail(CatalogLoadCode::SchemaMismatch);
    }

    const CveValue* cls = obj.find("class");
    if (!cls || !cls->is_str()) fail(CatalogLoadCode::SchemaMismatch);
    UnitClassV1 uc{};
    if (!unit_class_from_string(cls->s, uc)) fail(CatalogLoadCode::InvalidUnit);

    const CveValue* tags = obj.find("tags");
    if (!tags || !tags->is_arr()) fail(CatalogLoadCode::SchemaMismatch);
    uint8_t tags_mask = 0;
    for (const auto& t : tags->arr) {
        if (!t.is_str()) fail(CatalogLoadCode::InvalidUnit);
        uint8_t bit = 0;
        if (!tag_bit_from_string(t.s, bit)) fail(CatalogLoadCode::InvalidUnit);
        tags_mask = static_cast<uint8_t>(tags_mask | (1u << bit));
    }

    const CveValue* stats = obj.find("stats");
    if (!stats || !stats->is_obj()) fail(CatalogLoadCode::SchemaMismatch);
    for (const auto& kv : stats->obj) {
        if (!is_known_stats_key(kv.first)) fail(CatalogLoadCode::SchemaMismatch);
    }
    auto req_int = [&](const char* key) -> int64_t {
        const CveValue* v = stats->find(key);
        if (!v || !v->is_int()) fail(CatalogLoadCode::SchemaMismatch);
        return v->i;
    };
    const int64_t hp = req_int("hp");
    const int64_t attack = req_int("attack");
    const int64_t range_mt = req_int("range_millitiles");
    const int64_t speed = req_int("speed_millitile_tick");
    const int64_t morale = req_int("morale");
    const int64_t build_time = req_int("build_time_ticks");

    if (hp < 1 || hp > 1000000) fail(CatalogLoadCode::InvalidUnit);
    if (attack < 0 || attack > 1000000) fail(CatalogLoadCode::InvalidUnit);
    if (range_mt < 0 || range_mt > 100000) fail(CatalogLoadCode::InvalidUnit);
    if (speed < 1 || speed > 100000) fail(CatalogLoadCode::InvalidUnit);
    if (morale < 0 || morale > 100) fail(CatalogLoadCode::InvalidUnit);
    if (build_time < 1 || build_time > 1000000) fail(CatalogLoadCode::InvalidUnit);

    UnitDefinitionV1 def{};
    def.id = id;
    def.unit_class = uc;
    def.tags_mask = tags_mask;
    def.hp = static_cast<int32_t>(hp);
    def.attack = static_cast<int32_t>(attack);
    def.range_millitiles = static_cast<int32_t>(range_mt);
    def.speed_millitile_tick = static_cast<int32_t>(speed);
    def.morale = static_cast<int32_t>(morale);
    def.build_time_ticks = static_cast<int32_t>(build_time);
    for (int k = 0; k < 6; ++k) def.bonus_vs_bp[k] = 0;

    // Sprint 1.2 (SPEC-004 §11.1/§12.1/§12.4): costes de entrenamiento, pop_cost
    // constante, epoch_window.
    parse_resource_costs(obj, def.cost_a, def.cost_b, def.cost_me, CatalogLoadCode::InvalidUnit);
    def.pop_cost = 1;  // constante v1, no viene de datos
    parse_epoch_window(obj, def.epoch_min, def.epoch_max, CatalogLoadCode::InvalidUnit);

    if (const CveValue* bonus = obj.find("bonus_vs_bp")) {
        if (!bonus->is_obj()) fail(CatalogLoadCode::SchemaMismatch);
        for (const auto& kv : bonus->obj) {
            size_t idx = 0;
            if (!bonus_index_from_string(kv.first, idx)) fail(CatalogLoadCode::InvalidUnit);
            if (!kv.second.is_int()) fail(CatalogLoadCode::SchemaMismatch);
            if (kv.second.i < -10000 || kv.second.i > 10000) fail(CatalogLoadCode::InvalidUnit);
            def.bonus_vs_bp[idx] = static_cast<int32_t>(kv.second.i);
        }
    }
    return def;
}

// ---------------------------------------------------------------------------
// Sprint 1.1 (SPEC-004 §2): tabla tipada de edificios.
// ---------------------------------------------------------------------------

// Mapea una string del "resource enum" (SPEC-002 common.schema.json) al bit de
// dropoff_mask que el kernel v1 rastrea (A=bit0, B=bit1, Me=bit2). Devuelve
// `false` SOLO si la string no pertenece al enum (dato corrupto); recursos
// fuera de A/B/Me (P/W/F/I/El) son válidos mas fuera de alcance en Parte I —
// se reconocen (no fallan la carga) pero no marcan ningún bit (`tracked=false`).
inline bool building_resource_bit_from_string(const std::string& s, uint8_t& bit, bool& tracked) noexcept {
    static constexpr const char* KNOWN[] = {"A", "B", "P", "W", "Me", "F", "I", "El"};
    bool valid = false;
    for (const char* k : KNOWN) if (s == k) { valid = true; break; }
    if (!valid) return false;
    tracked = true;
    if (s == "A") bit = 0;
    else if (s == "B") bit = 1;
    else if (s == "Me") bit = 2;
    else tracked = false;
    return true;
}

// Sprint 1.2 (SPEC-004 §11.1): referencias de building AÚN sin resolver a la
// hora de parsear el record (trains apunta a unit — resoluble en el momento;
// researches apunta a tech — sección posterior en el blob, kind=4 tras
// kind=3; required_capabilities apunta a la tabla de capacidades del
// manifest). `load_impl` resuelve LAS TRES en un único paso posterior, tras
// haber parseado TODAS las secciones, por uniformidad (ver su comentario).
struct BuildingRawRefs {
    std::vector<std::string> trains;
    std::vector<std::string> researches;
    std::vector<std::string> required_capabilities;
};

// Reconstruye y valida un BuildingDefinitionV1 desde su objeto CVE ya parseado
// (SPEC-004 §2: rangos exactos; ver data/schemas/building.schema.json). Mismo
// rigor que build_unit_definition, pero sin el gate "is_known_key" (la
// validación semántica completa de campos no tipados en Parte I —
// recipes/kind/civ_id/... — la sigue ejerciendo chunsa_data_compiler.py,
// igual que documenta el resto de kinds no-unit de este loader). Sprint 1.2
// (SPEC-004 §11.1/§12.1/§12.4) añade epoch_window y las tres listas de
// referencia (`raw_out`, sin resolver todavía).
inline BuildingDefinitionV1 build_building_definition(const CveValue& obj, BuildingId id,
                                                      BuildingRawRefs& raw_out) {
    if (!obj.is_obj()) fail(CatalogLoadCode::SchemaMismatch);

    const CveValue* footprint = obj.find("footprint");
    if (!footprint || !footprint->is_obj()) fail(CatalogLoadCode::SchemaMismatch);
    const CveValue* w = footprint->find("width_cells");
    const CveValue* h = footprint->find("height_cells");
    if (!w || !w->is_int() || !h || !h->is_int()) fail(CatalogLoadCode::SchemaMismatch);
    // SPEC-004 §2: footprint 1..8 tiles — más estricto que el 1..32 del schema
    // de datos (el kernel v1 restringe further; ver RESULT, desviación §D1).
    if (w->i < 1 || w->i > 8) fail(CatalogLoadCode::InvalidBuilding);
    if (h->i < 1 || h->i > 8) fail(CatalogLoadCode::InvalidBuilding);

    const CveValue* stats = obj.find("stats");
    if (!stats || !stats->is_obj()) fail(CatalogLoadCode::SchemaMismatch);
    const CveValue* hpv = stats->find("hp");
    if (!hpv || !hpv->is_int()) fail(CatalogLoadCode::SchemaMismatch);
    if (hpv->i < 1 || hpv->i > 10000000) fail(CatalogLoadCode::InvalidBuilding);

    const CveValue* constructible_v = obj.find("constructible");
    if (!constructible_v || !(constructible_v->tag == 0x01u || constructible_v->tag == 0x02u)) {
        fail(CatalogLoadCode::SchemaMismatch);
    }
    const uint8_t constructible = static_cast<uint8_t>(constructible_v->tag == 0x02u ? 1u : 0u);

    const CveValue* btv = obj.find("build_time_ticks");
    if (!btv || !btv->is_int()) fail(CatalogLoadCode::SchemaMismatch);
    // Enmienda del Arquitecto 2026-07-23 (SPEC-004 §4.1.2/§4.3): >= 0, no >= 1.
    // Los centros iniciales de escenario son `constructible:false` +
    // `build_time_ticks:0` (nacen completos: progress 0 >= T 0); el schema de
    // datos exige coste positivo solo para constructible:true, así que 0 es
    // legítimo aquí y NO es un caso especial para el loader.
    if (btv->i < 0 || btv->i > 10000000) fail(CatalogLoadCode::InvalidBuilding);

    int64_t cost_a = 0, cost_b = 0, cost_me = 0;
    if (const CveValue* costs = obj.find("resource_costs")) {
        if (!costs->is_obj()) fail(CatalogLoadCode::SchemaMismatch);
        for (const auto& kv : costs->obj) {
            if (!kv.second.is_int()) fail(CatalogLoadCode::SchemaMismatch);
            if (kv.second.i < 0 || kv.second.i > 1000000) fail(CatalogLoadCode::InvalidBuilding);
            if (kv.first == "A") cost_a = kv.second.i;
            else if (kv.first == "B") cost_b = kv.second.i;
            else if (kv.first == "Me") cost_me = kv.second.i;
            // Otros recursos del schema (P/W/F/I/El): fuera de alcance en
            // Parte I (economía kernel v1 solo rastrea A/B/Me); se aceptan
            // sin tipar, igual que trains/researches/recipes.
        }
    }

    uint8_t dropoff_mask = 0;
    if (const CveValue* dr = obj.find("dropoff_resources")) {
        if (!dr->is_arr()) fail(CatalogLoadCode::SchemaMismatch);
        for (const auto& item : dr->arr) {
            if (!item.is_str()) fail(CatalogLoadCode::SchemaMismatch);
            uint8_t bit = 0;
            bool tracked = false;
            if (!building_resource_bit_from_string(item.s, bit, tracked)) {
                fail(CatalogLoadCode::InvalidBuilding);
            }
            if (tracked) dropoff_mask = static_cast<uint8_t>(dropoff_mask | (1u << bit));
        }
    }

    BuildingDefinitionV1 def{};
    def.id = id;
    def.hp = static_cast<int32_t>(hpv->i);
    def.footprint_w = static_cast<uint8_t>(w->i);
    def.footprint_h = static_cast<uint8_t>(h->i);
    def.build_time_ticks = static_cast<uint32_t>(btv->i);
    def.cost_a = static_cast<int32_t>(cost_a);
    def.cost_b = static_cast<int32_t>(cost_b);
    def.cost_me = static_cast<int32_t>(cost_me);
    def.dropoff_mask = dropoff_mask;
    def.constructible = constructible;

    // Sprint 1.2 (SPEC-004 §11.1/§12.1/§12.4): epoch_window + listas de
    // referencia crudas (resueltas más tarde por load_impl).
    parse_epoch_window(obj, def.epoch_min, def.epoch_max, CatalogLoadCode::InvalidBuilding);
    raw_out.trains = parse_string_array(obj, "trains");
    raw_out.researches = parse_string_array(obj, "researches");
    raw_out.required_capabilities = parse_string_array(obj, "required_capabilities");
    def.train_count = 0;
    def.research_count = 0;
    def.required_capabilities_count = 0;
    for (uint32_t k = 0; k < PROD_TRAINS_MAX; ++k) def.trains[k] = INVALID_UNIT_ID;
    for (uint32_t k = 0; k < PROD_TECHS_MAX; ++k) def.researches[k] = INVALID_TECH_ID;
    for (uint32_t k = 0; k < BUILDING_REQCAP_MAX; ++k) def.required_capabilities[k] = INVALID_CAPABILITY_ID;

    return def;
}

// ---------------------------------------------------------------------------
// Sprint 1.2 (SPEC-004 §12.1): tabla tipada de tecnologías.
// ---------------------------------------------------------------------------

// Referencias de tech aún sin resolver (mismo motivo que BuildingRawRefs:
// prerequisites/mutually_exclusive_with son autorreferencias dentro de la
// MISMA sección tech, que puede citar un record_id alfabéticamente posterior
// — imposible de resolver mientras se recorre esa misma sección; grants.
// capabilities SÍ sería resoluble en el momento, pero se difiere igual por
// uniformidad, ver comentario de `load_impl`).
struct TechRawRefs {
    std::vector<std::string> prerequisites;
    std::vector<std::string> mutually_exclusive_with;
    std::vector<std::string> grants_capabilities;
};

// Reconstruye y valida un TechDefinitionV1 desde su objeto CVE ya parseado
// (SPEC-004 §12.1: rangos exactos; ver data/schemas/tech.schema.json). Mismo
// patrón que build_building_definition (sin gate is_known_key: available_to/
// branch/evidence/playable_period_ids/availability_mode/provenance/
// material_costs/regional_variant_group/required_buildings NO se tipan en
// Parte II — ver el comentario de TechDefinitionV1 sobre required_buildings).
inline TechDefinitionV1 build_tech_definition(const CveValue& obj, TechId id, TechRawRefs& raw_out) {
    if (!obj.is_obj()) fail(CatalogLoadCode::SchemaMismatch);

    const CveValue* epoch_v = obj.find("epoch");
    if (!epoch_v || !epoch_v->is_int()) fail(CatalogLoadCode::SchemaMismatch);
    if (epoch_v->i < 1 || epoch_v->i > 15) fail(CatalogLoadCode::InvalidTech);

    const CveValue* rtt = obj.find("research_time_ticks");
    if (!rtt || !rtt->is_int()) fail(CatalogLoadCode::SchemaMismatch);
    if (rtt->i < 1 || rtt->i > 10000000) fail(CatalogLoadCode::InvalidTech);

    TechDefinitionV1 def{};
    def.id = id;
    def.epoch = static_cast<uint8_t>(epoch_v->i);
    def.research_time_ticks = static_cast<uint32_t>(rtt->i);
    parse_resource_costs(obj, def.cost_a, def.cost_b, def.cost_me, CatalogLoadCode::InvalidTech);

    def.prereq_count = 0; def.grant_count = 0; def.mutex_count = 0;
    for (uint32_t k = 0; k < TECH_PREREQ_MAX; ++k) def.prerequisites[k] = INVALID_TECH_ID;
    for (uint32_t k = 0; k < TECH_GRANT_MAX; ++k) def.grants[k] = INVALID_CAPABILITY_ID;
    for (uint32_t k = 0; k < TECH_MUTEX_MAX; ++k) def.mutually_exclusive_with[k] = INVALID_TECH_ID;

    raw_out.prerequisites = parse_string_array(obj, "prerequisites");
    raw_out.mutually_exclusive_with = parse_string_array(obj, "mutually_exclusive_with");
    const CveValue* grants_obj = obj.find("grants");
    if (!grants_obj || !grants_obj->is_obj()) fail(CatalogLoadCode::SchemaMismatch);
    raw_out.grants_capabilities = parse_string_array(*grants_obj, "capabilities");

    return def;
}

}  // namespace data_catalog_detail

// ============================================================================
// Storage (Pimpl): posee la memoria estable a la que apuntan los `const char*`
// y punteros de `DataCatalogV1`. Definido tras el detalle porque necesita los
// tipos anteriores completos.
// ============================================================================
struct DataCatalogStorageV1::Impl {
    std::string package_id;
    std::vector<std::string> unit_ids;         // storage estable (reserve exacto antes de llenar)
    std::vector<UnitDefinitionV1> units;
    std::vector<UnitNameIndexV1> unit_names;
    // Sprint 1.1 (SPEC-004 §2): espejo de unit_ids/units/unit_names.
    std::vector<std::string> building_ids;
    std::vector<BuildingDefinitionV1> buildings;
    std::vector<BuildingNameIndexV1> building_names;
    // Sprint 1.2 (SPEC-004 §12.1): espejo de unit_ids/units/unit_names, y
    // tabla de capacidades (manifest.declared_capabilities).
    std::vector<std::string> tech_ids;
    std::vector<TechDefinitionV1> techs;
    std::vector<TechNameIndexV1> tech_names;
    std::vector<std::string> capability_ids;
    std::vector<CapabilityNameIndexV1> capability_names;
    // Referencias diferidas (resueltas tras parsear TODAS las secciones, ver
    // el comentario de `load_impl`); índice paralelo a buildings/techs.
    std::vector<std::vector<std::string>> pending_building_trains;
    std::vector<std::vector<std::string>> pending_building_researches;
    std::vector<std::vector<std::string>> pending_building_reqcaps;
    std::vector<std::vector<std::string>> pending_tech_prereqs;
    std::vector<std::vector<std::string>> pending_tech_mutex;
    std::vector<std::vector<std::string>> pending_tech_grants_caps;
    std::vector<uint8_t> binding_bytes;
    DataCatalogV1 cat{};
};

inline DataCatalogStorageV1::DataCatalogStorageV1() noexcept = default;

inline DataCatalogStorageV1::~DataCatalogStorageV1() noexcept { delete impl_; }

inline DataCatalogStorageV1::DataCatalogStorageV1(DataCatalogStorageV1&& o) noexcept
    : impl_(o.impl_) {
    o.impl_ = nullptr;
}

inline DataCatalogStorageV1& DataCatalogStorageV1::operator=(DataCatalogStorageV1&& o) noexcept {
    if (this != &o) {
        delete impl_;
        impl_ = o.impl_;
        o.impl_ = nullptr;
    }
    return *this;
}

inline bool DataCatalogStorageV1::valid() const noexcept { return impl_ != nullptr; }

inline const DataCatalogV1& DataCatalogStorageV1::catalog() const noexcept {
    // P2 (auditoría de seguridad post-integración): precondición explícita
    // `valid()` — un `assert` documenta y detecta en debug builds la
    // desreferencia de `impl_==nullptr` tras una carga fallida (contrato ya
    // exigía `valid()` antes de llamar; esto lo hace ruidoso en vez de UB
    // silencioso bajo NDEBUG=0). No sustituye la responsabilidad del caller.
    assert(impl_ != nullptr && "DataCatalogStorageV1::catalog(): precondición valid() violada");
    return impl_->cat;
}

namespace data_catalog_detail {

inline void push_u16(std::vector<uint8_t>& b, uint16_t v) {
    b.push_back(static_cast<uint8_t>(v & 0xFFu));
    b.push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
}

struct KindSpec { uint16_t kind; uint16_t version; uint32_t cap; };

// KIND_INFO (SPEC-002 §6.1): manifest, unit, building, tech, civ, map, ai-profile.
inline constexpr KindSpec kKindTable[7] = {
    {1, 1, 1}, {2, 2, 65535}, {3, 1, 65535}, {4, 1, 65535},
    {5, 1, 1024}, {6, 1, 1024}, {7, 1, 1024},
};

// Núcleo del loader: valida el blob completo (header→directorio→records) y
// construye un Impl* con el catálogo. Lanza LoadFail en cualquier violación;
// el caller (catalog_load_bytes_v1) atrapa esto y cualquier bad_alloc.
inline DataCatalogStorageV1::Impl* load_impl(const uint8_t* bytes, size_t size,
                                             CatalogLoadProfile profile) {
    if (bytes == nullptr) fail(CatalogLoadCode::Io);
    if (size > HARD_MAX_CHDB_FILE_BYTES) fail(CatalogLoadCode::TooLarge);
    if (size < HEADER_SIZE) fail(CatalogLoadCode::BadMagic);

    RawCursor c{bytes, size};

    // ---- Header fijo (40 bytes) --------------------------------------------
    const uint8_t* magic = c.take(8);
    static constexpr char kMagic[8] = {'C', 'H', 'N', 'S', 'D', 'B', '1', '\0'};
    if (std::memcmp(magic, kMagic, 8) != 0) fail(CatalogLoadCode::BadMagic);

    const uint16_t fmt_major = c.u16();
    const uint16_t fmt_minor = c.u16();
    const uint32_t schema_set = c.u32();
    if (fmt_major != 1 || fmt_minor != 0 || schema_set != 1) {
        fail(CatalogLoadCode::UnsupportedVersion);
    }

    const uint32_t flags = c.u32();
    if ((flags & ~0x1u) != 0u) fail(CatalogLoadCode::UnknownFlags);  // rechaza HAS_PATCHES (D1) y bits desconocidos
    const bool unverified = (flags & 0x1u) != 0u;
    if (unverified && profile == CatalogLoadProfile::Verified) {
        fail(CatalogLoadCode::UnverifiedForbidden);
    }

    const uint32_t section_count = c.u32();
    if (section_count != SECTION_COUNT_D1) fail(CatalogLoadCode::SchemaMismatch);

    const uint32_t entry_size = c.u32();
    if (entry_size != DIRECTORY_ENTRY_SIZE) fail(CatalogLoadCode::SchemaMismatch);

    const uint32_t reserved = c.u32();
    if (reserved != 0u) fail(CatalogLoadCode::SchemaMismatch);

    const uint64_t file_size = c.u64();
    if (file_size != static_cast<uint64_t>(size)) fail(CatalogLoadCode::Bounds);

    // ---- Directorio (7 × 24 bytes) ------------------------------------------
    struct DirEntry { uint16_t kind; uint16_t version; uint32_t count; uint64_t offset; uint64_t byte_size; };
    DirEntry dir[7];
    for (uint32_t k = 0; k < section_count; ++k) {
        DirEntry e{};
        e.kind = c.u16();
        e.version = c.u16();
        e.count = c.u32();
        e.offset = c.u64();
        e.byte_size = c.u64();
        const KindSpec& spec = kKindTable[k];
        if (e.kind != spec.kind || e.version != spec.version) fail(CatalogLoadCode::SchemaMismatch);
        if (e.count > spec.cap) fail(CatalogLoadCode::Bounds);
        if (spec.kind == 1 && e.count != 1) fail(CatalogLoadCode::SchemaMismatch);  // MANIFEST: exactamente 1
        dir[k] = e;
    }
    if (c.pos != HEADER_SIZE + static_cast<size_t>(section_count) * DIRECTORY_ENTRY_SIZE) {
        fail(CatalogLoadCode::Bounds);
    }
    const uint64_t directory_end = c.pos;
    if (dir[0].offset != directory_end) fail(CatalogLoadCode::Bounds);

    uint64_t cursor = directory_end;
    for (uint32_t k = 0; k < section_count; ++k) {
        if (dir[k].offset != cursor) fail(CatalogLoadCode::Bounds);
        if (dir[k].byte_size > static_cast<uint64_t>(size) - dir[k].offset) fail(CatalogLoadCode::Bounds);
        // Un record ocupa como mínimo 5 bytes (u32 payload_size + 1 byte de tag).
        if (dir[k].count > 0 && dir[k].count > dir[k].byte_size / 5u) fail(CatalogLoadCode::Bounds);
        cursor = dir[k].offset + dir[k].byte_size;
    }
    if (cursor != static_cast<uint64_t>(size)) fail(CatalogLoadCode::NonCanonical);  // trailing bytes

    // ---- Records por sección --------------------------------------------------
    // P1-A (auditoría de seguridad post-integración): `impl` se posee con
    // unique_ptr durante TODA la construcción. `fail()` lanza `LoadFail` en
    // cualquier punto posterior (incluidos los throws de
    // `build_unit_definition`/`cve_parse` llamados más abajo); antes de este
    // fix, esas rutas de error saltaban fuera de esta función con un `Impl*`
    // crudo sin liberar (fuga de varios MB por carga fallida — DoS no
    // acotado con blobs hostiles no triviales). El unwinding de C++ ahora
    // libera `impl` automáticamente en cualquier salida por excepción;
    // `impl.release()` se llama SOLO en el único `return` de éxito, al
    // final de la función.
    std::unique_ptr<DataCatalogStorageV1::Impl> impl(new DataCatalogStorageV1::Impl());
    // unit_ids necesita direcciones estables: reserve exacto ANTES de llenar
    // (evita relocación por SSO al hacer push_back más adelante).
    impl->unit_ids.reserve(dir[1].count);
    impl->units.reserve(dir[1].count);
    impl->unit_names.reserve(dir[1].count);
    // Sprint 1.1 (SPEC-004 §2): building_ids necesita direcciones estables,
    // mismo motivo que unit_ids. dir[2] es la sección building (kind=3).
    impl->building_ids.reserve(dir[2].count);
    impl->buildings.reserve(dir[2].count);
    impl->building_names.reserve(dir[2].count);
    impl->pending_building_trains.reserve(dir[2].count);
    impl->pending_building_researches.reserve(dir[2].count);
    impl->pending_building_reqcaps.reserve(dir[2].count);
    // Sprint 1.2 (SPEC-004 §12.1): tech_ids necesita direcciones estables,
    // mismo motivo. dir[3] es la sección tech (kind=4). TECH_HARD_CAP es un
    // cap del KERNEL más estricto que el cap 65535 del blob (mismo espíritu
    // que el footprint 1..8 de building en Parte I) — se rechaza aquí, antes
    // de reservar nada dependiente de un conteo desproporcionado.
    if (dir[3].count > TECH_HARD_CAP) fail(CatalogLoadCode::Bounds);
    impl->tech_ids.reserve(dir[3].count);
    impl->techs.reserve(dir[3].count);
    impl->tech_names.reserve(dir[3].count);
    impl->pending_tech_prereqs.reserve(dir[3].count);
    impl->pending_tech_mutex.reserve(dir[3].count);
    impl->pending_tech_grants_caps.reserve(dir[3].count);

    bool have_package_id = false;

    for (uint32_t k = 0; k < section_count; ++k) {
        const KindSpec& spec = kKindTable[k];
        RawCursor section{bytes + dir[k].offset, static_cast<size_t>(dir[k].byte_size)};
        std::string previous_id;
        bool has_previous = false;

        for (uint32_t r = 0; r < dir[k].count; ++r) {
            const uint32_t payload_size = section.u32();
            const uint32_t payload_cap = (spec.kind == 6) ? RECORD_PAYLOAD_CAP_MAP : RECORD_PAYLOAD_CAP;
            if (payload_size > payload_cap) fail(CatalogLoadCode::Bounds);
            const uint8_t* payload = section.take(payload_size);

            RawCursor precord{payload, payload_size};
            uint32_t nodes = 0;
            CveValue value = cve_parse(precord, 1, nodes);
            if (precord.pos != payload_size) fail(CatalogLoadCode::NonCanonical);  // trailing dentro del record
            if (!value.is_obj()) fail(CatalogLoadCode::SchemaMismatch);

            const char* id_key = (spec.kind == 1) ? "package_id" : "id";
            const CveValue* id_field = value.find(id_key);
            if (!id_field || !id_field->is_str()) fail(CatalogLoadCode::SchemaMismatch);
            const std::string& record_id = id_field->s;
            if (record_id.empty()) fail(CatalogLoadCode::SchemaMismatch);

            if (has_previous && !(previous_id < record_id)) fail(CatalogLoadCode::NonCanonical);
            previous_id = record_id;
            has_previous = true;

            if (spec.kind == 1) {
                if (record_id.size() < 1 || record_id.size() > 64) fail(CatalogLoadCode::SchemaMismatch);
                impl->package_id = record_id;
                have_package_id = true;

                // Sprint 1.2 (SPEC-004 §12.1): tabla de capacidades = manifest.
                // declared_capabilities. IMPORTANTE (hallazgo de integración):
                // el compilador Python (`_normalize`) ordena los "sets" del
                // schema (incluido `declared_capabilities`) por
                // `cve_encode(valor)`, NO por el string en sí — para strings,
                // `cve_encode` antepone la LONGITUD (u32 LE) a los bytes UTF-8,
                // así que el criterio real es "longitud primero, bytes después"
                // (verificado contra el blob real del repo: el orden que trae
                // NO es bytewise-ascendente por record_id). El loader, por
                // tanto, NO exige ni asume ningún orden de entrada aquí: solo
                // valida que sean strings, y construye la tabla ORDENÁNDOLA
                // él mismo — bytewise ascendente por record_id (mismo criterio
                // que `catalog_find_capability`/binary search) — para que la
                // tabla sea buscable con independencia del orden real del
                // blob. `record_id_set` del schema exige uniqueItems: un
                // duplicado tras ordenar (adyacentes iguales) se rechaza como
                // NonCanonical (dato corrupto/hostil, no debería ocurrir con
                // un compilador conforme al schema).
                const CveValue* caps = value.find("declared_capabilities");
                if (!caps || !caps->is_arr()) fail(CatalogLoadCode::SchemaMismatch);
                if (caps->arr.size() > CAP_HARD_CAP) fail(CatalogLoadCode::Bounds);
                impl->capability_ids.reserve(caps->arr.size());
                for (const auto& item : caps->arr) {
                    if (!item.is_str()) fail(CatalogLoadCode::SchemaMismatch);
                    impl->capability_ids.push_back(item.s);
                }
                std::sort(impl->capability_ids.begin(), impl->capability_ids.end());
                for (size_t ci = 1; ci < impl->capability_ids.size(); ++ci) {
                    if (!(impl->capability_ids[ci - 1] < impl->capability_ids[ci])) {
                        fail(CatalogLoadCode::NonCanonical);  // duplicado tras ordenar
                    }
                }
            } else if (spec.kind == 2) {
                if (record_id.size() > 0xFFFFu) fail(CatalogLoadCode::Bounds);
                const UnitId uid = static_cast<UnitId>(impl->units.size());
                UnitDefinitionV1 def = build_unit_definition(value, uid);
                impl->unit_ids.push_back(record_id);
                impl->units.push_back(def);
            } else if (spec.kind == 3) {
                // Sprint 1.1 (SPEC-004 §2): building, ahora tipado (antes solo
                // estructural). Mismo patrón que kind==2. Sprint 1.2 añade las
                // referencias crudas (trains/researches/required_capabilities),
                // resueltas más abajo tras parsear TODAS las secciones.
                if (record_id.size() > 0xFFFFu) fail(CatalogLoadCode::Bounds);
                const BuildingId bid = static_cast<BuildingId>(impl->buildings.size());
                BuildingRawRefs raw{};
                BuildingDefinitionV1 def = build_building_definition(value, bid, raw);
                impl->building_ids.push_back(record_id);
                impl->buildings.push_back(def);
                impl->pending_building_trains.push_back(std::move(raw.trains));
                impl->pending_building_researches.push_back(std::move(raw.researches));
                impl->pending_building_reqcaps.push_back(std::move(raw.required_capabilities));
            } else if (spec.kind == 4) {
                // Sprint 1.2 (SPEC-004 §12.1): tech, tipado. Mismo patrón.
                if (record_id.size() > 0xFFFFu) fail(CatalogLoadCode::Bounds);
                const TechId tid = static_cast<TechId>(impl->techs.size());
                TechRawRefs raw{};
                TechDefinitionV1 def = build_tech_definition(value, tid, raw);
                impl->tech_ids.push_back(record_id);
                impl->techs.push_back(def);
                impl->pending_tech_prereqs.push_back(std::move(raw.prerequisites));
                impl->pending_tech_mutex.push_back(std::move(raw.mutually_exclusive_with));
                impl->pending_tech_grants_caps.push_back(std::move(raw.grants_capabilities));
            }
            // civ/map/ai-profile: validados estructural + orden (deviación
            // documentada arriba); no se reconstruye tipo semántico.
        }
        if (section.pos != section.len) fail(CatalogLoadCode::NonCanonical);  // trailing de sección
    }

    if (!have_package_id) fail(CatalogLoadCode::SchemaMismatch);

    // ---- Sprint 1.2 (SPEC-004 §11.1/§12.1): resolución de referencias
    // diferidas. TODAS las tablas fuente (unit_ids/building_ids/tech_ids/
    // capability_ids) están completas en este punto (todas las secciones ya
    // se parsearon, incluida tech kind=4, que va DESPUÉS de building kind=3 en
    // el blob — por eso building.researches no podía resolverse en el mismo
    // pase que building.trains/required_capabilities). Cualquier referencia
    // no resoluble o que exceda el cap del kernel ⇒ catálogo entero rechazado.
    for (size_t bi = 0; bi < impl->buildings.size(); ++bi) {
        BuildingDefinitionV1& bd = impl->buildings[bi];

        const auto& trains_raw = impl->pending_building_trains[bi];
        if (trains_raw.size() > PROD_TRAINS_MAX) fail(CatalogLoadCode::InvalidBuilding);
        bd.train_count = static_cast<uint8_t>(trains_raw.size());
        for (size_t k = 0; k < trains_raw.size(); ++k) {
            uint32_t idx = 0;
            if (!resolve_id(impl->unit_ids, trains_raw[k], idx)) fail(CatalogLoadCode::InvalidBuilding);
            bd.trains[k] = static_cast<UnitId>(idx);
        }

        const auto& researches_raw = impl->pending_building_researches[bi];
        if (researches_raw.size() > PROD_TECHS_MAX) fail(CatalogLoadCode::InvalidBuilding);
        bd.research_count = static_cast<uint8_t>(researches_raw.size());
        for (size_t k = 0; k < researches_raw.size(); ++k) {
            uint32_t idx = 0;
            if (!resolve_id(impl->tech_ids, researches_raw[k], idx)) fail(CatalogLoadCode::InvalidBuilding);
            bd.researches[k] = static_cast<TechId>(idx);
        }

        const auto& reqcaps_raw = impl->pending_building_reqcaps[bi];
        if (reqcaps_raw.size() > BUILDING_REQCAP_MAX) fail(CatalogLoadCode::InvalidBuilding);
        bd.required_capabilities_count = static_cast<uint8_t>(reqcaps_raw.size());
        for (size_t k = 0; k < reqcaps_raw.size(); ++k) {
            uint32_t idx = 0;
            if (!resolve_id(impl->capability_ids, reqcaps_raw[k], idx)) fail(CatalogLoadCode::InvalidBuilding);
            bd.required_capabilities[k] = static_cast<CapabilityId>(idx);
        }
    }
    for (size_t ti = 0; ti < impl->techs.size(); ++ti) {
        TechDefinitionV1& td = impl->techs[ti];

        const auto& prereq_raw = impl->pending_tech_prereqs[ti];
        if (prereq_raw.size() > TECH_PREREQ_MAX) fail(CatalogLoadCode::InvalidTech);
        td.prereq_count = static_cast<uint8_t>(prereq_raw.size());
        for (size_t k = 0; k < prereq_raw.size(); ++k) {
            uint32_t idx = 0;
            if (!resolve_id(impl->tech_ids, prereq_raw[k], idx)) fail(CatalogLoadCode::InvalidTech);
            td.prerequisites[k] = static_cast<TechId>(idx);
        }

        const auto& mutex_raw = impl->pending_tech_mutex[ti];
        if (mutex_raw.size() > TECH_MUTEX_MAX) fail(CatalogLoadCode::InvalidTech);
        td.mutex_count = static_cast<uint8_t>(mutex_raw.size());
        for (size_t k = 0; k < mutex_raw.size(); ++k) {
            uint32_t idx = 0;
            if (!resolve_id(impl->tech_ids, mutex_raw[k], idx)) fail(CatalogLoadCode::InvalidTech);
            td.mutually_exclusive_with[k] = static_cast<TechId>(idx);
        }

        const auto& grants_raw = impl->pending_tech_grants_caps[ti];
        if (grants_raw.size() > TECH_GRANT_MAX) fail(CatalogLoadCode::InvalidTech);
        td.grant_count = static_cast<uint8_t>(grants_raw.size());
        for (size_t k = 0; k < grants_raw.size(); ++k) {
            uint32_t idx = 0;
            if (!resolve_id(impl->capability_ids, grants_raw[k], idx)) fail(CatalogLoadCode::InvalidTech);
            td.grants[k] = static_cast<CapabilityId>(idx);
        }
    }

    // ---- unit_names: mismo orden que units (ya ascendente por record_id) ----
    for (size_t i = 0; i < impl->units.size(); ++i) {
        UnitNameIndexV1 ni{};
        ni.record_id_utf8 = impl->unit_ids[i].c_str();
        ni.record_id_bytes = static_cast<uint16_t>(impl->unit_ids[i].size());
        ni.id = impl->units[i].id;
        impl->unit_names.push_back(ni);
    }

    // ---- building_names: mismo orden que buildings (Sprint 1.1) ----
    for (size_t i = 0; i < impl->buildings.size(); ++i) {
        BuildingNameIndexV1 ni{};
        ni.record_id_utf8 = impl->building_ids[i].c_str();
        ni.record_id_bytes = static_cast<uint16_t>(impl->building_ids[i].size());
        ni.id = impl->buildings[i].id;
        impl->building_names.push_back(ni);
    }

    // ---- tech_names: mismo orden que techs (Sprint 1.2) ----
    for (size_t i = 0; i < impl->techs.size(); ++i) {
        TechNameIndexV1 ni{};
        ni.record_id_utf8 = impl->tech_ids[i].c_str();
        ni.record_id_bytes = static_cast<uint16_t>(impl->tech_ids[i].size());
        ni.id = impl->techs[i].id;
        impl->tech_names.push_back(ni);
    }

    // ---- capability_names: mismo orden que capability_ids (Sprint 1.2) ----
    for (size_t i = 0; i < impl->capability_ids.size(); ++i) {
        CapabilityNameIndexV1 ni{};
        ni.record_id_utf8 = impl->capability_ids[i].c_str();
        ni.record_id_bytes = static_cast<uint16_t>(impl->capability_ids[i].size());
        ni.id = static_cast<CapabilityId>(i);
        impl->capability_names.push_back(ni);
    }

    // ---- content hash: SHA256("CHUNSA_CONTENT_V1\0" || bytes completos) ----
    static constexpr char kHashDomain[] = "CHUNSA_CONTENT_V1";
    ContentHashV1 hash{};
    {
        Sha256 h;
        h.init();
        h.update(kHashDomain, sizeof(kHashDomain));  // incluye el NUL final (§7.1)
        h.update(bytes, size);
        h.final(hash.bytes);
    }

    // ---- ContentBindingManifestV1 D1 (mode=0, sin patches) — SPEC-002 §7.1 --
    impl->binding_bytes.reserve(2 + 2 + 2 + impl->package_id.size() + 32);
    push_u16(impl->binding_bytes, 1u);  // binding_version
    push_u16(impl->binding_bytes, 0u);  // mode = single_package_d1
    push_u16(impl->binding_bytes, static_cast<uint16_t>(impl->package_id.size()));
    for (unsigned char ch : impl->package_id) impl->binding_bytes.push_back(ch);
    for (uint8_t b : hash.bytes) impl->binding_bytes.push_back(b);

    DataCatalogV1& cat = impl->cat;
    cat.content_hash = hash;
    cat.hash_algorithm = ContentHashAlgorithmId::Sha256;
    cat.hash_algorithm_version = 1;
    cat.blob_format_major = fmt_major;
    cat.blob_format_minor = fmt_minor;
    cat.schema_set_version = schema_set;
    cat.catalog_flags = flags;
    cat.base_package_id_utf8 = impl->package_id.c_str();
    cat.base_package_id_bytes = static_cast<uint16_t>(impl->package_id.size());
    cat.content_binding_bytes = impl->binding_bytes.data();
    cat.content_binding_size = static_cast<uint32_t>(impl->binding_bytes.size());
    cat.unit_count = static_cast<uint32_t>(impl->units.size());
    cat.units = impl->units.data();
    cat.unit_names = impl->unit_names.data();
    cat.building_count = static_cast<uint32_t>(impl->buildings.size());
    cat.buildings = impl->buildings.data();
    cat.building_names = impl->building_names.data();
    cat.tech_count = static_cast<uint32_t>(impl->techs.size());
    cat.techs = impl->techs.data();
    cat.tech_names = impl->tech_names.data();
    cat.capability_count = static_cast<uint32_t>(impl->capability_ids.size());
    cat.capability_names = impl->capability_names.data();

    // Único punto de éxito: transfiere la propiedad al caller. Cualquier
    // `fail()` anterior nunca llega aquí y el unique_ptr libera `impl` solo.
    return impl.release();
}

}  // namespace data_catalog_detail

inline CatalogLoadCode catalog_load_bytes_v1(const uint8_t* bytes, size_t size,
                                             CatalogLoadProfile profile,
                                             DataCatalogStorageV1& out) noexcept {
    out = DataCatalogStorageV1{};
    try {
        DataCatalogStorageV1::Impl* impl = data_catalog_detail::load_impl(bytes, size, profile);
        out.impl_ = impl;
        return CatalogLoadCode::Ok;
    } catch (const data_catalog_detail::LoadFail& lf) {
        return lf.code;
    } catch (const std::bad_alloc&) {
        return CatalogLoadCode::Bounds;
    } catch (...) {
        return CatalogLoadCode::Bounds;
    }
}

inline CatalogLoadCode catalog_load_file_v1(const char* path,
                                            CatalogLoadProfile profile,
                                            DataCatalogStorageV1& out) noexcept {
    out = DataCatalogStorageV1{};
    if (path == nullptr) return CatalogLoadCode::Io;
    std::FILE* f = std::fopen(path, "rb");
    if (!f) return CatalogLoadCode::Io;
    if (std::fseek(f, 0, SEEK_END) != 0) { std::fclose(f); return CatalogLoadCode::Io; }
    const long sz_signed = std::ftell(f);
    if (sz_signed < 0) { std::fclose(f); return CatalogLoadCode::Io; }
    if (static_cast<uint64_t>(sz_signed) > data_catalog_detail::HARD_MAX_CHDB_FILE_BYTES) {
        std::fclose(f);
        return CatalogLoadCode::TooLarge;
    }
    if (std::fseek(f, 0, SEEK_SET) != 0) { std::fclose(f); return CatalogLoadCode::Io; }
    try {
        std::vector<uint8_t> buf(static_cast<size_t>(sz_signed));
        const size_t rd = buf.empty() ? 0 : std::fread(buf.data(), 1, buf.size(), f);
        std::fclose(f);
        if (rd != buf.size()) return CatalogLoadCode::Io;
        return catalog_load_bytes_v1(buf.data(), buf.size(), profile, out);
    } catch (const std::bad_alloc&) {
        std::fclose(f);
        return CatalogLoadCode::Bounds;
    }
}

// ----------------------------------------------------------------------------
// Helper adicional (NO literal del brief): resuelve un record_id textual a
// UnitId por búsqueda binaria en `unit_names` (ya ordenado ascendente por
// construcción). Uso previsto: setup fuera de Step() (demo/CLI/tests), nunca
// dentro del tick caliente. Devuelve INVALID_UNIT_ID si no existe.
// ----------------------------------------------------------------------------
inline UnitId catalog_find_unit(const DataCatalogV1& cat, const char* name, size_t name_len) noexcept {
    uint32_t lo = 0, hi = cat.unit_count;
    while (lo < hi) {
        const uint32_t mid = lo + (hi - lo) / 2;
        const UnitNameIndexV1& e = cat.unit_names[mid];
        const size_t n = (e.record_id_bytes < name_len) ? e.record_id_bytes : name_len;
        int c = (n == 0) ? 0 : std::memcmp(e.record_id_utf8, name, n);
        if (c == 0 && e.record_id_bytes == name_len) return e.id;
        if (c < 0 || (c == 0 && e.record_id_bytes < name_len)) lo = mid + 1;
        else hi = mid;
    }
    return INVALID_UNIT_ID;
}

// Sprint 1.1 (SPEC-004 §2): espejo de catalog_find_unit para BuildingId.
inline BuildingId catalog_find_building(const DataCatalogV1& cat, const char* name, size_t name_len) noexcept {
    uint32_t lo = 0, hi = cat.building_count;
    while (lo < hi) {
        const uint32_t mid = lo + (hi - lo) / 2;
        const BuildingNameIndexV1& e = cat.building_names[mid];
        const size_t n = (e.record_id_bytes < name_len) ? e.record_id_bytes : name_len;
        int c = (n == 0) ? 0 : std::memcmp(e.record_id_utf8, name, n);
        if (c == 0 && e.record_id_bytes == name_len) return e.id;
        if (c < 0 || (c == 0 && e.record_id_bytes < name_len)) lo = mid + 1;
        else hi = mid;
    }
    return INVALID_BUILDING_ID;
}

// Sprint 1.2 (SPEC-004 §12.1): espejo de catalog_find_unit para TechId.
inline TechId catalog_find_tech(const DataCatalogV1& cat, const char* name, size_t name_len) noexcept {
    uint32_t lo = 0, hi = cat.tech_count;
    while (lo < hi) {
        const uint32_t mid = lo + (hi - lo) / 2;
        const TechNameIndexV1& e = cat.tech_names[mid];
        const size_t n = (e.record_id_bytes < name_len) ? e.record_id_bytes : name_len;
        int c = (n == 0) ? 0 : std::memcmp(e.record_id_utf8, name, n);
        if (c == 0 && e.record_id_bytes == name_len) return e.id;
        if (c < 0 || (c == 0 && e.record_id_bytes < name_len)) lo = mid + 1;
        else hi = mid;
    }
    return INVALID_TECH_ID;
}

// Sprint 1.2 (SPEC-004 §12.1): espejo de catalog_find_unit para CapabilityId.
inline CapabilityId catalog_find_capability(const DataCatalogV1& cat, const char* name, size_t name_len) noexcept {
    uint32_t lo = 0, hi = cat.capability_count;
    while (lo < hi) {
        const uint32_t mid = lo + (hi - lo) / 2;
        const CapabilityNameIndexV1& e = cat.capability_names[mid];
        const size_t n = (e.record_id_bytes < name_len) ? e.record_id_bytes : name_len;
        int c = (n == 0) ? 0 : std::memcmp(e.record_id_utf8, name, n);
        if (c == 0 && e.record_id_bytes == name_len) return e.id;
        if (c < 0 || (c == 0 && e.record_id_bytes < name_len)) lo = mid + 1;
        else hi = mid;
    }
    return INVALID_CAPABILITY_ID;
}

}  // namespace chunsa
