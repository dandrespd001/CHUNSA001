# RESULT — K2: la IA de 3 capas + escenario skirmish + gate de fase (Sonnet · Sprint 1.4, pieza 2)

Fecha: 2026-07-24
Rama: `sonnet/k2-ia-execute` desde `main` @ `ba0a2f8` (K1 ya integrado: victoria/derrota,
perfil de IA tipado, save v11/checksum v6)
Brief: `docs/briefs/SONNET_K2_IA_EXECUTE_SPRINT_1.4.md` — SPEC-005 §4/§5 (la IA) + §8.3/§8.4
(gate de fase)

Sin merges a `main`. `main` en HEAD siguió siendo `ba0a2f8` durante todo el trabajo
(verificado con un checkout limpio aparte, ver la sección de gates).

## Commits

```
31c8468 feat: cuerpo real de ai_execute — IA de 3 capas (SPEC-005 §4/§5)
4dc4508 test: fixtures de las 3 capas de la IA (SPEC-005 §8.4)
9610953 feat: escenario CLI de skirmish — gate de fase (SPEC-005 §8.3)
```

## Resumen del skirmish (el corazón del gate)

```
skirmish: OK end_tick=1236 game_over=1 winner=1 ai_executions=62
          state=fe34b80541a40007 cont=ca0b366d3cc88711 fatal=NONE
          alloc_delta_total=2
```

**Tick de fin: 1236** (muy por debajo del límite de 36000 = 30 min).
**Ganador: emisor 1** (el atacante — la IA de 3 capas real), determinista y reproducido
bit a bit en gcc y en el backend `portable` de Wide128 (mismos 4 valores exactos).

`AI_ALGO_VERSION` sube de **1 a 2** (el procedimiento de decisión cambió por completo —
`ai_stub.hpp`).

## Diseño del escenario (por qué gana la IA y por qué termina)

