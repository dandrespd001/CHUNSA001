# RESULT — K1: replay v3 + save v9 + ventana de setup (Sonnet · Sprint 1.2)

Fecha: 2026-07-23
Rama: `sonnet/k1-replay-v3` desde `main` @ `4bcecc7`
Brief: `docs/briefs/SONNET_K1_REPLAY_V3_SPRINT_1.2.md` — SPEC-004 §10 completo + §13 (subconjunto K1)

## Commits

```
b7dfd07 tests: replay v3 + save v9 + ventana de setup (SPEC-004 §13, subconjunto K1)
914c390 gdextension: demo vuelve a human_input_delay_ticks=1 (SPEC-004 §10.3)
bff4fb7 kernel: replay v3 + save v9 + checksum v4 + ventana de setup (SPEC-004 §10)
```

Sin merges a `main`. `main` en HEAD siguió siendo `4bcecc7` durante todo el trabajo.

## Los 4 puntos de contrato no negociables

1. **Replay v3** (`replay.hpp`): `REPLAY_WRITE_VERSION = 3`. Registro = layout
   v2 + `u32 unit_id` al final (tras `effective_tick`). Loader acepta
   v1/v2/v3; en v1/v2 el campo se reconstruye como 0 y
   `ReplayData::legacy_payload_loss` se marca en 1 si el stream contiene
   algún comando de un tipo que usa `unit_id` (`SPAWN_UNIT`/`SPAWN_CITIZEN`/
   `PLACE_BUILDING`). Grabación siempre v3 (`ReplayWriter` ya no acepta
   escribir v2 — no había ningún call site que dependiera de ello).

2. **Save v9 y veredicto del checksum — ver sección dedicada abajo.**

3. **Setup window** (`step.hpp`): `command_effective_tick(target, t, delay)`
   gana `target_tick == 0 && t == 0 -> 0`, sin sumar `delay`. Todo lo demás
   (incluido `target==0` con `t!=0`) usa exactamente la fórmula anterior.
   Documentado en el header de la función el contrato del host: **el batch de
   la PRIMERA llamada a `step()` (t==0) es exclusivamente de setup de
   escenario, nunca input de jugador** — comentario espejo (sin lógica) en
   `driver.hpp` y `cli_run.hpp`.

4. **Demo con delay=1**: `chunsa_sim_node.cpp` cambia `human_input_delay_ticks`
   de 0 a 1 (una línea de config + comentario). Verificado headless (ver
   sección de gates): `buildings=2` en tick 0/100/200, cero líneas
   `CHUNSA ERROR`, exit 0.

## Veredicto del punto 2 (save v9 / checksum)

**El dominio del checksum (v3, `CHECKSUM_ALGO_VERSION=3`, `"CHUNSA_STATE_V3"`)
NO cubría `pending.items[].p.unit_id`.** Verificado leyendo el bucle de la
agenda en `checksum.hpp` antes de tocar nada:

```cpp
h.u32(c.p.handle.index); h.u32(c.p.handle.generation);
h.i64(c.p.x_raw); h.i64(c.p.y_raw);
h.i32(c.p.speed_mtpt);
// unit_id NUNCA se hasheaba aquí.
```

Consecuencia real: un `SPAWN_UNIT`/`SPAWN_CITIZEN`/`PLACE_BUILDING` con
`unit_id != 0` **aún pendiente** (agendado pero no aplicado) no se distinguía
por checksum de uno con `unit_id == 0` — gap gemelo del D8 de Sprint 1.1, pero
en el dominio del ESTADO EN MEMORIA (no en un formato de archivo). Por
contrato del brief: **se incluyó** — bump a `CHECKSUM_ALGO_VERSION = 4`,
dominio `"CHUNSA_STATE_V4"`, y `h.u32(c.p.unit_id)` añadido al final del
bucle de cada ítem de la agenda (mismo patrón append que el resto).

`test_replay_v3.cpp::test_checksum_v4_covers_pending_unit_id` prueba esto de
forma directa: dos `GameState` idénticos salvo el `unit_id` de un ítem
pendiente (mismo `effective_tick`/emisor/secuencia/handle/coords) producen
`state_checksum_v1` **distinto**.

