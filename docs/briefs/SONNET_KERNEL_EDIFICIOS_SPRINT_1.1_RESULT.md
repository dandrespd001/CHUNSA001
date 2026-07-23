# RESULT — Kernel de edificios y construcción (Sonnet · Sprint 1.1)

Rama: `sonnet/buildings-1.1` (desde `main` @ `60dedd8`, que ya incluía la
enmienda del Arquitecto §4.1.2/§4.3 sobre exención de escenario en tick 0).
NO se hizo merge a `main`.

## 1. Commits

```
4956e28 kernel(datos): tabla tipada de edificios en el catálogo CHDB (SPEC-004 §2)
b87a324 kernel: edificios como entidades — estado, comandos y fases de Step (SPEC-004 §3-§7)
1e8e681 kernel: save v8 + checksum v3 con dominio de edificios (SPEC-004 §8)
0f560f3 tests: kernel de edificios y construcción (SPEC-004 §9.3)
```

Los cuatro commits se revisan/integran como una unidad (hay una dependencia
de compilación dura entre el 2º commit y el enum `CommandType` del 1er área:
`-Werror`/`-Wswitch` exige que el `switch` de `apply_command` maneje
`PLACE_BUILDING`/`ASSIGN_BUILD` en el MISMO commit que los añade el enum —
por eso `commands.hpp` y `step.hpp` viajan juntos en `b87a324`, no en
commits separados por archivo).

## 2. Qué se implementó (mapa SPEC → archivo)

| SPEC-004 | Archivo | Contenido |
|---|---|---|
| §2 | `data_catalog.hpp` | `BuildingDefinitionV1`, `BuildingNameIndexV1`, `catalog_find_building`, parsing `kind=building` (antes solo estructural), `CatalogLoadCode::InvalidBuilding` |
| §3 | `game_state.hpp` | `entity_kind/building_id/build_progress/bld_anchor_tx,ty/build_target`, `BUILD_NO_TARGET`, `zero_components`/`gs_init` actualizados |
| §4 | `commands.hpp`, `step.hpp` | `PLACE_BUILDING=7`/`ASSIGN_BUILD=8` (append-only), validación §4.1/§4.2 literal + exención de escenario |
| §5 | `step.hpp` | `construction_system`, fase nueva tras economía y antes de DESTROY |
| §6 | `step.hpp` | `find_building_dropoff` (wiring), guard de `economy_system` para citizens en construcción |
| §7 | `step.hpp` | targeting de edificios en `combat_system`/`aggro_system`, RPS artillery×2.0, restauración de `cost_grid` al reciclar |
| §8 | `checksum.hpp`, `serialize.hpp`, `save_io.hpp` | checksum v3 (dominio `CHUNSA_STATE_V3`), save v8 |
| §9 | `tests/unit/test_data_blob.cpp`, `tests/unit/test_buildings.cpp` | tests nuevos |

`economy.hpp` **no se tocó**: sigue autocontenido, sin conocer `GameState`
(el punto de dropoff resuelto se le sigue pasando desde el wiring de
`step.hpp`, exactamente como antes).

## 3. Desviaciones documentadas (numeradas contra el SPEC)

**D1 — §2, footprint 1..8 vs 1..32 del schema de datos.** El SPEC-004 literal
dice `footprint_w/h: tiles, 1..8`; `data/schemas/building.schema.json` permite
hasta 32. Implementado el rango del SPEC (más estricto): el kernel v1
restringe *further* que el schema de datos — no es una contradicción, es una
cota adicional del kernel sobre un superconjunto válido de datos. Los 4
edificios reales del slice (2x2, 3x3) caen dentro de ambos rangos, sin
impacto práctico.