El escenario (`chunsa/skirmish.hpp`, **archivo nuevo** — no se tocó `build_human_batch` ni
`drive()` de `driver.hpp`) es **humano-scripted (owner=0, "defensor") vs IA real (owner=1,
"atacante")**, la opción más simple de las dos que el brief permite (§8.3: "IA vs IA o
humano-scripted vs IA"). El defensor recibe **exclusivamente** el batch de setup en `t==0`
(su centro + su ejército inicial) y **jamás** vuelve a emitir un comando en el resto de la
partida — es "script" en sentido literal. El atacante es la IA de 3 capas de `ai_stub.hpp`
sin ninguna trampa ni atajo: lee el mismo `GameState` que cualquier jugador, decide por
utilidad, entrena refuerzos y dirige el asalto.

**Asimetría deliberada y documentada** (para que `winner` nunca sea `0xFF`, exigencia
literal de §8.3): el atacante arranca con 6 soldados contra los 4 del defensor, **y**, a
diferencia del defensor, sigue reforzando su ejército por `TRAIN_UNIT` durante toda la
partida (mismo catálogo — coste 0 — pero solo el atacante tiene una IA que emite esos
comandos). La ventaja numérica del atacante solo crece con el tiempo; nunca converge a un
empate por simetría perfecta.

**Por qué el catálogo del skirmish no tiene NINGÚN tipo "ciudadano" (0 en ambos bandos)** —
la deviación de diseño más importante de esta pieza, documentada en el header del archivo:
la derrota v1 (SPEC-005 §6, `step.hpp::detail::victory_check`, ya mergeada en K1) exige
"0 edificios **Y** 0 ciudadanos". El kernel (SPEC-004 §7, Sprint 1.1) **excluye
explícitamente** a los ciudadanos (`unit_class==3`) como objetivo de `combat_system` y de
`aggro_system` — son intocables en combate por diseño, en cualquier versión del kernel hasta
hoy. Si **cualquiera** de los dos jugadores conservara un solo ciudadano vivo, jamás
satisfaría esa pata de la derrota, sin importar cuántos edificios perdiera — la partida no
terminaría **nunca** por esta vía, violando directamente el requisito "< 36000 ticks" del
gate. Verifiqué esto leyendo el guard de `combat_system`/`aggro_system` línea a línea antes
de diseñar el escenario (no es una suposición). La solución más simple y robusta: un
catálogo puramente militar (sin unidad de clase `Citizen`) hace que esa pata de la derrota
se satisfaga trivialmente para ambos bandos, y el desenlace depende limpiamente de la caída
del centro del perdedor. La capa **económica** de `ai_execute` (SPEC-005 §4.1, "entrenar
ciudadanos") SÍ está implementada de forma completamente genérica en el cuerpo de la IA —
se ejercita aparte, con un catálogo que sí tiene ciudadanos, en
`tests/unit/test_ai_layers.cpp` — pero en este escenario concreto nunca encuentra un "tipo
entrenador de ciudadanos" en el catálogo (no existe) y por tanto su utilidad es siempre 0 y
se salta con gracia: no es un defecto de la capa, es que este catálogo no la necesita.

## La IA de 3 capas (`ai_stub.hpp`) — diseño y regla de oro

Un único barrido macro (`ai_scan_macro`) por ciclo de decisión cuenta ciudadanos/ejército
propio, localiza el ancla (primer edificio propio vivo, barrido ascendente) y el centroide
del ejército (suma de posiciones / cuenta, división entera). El perfil se resuelve vía
`catalog_find_ai_profile(cat, "base:demo_normal", ...)` con un fallback embebido de los
mismos valores literales para catálogos sin sección `ai-profile` (los fixtures aislados de
sprints previos que `ai_execute` también debe soportar sin romperse).

1. **Mantenimiento** (`ASSIGN_BUILD`): si hay un sitio propio incompleto sin ningún
   ciudadano con `build_target` apuntándole, y un ciudadano ocioso disponible, se asigna —
   independiente de qué intención estratégica gane el ciclo.
2. **Estratégica** (SPEC-005 §4.1): 4 intenciones mutuamente excluyentes (economía / construir
   / militarizar / tech-época), cada una con una utilidad entera en basis points tomada
   directamente de los pesos del perfil (`economy_focus_bp`/`military_focus_bp`/
   `tech_focus_bp`) cuando su condición de elegibilidad se cumple (afford/época/capacidades/
   cola/pop, todos enteros), 0 si no. Gana la de mayor utilidad; **empate → menor índice de
   intención** (economía < construir < militarizar < tech), sin RNG.
3. **Reactiva vs táctica** (SPEC-005 §4.2/§4.3): si hay una unidad de combate enemiga dentro
   de `AI_REACTIVE_RADIUS_MT` (20 tiles) del ancla → el ejército **defiende** (`MOVE_TO` al
   ancla) — prioridad estricta sobre atacar. Si no, y `army_count >= umbral` (umbral =
   `10 - expansion_aggressiveness_bp/1000`, clamp [1,10], entero) → **ataca** el edificio
   productor enemigo más cercano al centroide (`dist_sq_raw` entera, empate → menor índice,
   mismo criterio que `combat_system`/`aggro_system`).

### Cómo se garantizó la regla de oro (SPEC-005 §0) en cada capa

- **Cero `g.tick`/reloj real**: grep exhaustivo de `ai_stub.hpp` — toda mención de `g.tick`
  aparece exclusivamente en comentarios (verificado, ver sección de gates). El único punto
  donde una regla del KERNEL depende de `g.tick` (el gate (b) de `EPOCH_UP`, tiempo mínimo
  desde la época inicial, SPEC-004 §12.3) se deja **deliberadamente sin pre-verificar** en la
  capa TECH: solo se comprueba el gate (a) (recuento de edificios, función pura de `g` sin
  tick). La IA emite `EPOCH_UP` optimista y dependeix del kernel para rechazarlo si el tiempo
  no ha pasado — exactamente el patrón "emite lo que cree válido, lee el rechazo el ciclo
  siguiente" que SPEC-005 §5 describe, en vez de leer `g.tick` por la puerta trasera para
  adivinar el gate del kernel.
- **Cero float/double**: grep exhaustivo — cero ocurrencias fuera de comentarios. Toda
  aritmética (centroide, distancias, umbrales, basis points) es entera: `int32_t`/`int64_t`/
  `uint64_t`, incluida la división entera del centroide y del umbral de ataque (determinista
  por definición del lenguaje, sin UB).
- **Cero heap/STL dinámico**: grep exhaustivo — cero `new`/`malloc`/`std::vector`/
  `std::string` en `ai_stub.hpp`. Todo el estado intermedio (`AiMacroStateV1`,
  `AiTrainerTypeV1`, `AiOwnedBuildingV1`, `AiFreeCellV1`, `AiTargetV1`) son structs pequeños
  en pila; el resultado vive en `box.result[AI_MAX_COMMANDS]` (ya reservado en el propio
  `AiJobBox`). Prueba EMPÍRICA (no solo por inspección): `alloc_delta_total` del subcomando
  `skirmish` es **2** (la única `new GameState()` + el único `std::vector<RawCommand>`
  reservado UNA VEZ antes del bucle, mismo patrón que `driver.hpp::drive`) **idéntico** con
  `--ticks 50` (3 `ai_executions`), `--ticks 1236` (62) y `--ticks 36000` (62) — cero
  asignación escala con el número de ticks o de decisiones de la IA.
- **Cero entropía / RNG**: v1 **no usa RNG en absoluto** — grep confirma cero llamadas a
  `rng_draw`/`rng_range` en todo `ai_stub.hpp` (las únicas menciones de `AI_TIEBREAK` son en
  comentarios). Todo desempate (utilidad estratégica, distancia táctica/reactiva, elección de
  sitio de construcción) es determinista por **menor índice** — más simple de auditar que
  introducir el stream `AI_TIEBREAK` sin una necesidad real de aleatoriedad en v1.
- **Orden de emisión canónico ascendente**: el orden de emisión es el orden de *llamada* al
  lambda `emit` dentro del cuerpo de `ai_execute` — fijo por construcción (mantenimiento →
  estratégica → reactiva/táctica), y dentro de la reactiva/táctica, barrido ascendente por
  índice de entidad. `sequence` es estrictamente creciente (`seq_base + count + 1`), `count`
  se corta en `AI_MAX_COMMANDS` de forma determinista (mismo prefijo siempre para el mismo
  estado de entrada — verificado en `test_ai_execute_deterministic`).

## Tests obligatorios (§8.4)

### `chunsa_test_ai_layers` (6 tests, catálogo sintético en memoria — citizen/soldier,
center/barracks; NO usa el CHDB real)

- ✅ **Construye un cuartel**: `PLACE_BUILDING` (aceptado por el kernel) y, en el ciclo de
  decisión siguiente, `ASSIGN_BUILD` del ciudadano ocioso al sitio incompleto (también
  aceptado) — verificado aplicando de verdad los comandos vía `step()`, no solo inspeccionando
  `result[]`.
- ✅ **Entrena**: `TRAIN_UNIT` de un ciudadano desde el centro completo, aceptado; aparece un
  ciudadano nuevo (deviación de test documentada: `citizen.build_time_ticks==1` hace que
  `production_system` complete el ítem en el MISMO tick en que se encola, así que `prod_count`
  vuelve a 0 de inmediato — la evidencia estable de "entrenó" es la nueva entidad, no el
  contador transitorio).
- ✅ **Ataca**: con el ejército sobre el umbral y sin amenaza cerca de la base, `MOVE_TO` de
  las 5 unidades propias hacia el edificio enemigo más cercano — aceptado, `tgt_x/y`
  actualizado.
- ✅ **Defiende** (capa reactiva, no exigido explícitamente por el brief pero cubierto): un
  soldado enemigo a 5 tiles del ancla prioriza `MOVE_TO` al ancla sobre atacar.
- ✅ **Presupuesto**: 80 soldados propios → `result_count == AI_MAX_COMMANDS` exactamente (64),
  nunca lo excede.
- ✅ **Determinismo de `ai_execute`**: mismo `(g, source_tick, runtime)` → `result[]` idéntico
  campo a campo en dos llamadas independientes.

### `chunsa_test_ai_skirmish` (4 tests, GameState siempre en heap vía `make_unique`)

- ✅ La partida **concluye en victoria real**: `game_over==1`, `winner=1`, `end_tick=1236 <
  36000`, `ai_executions=62 > 0`.
- ✅ **Determinismo**: dos corridas INDEPENDIENTES (GameState/caja/runtime frescos cada una,
  mismos parámetros) dan el mismo `winner`, el mismo `end_tick` y los mismos
  `final_checksum`/`continuation_checksum`.
- ✅ **Save a mitad + continuar == corrida continua**: guarda en `end_tick/2` (tick 618) y
  recarga en un `GameState` nuevo; mismo `winner`/`end_tick`/`final_checksum`/
  `continuation_checksum` que la corrida continua. Ver desviación de test #6 abajo (re-enlazar
  el catálogo tras `load_game`).
