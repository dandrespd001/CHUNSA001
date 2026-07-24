# RESULT — K2: producción, tecnología y épocas (Sonnet · Sprint 1.2)

Fecha: 2026-07-24
Rama: `sonnet/k2-produccion-tech` desde `main` @ `5b79094` (main ya incluía K1:
replay v3, save v9, checksum v4, blob real building=6/tech=4).
Brief: `docs/briefs/SONNET_K2_PRODUCCION_TECH_SPRINT_1.2.md` — SPEC-004 §11+§12+§13

## Commits

```
7289085 tests: producción, tecnología y épocas (SPEC-004 §13, subconjunto K2)
4299183 kernel: producción, tecnología y épocas (SPEC-004 §11+§12, save v10)
```

Sin merges a `main`. `main` no se tocó durante todo el trabajo.

## Alcance ejecutado

- **`data_catalog.hpp`**: `TechDefinitionV1`/`TechNameIndexV1`/`CapabilityNameIndexV1`
  tipados con el MISMO patrón endurecido de unidades/edificios (CveValue
  acotado, unique_ptr, reserve exacto antes de llenar, rechazo del catálogo
  entero ante cualquier fallo, orden bytewise en `catalog_find_tech`/
  `catalog_find_capability`). `UnitDefinitionV1` gana `cost_a/b/me`, `pop_cost`
  (constante), `epoch_min/max`. `BuildingDefinitionV1` gana `epoch_min/max`,
  `trains[8]`/`train_count`, `researches[8]`/`research_count`,
  `required_capabilities[8]`/`required_capabilities_count`. Referencias
  building→tech y tech→tech se resuelven en un pase posterior a parsear TODAS
  las secciones (building.researches y tech.prerequisites/mutually_exclusive_with
  pueden citar un record_id que aún no existía en la tabla en el momento de su
  propio record — ver desviación 5 más abajo sobre el orden real de
  `declared_capabilities`).
- **`game_state.hpp`**: `prod_queue/prod_count/prod_progress`, `rally_x/y/set`,
  `pop_used` (§11.2); `player_techs/player_caps/player_epoch`, `research_tech/
  progress` (§12.2) + `epoch_initial` (desviación 2). `gs_init_epoch_from_catalog`
  nuevo, llamado explícitamente (no fusionado en `gs_bind_catalog`, ver desviación 9).
- **`commands.hpp`**: `TRAIN_UNIT=9`, `SET_RALLY=10`, `RESEARCH_TECH=11`,
  `EPOCH_UP=12`, append-only. `CmdPayload` sin cambios de layout (todos
  reutilizan campos existentes).
- **`step.hpp`**: los 4 comandos nuevos + `production_system`/`research_system`
  (tras `construction_system`, antes de DESTROY) + gating §12.4 retro-aplicado
  a `PLACE_BUILDING` (exento en la misma ventana de setup que constructible/
  costes) + contabilidad de `pop_used` en el destroy batch (muerte de unidad y
  muerte de edificio con cola pendiente).
- **`checksum.hpp`/`serialize.hpp`/`save_io.hpp`**: `CHECKSUM_ALGO_VERSION`
  4→5 (`CHUNSA_STATE_V5`), `SAVE_FORMAT_VERSION` 9→10; los 12 arrays nuevos
  (§11.2+§12.2+epoch_initial) al final, mismo orden en ambos archivos.
- **Tests**: `tests/unit/test_production_tech.cpp` (nuevo) + ajuste del
  fixture `mini_chdb::build()` de `test_data_blob.cpp` (Sprint 1.1) con los
  campos ahora obligatorios del schema real.

## Fórmula de época inicial (elegida y documentada en el código)

