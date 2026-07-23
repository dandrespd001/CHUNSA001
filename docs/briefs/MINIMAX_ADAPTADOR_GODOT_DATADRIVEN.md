# BRIEF — Migrar el adaptador Godot al catálogo data-driven (MiniMax M3)

Tarea MECÁNICA y ACOTADA: un solo archivo de lógica (`chunsa_sim_node.h/.cpp`) + copiar un archivo. TODO el código nuevo está escrito literal abajo — solo tienes que insertarlo/sustituirlo exactamente. **NO diseñes nada, NO improvises APIs de Godot ni del kernel: si algo del código de este brief no compila, PÁRATE y repórtalo, no inventes una alternativa.**

## Contexto (una frase)
El kernel ahora lee las stats de las unidades desde un catálogo binario (blob CHDB); el adaptador Godot todavía usa el contrato viejo (stats a mano) y por eso la demo dejó de spawnear unidades. Hay que cargar el catálogo y spawnear por `unit_id`.

## Rama y alcance
- **Crea la rama `mm/godot-datadriven` desde `arch/sprint-0.4-integration`. Jamás toques `main` ni `arch/...` directamente.**
- Archivos que puedes tocar: SOLO `addons/chunsa_sim/gdextension/chunsa_sim_node.h`, `addons/chunsa_sim/gdextension/chunsa_sim_node.cpp`, y copiar un archivo a `demo/`. NADA del `addons/chunsa_sim/core/`.

## ⚠️ Recursos (el equipo se apaga por térmica): builds `nice -n 19 ... -j2`, runs de Godot `nice -n 19`, UNO a la vez.

## API literal del kernel (cópiala tal cual — NO cambies nombres)
De `chunsa/data_catalog.hpp` (ya visible vía `#include <chunsa/game_state.hpp>`):
```cpp
namespace chunsa {
using UnitId = uint32_t;
inline constexpr UnitId INVALID_UNIT_ID = 0xFFFFFFFFu;
enum class CatalogLoadProfile : uint8_t { Verified = 0, Development = 1 };
enum class CatalogLoadCode : uint8_t { Ok = 0, Io, TooLarge, BadMagic, /*...*/ };
struct DataCatalogV1 { /* ... */ };
class DataCatalogStorageV1 {            // RAII, posee el catálogo, move-only
public:
    bool valid() const noexcept;
    const DataCatalogV1& catalog() const noexcept;
};
CatalogLoadCode catalog_load_file_v1(const char* path, CatalogLoadProfile profile,
                                     DataCatalogStorageV1& out) noexcept;
// resuelve record_id textual → UnitId (o INVALID_UNIT_ID); usar en setup, no en el tick
inline UnitId catalog_find_unit(const DataCatalogV1& cat, const char* name, size_t name_len) noexcept;
}
```
De `chunsa/game_state.hpp`: `GameState` tiene el miembro `const chunsa::DataCatalogV1* catalog;` (lo deja `nullptr` `gs_init`). De `chunsa/commands.hpp`: `CmdPayload` tiene `uint32_t unit_id;` — en el camino data-driven, TODOS los campos de stats del payload (`hp`, `attack`, `range_mt`, `unit_class`, `speed_mtpt`) DEBEN quedar en 0 y `unit_id` es un id válido del catálogo.

## Unidades del blob (record_id → uso en la demo)
- `egipto:chariot_warrior` (clase cavalry) → la "caballería" (owner 0).
- `egipto:work_crew` (clase citizen) → los "ciudadanos" (owner 0).
- `rome:ballista_crew` (clase artillery) → la "artillería" (owner 1).

---

## PASO 1 — `chunsa_sim_node.h`: añadir miembros
En la sección `private:`, JUSTO DESPUÉS de la línea `chunsa::GameState* gs = nullptr;`, inserta:
```cpp
    // Sprint 0.4: catálogo de datos (CHDB). `catalog_storage` posee el catálogo
    // (RAII) y vive tanto como el nodo; `gs->catalog` apunta a su interior. Los
    // uid_* se resuelven una vez en _ready() (record_id → UnitId).
    chunsa::DataCatalogStorageV1 catalog_storage;
    chunsa::UnitId uid_cavalry = chunsa::INVALID_UNIT_ID;
    chunsa::UnitId uid_citizen = chunsa::INVALID_UNIT_ID;
    chunsa::UnitId uid_artillery = chunsa::INVALID_UNIT_ID;
```

## PASO 2 — `chunsa_sim_node.cpp`: include nuevo
Junto a los demás `#include <godot_cpp/classes/...>` del `.cpp`, añade:
```cpp
#include <godot_cpp/classes/project_settings.hpp>
```
Y asegúrate de que `#include <cstring>` está presente (si no, añádelo).

