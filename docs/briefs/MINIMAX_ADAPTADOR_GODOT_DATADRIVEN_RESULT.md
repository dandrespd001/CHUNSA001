# RESULT — Adaptador Godot data-driven (MiniMax M3 + Arquitecto)

**Nota de proceso**: MiniMax aplicó las 4 ediciones del brief de forma literal y correcta, pero el build falló por un campo que el brief NO anticipó (`MatchConfig01A::allow_debug_stat_payload`, añadido al kernel por el sprint data-driven, que rompe la inicialización por agregado del adaptador bajo `-Werror=missing-field-initializers`). **MiniMax hizo lo correcto: paró y reportó en vez de tocar el kernel o inventar una alternativa** (exactamente la regla dura del brief). El Arquitecto completó el inicializador (1 línea del adaptador, no del kernel) y verificó.

## Qué hizo MiniMax (contrato literal, correcto)
- `chunsa_sim_node.h`: miembros `catalog_storage` (RAII) + `uid_cavalry/citizen/artillery`.
- `chunsa_sim_node.cpp`: `#include <godot_cpp/classes/project_settings.hpp>`; carga del catálogo en `_ready()` (env `CHUNSA_BLOB` o `res://chunsa_base.chdb` globalizado → `catalog_load_file_v1`); bind `gs->catalog`; resolución de los 3 `record_id` → `unit_id`; migración de los 3 spawns de `build_showcase_batch` a `c.p.unit_id` (payload de stats en 0).
- `demo/chunsa_base.chdb`: blob copiado.

## Cierre del Arquitecto (1 línea)
`MatchConfig01A cfg{..., DEMO_SEED, 0}` — añade `allow_debug_stat_payload = 0` (data-driven, sin payload de debug).

## Verificación (Arquitecto)
- Build del adaptador (`nice -19 -j2`): **limpio, 0 warnings**, `demo/bin/libchunsa_godot.so` regenerado.
- Demo headless (`--quit-after 1500`): las unidades **spawnean desde el catálogo** (cav=238, art=240, citizens=120 → combate 227/226), sin ninguna línea `CHUNSA ERROR`. El spawn data-driven solo es posible con catálogo cargado y `unit_id` válido, así que confirma la carga + resolución. La ligera diferencia con la demo hardcodeada previa (238 vs 240 iniciales) es esperada: las stats vienen ahora del dato real.
- Kernel intacto: solo se tocó `addons/chunsa_sim/gdextension/` + `demo/` (el `core/` sin diff).

## Veredicto
**ACEPTADO** — la demo vuelve a funcionar, ahora 100% data-driven. Con esto cae la última deuda que bloqueaba el cierre del Sprint 0.4.