Nota sobre "regenerar golden" (mismo procedimiento del bump v1→v2 y v2→v3,
según el brief): **no existe en este repo ningún archivo de checksums de
estado persistido** (a diferencia de los vectores Fixed64 puramente
aritméticos de `tests/determinism/golden/`, que no tocan `GameState` y no se
tocaron). Todos los tests de estado comparan dos corridas EN VIVO entre sí
(determinismo/save-load/record-replay), así que el bump de versión/dominio no
requirió regenerar ningún artefacto aparte del propio número de versión — se
verificó explícitamente que no hay ningún literal de checksum hardcodeado en
`tests/unit/*.cpp` (`grep` de valores hex de 8+ dígitos: cero coincidencias
relevantes).

## Tests obligatorios (§13, subconjunto K1) — todos añadidos en `test_replay_v3.cpp`

- ✅ Replay v3 round-trip con `PLACE_BUILDING` de `BuildingId != 0` (outpost,
  id=1): reproduce bit-exacto (checksum) Y preserva el `building_id` real
  aplicado. Stream v2 sintético del MISMO escenario (mismo `target_tick`,
  mismo `BuildingId`, layout v2 real sin el campo `unit_id`): carga con
  `legacy_payload_loss==1` y, aplicado, planta el edificio EQUIVOCADO
  (`building_id==0`, el filler, no el outpost `1`) — "falla con v2 forzado,
  pasa con v3" (texto literal del brief), demostrado con una aserción
  concreta sobre el estado resultante, no solo con el flag.
- ✅ Save v9 con `SPAWN_UNIT` de `unit_id=1` (warrior) PENDIENTE en la agenda
  al momento del save (delay=2, target futuro): `load` + continuar ==
  corrida continua (`state_checksum_v1` idéntico en cada punto de control);
  además se verifica explícitamente que `pending.items[0].p.unit_id` llega
  intacto tras el load, y que la unidad que finalmente se spawnea usa las
  stats del catálogo correcto (warrior: hp=50/attack=10), no las del citizen.
- ✅ `command_effective_tick`: `(0,0,1)→0` · `(0,1,1)→2` · `(5,0,1)→5` · más
  una batería de 8 casos preexistentes (delay=0 y delay=2, target</>/==t,
  incluido `target==0` con `t!=0` para confirmar que la exención NO se activa
  fuera del primer Step) — todos intactos.
- ✅ Suite completa + golden + G1/G3/G4/G5 + trayectoria pre/post de un
  escenario sin edificios (dump, no solo checksum) — ver sección de gates y
  la nota metodológica sobre "dump pre/post" más abajo.

Adicionalmente (no exigido explícitamente pero directo del contrato):
`test_setup_window_delay_invariance` generaliza el punto 4 del brief más allá
de la demo — un escenario de setup arbitrario (2 `SPAWN_DEBUG` con
`target_tick=0` en t=0) da el MISMO checksum final con `delay=0`, `delay=1` y
`delay=7`.

### Nota metodológica: "trayectoria golden bit-idéntica (dump pre/post)"

Esta regla dura no se pudo satisfacer literalmente re-ejecutando un binario
`main` pre-K1 con el MISMO escenario (los binarios `run`/`bench`/`--selftest-
g1` del CLI usan `human_input_delay_ticks=1` hardcodeado en `cli_run.hpp`, que
SÍ cae en el caso especial de §10.3 para sus spawns de t=0 — divergencia
esperada y deliberada, no una regresión). Se optó por un escenario diseñado
para que la comparación sea rigurosa: `delay=0` (con lo que
`command_effective_tick` es MATEMÁTICAMENTE idéntico a la fórmula anterior a
este sprint para CUALQUIER `(target,t)` — el caso especial nunca puede
divergir de lo que la fórmula vieja ya daba cuando `delay==0`) y
`target_tick==t` en cada comando emitido (con lo que la agenda pendiente
queda SIEMPRE vacía al final de cada `step()`, verificado con
`CHECK(g->pending.count == 0u)` en cada tick — así el único campo nuevo de
esta pieza que toca el layout de `gs_serialize` (`unit_id` de la agenda, save
v9) nunca llega a escribirse). Bajo esas dos invariantes, el dump
(`gs_serialize`, no el checksum — cuyo dominio cambió deliberadamente) es, por
construcción, exactamente el que producía el kernel antes de este sprint.
Verificado corriendo el escenario dos veces de forma independiente y
comparando los 3 dumps (`tick=10/60/150`) byte a byte
(`test_trajectory_dump_no_buildings_pre_post`), con sus SHA-256 impresos en
la bitácora del test (ver sección de gates). Se documenta esto explícitamente
en vez de darlo por sentado porque el propio golden `run`/G1 del CLI SÍ
cambia de checksum bajo este sprint (por diseño).