## PASO 3 — `chunsa_sim_node.cpp`: cargar el catálogo en `_ready()`
En `_ready()`, JUSTO DESPUÉS de la línea `chunsa::gs_init(*gs, cfg);`, inserta EXACTAMENTE:
```cpp
    // --- Sprint 0.4: cargar el catálogo de datos y bindearlo al GameState ---
    {
        godot::String blob_path = os_ptr->get_environment("CHUNSA_BLOB");
        if (blob_path.is_empty()) {
            blob_path = godot::ProjectSettings::get_singleton()
                            ->globalize_path("res://chunsa_base.chdb");
        }
        const chunsa::CatalogLoadCode code = chunsa::catalog_load_file_v1(
            blob_path.utf8().get_data(),
            chunsa::CatalogLoadProfile::Development, catalog_storage);
        if (code != chunsa::CatalogLoadCode::Ok || !catalog_storage.valid()) {
            godot::UtilityFunctions::print(
                "CHUNSA ERROR: catálogo no cargado (code=",
                static_cast<int64_t>(code), ") desde ", blob_path);
            return;  // sin catálogo no hay spawns data-driven
        }
        gs->catalog = &catalog_storage.catalog();
        uid_cavalry = chunsa::catalog_find_unit(
            *gs->catalog, "egipto:chariot_warrior", std::strlen("egipto:chariot_warrior"));
        uid_citizen = chunsa::catalog_find_unit(
            *gs->catalog, "egipto:work_crew", std::strlen("egipto:work_crew"));
        uid_artillery = chunsa::catalog_find_unit(
            *gs->catalog, "rome:ballista_crew", std::strlen("rome:ballista_crew"));
        godot::UtilityFunctions::print(
            "CHUNSA catálogo OK: cav_id=", static_cast<int64_t>(uid_cavalry),
            " cit_id=", static_cast<int64_t>(uid_citizen),
            " art_id=", static_cast<int64_t>(uid_artillery));
    }
```
(`os_ptr` ya existe en `_ready()` como `godot::OS* os_ptr = godot::OS::get_singleton();`.)

## PASO 4 — `chunsa_sim_node.cpp`: migrar los 3 spawns en `build_showcase_batch`
El `std::memset(&c, 0, sizeof(RawCommand))` de cada spawn YA deja todos los campos de stats en 0 (payload limpio). Solo hay que **BORRAR las asignaciones de stats** y **AÑADIR `c.p.unit_id = ...`**.

**4a. Caballería (owner 0):** encuentra la línea
```cpp
            c.p.speed_mtpt = 150;
            c.p.hp = 100; c.p.attack = 20; c.p.range_mt = 1500; c.p.unit_class = 1;
```
y REEMPLÁZALA por:
```cpp
            c.p.unit_id = uid_cavalry;  // stats del catálogo (payload en 0 por el memset)
```

**4b. Ciudadanos (owner 0):** encuentra la línea (en el bloque `SPAWN_CITIZEN`)
```cpp
            c.p.speed_mtpt = 800;  // desviación documentada (brief: 200)
```
y REEMPLÁZALA por:
```cpp
            c.p.unit_id = uid_citizen;  // stats del catálogo (payload en 0 por el memset)
```

**4c. Artillería (owner 1):** encuentra la línea
```cpp
            c.p.speed_mtpt = 80;
            c.p.hp = 100; c.p.attack = 20; c.p.range_mt = 1500; c.p.unit_class = 2;
```
y REEMPLÁZALA por:
```cpp
            c.p.unit_id = uid_artillery;  // stats del catálogo (payload en 0 por el memset)
```
NO toques el bloque `t == 1` (los `MOVE_TO`): esos no usan stats ni `unit_id`, se quedan igual.

## PASO 5 — Copiar el blob a la demo
```bash
cp data/compiled/chunsa_base.chdb demo/chunsa_base.chdb
```
`git add demo/chunsa_base.chdb` (debe quedar versionado; el adaptador lo carga vía `res://chunsa_base.chdb`).

## Verificación OBLIGATORIA antes de commitear
1. Compila el adaptador (térmica `nice -19 -j2`):
   ```
   nice -n 19 cmake -B build-godot -DCMAKE_BUILD_TYPE=Debug -DCHUNSA_BUILD_GODOT=ON >/dev/null 2>&1
   nice -n 19 cmake --build build-godot -j2 --target chunsa_godot 2>&1 | tail -3
   ```
   Debe compilar LIMPIO (0 warnings, 0 errores) y generar `demo/bin/libchunsa_godot.so`.
2. Corre la demo headless:
   ```
   nice -n 19 ./third_party_build/Godot_v4.7.1-stable_linux.x86_64 --headless --path demo --quit-after 1500 2>&1 | grep -E "catálogo|cav=|ERROR"
   ```
   En la salida DEBE aparecer `CHUNSA catálogo OK: cav_id=... cit_id=... art_id=...` con los tres ids **distintos de 4294967295** (ese número es INVALID_UNIT_ID = fallo de resolución), y varias líneas `CHUNSA cav=N art=M citizens=K stock_A=...` con **cav/art/citizens > 0** (si son 0, los spawns fallaron: revisa). NO debe aparecer ninguna línea `CHUNSA ERROR`.
3. `git add` de los 2 archivos del adaptador + `demo/bin/libchunsa_godot.so` + `demo/chunsa_base.chdb`; commit en la rama `mm/godot-datadriven`: "Sprint 0.4: adaptador Godot data-driven (spawns por unit_id del catálogo)".
4. Escribe `docs/briefs/MINIMAX_ADAPTADOR_GODOT_DATADRIVEN_RESULT.md`: qué cambiaste, la salida EXACTA del paso 2 (las líneas de `catálogo`/`cav=`), y cualquier desviación. Inclúyelo en el commit.

## Reglas duras
- NO toques `addons/chunsa_sim/core/` (el kernel). Si crees que necesitas tocarlo, PÁRATE y reporta — significa que algo del brief está mal, no lo arregles inventando.
- NO uses ninguna API de godot-cpp que no aparezca literal en este brief. Si `project_settings.hpp` o `globalize_path` no existieran con esa firma en esta versión, repórtalo en vez de improvisar.
- El comportamiento de la simulación CAMBIARÁ un poco respecto a antes (ahora las stats vienen del dato real, no de 100/20/1500 a mano) — eso es CORRECTO y esperado, es el objetivo. La verificación NO es comparar checksums, es que las unidades spawnean (cav/art/citizens > 0).
