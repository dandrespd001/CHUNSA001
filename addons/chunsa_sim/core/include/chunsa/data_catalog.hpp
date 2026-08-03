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
#include "chunsa/resources.hpp"
#include "chunsa/combat_damage.hpp"
// Sprint 1.6B (SPEC-004 §16): conversión mt→raw de resource_spawns (FX_ONE_RAW)
// y el cap kernel de depósitos por mapa (ECO_MAX_DEPOSITS) — single source of
// truth en vez de duplicar el número mágico 32/65536 en este loader. Ninguno
// de los dos módulos incluye de vuelta data_catalog.hpp (sin ciclo).
#include "chunsa/fixed64.hpp"
#include "chunsa/economy.hpp"

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
//    para UNIT/BUILDING/TECH/AI-PROFILE; CIV (kind=5) solo aporta su tabla
//    nombre-índice (CivNameIndexV1, Sprint 1.6B — sin definición propia) y
//    MAP (kind=6) solo tipa `resource_spawns` del primer record ("mapa
//    activo", Sprint 1.6B, SPEC-004 §16) — el resto de sus campos, y el resto
//    de campos no listados de unit/building/tech/ai-profile, solo se validan
//    estructuralmente + su "id"/"package_id" para el orden canónico. La
//    validación semántica completa de esos campos (referencias, ventanas de
//    época, etc.) ya la ejerce `chunsa_data_compiler.py` (gate `data_compile`);
//    duplicarla en C++ para lo que el kernel no consume está fuera de alcance.
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

// Sprint 1.6B (SPEC-004 §15.3): identidad de civilización tipada. CivId ==
// índice en DataCatalogV1::civ_names[] (mismo patrón que UnitId/BuildingId/
// TechId/AiProfileId — todas las tablas de nombre-índice de este loader
// comparten la convención "id == posición en la tabla ya ordenada
// ascendente por record_id"). CivNameIndexV1 es una tabla MÍNIMA (solo
// record_id + índice, sin reconstrucción de definición propia — igual que
// CapabilityNameIndexV1): el kernel v1 no consume ningún otro campo del
// record civ (historical_window/epoch_window/institutions/playable_periods/
// unit_ids/building_ids/tech_ids/...) — esa validación semántica completa
// (incluidas las referencias cruzadas civ↔unit/building/tech del propio
// record civ) la sigue ejerciendo chunsa_data_compiler.py, igual que
// documenta el resto de kinds no tipados de este loader (civ/map antes de
// este sprint). Lo que SÍ tipa este sprint es la referencia INVERSA:
// unit.civ_id/building.civ_id/tech.available_to, resueltos contra esta
// tabla (ver UnitDefinitionV1/BuildingDefinitionV1/TechDefinitionV1 más abajo).
using CivId = uint32_t;
inline constexpr CivId INVALID_CIV_ID = 0xFFFFFFFFu;

struct CivNameIndexV1 {
    const char* record_id_utf8;
    uint16_t record_id_bytes;
    CivId id;
};

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
    // Sprint 1.18 (SPEC-004 Parte VI): armadura PLANA por tipo de dano y tipo
    // de arma propio. Antes no existia armadura ninguna: un aldeano y un
    // legionario recibian lo mismo de la misma flecha.
    int32_t armor[DAMAGE_TYPE_COUNT];
    DamageTypeV1 attack_type;
    // Sprint 1.8A (SPEC-007 §9.3/§11): vector completo de costes (de
    // resource_costs, ausente=0). A/B/Me conservan los índices 0/1/2 y
    // 3..31 quedan en cero hasta que existan datos de recursos ampliados.
    // Sprint 1.2: pop_cost
    // pop_cost (constante v1=1, NO viene de datos). §12.4: epoch_window
    // (mismo campo del schema que building; unit.schema.json NO declara
    // required_capabilities — el gate correspondiente pasa trivialmente
    // sobre el conjunto vacío, ver step.hpp/TRAIN_UNIT y RESULT del sprint).
    int32_t cost[RESOURCE_COUNT];
    int32_t pop_cost;
    uint8_t epoch_min, epoch_max;  // 1..15, epoch_min <= epoch_max
    // Sprint 1.6B (SPEC-004 §15.3/§17): civ_id resuelto (unit.schema.json lo
    // declara `required`, referencia record_id a un record kind=civ) —
    // referencia diferida (la sección civ, kind=5, va DESPUÉS de unit en el
    // blob); resolución en el mismo paso que building.trains/tech.prereqs,
    // ver load_impl. No resoluble ⇒ catálogo entero rechazado (InvalidUnit).
    CivId civ_id;
};

struct UnitNameIndexV1 {
    const char* record_id_utf8;
    uint16_t record_id_bytes;
    UnitId id;
};

// Sprint 1.8C (SPEC-007 §9.2 / SPEC-006 §13): tabla tipada de recursos.
// ResourceId == posición del record en resources[] (orden bytewise por
// record_id, como UnitId/BuildingId/TechId); `ResourceDefinitionV1::index`
// es el slot de stock independiente que asigna el compilador.
using ResourceId = uint32_t;
inline constexpr ResourceId INVALID_RESOURCE_ID = 0xFFFFFFFFu;

enum class ResourceFamilyV1 : uint8_t {
    Invalid = 0,
    Subsistence,
    Construction,
    BaseMetals,
    Metallurgy,
    Chemistry,
    Energy,
    HighTech,
    // Sprint 1.9C: append-only. Anadir en medio renumeraria las familias ya
    // grabadas en el blob y en el HUD.
    Textiles,
};

enum class ResourceNatureV1 : uint8_t {
    Invalid = 0,
    Collected,
    Produced,
};

struct ResourceDefinitionV1 {
    uint8_t index;
    const char* display_name_key_utf8;
    uint16_t display_name_key_bytes;
    ResourceFamilyV1 family;
    uint8_t appearance_epoch;
    ResourceNatureV1 nature;
};

struct ResourceNameIndexV1 {
    const char* record_id_utf8;
    uint16_t record_id_bytes;
    ResourceId id;
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

// Sprint 1.9 (SPEC-007 §12). DESVIACION DEL SPEC, deliberada y documentada:
// §12.2 pedia una SECCION NUEVA del blob (kind=9) para las recetas. No hace
// falta: `building.schema.json` YA declara `recipes` con todos los campos
// (id, input_resource_costs, output_resource_id, output_amount,
// duration_ticks), asi que la receta viaja dentro del record del edificio y el
// compilador de datos NO se toca. El RecipeId global se asigna al cargar,
// recorriendo edificios en orden de id y sus recetas en orden de aparicion:
// determinista y estable, que es lo que el comando CRAFT necesita.
using RecipeId = uint32_t;
inline constexpr RecipeId INVALID_RECIPE_ID = 0xFFFFFFFFu;
inline constexpr uint32_t RECIPES_PER_BUILDING_MAX = 8;

struct RecipeV1 {
    RecipeId id;
    BuildingId building_id;
    int32_t input[RESOURCE_COUNT];
    uint8_t output_index;      // indice de recurso de salida
    int32_t output_amount;     // >= 1
    uint32_t duration_ticks;   // >= 1
};
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
    int32_t  cost[RESOURCE_COUNT];  // >= 0 (deducidos al aceptar PLACE_BUILDING)
    uint64_t dropoff_mask;          // un bit por índice de recurso (64 desde 1.9C)
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
    // Sprint 1.6B (SPEC-004 §15.3/§17): civ_id resuelto (building.schema.json
    // lo declara `required`), misma disciplina de referencia diferida que
    // trains/researches/required_capabilities — ver load_impl.
    CivId civ_id;
    // Sprint 1.14 (SPEC-004 §11.3): poblacion que aporta este edificio cuando
    // esta COMPLETO. 0 = no es vivienda. El tipo `housing` existia en
    // building.schema.json desde el 1.1 y ninguna parte del kernel lo miraba;
    // este campo es lo que le da consecuencia.
    int32_t population_provided;
    // Sprint 1.9 (SPEC-007 §12): recetas que ejecuta este edificio, como
    // RecipeId hacia la tabla plana del catalogo.
    RecipeId recipes[RECIPES_PER_BUILDING_MAX];
    uint8_t  recipe_count;
    // Sprint 1.18: los edificios tambien tienen armadura por tipo (no atacan,
    // asi que no llevan attack_type).
    int32_t  armor[DAMAGE_TYPE_COUNT];
    // Deposito que este edificio CREA al completarse (nucleo de depositos
    // regenerativos). OPCIONAL con defecto 0, mismo patron que
    // population_provided (Sprint 1.14): `creates_amount == 0` significa
    // "este edificio no crea deposito", el caso de los edificios existentes
    // que no declaran estos campos y deben seguir cargando.
    // `creates_resource_idx` es el indice de recurso (0=comida, 1=madera, ...)
    // validado contra RESOURCE_COUNT.
    uint8_t  creates_resource_idx;
    int32_t  creates_amount;
    int32_t  creates_regen_per_tick;
    int32_t  creates_cap;
    // resto de campos del schema (grants_capabilities/...) NO tipados
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
    int32_t cost[RESOURCE_COUNT];
    uint32_t research_time_ticks;  // >= 1
    uint8_t epoch;                 // 1..15
    TechId prerequisites[TECH_PREREQ_MAX];
    uint8_t prereq_count;
    CapabilityId grants[TECH_GRANT_MAX];
    uint8_t grant_count;
    TechId mutually_exclusive_with[TECH_MUTEX_MAX];
    uint8_t mutex_count;
    // Sprint 1.18 §28: efectos sobre estadisticas (herreria). Opcionales: una
    // tech sin `stat_effects` sigue siendo valida y no suma nada.
    TechEffectV1 stat_effects[TECH_EFFECTS_MAX];
    uint8_t stat_effect_count;
    // required_buildings del schema NO se tipa aquí: el kernel gatea
    // RESEARCH_TECH por `tech ∈ BuildingDefinitionV1::researches` (relación
    // inversa, ver §11.1), no por este campo (deviación documentada en el
    // RESULT — el compilador Python sí valida required_buildings, el kernel
    // C++ no lo consume en Parte II).
    // Sprint 1.6B (SPEC-004 §15.3/§17, DESVIACIÓN D1 del RESULT — conservador
    // ante un hueco de contrato): tech.schema.json NO declara un `civ_id`
    // escalar como unit/building — declara `available_to` (record_id_set de
    // civs, potencialmente MÚLTIPLES). El literal de §15.3 contrata un único
    // `CivId civ_id` también para TechDefinitionV1; para conciliarlo con el
    // schema real sin inventar semántica no especificada, el kernel v1 SOLO
    // soporta techs de UNA civ: `civ_id` es el único elemento de
    // `available_to` si `available_to.size()==1`; si tiene 0 o >1 elementos,
    // el catálogo entero se rechaza (InvalidTech) — ver load_impl. Las 4
    // techs reales del repo (Sprint 1.2) ya tienen exactamente 1 elemento en
    // `available_to`, así que esto no afecta al golden actual.
    CivId civ_id;
};