**D2 — §2, `build_time_ticks >= 0` (enmienda ya aplicada, no una desviación
mía).** Se registra aquí solo para trazabilidad: implementé literalmente la
enmienda del Arquitecto (commit `60dedd8`, ya en `main` cuando arrancó esta
rama) — el rango pasó de `>= 1` a `>= 0` porque los centros iniciales
(`egipto:settlement_center`, `rome:forum_center`) son `constructible:false` +
`build_time_ticks:0` y nacen completos (`progress 0 >= T 0`), sin caso
especial en construcción/dropoff. Confirmado con un test de control positivo
(§9.3, fixture propio) además del golden real.

**D3 — §4.1, "sin footprint de otro edificio vivo" implementado como
corolario, no como barrido separado.** El paso 5 exige DOS condiciones
("todas las celdas transitables" Y "sin solape con otro edificio vivo"). Al
colocar un edificio se marcan sus celdas `cost_grid = FF_WALL` (efecto de
PLACE_BUILDING); por construcción, cualquier solape futuro con ese edificio
falla el chequeo "transitable" (misma celda, mismo valor `FF_WALL`), así que
NO implementé un segundo barrido O(edificios vivos) para el no-solape — el
único chequeo de celdas ya cubre ambas condiciones del contrato (mismo
`RejectReason::ILLEGAL_STATE` para ambas, así que no hay pérdida de
información observable). Documentado por eficiencia/simplicidad, no cambia
ningún comportamiento observable frente al contrato.