- ✅ **Replay bit-exacto**: graba y reproduce en feed-mode; `ai_executions==0` en feed-mode,
  `schedule_mismatches==0` (agenda auto-verificada v2), mismo `final_checksum`/`winner`/
  `end_tick` que la grabación. Ver desviación de test #7 abajo (`ticks` del recorder = tick de
  fin real, no el límite de 36000).

## Salida de gates

**ctest** (`build/` gcc nativo y `build-portable/` con `-DCHUNSA_WIDE128_FORCE_PORTABLE=ON`,
ambos `-Wall -Wextra -Wshadow -Werror`):

```
100% tests passed, 19 tests total (props, golden, state, ring, flow_field, flow_move,
combat, morale, economy, aggro, replay_v2, data_blob, buildings, replay_v3,
production_tech, victory_ai_profile, ai_layers, ai_skirmish, data_compile)
```
Idéntico en ambas lanes.

**G1** (`chunsa_sim_cli run --selftest-g1`), gcc y portable:
```
G1 selftest: alloc_delta=0 OK checksum=2defd6416796e3d8
```
(No usa IA — este selftest es el mismo de siempre; ver más abajo el G1-con-IA vía el
`alloc_delta_total` del skirmish.)

**G1 con IA activa** (SPEC-005 §8.1, "la IA no asigna en Step"), evidencia empírica vía el
subcomando `skirmish` (no un ctest — medido directamente sobre `g_chunsa_allocs`):
```
--ticks 50:    alloc_delta_total=2  (ai_executions=3,  game_over=0, NO-CONCLUYENTE)
--ticks 1236:  alloc_delta_total=2  (ai_executions=62, game_over=1, OK)
--ticks 36000: alloc_delta_total=2  (ai_executions=62, game_over=1, OK)
```
Las 2 asignaciones son el único `new GameState()` + el único `std::vector<RawCommand>`
reservado ANTES del bucle (mismo patrón que `driver.hpp::drive`) — constantes sin importar
cuántos ticks o decisiones de IA ocurran. `ai_execute`/`step()` no asignan.