struct TechNameIndexV1 {
    const char* record_id_utf8;
    uint16_t record_id_bytes;
    TechId id;
};

// Sprint 1.4 (SPEC-005 §3): tabla tipada de perfiles de IA. Espejo EXACTO
// del patrón endurecido de TechDefinitionV1 (unique_ptr, reserve, rechazo
// del catálogo entero, catalog_find_ai_profile bytewise) — sin referencias
// cruzadas a otras secciones (a diferencia de building/tech): todos sus
// campos son valores inline del propio record `kind=ai-profile`. El blob NO
// cambia de formato (el record ya viaja desde antes; solo se tipifica).
using AiProfileId = uint32_t;
inline constexpr AiProfileId INVALID_AI_PROFILE_ID = 0xFFFFFFFFu;

// Literal de SPEC-005 §3: pesos estratégicos `_bp` (basis points 0..10000,
// ver strategic_weights_bp del schema — `diplomacy_openness_bp` NO se tipa
// aquí, fuera de alcance v1/1v1 según SPEC-005 §10), un subconjunto de
// difficulty_params, y tactical_behaviors[0] (v1 usa el primero — ver §3).
struct AiProfileV1 {
    AiProfileId id;
    int32_t economy_focus_bp, military_focus_bp, tech_focus_bp,
            expansion_aggressiveness_bp, risk_tolerance_bp;
    uint32_t decision_period_ticks;
    uint32_t reaction_latency_ticks;
    int32_t retreat_hp_threshold_bp, retreat_morale_threshold_bp;
};