`gs_init_epoch_from_catalog`: **mínimo de `epoch_window[0]` de cada
`UnitDefinitionV1`, `epoch_window[0]` de cada `BuildingDefinitionV1`, y
`epoch` de cada `TechDefinitionV1`** en el catálogo ya enlazado — es decir,
la época más temprana jugable de cualquier dato del slice, **catálogo-ancho**
(no por civilización/jugador: el kernel no tipa `civ_id` por unidad/edificio,
así que no hay forma de derivar "la época inicial de ESTE jugador" sin ese
dato; §12.2 tampoco distingue civilización). Si el catálogo está vacío, 1 por
defecto (caso defensivo, inalcanzable con un catálogo real).

Con el blob real del repo (building=6/tech=4): el mínimo lo fijan los
edificios/unidad egipcios de época 3 (`egipto:settlement_center`,
`egipto:shena_granary`, `egipto:chariotry_stable`, `egipto:work_crew`, todos
`epoch_window[0]==3`) → **época inicial = 3**. Esto exige que el gate §12.4
de `PLACE_BUILDING` se exima en la ventana de setup del tick 0 (igual que
constructible/costes): los edificios romanos iniciales (`epoch_window`
`[5,5]`) se colocan antes de que el jugador tenga época 5, y solo la
exención de escenario lo permite — ver desviación 8.

`gs_init_epoch_from_catalog` se llama EXPLÍCITAMENTE tras `gs_bind_catalog`
(no se fusionaron): ver desviación 9 sobre por qué y su implicación para el
adaptador Godot.

## Salida de gates

**ctest** (`build/`, gcc nativo, `-Werror`, build limpio desde cero):

```
100% tests passed, 16 tests total (props, golden, state, ring, flow_field,
flow_move, combat, morale, economy, aggro, replay_v2, data_blob, buildings,
replay_v3, production_tech, data_compile)
```

**G1** (`chunsa_sim_cli run --selftest-g1`):
```
G1 selftest: alloc_delta=0 OK checksum=544ae6be04ced646
```

**G3** (`savetest --units 200 --resume-to 400 --save-at 200`):
```
G3 savetest(save@200): OK state=309d6ecce04905ff cont=dcf70d737c3f6584
```

**G4** (`savetest ... --ai`):
```
G4 savetest(save@200): OK state=86cbed58173a9c80 cont=590d7a84a721ea9f
```

**G5** (`record --units 200 --ticks 400` + `verify`):
```
record: 400 ticks → g5.curp checksum=86cbed58173a9c80
G5 verify: OK ai_executions=0 schedule_mismatches=0 replay_v=3 checksum=86cbed58173a9c80
```

**golden** (vectores Fixed64, `tests/determinism/golden/`), 2 lanes locales
(gcc + portable — clang no compila en este entorno, ver nota abajo):
```
gcc:      GOLDEN backend=int128    casos=1074 fallos=0  [OK]
portable: GOLDEN backend=portable  casos=1074 fallos=0  [OK]
```
`gcc` vs `portable` normalizados (`backend=X`) idénticos byte a byte.

**Nota de entorno (no es una desviación de K2, idéntica a la documentada en
K1):** la lane `clang` de `scripts_ci/local_gates.sh` sigue sin compilar en
este entorno — mismo error de tipo incompleto
(`std::pair<std::string, CveValue>`) al combinar `clang++` con los headers de
`libstdc++` de GCC 16 en `data_catalog.hpp` (archivo que K1 ya reportó con el
mismo fallo, ANTES de que K2 tocara nada de este sprint). Confirmado
reproduciendo `main` @ `5b79094` (pre-K2) con el mismo error. Se ejecutaron
manualmente las lanes `gcc` y `portable` (idénticas) como evidencia
suficiente de que el kernel no diverge entre backends de aritmética de 128 bits.