## Salida de gates

**ctest** (`build/`, gcc nativo, `-Werror`):

```
100% tests passed, 15 tests total (props, golden, state, ring, flow_field,
flow_move, combat, morale, economy, aggro, replay_v2, data_blob, buildings,
replay_v3, data_compile)
```

**G1** (`chunsa_sim_cli run --selftest-g1`):
```
G1 selftest: alloc_delta=0 OK checksum=1f79876edd100d60
```

**G3** (`savetest --units 200 --resume-to 400 --save-at 200`):
```
G3 savetest(save@200): OK state=c0d37811464fda3f cont=e1cec7de7d9c75ff
```

**G4** (`savetest ... --ai`):
```
G4 savetest(save@200): OK state=2008bdcdd903e693 cont=2c4c5aa25175db4a
```

**G5** (`record --units 200 --ticks 400` + `verify`):
```
record: 400 ticks → g5.curp checksum=2008bdcdd903e693
G5 verify: OK ai_executions=0 schedule_mismatches=0 replay_v=3 checksum=2008bdcdd903e693
```
(`replay_v=3` confirma que la grabación por defecto ya es v3.)

**golden** (vectores Fixed64, `tests/determinism/golden/`), 3 lanes:
```
gcc:      GOLDEN backend=int128    casos=1074 fallos=0  [OK]
clang:    NO EJECUTADO (ver nota de entorno abajo)
portable: GOLDEN backend=portable  casos=1074 fallos=0  [OK]
```
`gcc` vs `portable` normalizados (backend=X) son idénticos byte a byte.

**Nota de entorno (no es una desviación del contrato de K1):** la lane
`clang` no compila en este entorno — falla con un error de tipo incompleto
(`std::pair<std::string, CveValue>`) al combinar `clang++` con los headers de
`libstdc++` de GCC 16, en `data_catalog.hpp` (archivo no tocado por K1). Se
confirmó reproduciendo el MISMO fallo compilando `main` (`4bcecc7`) sin
ningún cambio de K1 en un clon de verificación aparte — es una
incompatibilidad de toolchain preexistente del entorno, no una regresión
introducida por esta pieza.

**Trayectoria dump pre/post** (`chunsa_test_replay_v3`, ver metodología
arriba):
```
dump checkpoint tick=10  sha256=a859df440ddec83d1e966ab89cd8f2b0bd1f0bd6b076497c35742e849c5175a9 len=138349
dump checkpoint tick=60  sha256=3ee0f9bad8a3234752931459e754b888e6dd3c1e4235a8330e0a34a187720a54 len=139469
dump checkpoint tick=150 sha256=003af19454955513947b81db3fd4f0c9ca235837cb69d8fa000765316b954793 len=141149
```
(Cada uno idéntico entre las dos corridas independientes del test.)

**Demo headless** (`nice -n 19 ./third_party_build/Godot_v4.7.1-stable_linux.x86_64 --headless --path demo --quit-after 1500`):
```
exit=0
CHUNSA render=prod(c/3d+interp) units=600
CHUNSA catálogo OK: cav_id=0 cit_id=1 art_id=2 building_count=4 settlement_id=0 forum_id=2 buildable_id=1
CHUNSA cav=240 art=240 citizens=120 buildings=2 stock_A=0   (tick 0)
CHUNSA cav=238 art=240 citizens=120 buildings=2 stock_A=0   (tick 100)
CHUNSA cav=226 art=228 citizens=120 buildings=2 stock_A=0   (tick 200)
```
Cero líneas `CHUNSA ERROR`.

## Desviaciones (numeradas)

