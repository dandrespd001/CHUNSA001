# Brief cerrado — Sonnet 5 — kernel data-driven, save v7 y replay v3

Fecha: 2026-07-22  
Autoridad: Arquitecto Jefe de CHUNSA  
Rama/worktree asignado: se indicará al invocar  
Estado: preparado; no ejecutar hasta que el golden CHDB D1 esté integrado

## 0. Fuente de verdad y orden de lectura

Lee completos, en este orden:

1. `/home/adquiod/Imágenes/Project/SPEC-002_DATOS_Y_SCHEMAS.md`, en particular
   §§5–10 y §12.
2. `docs/PLAN_MAESTRO.md` §4.
3. `addons/chunsa_sim/core/include/chunsa/commands.hpp`
4. `addons/chunsa_sim/core/include/chunsa/game_state.hpp`
5. `addons/chunsa_sim/core/include/chunsa/step.hpp`
6. `addons/chunsa_sim/core/include/chunsa/checksum.hpp`
7. `addons/chunsa_sim/core/include/chunsa/serialize.hpp`
8. `addons/chunsa_sim/core/include/chunsa/save_io.hpp`
9. `addons/chunsa_sim/core/include/chunsa/replay.hpp`
10. `addons/chunsa_sim/core/include/chunsa/ai_stub.hpp`, `driver.hpp`,
    `cli_run.hpp`, `addons/chunsa_sim/cli/main.cpp`, CMake y tests actuales.

En conflicto manda SPEC-002. No uses los schemas históricos de doc 10 ni las
SPECs supersedidas como contrato binario.

## 1. Resultado requerido

Implementa la parte C++ completa de SPEC-002 D1:

- loader CHDB v1 no confiable y catálogo inmutable;
- `MatchConfig01A` completo y binding exacto al catálogo;
- comandos v2 con origen y `unit_id`;
- `SPAWN_UNIT` y `SPAWN_CITIZEN` data-driven, con payload legado solo en modo
  debug explícito;
- `unit_id` en `GameState`, serialización y checksum v2;
- save v7 con binding/content hash y Zstd v1.5.7;
- replay v3 que graba intentos y agenda efectiva, con catálogo externo;
- CLI `--data` para todos los flujos data-driven;
- tests C++ del loader, corrupción, spawn, checksum, save y replay;
- CTest `data_compile` y target `chunsa_test_data_blob`.

No toques Godot/demo en esta tarea. El Arquitecto hará ese wiring después.
No implementes D2/mods/patches. No actives `bonus_vs_bp` en combate.

## 2. API literal obligatoria

No renombres ni sustituyas estos tipos o firmas:

```cpp
using UnitId = uint32_t;
inline constexpr UnitId INVALID_UNIT_ID = UINT32_MAX;

struct ContentHashV1 {
    uint8_t bytes[32];
};

enum class ContentHashAlgorithmId : uint16_t { Sha256 = 1 };

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
};

struct UnitNameIndexV1 {
    const char* record_id_utf8;
    uint16_t record_id_bytes;
    UnitId id;
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
private:
    struct Impl;
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

enum class StartMatchCode : uint8_t {
    Ok = 0, InvalidConfig, MissingCatalog, SchemaMismatch, HashMismatch,
    UnverifiedCatalog, CapacityError,
};

StartMatchCode gs_init(GameState& out, MatchConfig01A cfg,
                       const DataCatalogV1& catalog,
                       MatchLaunchPolicy policy) noexcept;

int load_game(GameState& g, AiJobBox& box, AiRuntimeV1& rt,
              const DataCatalogV1& catalog, MatchLaunchPolicy policy,
              const char* path);

int replay_load(const char* path, const DataCatalogV1& catalog,
                MatchLaunchPolicy policy, ReplayData& out);
```

`UnitClassV1` usa exactamente estos ordinales:

```cpp
enum class UnitClassV1 : uint8_t {
    Infantry = 0,
    Cavalry = 1,
    Artillery = 2,
    Citizen = 3,
    Siege = 4,
    NavalLight = 5,
};
```

## 3. Config y command stream literales

La proyección `MatchConfigPersistedV1` tiene exactamente 92 bytes y este orden:

```text
u32 max_entities
u8  player_count
u8  ai_player_mask
u8  allow_debug_stat_payload
u8  reserved_zero
u32 max_commands_per_tick_per_emitter
u32 pending_commands_capacity
u16 max_future_command_ticks
u16 human_input_delay_ticks
u16 ai_input_delay_ticks
u16 ai_decision_period_ticks
u16 checksum_every_ticks
u16 receipt_mailbox_capacity
u32 event_journal_capacity
u32 map_tiles_x
u32 map_tiles_y
u64 seed
u16 blob_format_major
u16 blob_format_minor
u32 data_schema_set_version
u16 content_hash_algorithm_id
u16 content_hash_algorithm_version
u8[32] content_hash
```

El runtime añade `const DataCatalogV1* data_catalog`, que jamás se serializa ni
se checksummea. La identidad persistida sí se valida antes de hidratar/bindear.

No cambies los valores de `CommandType`: `SPAWN_UNIT=5` y
`SPAWN_CITIZEN=6`. Añade:

```cpp
enum class CommandOriginV1 : uint8_t { Human=0, Ai=1, System=2 };
```

`RawCommandV2` y `ScheduledCommandV2` conservan `origin` y un
`reserved_zero:u8`. Si mantienes los nombres C++ existentes `RawCommand` y
`ScheduledCommand`, sus bytes/semántica deben ser v2 y no puede existir un
segundo layout divergente.

Orden único de `CmdPayloadV2`, compartido por save, replay y checksum:

```text
handle.index:u32
handle.generation:u32
x_raw:i64
y_raw:i64
speed_mtpt:i32
hp:i32
attack:i32
range_mt:i32
unit_id:u32
unit_class:u8
reserved_zero:u8[3]
```

Implementa helpers únicos de escritura/lectura/hash del payload. Prohibido
mantener listas de campos independientes.

## 4. Reglas de spawn que deben quedar testeadas

Camino normal:

- `unit_id != INVALID_UNIT_ID` y está en bounds;
- `hp/attack/range_mt/speed_mtpt/unit_class` del payload son cero;
- stats y moral se copian solo de `catalog.units[unit_id]`;
- se persiste `g.unit_id[index]`.

Camino debug:

- `unit_id == INVALID_UNIT_ID`;
- solo legal si `allow_debug_stat_payload==1`;
- usa payload legado validado y guarda `INVALID_UNIT_ID`;
- moral inicial exactamente `MORALE_MAX=100`.

`SPAWN_CITIZEN` exige clase `Citizen` y la misma máquina económica que un
`SPAWN_UNIT` de clase Citizen. `Siege` y `NavalLight` se cargan/validan pero el
spawn devuelve `ILLEGAL_STATE`. Los campos no usados y reserved deben ser cero.

Origen/admisión:

- Human aplica delay humano;
- Ai exige bit de `ai_player_mask`, `target_tick==tick` al integrar y jamás se
  retimea;
- System exige `target_tick>=tick` y usa el target exacto;
- origen/reserved inválido es `MALFORMED`.

## 5. Loader CHDB no confiable

Implementa literalmente SPEC-002 §§5–7:

- cap de archivo antes de reservar;
- header de 64 bytes, flags conocidos y reserved cero;
- 6 secciones exactas, orden canónico por kind y records por `record_id`;
- offsets/tamaños/counts con aritmética checked y sin solapamiento/trailing;
- CVE mínimo, UTF-8/NFC, límites de depth/nodes/string/collection;
- validación schema/semántica tipada al reconstruir `UnitDefinitionV1`;
- recomputar `SHA256("CHUNSA_CONTENT_V1\0" || complete_CHDB_bytes)`;
- construir binding D1 exacto y lookup binario fuera de `Step()`;
- convertir `bad_alloc`/fallos a código estable; ninguna excepción cruza API.

El loader debe aceptar el golden del compilador integrado y rechazar, al menos:
truncación, offsets solapados, overflow, sección duplicada/faltante, CVE no
mínimo, record fuera de orden, flags desconocidos y `UNVERIFIED` bajo perfil
Verified.

## 6. Checksum v2 y serialización

Define sin cambiar el significado histórico de v1:

```text
CHECKSUM_ALGO_VERSION=2
state domain = CHUNSA_STATE_V2
continuation domain = CHUNSA_CONT_V2
```