**G3** (`savetest --units 200 --resume-to 400 --save-at 200`, SIN IA), gcc y portable:
```
G3 savetest(save@200): OK state=794d43a2dd8333a8 cont=8b3a30f0b0eb11f6 ai_executions=0
```
**Idéntico** al mismo comando corrido contra un checkout limpio de `main` @ `ba0a2f8` (ver
nota metodológica "trayectoria golden" más abajo).

**G4** (`savetest ... --ai`, CON la IA real de K2), gcc y portable:
```
G4 savetest(save@200): OK state=2681ad5f3eb161ad cont=486c9601301ff753 ai_executions=30
```
Idéntico bit a bit entre gcc y portable (matriz del gate).

**G5** (`record --units 200 --ticks 400` + `verify`), gcc y portable:
```
record: 400 ticks → g5.curp checksum=2681ad5f3eb161ad
G5 verify: OK ai_executions=0 schedule_mismatches=0 replay_v=3 checksum=2681ad5f3eb161ad
```
El mismo archivo `.curp` grabado en gcc se verifica OK también en el binario portable
(mismo checksum, `ai_executions=0`, `schedule_mismatches=0`).

**golden** (vectores Fixed64, `tests/determinism/golden/`), gcc y portable:
```
gcc:      GOLDEN backend=int128    casos=1074 fallos=0  [OK]
portable: GOLDEN backend=portable  casos=1074 fallos=0  [OK]
```
Idéntico a `main` @ `ba0a2f8` (comparación directa, ver abajo).