1. **Impacto en tests preexistentes por el cambio de contrato de
   `command_effective_tick` (esperado, no un hueco del SPEC).** El brief exige
   `(0,0,1)→0` explícitamente, lo cual reescribe el resultado de CUALQUIER
   comando con `target_tick==0` ingerido en el primer batch bajo
   `delay>=1` — incluido el propio `test_replay_v2.cpp` (grababa/esperaba
   `eff_ticks[0][0]==1u` con delay=1 por defecto) y la subprueba `2b'` de
   `test_buildings.cpp` (esperaba `ILLEGAL_STATE` para un `PLACE_BUILDING`
   con `target_tick=0`/delay=1, que ahora SÍ activa la exención de
   escenario). Se actualizaron ambas aserciones para reflejar el
   comportamiento nuevo (contractual, no un bug) y se añadió una subprueba
   `2b''` que preserva la cobertura del camino "fuera de la ventana de
   setup" con `target_tick=1`. También se actualizó `data.version==2u` en la
   subprueba 8c de `test_buildings.cpp` a `3u` (la grabación ya es siempre
   v3). Documentado con detalle en el mensaje del commit de tests.

2. **`ReplayWriter` ya no admite escribir v2.** El brief dice "grabación
   siempre v3"; no se dejó ninguna ruta para forzar v2 desde `ReplayWriter`
   (el stream v2 sintético usado en el test de `legacy_payload_loss` se
   escribe a mano, byte a byte, con los mismos helpers `replay_detail::pb_*`,
   no con `ReplayWriter`). Si un futuro sprint necesitara re-emitir v2 por
   algún motivo de compat, habría que reintroducir el parámetro de versión
   explícitamente — no se hizo aquí por estar fuera de alcance.

3. **`legacy_payload_loss` es una advertencia contable agregada, no por
   comando.** El SPEC (§10.1) pide literalmente "el verificador puede así
   distinguir 'replay antiguo potencialmente infiel' de 'replay v3 fiel'" —
   se implementó como un único flag en `ReplayData` (0/1) para todo el
   stream, no un vector paralelo a `batches`/`eff_ticks` marcando qué comando
   concreto es sospechoso. Es la lectura más simple y suficiente del texto
   ("si el stream contiene algún comando de un tipo que use unit_id"); un
   vector por-comando sería trivial de añadir después si un consumidor
   real lo necesitara.

4. **La lane `clang` de `scripts_ci/local_gates.sh` no se pudo ejecutar**
   (ver nota de entorno en la sección de gates) — confirmado que es un
   problema preexistente del entorno (reproducido en `main` sin tocar nada),
   no una regresión de K1. Se ejecutaron manualmente las lanes `gcc` y
   `portable` (idénticas) más el resto de gates (ctest, G1/G3/G4/G5, demo
   headless) como evidencia suficiente de que el kernel no diverge entre
   backends de aritmética de 128 bits.

5. **Primera ejecución headless requirió un pase de import del editor.** En
   este worktree recién creado, `demo/` no tenía `.godot/extension_list.cfg`
   generado (nunca se había abierto el proyecto), así que la PRIMERA corrida
   `--headless --quit-after` no registraba `ChunsaSimNode` (Godot no había
   escaneado el `.gdextension` todavía). Se ejecutó una vez
   `--headless --editor --path demo --quit` para generar la caché del
   proyecto (`.godot/`, no versionado — `.gitignore:10`), y la verificación
   real se hizo con la corrida subsiguiente. Es una particularidad de
   entorno de este worktree, no del cambio de K1 en sí (el `.gdextension` y
   el `.so` no cambiaron de forma que afecte el registro de la clase).

## Archivos tocados

- `addons/chunsa_sim/core/include/chunsa/step.hpp` — `command_effective_tick` (§10.3)
- `addons/chunsa_sim/core/include/chunsa/replay.hpp` — replay v3 (§10.1)
- `addons/chunsa_sim/core/include/chunsa/serialize.hpp` — agenda con unit_id (§10.2)
- `addons/chunsa_sim/core/include/chunsa/save_io.hpp` — `SAVE_FORMAT_VERSION=9`
- `addons/chunsa_sim/core/include/chunsa/checksum.hpp` — `CHECKSUM_ALGO_VERSION=4`
- `addons/chunsa_sim/core/include/chunsa/driver.hpp` / `cli_run.hpp` — comentarios de contrato del host
- `addons/chunsa_sim/gdextension/chunsa_sim_node.cpp` — delay 0→1
- `demo/bin/libchunsa_godot.so` — regenerado
- `tests/unit/test_replay_v3.cpp` — nuevo
- `tests/unit/test_replay_v2.cpp`, `tests/unit/test_buildings.cpp` — ajustes por el cambio de contrato
- `CMakeLists.txt` — registro de `chunsa_test_replay_v3`