**Trayectoria dump pre/post** (`chunsa_test_replay_v3`, escenario SIN
comandos de producción/tech — solo tipos de comando pre-K2 — delay=0,
`target_tick==t` en cada comando, agenda pendiente siempre vacía en cada
punto de control): idéntico entre dos corridas independientes, exactamente
el mismo estándar y metodología que documentó K1. El dump CRECE en longitud
(campos nuevos añadidos al final de `gs_serialize`, cambio de formato
intencional del bump save v9→v10) pero es bit-idéntico entre las dos
corridas de ESTE sprint:
```
dump checkpoint tick=10  sha256=d57846d852ec44ddeb013791a9329049c7c2c81c56451258688bf5dad3150031 len=141669
dump checkpoint tick=60  sha256=e951e57cb2884967e338fba3b07919e5f91f14d074eb4e3c41728ff7ef211cc2 len=142789
dump checkpoint tick=150 sha256=78b79e0d35b0cd3000abcf9c2be20e78e9cbbe2943f848746f38bc1250012008 len=144469
```
(K1 reportó 138349/139469/141149 bytes en los mismos checkpoints; el
crecimiento de +3320 bytes por checkpoint corresponde exactamente a los 12
arrays nuevos de §11.2/§12.2 sobre las 44 entidades + `MAX_EMITTERS=16` del
escenario de ese test.)

**Cero float/heap en Step**: `grep -n "float\|double\|new \|malloc\|std::vector\|std::string" step.hpp` → sin coincidencias (verificado tras añadir `production_system`/`research_system`, que no asignan memoria ni usan floats).

**Demo headless**: NO se corrió (no se tocó el adaptador Godot; `CHUNSA_BUILD_GODOT=OFF` por defecto y no hay gate automatizado que la ejercite en este repo — ver desviación 9 sobre el riesgo documentado para la demo interactiva).

## Checksums/versiones nuevas

- `CHECKSUM_ALGO_VERSION`: 4 → **5** (dominio `"CHUNSA_STATE_V5"`).
- `SAVE_FORMAT_VERSION`: 9 → **10**.
- `REPLAY_WRITE_VERSION`: sin cambios (sigue en 3; los comandos 9-12 viajan
  como cualquier `CommandType` existente, sin cambio de formato).

## Desviaciones (numeradas)

1. **Época inicial catálogo-ancho, no por civilización.** El kernel no tipa
   `civ_id` por unidad/edificio (Sprint 0.4/1.1 lo dejaron fuera de alcance),
   así que "la época inicial de este jugador" no es derivable por civ. La
   fórmula usa el mínimo GLOBAL del catálogo — ver sección dedicada arriba.

2. **`GameState::epoch_initial[MAX_EMITTERS]` no está en el pseudocódigo
   literal de §12.2**, pero el gate (b) de `EPOCH_UP` (§12.3) exige
   "época_inicial" como término de la fórmula de tiempo mínimo, y sin
   persistirlo aparte de `player_epoch` (que SÍ cambia con cada `EPOCH_UP`)
   la fórmula no es reconstruible tras un `EPOCH_UP` + save/load. Se añadió
   como campo de estado nuevo, serializado/checksummeado igual que el resto
   de §12.2, fijado una única vez en `gs_init_epoch_from_catalog`.

3. **`UnitDefinitionV1` no tipa `required_capabilities`.** El schema
   `unit.schema.json` v2 no declara ese campo (a diferencia de building/tech).
   El gate §12.4 correspondiente de `TRAIN_UNIT` pasa trivialmente sobre el
   conjunto vacío — coincide literalmente con la nota del brief "los defs
   iniciales del slice no requieren capacidades (compat)".

4. **`TechDefinitionV1` no tipa `required_buildings`.** El kernel gatea
   `RESEARCH_TECH` por `BuildingDefinitionV1::researches` (relación inversa,
   §11.1), no por ese campo del schema de tech. El compilador Python sí
   valida `required_buildings` (referencia resoluble a building) a nivel de
   datos; el consumo en `Step()` de Parte II no lo necesita.