**D4 — §4.1, endurecimiento de cota de footprint contra `FF_AXIS` (256).**
El `cost_grid` de navegación es fijo 256×256 (`FF_AXIS`) independientemente
de `map_tiles_x/y` (que puede configurarse hasta `WORLD_TILES_MAX=8192`) —
la misma tensión preexistente que `FLOW_MOVE` ya "endurece" en el código del
Arquitecto (comentario "el contrato usaba `world_contains` (cota 8192), pero
el campo es FF_AXIS=256"). Añadí el mismo endurecimiento al paso 4 de
PLACE_BUILDING (`MALFORMED` si el footprint excede `FF_AXIS`, no solo si
excede `map_tiles_x/y`) para evitar una lectura fuera de rango de
`cost_grid` si algún match se configurara con mapa >256. Ningún escenario
existente usa mapas >256, así que no cambia trayectoria alguna.

**D5 — §5, `construction_system` mueve al ciudadano directamente, sin pasar
por `tgt_x/tgt_y` ni por `movement_v1`.** El texto literal dice `tgt[i] =
p_cerca // el movement system lo lleva`. Pero `movement_v1` está marcada
**CONGELADA** (SPEC-001 §12) y excluye incondicionalmente `unit_class > 2`
(ciudadanos) sin excepción para "ciudadano en construcción" — delegarle el
desplazamiento habría exigido tocar código congelado (añadir una excepción
al guard de citizens). Elegí NO tocar `movement_v1`: `construction_system`
mueve al ciudadano con el MISMO algoritmo snap-si-cubre/normalize+step que
ya usa `economy.hpp::try_move` (mismas primitivas: `dist_sq_raw`,
`normalize_v1`, `fx_mul`, `fx_add`), sin estado nuevo. El comportamiento
observable — el ciudadano converge a `p_cerca` y, al llegar, suma progreso —
es idéntico al descrito por el contrato; solo cambia el mecanismo interno de
locomoción. Alternativa descartada: modificar el guard de `movement_v1`
(riesgo mayor sobre código explícitamente congelado, para un beneficio nulo
en el comportamiento observable).

**D6 — §7, `entity_kind[j]!=1` como excepción al guard `unit_class[j]>2` en
combat/aggro, en vez de una tabla RPS 4×4.** Para que un edificio (siempre
`unit_class=255`) sea objetivo válido sin reescribir la tabla `rps_mult_bp`
existente (3×3, congelada), añadí `rps_mult_vs_building_bp` como función
aparte (artillery/siege ×2.0, resto ×1.0) y la rama `if (entity_kind[best]
== 1) ... else rps_mult_bp(...)` en el punto de aplicación de daño. La tabla
3×3 original queda intacta byte a byte.

**D7 — §8, sin migración v7→v8 (loader exige versión exacta).** El SPEC dice
"El loader v8 acepta v7 (...) siguiendo el patrón de migración existente" —
pero **no existe tal patrón** en el código: `load_game` rechaza cualquier
archivo cuyo `SAVE_FORMAT_VERSION` no coincida EXACTAMENTE (comprobado; no
hay ninguna rama v6→v7 en el `save_io.hpp` heredado). Esto ya está
documentado como desviación del propio Sprint 0.4 frente a su brief
(`SONNET_KERNEL_DATOS_SPEC002_RESULT.md` §2.1: *"No implementé save
v7/replay v3 literales (...) Forzar esa migración completa (...) es trabajo
de plomería masivo sin beneficio funcional en este incremento"*). Mismo
razonamiento, mismo precedente: no implementé una migración v7→v8 real (no
había nada sobre lo que construirla). Un save v7 real simplemente no carga
bajo este binario v8 (falla el chequeo de versión del envelope, código de
retorno 1, "malformado/versión" — comportamiento seguro, no crash).

**D8 — §8/§9.2, gap heredado de `unit_id` en el formato de replay v2 (no
tocado; escenario de test diseñado para sortearlo).** `replay.hpp` (v2, sin
cambios de formato desde Sprint 0.4) **no serializa** `CmdPayload::unit_id`
— gap ya documentado en `SONNET_KERNEL_DATOS_SPEC002_RESULT.md` §2.4/§2.6,
nunca ejercitado porque ningún escenario previo usaba `SPAWN_UNIT`/`CITIZEN`
con `unit_id` real. `PLACE_BUILDING` reutiliza ese mismo campo como
`BuildingId`, así que un replay grabado con `PLACE_BUILDING` perdería el
`BuildingId` real (se reconstruye como `0` al cargar) — rompiendo, en
general, la reproducción bit-exacta. SPEC-004 §8 exige explícitamente "v2
sin cambios de formato"; tocar `replay.hpp` (código compartido, usado por
`driver.hpp`/`cli_run.hpp`, sensible a la trayectoria golden) estaba fuera
del alcance conservador de este sprint. **Resolución**: el escenario de test
`test_gate_equivalents` (§9.2-equivalente) usa DELIBERADAMENTE `BuildingId=0`
y `UnitId=0` (citizen) como ÚNICOS valores de catálogo referenciados por
`PLACE_BUILDING`/`SPAWN_CITIZEN` en ese escenario concreto — la truncación a
`0` de un campo no serializado coincide, por diseño del fixture de test, con
el valor real, así que la reproducción sigue siendo bit-exacta sin parchear
`replay.hpp`. Esto NO cierra el gap general (un replay real con más de un
tipo de edificio SÍ divergiría); queda documentado aquí para que el
Arquitecto decida si amerita un replay v3 en un sprint futuro (fuera de
alcance de "Parte I: Construcción").

**D9 — §9.2, "G3/G4/G5 verdes con PLACE_BUILDING/ASSIGN_BUILD incluido" vía
un ctest propio, no vía `driver.hpp`/`cli_run.hpp`.** El escenario compartido
del CLI (`build_human_batch` en `driver.hpp`, usado por `savetest`/`record`/
`verify`) es código sensible a la trayectoria golden (SPEC-004 §9.1 exige
esa trayectoria bit-idéntica) — no lo toqué para inyectar comandos de
edificios. En su lugar, `tests/unit/test_buildings.cpp::test_gate_equivalents`
implementa un escenario propio, autocontenido, que ejercita exactamente los
mismos mecanismos que G3 (`save_game`/`load_game` + continuar == corrida
continua)/G4 (mismo, con AiJobBox presente aunque sin IA activa — el
escenario no la necesita)/G5 (`ReplayWriter`/`replay_load` + reproducción,
`schedule_mismatches==0`) pero con `PLACE_BUILDING`+`ASSIGN_BUILD` en la
agenda. Los G3/G4/G5 **reales** del CLI (sin edificios) se corrieron tal
cual y siguen verdes (ver §4).

## 4. Salida de gates (§9)

Build: `nice -n 19 cmake --build build -j2` — **0 warnings, 0 errores**
(`-Wall -Wextra -Wshadow -Werror`). Compilado también en la lane
`gcc-portable` (`-DCHUNSA_WIDE128_FORCE_PORTABLE=ON`): mismo resultado, 14/14
tests, golden 1074/1074. (Nota aparte, no bloqueante: la lane `clang-nativo`
de este equipo de desarrollo —clang 22 + libstdc++ 16— falla al compilar
`data_catalog.hpp` con un error de instanciación de plantillas sobre
`CveValue`/`std::pair` recursivo, **preexistente**: reproducido idéntico
compilando el `data_catalog.hpp` de ANTES de este sprint con el mismo
compilador. No es una regresión de este trabajo; no se investigó más por
estar fuera de alcance.)

**§9.1 — Golden 1074/1074:**
```
$ ./build/chunsa_sim_cli golden --vectors tests/determinism/golden
GOLDEN backend=int128 casos=1074 fallos=0  [OK]
```
(vectores `fixed64_v1.csv`/`normalize_v1.csv`, sin relación con este sprint —
verificado que siguen en verde porque no se tocó `fixed64.hpp`/`vec2fx.hpp`.)

**§9.1 — Trayectoria bit-idéntica pre/post (dump comparativo, no solo
checksum):** se construyó un binario con los headers de ANTES de este
sprint (`git archive 60dedd8`) y otro con los de DESPUÉS, ambos corriendo (a)
el escenario de movimiento sintético (600 unidades, `SPAWN_DEBUG`+`MOVE_TO`
periódico, 2000 ticks, volcando `pos_x/pos_y/hp` de 5 unidades por tick) y
(b) el escenario de economía (8 ciudadanos, 500 ticks, volcando
`player_stock[0]`/`deposits[0].remaining` por tick):
```
$ diff pre_movement.txt post_movement.txt && echo "MOVEMENT: IDENTICO"
MOVEMENT: IDENTICO   (10000 líneas cada uno)
$ diff pre_economy.txt post_economy.txt && echo "ECONOMY: IDENTICO"
ECONOMY: IDENTICO    (500 líneas cada uno)
```
(Los checksums SÍ cambian — es el cambio de dominio intencional v2→v3 de
§8 — pero la trayectoria campo a campo es idéntica byte a byte.)

**§9.2 — G1 alloc_delta=0:**
```
$ ./build/chunsa_sim_cli run --selftest-g1
G1 selftest: alloc_delta=0 OK checksum=1b2dc94a0dd12439
```

**§9.2 — G3/G4/G5 (CLI real, escenario sintético sin edificios — intacto):**
```
$ ./build/chunsa_sim_cli savetest --units 200 --resume-to 400 --save-at 200
G3 savetest(save@200): OK state=2a457d7dcf1d48d8 cont=ede555124bdb2e0c
$ ./build/chunsa_sim_cli savetest --units 200 --resume-to 400 --save-at 200 --ai
G4 savetest(save@200): OK state=795c204be40a4e28 cont=5ba7af51544fe681
$ ./build/chunsa_sim_cli record --out /tmp/g5_final.curp
record: 400 ticks → /tmp/g5_final.curp checksum=795c204be40a4e28
$ ./build/chunsa_sim_cli verify --replay /tmp/g5_final.curp
G5 verify: OK ai_executions=0 schedule_mismatches=0 replay_v=2 checksum=795c204be40a4e28
```

**§9.2 — G3/G4/G5-equivalente con PLACE_BUILDING/ASSIGN_BUILD (ver D9),
dentro de `ctest buildings`:** determinismo puro (dos corridas idénticas),
save-a-mitad+continuar == corrida continua, y record+replay con
`schedule_mismatches==0` — los tres verdes (ver §5, `buildings: OK`).

**§9.3 — ctest completo:**
```
$ ctest --output-on-failure
100% tests passed, 14 tests passed, 0 tests failed out of 14
```
(`props, golden, state, ring, flow_field, flow_move, combat, morale, economy,
aggro, replay_v2, data_blob, buildings, data_compile` — los 13 preexistentes
+ `buildings` nuevo, todos verdes.)

**§9.4 — `-Werror` + cero floats/heap en Step:** build limpio (arriba);
`grep -n "float\|double"` sobre `step.hpp/game_state.hpp/data_catalog.hpp/
commands.hpp` → 0 resultados; `grep` de `new/malloc/std::vector/std::string/
push_back/resize/reserve` sobre `step.hpp` → 0 resultados; `G1
alloc_delta=0` (arriba) lo confirma empíricamente sobre el escenario
sintético existente (el nuevo código de edificios no se ejercita en él, pero
tampoco introduce ningún operador `new`/contenedor STL dentro de
`namespace detail` de `step.hpp` — inspección visual + grep confirmatorio).

## 5. Checksums nuevos

- `CHECKSUM_ALGO_VERSION`: **2 → 3**. Dominio: `"CHUNSA_STATE_V3"` (antes
  `V2`). Símbolo `state_checksum_v1` sin cambios (mismo criterio que el bump
  v1→v2 de Sprint 0.4: no tocar los ~6 call sites existentes).
- `SAVE_FORMAT_VERSION`: **7 → 8**.
- Checksum del escenario de test `buildings` (determinismo, `ctest_gate_equivalents`,
  20 ticks, PLACE_BUILDING+SPAWN_CITIZEN+ASSIGN_BUILD): estable entre corridas
  (verificado en el test, no hardcodeado — el valor exacto depende del layout
  interno y no aporta valor como referencia externa).
- Checksums del CLI real (escenario sintético sin edificios, dominio v3):
  `run --selftest-g1` → `1b2dc94a0dd12439`; `savetest` (sin IA) →
  `2a457d7dcf1d48d8`; `savetest --ai`/`record` → `795c204be40a4e28`.
  Estos SON el "regen de dominio" del golden mencionado en §9.1 — no hay
  archivos de fixture de checksum versionados en el repo (los tests
  comparan dos corridas en caliente, no contra una constante hardcodeada;
  confirmado — no había ningún `.curp`/`.sav` checked-in que regenerar).

## 6. Notas de alcance no-desviación (documentadas para transparencia)

- El catálogo real de edificios (`data/buildings/*.yaml`,
  `data/compiled/chunsa_base.chdb`, `building_count=4`) ya estaba en `main`
  cuando arrancó esta rama (tarea paralela de MiniMax + cierre del
  Arquitecto). Se usó tal cual para el test de "golden real" del catálogo
  (§9.3); no se tocó ningún archivo de `data/`.
- `find_building_dropoff`/`construction_system` recorren linealmente todas
  las entidades filtrando por `entity_kind==1` (sin aceleración por spatial
  hash) — igual de simple que el pseudocódigo de §5/§6 del propio SPEC, que
  tampoco pide aceleración. Con pocos edificios por partida (el slice actual
  tiene 4 tipos) el costo es despreciable; no se optimizó por no estar
  pedido.
- `ASSIGN_BUILD` no valida que `p.x_raw/p.y_raw` estén dentro de la cota de
  mundo antes de buscar el edificio — un tile fuera de cualquier footprint
  simplemente no encuentra match (`INVALID_ENTITY`), que es el único código
  de rechazo que el §4.2 define para ese caso; no hay un `MALFORMED`
  separado por coordenada "rara" (arbitrariamente grande/negativa) porque el
  contrato no lo pide.