**skirmish** (nuevo, el gate §8.3), gcc y portable — valores **idénticos**:
```
skirmish: OK end_tick=1236 game_over=1 winner=1 ai_executions=62
          state=fe34b80541a40007 cont=ca0b366d3cc88711 fatal=NONE alloc_delta_total=2
```

### Nota metodológica: "trayectoria golden de escenarios SIN IA bit-idéntica (dump pre/post)"

A diferencia de sprints anteriores (donde a veces solo se pudo argumentar "por inspección
de código"), aquí se hizo la comparación **directa**: se clonó `main` @ `ba0a2f8` a un
directorio aparte (`git clone --local --no-hardlinks --branch main`), se construyó su CLI sin
ningún cambio de K2, y se corrieron `G1`, `G3` (sin `--ai`) y `golden` contra ese binario.
Los tres checksums/resultados (`2defd6416796e3d8`, `794d43a2dd8333a8`/`8b3a30f0b0eb11f6`,
`1074/1074`) son **exactamente los mismos** que en la rama `sonnet/k2-ia-execute`. Esto es
evidencia más fuerte que "por diseño no debería cambiar": es la comparación real pre/post que
pedía el brief. Tiene sentido categóricamente — K2 solo tocó `ai_stub.hpp` (código que
`step()`/los escenarios sin IA jamás invocan), `skirmish.hpp` (archivo nuevo y autocontenido)
y `main.cpp` (un subcomando nuevo + dos campos informativos añadidos a la salida existente,
cero cambio de lógica) — ningún escenario sin IA puede divergir por construcción.

## Desviaciones (numeradas)

1. **Escenario del gate sin ciudadanos (0 en ambos bandos).** Ver la sección de diseño
   arriba — es la deviación central de esta pieza, forzada por una interacción real entre
   SPEC-005 §6 (derrota v1) y SPEC-004 §7 (los ciudadanos son intocables en combate desde
   Sprint 1.1). Sin esto, el gate §8.3 no podría cumplirse nunca (ninguna partida terminaría
   en <36000 ticks). Documentado en detalle en el propio `chunsa/skirmish.hpp`.
2. **"Humano-scripted vs IA" en vez de "IA vs IA".** El brief permite ambas opciones
   explícitamente; se eligió la más simple (un único `AiJobBox`, reutilizando el lifecycle
   0.1B sin ninguna extensión) para minimizar el riesgo de divergencia en la pieza más
   sensible del sprint. El defensor "scripted" es, literalmente, el batch de setup de t==0 y
   nada más — no hay ninguna lógica de "script" oculta que decida nada en tiempo de partida.
3. **`retreat_hp_threshold_bp`/`retreat_morale_threshold_bp` del perfil NO se leen en
   `ai_execute`.** SPEC-005 §4.3 asigna esa responsabilidad textualmente al "kernel de moral
   YA EXISTENTE" (Sprint 0.3, `morale_system`), que usa sus propias constantes fijas
   (`MORALE_PANIC`/`MORALE_RALLY`) y no está parametrizado por el perfil — conectar esos
   campos a `morale_system` está fuera de alcance de K2 (no hay mandato de tocar un sistema
   congelado desde 0.3 más allá de lo que el propio brief pide).
4. **`SET_RALLY` no se usa en v1.** No había necesidad funcional: los reclutas nuevos
   (`production_system` los spawnea en el punto fijo bajo el edificio si no hay rally) se
   incorporan automáticamente al "ejército" (barrido ascendente por `unit_class<=2`) en el
   siguiente ciclo de decisión, sin importar su posición de spawn.
5. **"Un edificio por tipo entrenador" — simplificación v1 de la capa CONSTRUIR.** Si el
   jugador IA ya posee (viva, completa o no) una instancia de un tipo de edificio "cuartel"
   (definido genéricamente como: el primer tipo del catálogo, ascendente, con `train_count>0`
   y al menos una entrada no-ciudadano), la utilidad de "construir" cae a 0 — v1 no
   contempla construir un SEGUNDO cuartel del mismo tipo aunque el perfil/economía lo
   justificarían. Documentado en el comentario de `ai_find_owned_building_of_type`/el
   candidato CONSTRUIR de `ai_execute`.
6. **Umbral de ataque — fórmula v1 concreta.** `10 - expansion_aggressiveness_bp/1000`
   (clamp [1,10], entero) es una fórmula v1 explícita para traducir el peso bp en un tamaño
   mínimo de ejército; SPEC-005 §4.2 no fija una fórmula literal, solo dice "≥ umbral de
   `expansion_aggressiveness_bp`" — se eligió esta por ser simple, monótona y enteramente
   entera. `AI_ECON_TARGET_CITIZENS=6` y `AI_BASE_SEARCH_RADIUS_TILES=24` son constantes v1
   análogas (mismo espíritu que `POP_CAP_V1`/`EPOCH_MIN_TICKS` en `step.hpp`), no derivadas
   del perfil.
7. **Test de save/continuar (§8.3): re-enlazar el catálogo tras `load_game` es
   responsabilidad EXPLÍCITA del caller.** Detectado durante el desarrollo del test (no del
   kernel): `g.catalog` es un binding runtime puro (documentado desde Sprint 0.4 en
   `game_state.hpp`) que **no** se serializa; tras `load_game`, `g->catalog` queda `nullptr`
   hasta llamar a `gs_bind_catalog` de nuevo. Sin ese re-enlace, la capa estratégica de la IA
   se apaga en silencio a partir del punto de carga (sigue funcionando la reactiva/táctica,
   que no depende del catálogo) — el `winner`/`end_tick` coincidían igual por la ventaja
   numérica ya acumulada, pero el `final_checksum` divergía. El test ahora re-enlaza
   explícitamente; se documenta aquí como advertencia para cualquier integrador futuro del
   escenario (CLI/Godot) que implemente su propio "continuar partida guardada".
8. **Test de replay (§8.3): el recorder necesita el tick de fin EXACTO, no el límite de
   36000.** `ReplayWriter::begin` fija `ticks` en la cabecera del archivo y `replay_load`
   exige encontrar exactamente esa cantidad de registros; como la partida termina temprano
   (`game_over` congela el bucle de `drive_skirmish` mucho antes de `o.ticks`), grabar con
   `ticks=36000` produciría una cabecera que promete 36000 registros con solo ~1236
   presentes → `replay_load` fallaría por EOF prematuro. El test corre primero SIN grabar
   para conocer el tick de fin real y graba una segunda vez con `ticks` = ese valor exacto —
   no reintroduce no-determinismo (la corrida grabada reproduce el mismo resultado que
   cualquier otra corrida idéntica, ya probado en el test de determinismo).
9. **CLI: `savetest` gana el campo `ai_executions` en su línea de salida** (suma de
   `ai_executions` de ambas corridas A/B) — adición puramente informativa para poder
   reportar el gate G4 con evidencia de que la IA realmente ejecutó, sin cambiar ningún
   comportamiento ni formato de archivo.

## Archivos tocados

- `addons/chunsa_sim/core/include/chunsa/ai_stub.hpp` — cuerpo real de `ai_execute` (IA de 3
  capas), `AI_ALGO_VERSION` 1→2, lifecycle del job intacto.
- `addons/chunsa_sim/core/include/chunsa/skirmish.hpp` — **nuevo**: catálogo sintético,
  batch de setup, `drive_skirmish`/`drive_skirmish_fresh`.
- `addons/chunsa_sim/cli/main.cpp` — subcomando `skirmish`; `ai_executions` en `savetest`.
- `tests/unit/test_ai_layers.cpp` — **nuevo**: 6 tests de las 3 capas en fixture aislado.
- `tests/unit/test_ai_skirmish.cpp` — **nuevo**: 4 tests del gate §8.3.
- `CMakeLists.txt` — registro de `chunsa_test_ai_layers`/`chunsa_test_ai_skirmish`.

Sin cambios en `step.hpp`, `game_state.hpp`, `checksum.hpp`, `serialize.hpp`, `save_io.hpp`,
`replay.hpp`, `data_catalog.hpp`, `commands.hpp`, `driver.hpp` (`build_human_batch`/`drive`
intactos, per brief) ni en ningún dato de `data/`.