5. **Hallazgo de integración: `manifest.declared_capabilities` (y en general
   los "sets" del schema que `chunsa_data_compiler.py::_normalize` reordena)
   NO llegan al blob en orden bytewise-ascendente del record_id.** El
   criterio real de `_normalize` es `sorted(vals, key=cve_encode)`, y para
   strings `cve_encode` antepone la LONGITUD (u32 LE) a los bytes UTF-8 — el
   orden resultante es "longitud primero", no alfabético. Verificado contra
   el blob real: `rome:legion_drill`(18) < `egipto:corvee_levy`(19) <
   `egipto:coordinate_flood`(23) < `egipto:composite_bow_craft`(27) <
   `rome:maintain_public_infrastructure`(36) — CLARAMENTE por longitud, no
   alfabético. El loader NO asume ningún orden de entrada para esta lista:
   recoge las capacidades y las ORDENA ÉL MISMO (bytewise por record_id,
   `std::sort` + rechazo de duplicados adyacentes) antes de construir la
   tabla buscable — `CapabilityId` = índice en la tabla ya ordenada por el
   loader, no por el blob. NO se tocó el compilador Python (fuera de
   alcance); es una corrección de una suposición incorrecta en el lado C++,
   detectada por el propio test contra el golden real (`data_blob` falló con
   `NonCanonical` hasta corregirlo).

6. **Los 6 edificios reales del slice dejan `researches: []` vacío** (los
   YAML de `data/buildings/` no popularon ese campo pese a que las 4 techs sí
   declaran `required_buildings` apuntando a 2 de esos edificios — no hay
   reverse-population automática compilador↔loader para esta relación).
   Consecuencia: `RESEARCH_TECH` sobre el catálogo REAL del repo SIEMPRE
   rechaza con `MALFORMED` ("tech no está en researches") hasta que datos
   futuros pueblen ese campo. Es un hueco de DATOS (fuera del alcance del
   kernel — el loader tipifica fielmente lo que llega), no un bug del
   kernel; documentado conservadoramente. Los tests de comportamiento de
   RESEARCH_TECH (feliz/prereq/mutex/época/ocupado) usan un catálogo fixture
   in-memory con `researches` poblado explícitamente (mismo patrón que
   `test_buildings.cpp`/`test_replay_v3.cpp`), NO el blob real; el test del
   blob real (`test_catalog_real_golden`) solo verifica la resolución
   estructural de `trains`/`grants`, tal como pide el brief literalmente
   ("los 2 cuarteles resuelven sus `trains` y las 4 techs sus `grants`" —no
   dice nada de `researches`).