Incluye `unit_id`, origin y todo payload pendiente. El prefijo usa los campos
de config hasta `seed` en el orden de §8.3, excepto
`event_journal_capacity`. Excluye reserved, formato/schema/hash, puntero,
flags del catálogo y policy.

Un test muta individualmente cada campo no reservado del payload pendiente y
exige checksum diferente. Cambiar solo `event_journal_capacity` o
`content_hash` con estado igual no cambia XXH3, pero el binding con otro hash
debe fallar.

`zero_components`, reciclado, serialización y deserialización inicializan,
limpian y preservan `unit_id` de manera explícita. La deserialización recibe el
catálogo y solo publica un estado completamente validado.

## 7. Save v7 literal

Usa exactamente:

```text
PROTOCOL_VERSION=2
COMMAND_PAYLOAD_VERSION=2
SAVE_FORMAT_VERSION=7
SAVE_KERNEL_VERSION=2
SAVE_DATA_SCHEMA_VERSION=1
CHECKSUM_ALGO_VERSION=2
```

Envelope: 80 bytes. Header fijo: 124 bytes más el binding. Layout, caps, digest
y orden de carga son exactamente SPEC-002 §9.1; no inventes una variante.
Zstd: v1.5.7 vendorizado, nivel 3, frame checksum y content-size habilitados,
un único frame, sin diccionario. Carga streaming a destino acotado, rechazo de
multiframe/trailing y publicación transaccional.

Payload descomprimido:

```text
u32 state_size + STATE + u32 jobs_size + JOBS + u32 ai_size + AI
```

El digest es:

```text
SHA256("CHUNSA_SAVE_V1" || canonical_header || uncompressed_payload)
```

v6 se rechaza de forma estable. Añade negativos de envelope, tamaños,
reservados, binding, Zstd truncado/multiframe, digest, secciones y trailing.

## 8. Replay v3 literal

Implementa SPEC-002 §9.2 sin rederivar la agenda. Header fijo 140 bytes más
binding. Cada frame representa el punto anterior a ejecutar `capture_tick` y
contiene records de intento de 76 bytes. Graba todos los intentos, aceptados o
rechazados, en orden de llegada, con `effective_tick` real y
`expected_admission_result`.

En reproducción vuelve a ejecutar la validación pura de admisión, exige el
mismo `RejectReason` y para ACCEPTED inserta el `effective_tick` grabado solo
después de verificarlo. IA debe cumplir literalmente
`effective_tick==target_tick==capture_tick`. No ejecutes IA durante verify.

Trailer/digest/caps son exactamente §9.2. v2 se rechaza. Deben existir tests de
mutación para effective tick, origin, admission result, cada campo del payload,
binding, digest, counts extremos y truncación.

## 9. CLI, tests y CMake

- Añade `--data <path>` a `run`, `record`, `savetest` y `verify`.
- Selftests que crean estado cargan el golden mínimo y usan el lifecycle único.
- Tests de combate/moral/economía usan IDs resueltos por nombre; solo tests del
  camino debug activan `allow_debug_stat_payload`.
- CTest ejecuta `data_compile` y el nuevo `chunsa_test_data_blob`.
- No dependas del cwd implícito: usa rutas CMake/source dir estables.
- El golden se genera por el compilador Python; no escribas bytes CHDB a mano.

## 10. Restricciones P0

- C++20; warnings existentes siguen limpios.
- Cero float/double en estado o reglas de simulación.
- Cero heap, I/O, lookup textual o excepciones dentro de `Step()`.
- Ninguna iteración no determinista.
- `CommandType` append-only, jamás renumerar.
- No cambiar golden para esconder una regresión accidental.
- No tocar archivos fuera de este worktree.
- No hacer commit ni merge; entrega diff y reporte al Arquitecto.
- No compilar localmente: el Arquitecto ejecuta una única tarea térmica con
  `nice -n 19` y `-j2` tras revisar el diff.

## 11. Entrega

Al terminar informa:

1. archivos modificados/nuevos;
2. decisiones exactas y cualquier desviación de SPEC (idealmente ninguna);
3. tests añadidos y qué invariante prueba cada uno;
4. búsquedas/inspecciones estáticas ejecutadas;
5. riesgos que deba revisar el Arquitecto.

No declares gates verdes: la verificación térmica e independiente pertenece al
Arquitecto.