struct AiProfileNameIndexV1 {
    const char* record_id_utf8;
    uint16_t record_id_bytes;
    AiProfileId id;
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

// Sprint 1.6B (SPEC-004 §16): depósito de recurso tipado desde el mapa
// activo (`resource_spawns` del record kind=map, schema map.schema.json).
// "Mapa activo" = el PRIMER record map (record_id ascendente, ya validado
// por el chequeo genérico de orden) del catálogo — el slice real trae
// EXACTAMENTE 1 mapa; el loader no implementa mecanismo de selección
// multi-mapa (fuera de alcance de este sprint, ver comentario de load_impl
// y el RESULT — desviación D2). `resource_idx` 0=A/1=B/2=Me; el orden del
// array es el orden canónico del blob (el compilador Python ya reordena
// `resource_spawns` por (y,x,kind,id,amount) antes de escribirlo — ver
// `_normalize` en chunsa_data_compiler.py — así que el loader NO reordena,
// solo consume tal cual).
struct ResourceSpawnV1 {
    uint8_t resource_idx;  // 0=A, 1=B, 2=Me
    int64_t x_raw, y_raw;  // raw = x_millitiles * FX_ONE_RAW / 1000 (exacto en enteros)
    int32_t amount;
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
    // Sprint 1.4 (SPEC-005 §3): espejo de unit_count/units/unit_names.
    uint32_t ai_profile_count;
    const AiProfileV1* ai_profiles;
    const AiProfileNameIndexV1* ai_profile_names;
    // Sprint 1.9 (SPEC-007 §12): tabla plana de recetas, indexada por RecipeId.
    uint32_t recipe_count;
    const RecipeV1* recipes;
    // Sprint 1.6B (SPEC-004 §15.3): tabla de civilizaciones (kind=civ),
    // espejo mínimo de unit_count/units/unit_names (sin `civs`: no hay
    // definición propia reconstruida, ver CivNameIndexV1).
    uint32_t civ_count;
    const CivNameIndexV1* civ_names;
    // Sprint 1.6B (SPEC-004 §16): resource_spawns tipados del mapa activo
    // (ver ResourceSpawnV1). count==0 ⇒ sin catálogo de mapa útil (el caller,
    // game_state.hpp::gs_init_economy_from_catalog, hace NO-OP y conserva el
    // fallback legacy fijo — SPEC-004 §16).
    uint32_t map_resource_spawn_count;
    const ResourceSpawnV1* map_resource_spawns;
    // Sprint 1.8C: espejo de unit_count/units/unit_names para los metadatos
    // descriptivos de los recursos. No forma parte del estado de simulación.
    uint32_t resource_count;
    const ResourceDefinitionV1* resources;
    const ResourceNameIndexV1* resource_names;
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
    InvalidAiProfile, // Sprint 1.4 (SPEC-005 §3); append-only, no renumerar.
    InvalidMap,       // Sprint 1.6B (SPEC-004 §16); append-only, no renumerar.
    InvalidResource,  // Sprint 1.8B (SPEC-007 §18); append-only.
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
inline constexpr uint32_t SECTION_COUNT_LEGACY = 7;
inline constexpr uint32_t SECTION_COUNT_RESOURCE_V1 = 8;

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
        "epoch_window", "class", "tags", "resource_costs",
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
        // Sprint 1.18 (SPEC-004 Parte VI)
        "armor", "attack_damage_type",
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

inline bool resolve_resource_index(
        const std::vector<std::string>& resource_ids,
        const std::vector<uint8_t>& resource_indices,
        const std::string& target,
        uint8_t& out_index) noexcept {
    if (resource_ids.size() != resource_indices.size()) return false;
    for (size_t pos = 0; pos < resource_ids.size(); ++pos) {
        if (resource_ids[pos] == target) {
            out_index = resource_indices[pos];
            return true;
        }
    }
    return false;
}

// `resource_costs` (common.schema.json): objeto de record_id→cantidad. La
// tabla id→slot viene del índice asignado por el compilador en kind=8; para
// CHDB 1.0 legados, load_impl instala la tabla histórica A/B/Me. Ausencia de
// la clave ⇒ costes en 0 (defensivo: los schemas la exigen donde corresponde).
// Sprint 1.9: variante con NOMBRE de campo, para `input_resource_costs` de las
// recetas. La logica es identica; extraerla evita una segunda copia que pueda
// divergir en las validaciones de rango.
// Sprint 1.18: armadura por tipo de dano, dentro de `stats`. Los tres campos
// son obligatorios en el esquema, asi que su ausencia es SchemaMismatch y no
// un cero silencioso: un cero por defecto en armadura seria una unidad
// invulnerable-por-descuido que nadie detectaria mirando los datos.
// Sprint 1.18 §28: `stat_effects` de una tecnologia. OPCIONAL: su ausencia no
// es error (una tech puede conceder solo capacidades).
inline void parse_stat_effects(const CveValue& obj,
                               TechEffectV1 (&out)[TECH_EFFECTS_MAX],
                               uint8_t& out_count,
                               CatalogLoadCode range_fail) {
    out_count = 0;
    const CveValue* arr = obj.find("stat_effects");
    if (!arr) return;
    if (!arr->is_arr()) fail(CatalogLoadCode::SchemaMismatch);
    if (arr->arr.size() > TECH_EFFECTS_MAX) fail(range_fail);
    for (const auto& item : arr->arr) {
        if (!item.is_obj()) fail(CatalogLoadCode::SchemaMismatch);
        TechEffectV1 e{};
        const CveValue* st = item.find("stat");
        if (!st || !st->is_str()) fail(CatalogLoadCode::SchemaMismatch);
        if (st->s == "attack") e.stat = StatEffectV1::Attack;
        else if (st->s == "armor_cut") e.stat = StatEffectV1::ArmorCut;
        else if (st->s == "armor_pierce") e.stat = StatEffectV1::ArmorPierce;
        else if (st->s == "armor_impact") e.stat = StatEffectV1::ArmorImpact;
        else fail(range_fail);
        const CveValue* am = item.find("amount");
        if (!am || !am->is_int()) fail(CatalogLoadCode::SchemaMismatch);
        if (am->i < -1000 || am->i > 1000) fail(range_fail);
        e.amount = static_cast<int32_t>(am->i);
        const CveValue* ap = item.find("applies_to");
        if (!ap || !ap->is_arr() || ap->arr.empty()) fail(CatalogLoadCode::SchemaMismatch);
        e.class_mask = 0;
        for (const auto& c : ap->arr) {
            if (!c.is_str()) fail(CatalogLoadCode::SchemaMismatch);
            uint8_t idx = 0xFFu;
            if (c.s == "infantry") idx = 0;
            else if (c.s == "cavalry") idx = 1;
            else if (c.s == "artillery") idx = 2;
            else if (c.s == "citizen") idx = 3;
            else if (c.s == "siege") idx = 4;
            else if (c.s == "naval_light") idx = 5;
            else fail(range_fail);
            e.class_mask = static_cast<uint8_t>(e.class_mask | (1u << idx));
        }
        out[out_count++] = e;
    }
}

inline void parse_armor(const CveValue& stats, int32_t (&armor)[DAMAGE_TYPE_COUNT],
                        CatalogLoadCode range_fail) {
    const CveValue* a = stats.find("armor");
    if (!a || !a->is_obj()) fail(CatalogLoadCode::SchemaMismatch);
    static const char* const kKeys[DAMAGE_TYPE_COUNT] = {"cut", "pierce", "impact"};
    for (uint32_t k = 0; k < DAMAGE_TYPE_COUNT; ++k) {
        const CveValue* v = a->find(kKeys[k]);
        if (!v || !v->is_int()) fail(CatalogLoadCode::SchemaMismatch);
        if (v->i < 0 || v->i > 100000) fail(range_fail);
        armor[k] = static_cast<int32_t>(v->i);
    }
}

inline DamageTypeV1 parse_damage_type(const CveValue& stats, CatalogLoadCode range_fail) {
    const CveValue* v = stats.find("attack_damage_type");
    if (!v || !v->is_str()) fail(CatalogLoadCode::SchemaMismatch);
    if (v->s == "cut") return DamageTypeV1::Cut;
    if (v->s == "pierce") return DamageTypeV1::Pierce;
    if (v->s == "impact") return DamageTypeV1::Impact;
    fail(range_fail);
    return DamageTypeV1::Cut;
}

inline void parse_named_resource_costs(const CveValue& obj,
                                       const char* key,
                                       int32_t (&cost)[RESOURCE_COUNT],
                                       CatalogLoadCode range_fail,
                                       const std::vector<std::string>& resource_ids,
                                       const std::vector<uint8_t>& resource_indices) {
    for (uint32_t resource = 0; resource < RESOURCE_COUNT; ++resource) {
        cost[resource] = 0;
    }
    const CveValue* costs = obj.find(key);
    if (!costs) return;
    if (!costs->is_obj()) fail(CatalogLoadCode::SchemaMismatch);
    for (const auto& kv : costs->obj) {
        if (!kv.second.is_int()) fail(CatalogLoadCode::SchemaMismatch);
        if (kv.second.i < 0 || kv.second.i > 1000000) fail(range_fail);
        uint8_t resource_index = 0;
        if (!resolve_resource_index(
                resource_ids, resource_indices, kv.first, resource_index)
                || resource_index >= RESOURCE_COUNT) {
            fail(range_fail);
        }
        cost[resource_index] = static_cast<int32_t>(kv.second.i);
    }
}

inline void parse_resource_costs(const CveValue& obj,
                                 int32_t (&cost)[RESOURCE_COUNT],
                                 CatalogLoadCode range_fail,
                                 const std::vector<std::string>& resource_ids,
                                 const std::vector<uint8_t>& resource_indices) {
    for (uint32_t resource = 0; resource < RESOURCE_COUNT; ++resource) {
        cost[resource] = 0;
    }
    const CveValue* costs = obj.find("resource_costs");
    if (!costs) return;
    if (!costs->is_obj()) fail(CatalogLoadCode::SchemaMismatch);
    for (const auto& kv : costs->obj) {
        if (!kv.second.is_int()) fail(CatalogLoadCode::SchemaMismatch);
        if (kv.second.i < 0 || kv.second.i > 1000000) fail(range_fail);
        uint8_t resource_index = 0;
        if (!resolve_resource_index(
                resource_ids, resource_indices, kv.first, resource_index)
                || resource_index >= RESOURCE_COUNT) {
            fail(range_fail);
        }
        cost[resource_index] = static_cast<int32_t>(kv.second.i);
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

inline bool resource_family_from_string(
        const std::string& value, ResourceFamilyV1& out) noexcept {
    if (value == "subsistence") {
        out = ResourceFamilyV1::Subsistence;
    } else if (value == "construction") {
        out = ResourceFamilyV1::Construction;
    } else if (value == "base_metals") {
        out = ResourceFamilyV1::BaseMetals;
    } else if (value == "metallurgy") {
        out = ResourceFamilyV1::Metallurgy;
    } else if (value == "chemistry") {
        out = ResourceFamilyV1::Chemistry;
    } else if (value == "energy") {
        out = ResourceFamilyV1::Energy;
    } else if (value == "high_tech") {
        out = ResourceFamilyV1::HighTech;
    } else if (value == "textiles") {
        out = ResourceFamilyV1::Textiles;
    } else {
        return false;
    }
    return true;
}

inline bool resource_nature_from_string(
        const std::string& value, ResourceNatureV1& out) noexcept {
    if (value == "collected") {
        out = ResourceNatureV1::Collected;
    } else if (value == "produced") {
        out = ResourceNatureV1::Produced;
    } else {
        return false;
    }
    return true;
}

// Reconstruye los cuatro metadatos authored y el índice compiler-owned de un
// recurso. La clave se devuelve aparte para que Impl sea dueño de su memoria;
// el puntero estable de la definición se instala una vez llenado el vector.
inline ResourceDefinitionV1 build_resource_definition(
        const CveValue& obj, std::string& display_name_key_out) {
    if (!obj.is_obj()) fail(CatalogLoadCode::SchemaMismatch);

    const CveValue* index = obj.find("index");
    const CveValue* display_name_key = obj.find("display_name_key");
    const CveValue* family = obj.find("family");
    const CveValue* appearance_epoch = obj.find("appearance_epoch");
    const CveValue* nature = obj.find("nature");
    if (!index || !index->is_int()
            || !display_name_key || !display_name_key->is_str()
            || !family || !family->is_str()
            || !appearance_epoch || !appearance_epoch->is_int()
            || !nature || !nature->is_str()) {
        fail(CatalogLoadCode::SchemaMismatch);
    }

    if (index->i < 0
            || index->i >= static_cast<int64_t>(RESOURCE_COUNT)
            || display_name_key->s.empty()
            || display_name_key->s.size() > 0xFFFFu
            || appearance_epoch->i < 1
            || appearance_epoch->i > 15) {
        fail(CatalogLoadCode::InvalidResource);
    }

    ResourceDefinitionV1 definition{};
    definition.index = static_cast<uint8_t>(index->i);
    definition.display_name_key_bytes =
        static_cast<uint16_t>(display_name_key->s.size());
    if (!resource_family_from_string(family->s, definition.family)
            || !resource_nature_from_string(nature->s, definition.nature)) {
        fail(CatalogLoadCode::InvalidResource);
    }
    definition.appearance_epoch =
        static_cast<uint8_t>(appearance_epoch->i);
    display_name_key_out = display_name_key->s;
    return definition;
}

// Reconstruye y valida un UnitDefinitionV1 desde su objeto CVE ya parseado
// (SPEC-002 §8.1: rangos exactos del schema; ver data/schemas/unit.schema.json).
// Sprint 1.6B (SPEC-004 §15.3): `civ_id_raw_out` recibe el record_id textual
// del campo `civ_id` (SIN resolver — la sección civ, kind=5, va después de
// unit en el blob; load_impl resuelve tras parsear todas las secciones,
// mismo patrón que las referencias diferidas de building/tech).
inline UnitDefinitionV1 build_unit_definition(const CveValue& obj, UnitId id,
                                              std::string& civ_id_raw_out,
                                              const std::vector<std::string>& resource_ids,
                                              const std::vector<uint8_t>& resource_indices) {
    if (!obj.is_obj()) fail(CatalogLoadCode::SchemaMismatch);
    for (const auto& kv : obj.obj) {
        if (!is_known_unit_key(kv.first)) fail(CatalogLoadCode::SchemaMismatch);
    }

    // Sprint 1.6B (SPEC-004 §15.3): civ_id, record_id textual (referencia
    // diferida, ver comentario de la firma más arriba).
    const CveValue* civ_v = obj.find("civ_id");
    if (!civ_v || !civ_v->is_str()) fail(CatalogLoadCode::SchemaMismatch);
    civ_id_raw_out = civ_v->s;

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
    parse_armor(*stats, def.armor, CatalogLoadCode::InvalidUnit);
    def.attack_type = parse_damage_type(*stats, CatalogLoadCode::InvalidUnit);
    def.morale = static_cast<int32_t>(morale);
    def.build_time_ticks = static_cast<int32_t>(build_time);
    for (int k = 0; k < 6; ++k) def.bonus_vs_bp[k] = 0;

    // Sprint 1.2 (SPEC-004 §11.1/§12.1/§12.4): costes de entrenamiento, pop_cost
    // constante, epoch_window.
    parse_resource_costs(
        obj, def.cost, CatalogLoadCode::InvalidUnit,
        resource_ids, resource_indices);
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
    def.civ_id = INVALID_CIV_ID;  // resuelto más tarde por load_impl
    return def;
}

// ---------------------------------------------------------------------------
// Sprint 1.1 (SPEC-004 §2): tabla tipada de edificios.
// ---------------------------------------------------------------------------

// Sprint 1.2 (SPEC-004 §11.1): referencias de building AÚN sin resolver a la
// hora de parsear el record (trains apunta a unit — resoluble en el momento;
// researches apunta a tech — sección posterior en el blob, kind=4 tras
// kind=3; required_capabilities apunta a la tabla de capacidades del
// manifest). `load_impl` resuelve LAS TRES en un único paso posterior, tras
// haber parseado TODAS las secciones, por uniformidad (ver su comentario).
struct BuildingRawRefs {
    // Sprint 1.9: recetas ya resueltas salvo el RecipeId global, que asigna
    // load_impl al recorrer los edificios en orden.
    std::vector<RecipeV1> recipes;
    std::vector<std::string> trains;
    std::vector<std::string> researches;
    std::vector<std::string> required_capabilities;
    // Sprint 1.6B (SPEC-004 §15.3): civ_id, record_id textual sin resolver
    // (mismo motivo que researches: la sección civ, kind=5, va DESPUÉS de
    // building en el blob).
    std::string civ_id;
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
                                                      BuildingRawRefs& raw_out,
                                                      const std::vector<std::string>& resource_ids,
                                                      const std::vector<uint8_t>& resource_indices) {
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
    int32_t building_armor[DAMAGE_TYPE_COUNT] = {};
    parse_armor(*stats, building_armor, CatalogLoadCode::InvalidBuilding);

    const CveValue* constructible_v = obj.find("constructible");
    if (!constructible_v || !(constructible_v->tag == 0x01u || constructible_v->tag == 0x02u)) {
        fail(CatalogLoadCode::SchemaMismatch);
    }
    const uint8_t constructible = static_cast<uint8_t>(constructible_v->tag == 0x02u ? 1u : 0u);

    // Sprint 1.14: OPCIONAL con defecto 0. Los 36 edificios anteriores no lo
    // declaran y deben seguir cargando; hacerlo obligatorio habria invalidado
    // el catalogo entero por un campo que solo importa a las viviendas.
    int32_t population_provided = 0;
    if (const CveValue* ppv = obj.find("population_provided")) {
        if (!ppv->is_int()) fail(CatalogLoadCode::SchemaMismatch);
        if (ppv->i < 0 || ppv->i > 10000) fail(CatalogLoadCode::InvalidBuilding);
        population_provided = static_cast<int32_t>(ppv->i);
    }

    // Deposito que crea el edificio al completarse. OPCIONAL con defecto 0,
    // mismo patron que population_provided: los 38 edificios existentes no
    // declaran estos campos y deben seguir cargando (`creates_amount == 0`
    // significa "este edificio no crea deposito"). El indice de recurso se
    // valida contra RESOURCE_COUNT (0..63) como el resto de indices de este
    // loader; los otros tres campos son cantidades >= 0 con los caps del
    // schema de datos.
    uint8_t creates_resource_idx = 0;
    if (const CveValue* cri_v = obj.find("creates_resource_idx")) {
        if (!cri_v->is_int()) fail(CatalogLoadCode::SchemaMismatch);
        if (cri_v->i < 0 || cri_v->i >= static_cast<int64_t>(RESOURCE_COUNT)) {
            fail(CatalogLoadCode::InvalidBuilding);
        }
        creates_resource_idx = static_cast<uint8_t>(cri_v->i);
    }
    int32_t creates_amount = 0;
    if (const CveValue* ca_v = obj.find("creates_amount")) {
        if (!ca_v->is_int()) fail(CatalogLoadCode::SchemaMismatch);
        if (ca_v->i < 0 || ca_v->i > 1000000) fail(CatalogLoadCode::InvalidBuilding);
        creates_amount = static_cast<int32_t>(ca_v->i);
    }
    int32_t creates_regen_per_tick = 0;
    if (const CveValue* crt_v = obj.find("creates_regen_per_tick")) {
        if (!crt_v->is_int()) fail(CatalogLoadCode::SchemaMismatch);
        if (crt_v->i < 0 || crt_v->i > 100000) fail(CatalogLoadCode::InvalidBuilding);
        creates_regen_per_tick = static_cast<int32_t>(crt_v->i);
    }
    int32_t creates_cap = 0;
    if (const CveValue* cc_v = obj.find("creates_cap")) {
        if (!cc_v->is_int()) fail(CatalogLoadCode::SchemaMismatch);
        if (cc_v->i < 0 || cc_v->i > 1000000) fail(CatalogLoadCode::InvalidBuilding);
        creates_cap = static_cast<int32_t>(cc_v->i);
    }

    const CveValue* btv = obj.find("build_time_ticks");
    if (!btv || !btv->is_int()) fail(CatalogLoadCode::SchemaMismatch);
    // Enmienda del Arquitecto 2026-07-23 (SPEC-004 §4.1.2/§4.3): >= 0, no >= 1.
    // Los centros iniciales de escenario son `constructible:false` +
    // `build_time_ticks:0` (nacen completos: progress 0 >= T 0); el schema de
    // datos exige coste positivo solo para constructible:true, así que 0 es
    // legítimo aquí y NO es un caso especial para el loader.
    if (btv->i < 0 || btv->i > 10000000) fail(CatalogLoadCode::InvalidBuilding);

    int32_t cost[RESOURCE_COUNT] = {};
    parse_resource_costs(
        obj, cost, CatalogLoadCode::InvalidBuilding,
        resource_ids, resource_indices);

    uint64_t dropoff_mask = 0;
    if (const CveValue* dr = obj.find("dropoff_resources")) {
        if (!dr->is_arr()) fail(CatalogLoadCode::SchemaMismatch);
        for (const auto& item : dr->arr) {
            if (!item.is_str()) fail(CatalogLoadCode::SchemaMismatch);
            uint8_t bit = 0;
            if (!resolve_resource_index(
                    resource_ids, resource_indices, item.s, bit)
                    || bit >= RESOURCE_COUNT) {
                fail(CatalogLoadCode::InvalidBuilding);
            }
            dropoff_mask |= (uint64_t{1} << bit);
        }
    }

    // Sprint 1.9 (SPEC-007 §12.2). Las recetas viajan DENTRO del record del
    // edificio (building.schema.json ya las declara), asi que no hace falta una
    // seccion nueva del blob. Los record_id de recurso se resuelven aqui mismo:
    // una referencia no resoluble tumba el catalogo entero, igual que el resto.
    if (const CveValue* rec = obj.find("recipes")) {
        if (!rec->is_arr()) fail(CatalogLoadCode::SchemaMismatch);
        if (rec->arr.size() > RECIPES_PER_BUILDING_MAX) fail(CatalogLoadCode::InvalidBuilding);
        for (const auto& item : rec->arr) {
            if (!item.is_obj()) fail(CatalogLoadCode::SchemaMismatch);
            RecipeV1 r{};
            r.id = INVALID_RECIPE_ID;
            r.building_id = id;
            parse_named_resource_costs(item, "input_resource_costs", r.input,
                                       CatalogLoadCode::InvalidBuilding,
                                       resource_ids, resource_indices);
            const CveValue* out = item.find("output_resource_id");
            if (!out || !out->is_str()) fail(CatalogLoadCode::SchemaMismatch);
            uint8_t out_idx = 0;
            if (!resolve_resource_index(resource_ids, resource_indices, out->s, out_idx) ||
                out_idx >= RESOURCE_COUNT) {
                fail(CatalogLoadCode::InvalidBuilding);
            }
            r.output_index = out_idx;
            const CveValue* amt = item.find("output_amount");
            if (!amt || !amt->is_int()) fail(CatalogLoadCode::SchemaMismatch);
            if (amt->i < 1 || amt->i > 1000000) fail(CatalogLoadCode::InvalidBuilding);
            r.output_amount = static_cast<int32_t>(amt->i);
            const CveValue* dur = item.find("duration_ticks");
            if (!dur || !dur->is_int()) fail(CatalogLoadCode::SchemaMismatch);
            if (dur->i < 1 || dur->i > 10000000) fail(CatalogLoadCode::InvalidBuilding);
            r.duration_ticks = static_cast<uint32_t>(dur->i);
            raw_out.recipes.push_back(r);
        }
    }

    // Sprint 1.6B (SPEC-004 §15.3): civ_id, record_id textual (referencia
    // diferida, resuelta más tarde por load_impl).
    const CveValue* civ_v = obj.find("civ_id");
    if (!civ_v || !civ_v->is_str()) fail(CatalogLoadCode::SchemaMismatch);
    raw_out.civ_id = civ_v->s;

    BuildingDefinitionV1 def{};
    for (uint32_t k = 0; k < DAMAGE_TYPE_COUNT; ++k) def.armor[k] = building_armor[k];
    def.id = id;
    def.civ_id = INVALID_CIV_ID;  // resuelto más tarde por load_impl
    def.hp = static_cast<int32_t>(hpv->i);
    def.footprint_w = static_cast<uint8_t>(w->i);
    def.footprint_h = static_cast<uint8_t>(h->i);
    def.build_time_ticks = static_cast<uint32_t>(btv->i);
    for (uint32_t resource = 0; resource < RESOURCE_COUNT; ++resource) {
        def.cost[resource] = cost[resource];
    }
    def.dropoff_mask = dropoff_mask;
    def.constructible = constructible;
    def.population_provided = population_provided;
    def.creates_resource_idx = creates_resource_idx;
    def.creates_amount = creates_amount;
    def.creates_regen_per_tick = creates_regen_per_tick;
    def.creates_cap = creates_cap;

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
    // Sprint 1.6B (SPEC-004 §15.3, desviación D1 — ver TechDefinitionV1):
    // `available_to` crudo (record_id_set de civs); load_impl exige tamaño
    // exacto 1 para derivar el `civ_id` escalar.
    std::vector<std::string> available_to;
};

// Reconstruye y valida un TechDefinitionV1 desde su objeto CVE ya parseado
// (SPEC-004 §12.1: rangos exactos; ver data/schemas/tech.schema.json). Mismo
// patrón que build_building_definition (sin gate is_known_key: available_to/
// branch/evidence/playable_period_ids/availability_mode/provenance/
// regional_variant_group/required_buildings NO se tipan en
// Parte II — ver el comentario de TechDefinitionV1 sobre required_buildings).
inline TechDefinitionV1 build_tech_definition(
        const CveValue& obj, TechId id, TechRawRefs& raw_out,
        const std::vector<std::string>& resource_ids,
        const std::vector<uint8_t>& resource_indices) {
    if (!obj.is_obj()) fail(CatalogLoadCode::SchemaMismatch);

    const CveValue* epoch_v = obj.find("epoch");
    if (!epoch_v || !epoch_v->is_int()) fail(CatalogLoadCode::SchemaMismatch);
    if (epoch_v->i < 1 || epoch_v->i > 15) fail(CatalogLoadCode::InvalidTech);

    const CveValue* rtt = obj.find("research_time_ticks");
    if (!rtt || !rtt->is_int()) fail(CatalogLoadCode::SchemaMismatch);
    if (rtt->i < 1 || rtt->i > 10000000) fail(CatalogLoadCode::InvalidTech);

    TechDefinitionV1 def{};
    def.id = id;
    def.civ_id = INVALID_CIV_ID;  // resuelto más tarde por load_impl
    def.epoch = static_cast<uint8_t>(epoch_v->i);
    def.research_time_ticks = static_cast<uint32_t>(rtt->i);
    parse_resource_costs(
        obj, def.cost, CatalogLoadCode::InvalidTech,
        resource_ids, resource_indices);

    def.prereq_count = 0; def.grant_count = 0; def.mutex_count = 0;
    parse_stat_effects(obj, def.stat_effects, def.stat_effect_count,
                       CatalogLoadCode::InvalidTech);
    for (uint32_t k = 0; k < TECH_PREREQ_MAX; ++k) def.prerequisites[k] = INVALID_TECH_ID;
    for (uint32_t k = 0; k < TECH_GRANT_MAX; ++k) def.grants[k] = INVALID_CAPABILITY_ID;
    for (uint32_t k = 0; k < TECH_MUTEX_MAX; ++k) def.mutually_exclusive_with[k] = INVALID_TECH_ID;

    raw_out.prerequisites = parse_string_array(obj, "prerequisites");
    raw_out.mutually_exclusive_with = parse_string_array(obj, "mutually_exclusive_with");
    const CveValue* grants_obj = obj.find("grants");
    if (!grants_obj || !grants_obj->is_obj()) fail(CatalogLoadCode::SchemaMismatch);
    raw_out.grants_capabilities = parse_string_array(*grants_obj, "capabilities");

    // Sprint 1.6B (SPEC-004 §15.3, desviación D1): `available_to` crudo
    // (tech.schema.json lo declara `required`); load_impl exige tamaño == 1.
    raw_out.available_to = parse_string_array(obj, "available_to");

    return def;
}

// ---------------------------------------------------------------------------
// Sprint 1.4 (SPEC-005 §3): tabla tipada de perfiles de IA.
// ---------------------------------------------------------------------------

// Reconstruye y valida un AiProfileV1 desde su objeto CVE ya parseado
// (SPEC-005 §3: rangos exactos del schema; ver
// data/schemas/ai-profile.schema.json). Mismo patrón que
// build_tech_definition (sin gate is_known_key: personality/difficulty/
// utility_curves/performance_lod/provenance/diplomacy_openness_bp/el resto
// de tactical_behaviors[1..]/micro_quality_bp/build_order_variance_bp/
// scouting_thoroughness_bp/counter_reaction_delay_ticks del schema se
// validan estructuralmente cuando se leen pero NO se tipan en AiProfileV1 —
// SPEC-005 §3 solo contrata los campos declarados en el struct; el resto es
// dato que la IA v1 de K2 todavía no consume). SIN referencias cruzadas a
// otras secciones del catálogo (a diferencia de building/tech): no hay
// resolución diferida que hacer, todo es valor inline del propio record.
inline AiProfileV1 build_ai_profile_definition(const CveValue& obj, AiProfileId id) {
    if (!obj.is_obj()) fail(CatalogLoadCode::SchemaMismatch);

    const CveValue* sw = obj.find("strategic_weights_bp");
    if (!sw || !sw->is_obj()) fail(CatalogLoadCode::SchemaMismatch);
    auto req_bp = [&](const CveValue& parent, const char* key) -> int64_t {
        const CveValue* v = parent.find(key);
        if (!v || !v->is_int()) fail(CatalogLoadCode::SchemaMismatch);
        if (v->i < 0 || v->i > 10000) fail(CatalogLoadCode::InvalidAiProfile);
        return v->i;
    };
    const int64_t economy_bp = req_bp(*sw, "economy_focus_bp");
    const int64_t military_bp = req_bp(*sw, "military_focus_bp");
    const int64_t tech_bp = req_bp(*sw, "tech_focus_bp");
    const int64_t expansion_bp = req_bp(*sw, "expansion_aggressiveness_bp");
    const int64_t risk_bp = req_bp(*sw, "risk_tolerance_bp");
    // diplomacy_openness_bp: presencia y rango SÍ se validan (el schema lo
    // exige; un record que lo omita o exceda 0..10000 rechaza el catálogo
    // entero, mismo rigor que el resto de campos) pero su VALOR no se tipa
    // en AiProfileV1 (SPEC-005 §10: diplomacia fuera de alcance, IA v1 1v1).
    (void)req_bp(*sw, "diplomacy_openness_bp");

    const CveValue* dp = obj.find("difficulty_params");
    if (!dp || !dp->is_obj()) fail(CatalogLoadCode::SchemaMismatch);
    auto req_ticks = [&](const char* key) -> int64_t {
        const CveValue* v = dp->find(key);
        if (!v || !v->is_int()) fail(CatalogLoadCode::SchemaMismatch);
        if (v->i < 1 || v->i > 1000000) fail(CatalogLoadCode::InvalidAiProfile);
        return v->i;
    };
    const int64_t decision_period = req_ticks("decision_period_ticks");
    const int64_t reaction_latency = req_ticks("reaction_latency_ticks");

    const CveValue* tb = obj.find("tactical_behaviors");
    if (!tb || !tb->is_arr()) fail(CatalogLoadCode::SchemaMismatch);
    // v1 usa el PRIMERO (SPEC-005 §3): un perfil sin ningún tactical_behavior
    // no es utilizable por la capa táctica/reactiva de K2 → se rechaza el
    // catálogo entero (mismo espíritu "referencia no resoluble" de
    // building/tech: aquí la "referencia" es al índice [0], inexistente en
    // un array vacío — ver el fixture de rechazo del RESULT del sprint).
    if (tb->arr.empty()) fail(CatalogLoadCode::InvalidAiProfile);
    const CveValue& tb0 = tb->arr[0];
    if (!tb0.is_obj()) fail(CatalogLoadCode::SchemaMismatch);
    const CveValue* rh = tb0.find("retreat_hp_threshold_bp");
    const CveValue* rm = tb0.find("retreat_morale_threshold_bp");
    if (!rh || !rh->is_int() || !rm || !rm->is_int()) fail(CatalogLoadCode::SchemaMismatch);
    if (rh->i < 0 || rh->i > 10000) fail(CatalogLoadCode::InvalidAiProfile);
    if (rm->i < 0 || rm->i > 10000) fail(CatalogLoadCode::InvalidAiProfile);

    AiProfileV1 def{};
    def.id = id;
    def.economy_focus_bp = static_cast<int32_t>(economy_bp);
    def.military_focus_bp = static_cast<int32_t>(military_bp);
    def.tech_focus_bp = static_cast<int32_t>(tech_bp);
    def.expansion_aggressiveness_bp = static_cast<int32_t>(expansion_bp);
    def.risk_tolerance_bp = static_cast<int32_t>(risk_bp);
    def.decision_period_ticks = static_cast<uint32_t>(decision_period);
    def.reaction_latency_ticks = static_cast<uint32_t>(reaction_latency);
    def.retreat_hp_threshold_bp = static_cast<int32_t>(rh->i);
    def.retreat_morale_threshold_bp = static_cast<int32_t>(rm->i);
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
    // Sprint 1.9 (SPEC-007 §12): tabla plana de recetas. El RecipeId es la
    // posicion en este vector, asignada al recorrer edificios en orden.
    std::vector<RecipeV1> recipes;
    std::vector<BuildingNameIndexV1> building_names;
    // Sprint 1.2 (SPEC-004 §12.1): espejo de unit_ids/units/unit_names, y
    // tabla de capacidades (manifest.declared_capabilities).
    std::vector<std::string> tech_ids;
    std::vector<TechDefinitionV1> techs;
    std::vector<TechNameIndexV1> tech_names;
    std::vector<std::string> capability_ids;
    std::vector<CapabilityNameIndexV1> capability_names;
    // Sprint 1.4 (SPEC-005 §3): espejo de unit_ids/units/unit_names. Sin
    // referencias diferidas (ai-profile no referencia otras secciones).
    std::vector<std::string> ai_profile_ids;
    std::vector<AiProfileV1> ai_profiles;
    std::vector<AiProfileNameIndexV1> ai_profile_names;
    // Sprint 1.6B (SPEC-004 §15.3): civ_ids necesita direcciones estables,
    // mismo motivo que unit_ids/building_ids/tech_ids/ai_profile_ids. Sin
    // definición propia reconstruida (ver CivNameIndexV1) — solo la tabla
    // nombre-índice.
    std::vector<std::string> civ_ids;
    std::vector<CivNameIndexV1> civ_names;
    // Sprint 1.8B (SPEC-007 §18): record_id de recurso (orden bytewise del
    // blob) y slot numérico asignado por el compilador (vector paralelo).
    // Máximo RESOURCE_COUNT; solo se usa durante carga/resolución.
    std::vector<std::string> resource_ids;
    std::vector<uint8_t> resource_indices;
    // Sprint 1.8C: misma propiedad/estabilidad que unit_ids/units/unit_names.
    // Las definiciones siguen el orden bytewise de resource_ids; `index`
    // conserva el slot compiler-owned, que puede diferir de ResourceId.
    std::vector<std::string> resource_display_name_keys;
    std::vector<ResourceDefinitionV1> resources;
    std::vector<ResourceNameIndexV1> resource_names;
    // Sprint 1.6B (SPEC-004 §16): resource_spawns tipados del mapa activo
    // (ver ResourceSpawnV1 y el comentario de load_impl sobre "mapa activo").
    std::vector<ResourceSpawnV1> map_resource_spawns;
    // Referencias diferidas (resueltas tras parsear TODAS las secciones, ver
    // el comentario de `load_impl`); índice paralelo a units/buildings/techs.
    std::vector<std::string> pending_unit_civ;
    std::vector<std::vector<std::string>> pending_building_trains;
    std::vector<std::vector<std::string>> pending_building_researches;
    std::vector<std::vector<std::string>> pending_building_reqcaps;
    std::vector<std::string> pending_building_civ;
    std::vector<std::vector<std::string>> pending_tech_prereqs;
    std::vector<std::vector<std::string>> pending_tech_mutex;
    std::vector<std::vector<std::string>> pending_tech_grants_caps;
    std::vector<std::vector<std::string>> pending_tech_available_to;
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

// CHDB 1.1 añade kind=8 resource de forma append-only.
inline constexpr KindSpec kKindTable[8] = {
    {1, 1, 1}, {2, 2, 65535}, {3, 1, 65535}, {4, 1, 65535},
    {5, 1, 1024}, {6, 1, 1024}, {7, 1, 1024}, {8, 1, RESOURCE_COUNT},
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
    const bool legacy_format =
        fmt_major == 1 && fmt_minor == 0 && schema_set == 1;
    const bool resource_format =
        fmt_major == 1 && fmt_minor == 1 && schema_set == 2;
    if (!legacy_format && !resource_format) {
        fail(CatalogLoadCode::UnsupportedVersion);
    }

    const uint32_t flags = c.u32();
    if ((flags & ~0x1u) != 0u) fail(CatalogLoadCode::UnknownFlags);  // rechaza HAS_PATCHES (D1) y bits desconocidos
    const bool unverified = (flags & 0x1u) != 0u;
    if (unverified && profile == CatalogLoadProfile::Verified) {
        fail(CatalogLoadCode::UnverifiedForbidden);
    }

    const uint32_t section_count = c.u32();
    const uint32_t expected_section_count = legacy_format
        ? SECTION_COUNT_LEGACY : SECTION_COUNT_RESOURCE_V1;
    if (section_count != expected_section_count) {
        fail(CatalogLoadCode::SchemaMismatch);
    }

    const uint32_t entry_size = c.u32();
    if (entry_size != DIRECTORY_ENTRY_SIZE) fail(CatalogLoadCode::SchemaMismatch);

    const uint32_t reserved = c.u32();
    if (reserved != 0u) fail(CatalogLoadCode::SchemaMismatch);

    const uint64_t file_size = c.u64();
    if (file_size != static_cast<uint64_t>(size)) fail(CatalogLoadCode::Bounds);

    // ---- Directorio (7×24 legacy; 8×24 con recursos) -----------------------
    struct DirEntry { uint16_t kind; uint16_t version; uint32_t count; uint64_t offset; uint64_t byte_size; };
    DirEntry dir[SECTION_COUNT_RESOURCE_V1];
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
    // Sprint 1.6B (SPEC-004 §15.3): civ_id crudo de unit, paralelo a units.
    impl->pending_unit_civ.reserve(dir[1].count);
    // Sprint 1.1 (SPEC-004 §2): building_ids necesita direcciones estables,
    // mismo motivo que unit_ids. dir[2] es la sección building (kind=3).
    impl->building_ids.reserve(dir[2].count);
    impl->buildings.reserve(dir[2].count);
    impl->building_names.reserve(dir[2].count);
    impl->pending_building_trains.reserve(dir[2].count);
    impl->pending_building_researches.reserve(dir[2].count);
    impl->pending_building_reqcaps.reserve(dir[2].count);
    // Sprint 1.6B (SPEC-004 §15.3): civ_id crudo de building, paralelo a
    // buildings, mismo motivo.
    impl->pending_building_civ.reserve(dir[2].count);
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
    // Sprint 1.6B (SPEC-004 §15.3): available_to crudo de tech, paralelo a
    // techs, mismo motivo.
    impl->pending_tech_available_to.reserve(dir[3].count);
    // Sprint 1.4 (SPEC-005 §3): ai_profile_ids necesita direcciones estables,
    // mismo motivo que unit_ids/building_ids/tech_ids. dir[6] es la sección
    // ai-profile (kind=7, último del KIND_INFO). Cap ya acotado por
    // kKindTable (1024, ver el bucle del directorio arriba) — sin cap
    // adicional del kernel (a diferencia de tech/cap): no hay bitmask
    // por-jugador dimensionado en múltiplos de 64 que necesite un
    // AI_PROFILE_HARD_CAP propio.
    impl->ai_profile_ids.reserve(dir[6].count);
    impl->ai_profiles.reserve(dir[6].count);
    impl->ai_profile_names.reserve(dir[6].count);
    // Sprint 1.6B (SPEC-004 §15.3): civ_ids necesita direcciones estables,
    // mismo motivo que unit_ids/building_ids/tech_ids/ai_profile_ids. dir[4]
    // es la sección civ (kind=5). Sin cap adicional del kernel más allá del
    // 1024 de kKindTable (no hay bitmask por-jugador dimensionado en
    // múltiplos de 64 que dependa del número de civs, a diferencia de
    // TECH_HARD_CAP/CAP_HARD_CAP — mismo espíritu que ai-profile).
    impl->civ_ids.reserve(dir[4].count);

    // Sprint 1.8B (SPEC-007 §18): la sección resource es append-only (kind=8)
    // y aparece después de los records que la referencian. Se prelee aquí
    // para que unit/building/tech/map resuelvan record_id→slot mientras se
    // reconstruyen. El bucle genérico posterior la vuelve a parsear para
    // conservar una única validación estructural/canónica de secciones.
    if (legacy_format) {
        impl->resource_ids = {"A", "B", "Me"};
        impl->resource_indices = {
            static_cast<uint8_t>(RESOURCE_INDEX_FOOD),
            static_cast<uint8_t>(RESOURCE_INDEX_WOOD),
            static_cast<uint8_t>(RESOURCE_INDEX_STONE),
        };
    } else {
        impl->resource_ids.reserve(dir[7].count);
        impl->resource_indices.reserve(dir[7].count);
        impl->resource_display_name_keys.reserve(dir[7].count);
        impl->resources.reserve(dir[7].count);
        impl->resource_names.reserve(dir[7].count);
        bool used_indices[RESOURCE_COUNT] = {};
        RawCursor resource_section{
            bytes + dir[7].offset,
            static_cast<size_t>(dir[7].byte_size),
        };
        std::string previous_resource_id;
        bool have_previous_resource = false;
        for (uint32_t r = 0; r < dir[7].count; ++r) {
            const uint32_t payload_size = resource_section.u32();
            if (payload_size > RECORD_PAYLOAD_CAP) {
                fail(CatalogLoadCode::Bounds);
            }
            const uint8_t* payload = resource_section.take(payload_size);
            RawCursor record_cursor{payload, payload_size};
            uint32_t nodes = 0;
            CveValue value = cve_parse(record_cursor, 1, nodes);
            if (record_cursor.pos != payload_size || !value.is_obj()) {
                fail(CatalogLoadCode::NonCanonical);
            }
            const CveValue* id_value = value.find("id");
            const CveValue* index_value = value.find("index");
            if (!id_value || !id_value->is_str()
                    || !index_value || !index_value->is_int()
                    || id_value->s.empty()
                    || id_value->s.size() > 0xFFFFu) {
                fail(CatalogLoadCode::InvalidResource);
            }
            if (have_previous_resource
                    && !(previous_resource_id < id_value->s)) {
                fail(CatalogLoadCode::NonCanonical);
            }
            previous_resource_id = id_value->s;
            have_previous_resource = true;
            if (index_value->i < 0
                    || index_value->i >= static_cast<int64_t>(RESOURCE_COUNT)) {
                fail(CatalogLoadCode::InvalidResource);
            }
            const uint8_t index = static_cast<uint8_t>(index_value->i);
            if (used_indices[index]) fail(CatalogLoadCode::InvalidResource);
            used_indices[index] = true;
            impl->resource_ids.push_back(id_value->s);
            impl->resource_indices.push_back(index);
        }
        if (resource_section.pos != resource_section.len) {
            fail(CatalogLoadCode::NonCanonical);
        }
        // Verifica también la política determinista del productor: los tres
        // slots migrados son fijos; el resto recibe el menor slot libre en
        // orden bytewise de record_id. Así un blob hostil no puede reasignar
        // silenciosamente los mismos nombres a otra economía.
        bool expected_used[RESOURCE_COUNT] = {};
        for (size_t pos = 0; pos < impl->resource_ids.size(); ++pos) {
            uint8_t expected = 0;
            bool bootstrap = true;
            if (impl->resource_ids[pos] == "chunsa:food") {
                expected = static_cast<uint8_t>(RESOURCE_INDEX_FOOD);
            } else if (impl->resource_ids[pos] == "chunsa:wood") {
                expected = static_cast<uint8_t>(RESOURCE_INDEX_WOOD);
            } else if (impl->resource_ids[pos] == "chunsa:stone") {
                expected = static_cast<uint8_t>(RESOURCE_INDEX_STONE);
            } else {
                bootstrap = false;
            }
            if (bootstrap) {
                if (impl->resource_indices[pos] != expected
                        || expected_used[expected]) {
                    fail(CatalogLoadCode::InvalidResource);
                }
                expected_used[expected] = true;
            }
        }
        uint32_t next_free = 0;
        for (size_t pos = 0; pos < impl->resource_ids.size(); ++pos) {
            const std::string& resource_id = impl->resource_ids[pos];
            if (resource_id == "chunsa:food"
                    || resource_id == "chunsa:wood"
                    || resource_id == "chunsa:stone") {
                continue;
            }
            while (next_free < RESOURCE_COUNT && expected_used[next_free]) {
                ++next_free;
            }
            if (next_free >= RESOURCE_COUNT
                    || impl->resource_indices[pos] != next_free) {
                fail(CatalogLoadCode::InvalidResource);
            }
            expected_used[next_free] = true;
        }
    }

    bool have_package_id = false;
    // Sprint 1.6B (SPEC-004 §16): "mapa activo" = el PRIMER record map
    // (record_id ascendente) encontrado en la sección kind=6 — ver el
    // comentario de ResourceSpawnV1 sobre la desviación de selección
    // multi-mapa.
    bool map_active_seen = false;

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
                    // Paridad con unit/building/tech (endurecimiento del
                    // Arquitecto, auditoría Opus P2): CapabilityNameIndexV1
                    // guarda la longitud en uint16_t; un nombre > 0xFFFF la
                    // truncaría (sin OOB, pero desajusta catalog_find_capability).
                    if (item.s.empty() || item.s.size() > 0xFFFFu) fail(CatalogLoadCode::Bounds);
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
                std::string civ_raw;
                UnitDefinitionV1 def = build_unit_definition(
                    value, uid, civ_raw,
                    impl->resource_ids, impl->resource_indices);
                impl->unit_ids.push_back(record_id);
                impl->units.push_back(def);
                impl->pending_unit_civ.push_back(std::move(civ_raw));
            } else if (spec.kind == 3) {
                // Sprint 1.1 (SPEC-004 §2): building, ahora tipado (antes solo
                // estructural). Mismo patrón que kind==2. Sprint 1.2 añade las
                // referencias crudas (trains/researches/required_capabilities),
                // resueltas más abajo tras parsear TODAS las secciones.
                if (record_id.size() > 0xFFFFu) fail(CatalogLoadCode::Bounds);
                const BuildingId bid = static_cast<BuildingId>(impl->buildings.size());
                BuildingRawRefs raw{};
                BuildingDefinitionV1 def = build_building_definition(
                    value, bid, raw,
                    impl->resource_ids, impl->resource_indices);
                for (uint32_t rk = 0; rk < RECIPES_PER_BUILDING_MAX; ++rk) {
                    def.recipes[rk] = INVALID_RECIPE_ID;
                }
                def.recipe_count = 0;
                for (RecipeV1& rdef_new : raw.recipes) {
                    rdef_new.id = static_cast<RecipeId>(impl->recipes.size());
                    def.recipes[def.recipe_count++] = rdef_new.id;
                    impl->recipes.push_back(rdef_new);
                }
                impl->building_ids.push_back(record_id);
                impl->buildings.push_back(def);
                impl->pending_building_trains.push_back(std::move(raw.trains));
                impl->pending_building_researches.push_back(std::move(raw.researches));
                impl->pending_building_reqcaps.push_back(std::move(raw.required_capabilities));
                impl->pending_building_civ.push_back(std::move(raw.civ_id));
            } else if (spec.kind == 4) {
                // Sprint 1.2 (SPEC-004 §12.1): tech, tipado. Mismo patrón.
                if (record_id.size() > 0xFFFFu) fail(CatalogLoadCode::Bounds);
                const TechId tid = static_cast<TechId>(impl->techs.size());
                TechRawRefs raw{};
                TechDefinitionV1 def = build_tech_definition(
                    value, tid, raw,
                    impl->resource_ids, impl->resource_indices);
                impl->tech_ids.push_back(record_id);
                impl->techs.push_back(def);
                impl->pending_tech_prereqs.push_back(std::move(raw.prerequisites));
                impl->pending_tech_mutex.push_back(std::move(raw.mutually_exclusive_with));
                impl->pending_tech_grants_caps.push_back(std::move(raw.grants_capabilities));
                impl->pending_tech_available_to.push_back(std::move(raw.available_to));
            } else if (spec.kind == 5) {
                // Sprint 1.6B (SPEC-004 §15.3): civ, tabla mínima (solo
                // nombre-índice, ver comentario de CivNameIndexV1 — sin
                // reconstrucción semántica completa del resto del record).
                if (record_id.size() > 0xFFFFu) fail(CatalogLoadCode::Bounds);
                impl->civ_ids.push_back(record_id);
            } else if (spec.kind == 6) {
                // Sprint 1.6B (SPEC-004 §16): map — tipifica `resource_spawns`
                // SOLO del primer record map ("mapa activo", ver comentario de
                // ResourceSpawnV1); el resto de campos del record (terrain_rle/
                // cost_rle/starting_positions/...) sigue sin tipar (misma
                // deviación estructural-only que antes de este sprint).
                if (!map_active_seen) {
                    map_active_seen = true;
                    const CveValue* spawns = value.find("resource_spawns");
                    if (!spawns || !spawns->is_arr()) fail(CatalogLoadCode::SchemaMismatch);
                    // SPEC-004 §16: "si el mapa trae más [de ECO_MAX_DEPOSITS],
                    // se rechaza la carga (no se truncan datos en silencio)".
                    if (spawns->arr.size() > ECO_MAX_DEPOSITS) fail(CatalogLoadCode::InvalidMap);
                    impl->map_resource_spawns.reserve(spawns->arr.size());
                    for (const auto& item : spawns->arr) {
                        if (!item.is_obj()) fail(CatalogLoadCode::SchemaMismatch);
                        const CveValue* kind_v = item.find("kind");
                        const CveValue* id_v = item.find("id");
                        const CveValue* x_v = item.find("x_millitiles");
                        const CveValue* y_v = item.find("y_millitiles");
                        const CveValue* amt_v = item.find("amount");
                        if (!kind_v || !kind_v->is_str() || !id_v || !id_v->is_str()
                            || !x_v || !x_v->is_int() || !y_v || !y_v->is_int()
                            || !amt_v || !amt_v->is_int()) {
                            fail(CatalogLoadCode::SchemaMismatch);
                        }
                        // Compatibilidad de lectura CHDB 1.0: el schema viejo
                        // permitía spawns de material que el kernel ignoraba.
                        // CHDB 1.1 ya solo admite kind=resource.
                        if (legacy_format && kind_v->s == "material") continue;
                        if (kind_v->s != "resource") fail(CatalogLoadCode::InvalidMap);
                        uint8_t ridx = 0;
                        if (!resolve_resource_index(
                                impl->resource_ids, impl->resource_indices,
                                id_v->s, ridx)
                                || ridx >= RESOURCE_COUNT) {
                            fail(CatalogLoadCode::InvalidMap);
                        }
                        // Rangos estructurales del schema (map.schema.json):
                        // x/y_millitiles 0..2^31-1, amount 1..1000000.
                        if (x_v->i < 0 || x_v->i > 2147483647ll) fail(CatalogLoadCode::InvalidMap);
                        if (y_v->i < 0 || y_v->i > 2147483647ll) fail(CatalogLoadCode::InvalidMap);
                        if (amt_v->i < 1 || amt_v->i > 1000000) fail(CatalogLoadCode::InvalidMap);
                        ResourceSpawnV1 rs{};
                        rs.resource_idx = ridx;
                        // Conversión exacta en enteros (SPEC-004 §16):
                        // raw = mt * FX_ONE_RAW / 1000.
                        rs.x_raw = (x_v->i * static_cast<int64_t>(FX_ONE_RAW)) / 1000;
                        rs.y_raw = (y_v->i * static_cast<int64_t>(FX_ONE_RAW)) / 1000;
                        // P1 de la auditoría Opus (Sprint 1.6B): el rango del
                        // schema (2^31-1 mt ≈ 262x la cota del mundo) NO implica
                        // que el raw resultante esté DENTRO del mundo. Un blob
                        // con un depósito fuera de cota congelaba el kernel:
                        // dist_sq_raw marca FatalReason::WORLD_BOUNDS en el
                        // primer tick que un ciudadano lo evalúa y step() queda
                        // muerto para siempre. Como es entrada NO CONFIABLE, la
                        // política es rechazar el catálogo entero (SPEC-002 §7),
                        // no degradar en caliente.
                        if (rs.x_raw < 0 || rs.x_raw >= WORLD_RAW_MAX
                            || rs.y_raw < 0 || rs.y_raw >= WORLD_RAW_MAX) {
                            fail(CatalogLoadCode::InvalidMap);
                        }
                        rs.amount = static_cast<int32_t>(amt_v->i);
                        impl->map_resource_spawns.push_back(rs);
                    }
                }
            } else if (spec.kind == 7) {
                // Sprint 1.4 (SPEC-005 §3): ai-profile, ahora tipado (antes
                // solo estructural, igual que building/tech antes de sus
                // sprints respectivos). Mismo patrón, sin referencias
                // diferidas (ai-profile no referencia otras secciones).
                if (record_id.size() > 0xFFFFu) fail(CatalogLoadCode::Bounds);
                const AiProfileId aid = static_cast<AiProfileId>(impl->ai_profiles.size());
                AiProfileV1 def = build_ai_profile_definition(value, aid);
                impl->ai_profile_ids.push_back(record_id);
                impl->ai_profiles.push_back(def);
            } else if (spec.kind == 8) {
                // Sprint 1.8C: ResourceDefinitionV1, en el mismo orden
                // bytewise que resource_ids preleído. Se vuelve a comprobar
                // la correspondencia para no confiar en dos pases divergentes.
                const ResourceId resource_id =
                    static_cast<ResourceId>(impl->resources.size());
                if (resource_id >= impl->resource_ids.size()
                        || record_id != impl->resource_ids[resource_id]) {
                    fail(CatalogLoadCode::InvalidResource);
                }
                std::string display_name_key;
                ResourceDefinitionV1 definition =
                    build_resource_definition(value, display_name_key);
                if (definition.index != impl->resource_indices[resource_id]) {
                    fail(CatalogLoadCode::InvalidResource);
                }
                impl->resource_display_name_keys.push_back(
                    std::move(display_name_key));
                impl->resources.push_back(definition);
            }
            // civ (kind=5)/map (kind=6): Sprint 1.6B tipa civ_ids (tabla
            // nombre-índice) y resource_spawns del mapa activo (ramas de
            // arriba); el RESTO de sus campos sigue solo estructural +
            // orden (deviación documentada arriba de este archivo) — no se
            // reconstruye ninguna otra definición semántica.
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
    // Sprint 1.6B (SPEC-004 §15.3): civ_id de unit. civ_ids (sección kind=5)
    // ya está completa en este punto (parseada tras unit en el blob).
    for (size_t ui = 0; ui < impl->units.size(); ++ui) {
        uint32_t civ_idx = 0;
        if (!resolve_id(impl->civ_ids, impl->pending_unit_civ[ui], civ_idx)) {
            fail(CatalogLoadCode::InvalidUnit);
        }
        impl->units[ui].civ_id = static_cast<CivId>(civ_idx);
    }

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

        // Sprint 1.6B (SPEC-004 §15.3): civ_id de building.
        uint32_t civ_idx = 0;
        if (!resolve_id(impl->civ_ids, impl->pending_building_civ[bi], civ_idx)) {
            fail(CatalogLoadCode::InvalidBuilding);
        }
        bd.civ_id = static_cast<CivId>(civ_idx);
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

        // Sprint 1.6B (SPEC-004 §15.3, desviación D1): civ_id de tech desde
        // `available_to` — el kernel v1 SOLO soporta techs de UNA civ; 0 o
        // >1 elementos rechaza el catálogo entero (ver TechDefinitionV1).
        const auto& avail_raw = impl->pending_tech_available_to[ti];
        if (avail_raw.size() != 1) fail(CatalogLoadCode::InvalidTech);
        uint32_t civ_idx = 0;
        if (!resolve_id(impl->civ_ids, avail_raw[0], civ_idx)) fail(CatalogLoadCode::InvalidTech);
        td.civ_id = static_cast<CivId>(civ_idx);
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

    // ---- ai_profile_names: mismo orden que ai_profiles (Sprint 1.4) ----
    for (size_t i = 0; i < impl->ai_profiles.size(); ++i) {
        AiProfileNameIndexV1 ni{};
        ni.record_id_utf8 = impl->ai_profile_ids[i].c_str();
        ni.record_id_bytes = static_cast<uint16_t>(impl->ai_profile_ids[i].size());
        ni.id = impl->ai_profiles[i].id;
        impl->ai_profile_names.push_back(ni);
    }

    // P3 de la auditoría Opus (Sprint 1.6B): reserve exacto, paridad con
    // unit_names/building_names/tech_names/ai_profile_names (la estabilidad de
    // los c_str() ya la da el reserve de civ_ids, pero el espejo del patrón
    // endurecido debe ser exacto para que futuras revisiones no duden).
    impl->civ_names.reserve(impl->civ_ids.size());
    // ---- civ_names: mismo orden que civ_ids (Sprint 1.6B, ya ascendente
    // por el chequeo genérico de orden de record_id) ----
    for (size_t i = 0; i < impl->civ_ids.size(); ++i) {
        CivNameIndexV1 ni{};
        ni.record_id_utf8 = impl->civ_ids[i].c_str();
        ni.record_id_bytes = static_cast<uint16_t>(impl->civ_ids[i].size());
        ni.id = static_cast<CivId>(i);
        impl->civ_names.push_back(ni);
    }

    // ---- resource_names + display_name_key estable (Sprint 1.8C) ----------
    if (resource_format) {
        if (impl->resources.size() != impl->resource_ids.size()
                || impl->resources.size()
                    != impl->resource_display_name_keys.size()) {
            fail(CatalogLoadCode::InvalidResource);
        }
        for (size_t i = 0; i < impl->resources.size(); ++i) {
            ResourceDefinitionV1& definition = impl->resources[i];
            definition.display_name_key_utf8 =
                impl->resource_display_name_keys[i].c_str();

            ResourceNameIndexV1 name{};
            name.record_id_utf8 = impl->resource_ids[i].c_str();
            name.record_id_bytes =
                static_cast<uint16_t>(impl->resource_ids[i].size());
            name.id = static_cast<ResourceId>(i);
            impl->resource_names.push_back(name);
        }
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
    cat.ai_profile_count = static_cast<uint32_t>(impl->ai_profiles.size());
    cat.ai_profiles = impl->ai_profiles.data();
    cat.ai_profile_names = impl->ai_profile_names.data();
    cat.civ_count = static_cast<uint32_t>(impl->civ_ids.size());
    cat.civ_names = impl->civ_names.data();
    cat.map_resource_spawn_count = static_cast<uint32_t>(impl->map_resource_spawns.size());
    cat.map_resource_spawns = impl->map_resource_spawns.data();
    cat.resource_count = static_cast<uint32_t>(impl->resources.size());
    cat.resources = impl->resources.data();
    // Sprint 1.9 (SPEC-007 §12): tabla plana de recetas.
    cat.recipe_count = static_cast<uint32_t>(impl->recipes.size());
    cat.recipes = impl->recipes.data();
    cat.resource_names = impl->resource_names.data();

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

// Sprint 1.4 (SPEC-005 §3): espejo de catalog_find_unit para AiProfileId.
inline AiProfileId catalog_find_ai_profile(const DataCatalogV1& cat, const char* name, size_t name_len) noexcept {
    uint32_t lo = 0, hi = cat.ai_profile_count;
    while (lo < hi) {
        const uint32_t mid = lo + (hi - lo) / 2;
        const AiProfileNameIndexV1& e = cat.ai_profile_names[mid];
        const size_t n = (e.record_id_bytes < name_len) ? e.record_id_bytes : name_len;
        int c = (n == 0) ? 0 : std::memcmp(e.record_id_utf8, name, n);
        if (c == 0 && e.record_id_bytes == name_len) return e.id;
        if (c < 0 || (c == 0 && e.record_id_bytes < name_len)) lo = mid + 1;
        else hi = mid;
    }
    return INVALID_AI_PROFILE_ID;
}

// Sprint 1.6B (SPEC-004 §15.3): espejo de catalog_find_unit para CivId.
inline CivId catalog_find_civ(const DataCatalogV1& cat, const char* name, size_t name_len) noexcept {
    uint32_t lo = 0, hi = cat.civ_count;
    while (lo < hi) {
        const uint32_t mid = lo + (hi - lo) / 2;
        const CivNameIndexV1& e = cat.civ_names[mid];
        const size_t n = (e.record_id_bytes < name_len) ? e.record_id_bytes : name_len;
        int c = (n == 0) ? 0 : std::memcmp(e.record_id_utf8, name, n);
        if (c == 0 && e.record_id_bytes == name_len) return e.id;
        if (c < 0 || (c == 0 && e.record_id_bytes < name_len)) lo = mid + 1;
        else hi = mid;
    }
    return INVALID_CIV_ID;
}

// Sprint 1.8C: espejo de catalog_find_unit para ResourceId. El ResourceId
// devuelve la posición de la definición; su slot de stock está en `.index`.
inline ResourceId catalog_find_resource(
        const DataCatalogV1& cat, const char* name, size_t name_len) noexcept {
    uint32_t lo = 0, hi = cat.resource_count;
    while (lo < hi) {
        const uint32_t mid = lo + (hi - lo) / 2;
        const ResourceNameIndexV1& entry = cat.resource_names[mid];
        const size_t bytes =
            (entry.record_id_bytes < name_len)
                ? entry.record_id_bytes
                : name_len;
        const int comparison =
            (bytes == 0)
                ? 0
                : std::memcmp(entry.record_id_utf8, name, bytes);
        if (comparison == 0 && entry.record_id_bytes == name_len) {
            return entry.id;
        }
        if (comparison < 0
                || (comparison == 0 && entry.record_id_bytes < name_len)) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return INVALID_RESOURCE_ID;
}

}  // namespace chunsa