7. **`pop_used` se decrementa por CUALQUIER unidad que muera**, no solo las
   entrenadas vía `TRAIN_UNIT`/producción — `pop_cost=1` es constante v1
   para TODA unidad y `GameState` no distingue el origen del spawn
   (`SPAWN_UNIT`/`SPAWN_CITIZEN`/camino debug nunca incrementan `pop_used`).
   Se clampa a 0 para no ir negativo. Efecto: en escenarios mixtos (unidades
   fuera de la cola de producción conviviendo con entrenadas), `pop_used`
   puede terminar más bajo de lo estrictamente "correcto" (una unidad que
   nunca reservó población libera población de todos modos al morir). El
   brief no especifica cómo distinguir el origen del spawn; se optó por la
   lectura más simple y determinista del texto ("la muerte de una unidad
   reduce pop_used").

8. **El gate de época/capacidades (§12.4) se exime en la MISMA ventana de
   setup de escenario que constructible/costes para `PLACE_BUILDING`**
   (`effective_tick==0`) — extensión natural de la exención ya existente del
   Arquitecto (2026-07-23, SPEC-004 §4.1.2/§4.3): los edificios iniciales de
   escenario nacen antes de que `player_epoch`/`player_caps` tengan sentido
   para el jugador que los coloca (con la fórmula de época inicial=3, un
   edificio romano `epoch_window=[5,5]` colocado en el tick 0 fallaría el
   gate sin esta exención). Es una extensión razonada del contrato existente,
   no un re-litigio de §4.1.2.

9. **`gs_init_epoch_from_catalog` NO se fusiona en `gs_bind_catalog`** —
   llamada explícita separada. Motivo: `gs_bind_catalog` ya tenía 3+ call
   sites en tests de sprints previos (`test_data_blob.cpp`,
   `test_buildings.cpp`, `test_replay_v3.cpp`) que NO esperan que enlazar el
   catálogo mueva `player_epoch`; fusionar los dos habría cambiado su
   comportamiento en silencio. **Implicación documentada para el adaptador
   Godot** (`chunsa_sim_node.cpp`, fuera de alcance — NO tocado): enlaza el
   catálogo asignando `gs->catalog` DIRECTAMENTE (ni siquiera pasa por
   `gs_bind_catalog`) y nunca llama a `gs_init_epoch_from_catalog` —
   `player_epoch` queda en 0 (default de `gs_init`) en la demo interactiva.
   Con los datos reales del slice (todos los edificios `epoch_window[0]>=3`),
   CUALQUIER `PLACE_BUILDING` manual del jugador fuera de la ventana de
   setup del tick 0 sería rechazado por el gate §12.4 en la demo. Riesgo
   documentado para quien integre la UI de producción/tech/época — NO una
   regresión de los gates automatizados (`ctest`/golden/G1/G3/G4/G5, que no
   ejercitan la demo interactiva; `CHUNSA_BUILD_GODOT=OFF` por defecto, sin
   gate que la construya o corra en este repo).

10. **`EPOCH_MAX_V1=7` fijado como constante literal**, no derivado del
    catálogo pese a la prosa "época máxima del slice, de los datos del
    match" del brief — el propio brief cierra la frase con "v1: constante 7",
    tomado literalmente como el valor v1.

11. **Sin CANCEL de cola de producción ni de research** (pre-aceptado
    explícitamente en el brief, repetido aquí por completitud): la muerte
    del edificio pierde su cola/research en curso sin reembolso de costes ya
    pagados; solo se libera la población reservada de los ítems de la cola
    NO entrenados (los ya entrenados/spawneados siguen contando población
    hasta que ELLOS mueran, vía la desviación 7).

12. **Caps del kernel más estrictos que el blob**: `TECH_HARD_CAP=256` y
    `CAP_HARD_CAP=256` (el directorio del blob permite hasta 65535 records
    tech; el manifest no tiene un cap explícito de capacidades más allá de
    `MAX_COLLECTION=65535` del CVE) — mismo espíritu que el footprint 1..8 de
    `BuildingDefinitionV1` en Parte I (más estricto que el 1..32 del schema).
    Elegidos múltiplos de 64 a propósito (`TECH_WORDS`/`CAP_WORDS` en
    `GameState` sin resto). Con los datos reales (4 techs, 5 capacidades) no
    se estresan en absoluto.

## Archivos tocados

- `addons/chunsa_sim/core/include/chunsa/data_catalog.hpp` — tech/capacidades tipados, resolución diferida de referencias
- `addons/chunsa_sim/core/include/chunsa/game_state.hpp` — §11.2/§12.2 + `gs_init_epoch_from_catalog`
- `addons/chunsa_sim/core/include/chunsa/commands.hpp` — `TRAIN_UNIT`/`SET_RALLY`/`RESEARCH_TECH`/`EPOCH_UP`
- `addons/chunsa_sim/core/include/chunsa/step.hpp` — 4 comandos + production_system/research_system + gating §12.4 + pop_used en destroy
- `addons/chunsa_sim/core/include/chunsa/checksum.hpp` — `CHECKSUM_ALGO_VERSION=5`
- `addons/chunsa_sim/core/include/chunsa/serialize.hpp` — save v10 + rango de `CommandType` a 12
- `addons/chunsa_sim/core/include/chunsa/save_io.hpp` — `SAVE_FORMAT_VERSION=10`
- `tests/unit/test_data_blob.cpp` — fixture `mini_chdb::build()` con campos nuevos obligatorios
- `tests/unit/test_production_tech.cpp` — nuevo
- `CMakeLists.txt` — registro de `chunsa_test_production_tech`

**NO tocados** (explícitamente fuera de alcance): `addons/chunsa_sim/gdextension/*`,
`demo/*`, `data/*` (YAML/schemas/manifest), `tools/data_compile/chunsa_data_compiler.py`.
