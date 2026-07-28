# RESULT — K2: GATHER + la IA juega la apertura (Sprint 1.6B)

Autor: sonnet-5. Brief: `docs/briefs/SONNET_K2_GATHER_APERTURA_SPRINT_1.6B.md`.
Spec: `docs/specs/SPEC-004_SISTEMAS_PARTIDA.md` Parte III §18/§19/§20;
`docs/specs/SPEC-005_IA_OPONENTE.md` §0 (regla de oro).

## Rama y commits

Rama `sonnet/k2-gather-apertura`, creada desde `main` en el commit
`a0971e7103ecb14b584b11b4bf15d43c108fad39` (merge de K1 — civ tipada +
depósitos desde el mapa, revisado y auditado). **`main` no se tocó**.

3 commits atómicos:

1. `0e42c04` — `feat(kernel): comando GATHER + agotamiento por preferencia
   de recurso (SPEC-004 §18)` — `commands.hpp` (GATHER=13), `step.hpp`
   (validación + efecto), `economy.hpp` (endurece
   `eco_find_nearest_deposit`), `tests/unit/test_gather.cpp`,
   `CMakeLists.txt` (registra los 3 test binaries nuevos de esta pieza).
2. `e5d29ee` — `feat(ai): la IA juega la apertura — civ-aware trainer +
   capa económica GATHER (SPEC-004 §19)` — `ai_stub.hpp` (fix crítico de
   `ai_find_trainer_type` + capa 1.5 económica adaptativa + bump
   `AI_ALGO_VERSION` 2→3), `tests/unit/test_ai_apertura.cpp`.
3. `c85116a` — `feat(scenario): escenario del DoD — apertura económica real
   (SPEC-004 §20)` — `skirmish_apertura.hpp` (escenario nuevo, catálogo
   real), `tests/unit/test_ai_skirmish_apertura.cpp`.

## Qué se implementó

### §18 — GATHER (`step.hpp` + `economy.hpp`)

`GATHER = 13` (append-only). Validación EN ORDEN (testeada en
`test_gather.cpp`): handle vivo/propio (`INVALID_ENTITY`/`NOT_OWNER`) ·
`unit_class==3` (`ILLEGAL_STATE`) · resolver depósito: recorrido ASCENDENTE,
primer índice con `remaining>0` y `dist_sq(punto, depósito) <=
GATHER_PICK_RADIUS_RAW²` (1 tile) gana; ninguno ⇒ `INVALID_ENTITY`. Efecto:
`eco_assigned_deposit = ese índice`, `eco_state = SEEK`, `build_target =
BUILD_NO_TARGET` (recolectar cancela construir, decisión explícita del
contrato).

`eco_find_nearest_deposit` gana un parámetro `preferred_resource_idx`
(sentinela `ECO_ANY_RESOURCE` = "sin preferencia", preserva el
comportamiento legacy exacto para todo caller que no lo use). Dos pasadas
deterministas, mismo criterio dist_sq/desempate-por-menor-índice en ambas:
(1) solo depósitos vivos del recurso preferido; (2) si ninguno, cualquier
recurso vivo. En `eco_step_citizen::SEEK`, la reasignación por agotamiento
(`remaining<=0`) pasa como preferencia el recurso del depósito que se acaba
de agotar; la reasignación de un ciudadano NUNCA asignado
(`ECO_NO_DEPOSIT`/índice fuera de rango) sigue pasando `ECO_ANY_RESOURCE` —
sin cambio ahí, es exactamente el comportamiento pre-existente.

Sin cambio de save/checksum: GATHER reutiliza campos ya serializados/
checksummeados (`eco_assigned_deposit`, `eco_state`, `build_target`) — Save
v12 / `CHUNSA_STATE_V7` de K1 quedan intactos, ningún bump en esta pieza.

### §19 — La IA juega la apertura (`ai_stub.hpp`)

**Fix crítico no pedido literalmente por el brief pero indispensable para el
DoD** (desviación **D1**, ver abajo): `ai_find_trainer_type` gana el
parámetro `civ` (`CivId`). Antes de este sprint el catálogo real tenía UNA
sola civilización jugable a la vez; con las DOS reales de K1 (egipto/rome)
en el MISMO catálogo, el barrido ascendente civ-agnóstico original resolvía
SIEMPRE el primer edificio del catálogo por id (egipto, índices más bajos)
sin importar la civ del jugador que preguntaba — para el jugador de rome
esto significaba: nunca encontrar un edificio suyo
(`ai_find_owned_building_of_type` fallaba por no-ownership), utilidad
económica/militar siempre 0, IA paralizada en silencio (sin ningún rechazo
visible, solo inacción). `civ == INVALID_CIV_ID` preserva el barrido
original — cero impacto en fixtures/tests que nunca asignan civ (todos los
de Sprint 1.4/1.4-cierre).

**Capa 1.5 (económica adaptativa)**, insertada entre el mantenimiento
(ASSIGN_BUILD) y la capa estratégica: `ai_scan_economy` cuenta aldeanos
propios por recurso asignado (barrido ascendente, excluye al ciudadano que
ASSIGN_BUILD ya consumió este ciclo — `skip_index`, evita una orden
contradictoria GATHER-tras-ASSIGN_BUILD en el mismo tick) y localiza el
primer ocioso y el primero de cada recurso. Umbral entero
`AI_GATHER_STOCK_THRESHOLD_BASE * economy_focus_bp / 10000` (mismo patrón bp
que el resto de capas); el recurso con mayor déficit bajo ese umbral (empate
→ menor índice) dispara la redirección: el ocioso tiene PRIORIDAD; si no hay
ninguno, se toma el excedente del recurso con MÁS aldeanos asignados (>1,
para no vaciarlo). `ai_find_deposit_for_resource` resuelve el depósito vivo
más cercano de ese recurso (dist_sq, menor índice) y se emite **como mucho
un GATHER por ciclo** (v1, conservador).

Bump `AI_ALGO_VERSION` 2→3: el procedimiento de decisión cambió (ambos
puntos de arriba alteran qué comandos calcula `ai_execute` para el mismo
`GameState`) — exactamente el caso que SPEC-005 §7 exige bumpear. Sin
impacto en ningún test: `AI_ALGO_VERSION` solo se compara save-a-save dentro
del MISMO binario (ningún `.sav`/`.curp` está committeado en el repo).

### §20 — El escenario del DoD (`skirmish_apertura.hpp`)

Escenario nuevo (NO toca `skirmish.hpp`/`skirmish_eco.hpp`). Usa el
**catálogo real compilado** (`data/compiled/chunsa_base.chdb`): dos civs
reales (egipto/rome), sus centros/aldeanos reales, y los **12
`resource_spawns` reales del mapa** vía `gs_init_economy_from_catalog` — el
punto exacto del sprint (K1 la dejó *opt-in*, nadie la llamaba desde un
escenario hasta ahora). Resolución de ids TOTALMENTE dinámica
(`catalog_find_civ/building/unit`) — cero índices numéricos hardcodeados,
sobrevive a cualquier reordenamiento futuro del blob.

Setup (tick 0, único batch con la exención de escenario, SPEC-004 §10.3):
**SOLO centro + 3 aldeanos por jugador** — cero ejército, cero edificios
militares. Anclas de los centros calculadas para que su centro geométrico
(SPEC-004 §3) caiga EXACTO sobre los `starting_positions` reales del mapa
(20.5/128.5 y 235.5/128.5 tiles).

**Egipto (slot 0)** es "humano-scripted" (mismo precedente que
skirmish.hpp/skirmish_eco.hpp): solo emite el batch de setup, jamás otro
comando — su economía SÍ corre sola (`economy_system` no filtra por
`owner`). **Rome (slot 1)** es la IA de 3 capas real de `ai_stub.hpp`, sin
ninguna ventaja: debe recorrer sola recolectar → construir
(`rome:castra_barracks`) → entrenar (`rome:legionary`/`rome:ballista_crew`)
→ atacar. Se eligió a Rome como el actor que demuestra el camino completo
(desviación **D2**, ver abajo).

## El escenario del DoD — resultado real (§20)

Corrida única (seed `20260724`, `ticks` límite 36000):

```
apertura A: end_tick=12480 winner=1 ai_executions=624
            p0_gather=1 p1_gather=1 p1_built=1 p1_trained=1
            state=cf57ea3ca2266627 cont=5b69fbcea73bb432
```

- **Tick de fin**: 12480 (< 36000, con margen amplio — ~62 s de partida a
  20 Hz de las 30 min de presupuesto).
- **Ganador**: jugador 1 (rome, la IA).
- **Fases confirmadas** (bookkeeping externo al kernel, solo lectura de
  `GameState` al final de cada tick — no alimenta ninguna decisión de
  `ai_execute`/`step`, no rompe la regla de oro):
  - `p0_resources_gathered=true` — egipto (humano-scripted) SÍ acumuló
    stock > 0 en algún recurso (economía autónoma, sin comandos).
  - `p1_resources_gathered=true` — rome (IA) SÍ acumuló stock > 0.
  - `p1_built_military=true` — rome completó un edificio distinto de su
    centro (`castra_barracks`, `build_progress >= build_time_ticks`).
  - `p1_trained_military=true` — rome tiene al menos una unidad de combate
    viva (`unit_class<=2`, imposible al inicio: arranca con cero ejército).
- **624 `ai_executions`** en 12480 ticks (~1 cada 20 ticks, la fase de
  decisión fija de la IA) — sin ningún `ai_stalled`/timeout.

Todo por comandos: el único batch privilegiado es el de `t==0` (setup del
escenario, exención de SPEC-004 §10.3, igual que todos los escenarios
previos); desde ahí, GATHER (autónomo de `economy_system` + explícito de la
IA), TRAIN_UNIT, PLACE_BUILDING, ASSIGN_BUILD, MOVE_TO son TODOS comandos
normales sujetos a la validación completa del kernel.

## Gates completos

- **Golden**: `1074/1074` casos, `0` fallos (`backend=int128`).
- **G1** (alloc_delta con corrida sintética): `alloc_delta=0 OK
  checksum=fefa48125dd35736` — **idéntico al baseline pre-K2** (verificado
  con `git stash` + rebuild).
- **G3** (savetest sin IA): `OK state=969199722657b853
  cont=6145075498b2fb7d ai_executions=0` — idéntico al baseline.
- **G4** (savetest con IA): `OK state=774316057e5667fb
  cont=d52ac0019700684f ai_executions=30` — idéntico al baseline.
- **G5** (record+verify): `OK ai_executions=0 schedule_mismatches=0
  replay_v=3 checksum=774316057e5667fb` — idéntico al baseline.
- **`ctest`**: **25/25** (los 22 preexistentes + `gather`, `ai_apertura`,
  `ai_skirmish_apertura` nuevos), `100% tests passed`, `56 s` totales.
- Build `-Werror` limpio (gcc 16.1.1, `-Wall -Wextra -Wshadow -Werror`),
  térmica `nice -n 19 -j2` en cada build.
- `cero float/heap` en `step.hpp`/`ai_stub.hpp`: verificado por grep
  dedicado en TODO el código añadido (`float|double|new |malloc|std::vector
  |std::string|std::map`) — cero coincidencias fuera de comentarios
  explicativos. `g.tick` tampoco aparece en código (solo en comentarios que
  documentan la regla).

## Regresión: skirmish militar, skirmish_eco, sintéticos (dump pre/post)

Metodología: `git stash` de TODO el diff de esta pieza, rebuild en la misma
máquina/compilador, capturar checksums; `git stash pop`, rebuild, repetir.
Comparación byte a byte de los `state=`/`cont=` impresos.

| Escenario | `state` (pre==post) | `cont` (pre==post) | Resultado |
|---|---|---|---|
| `skirmish` (CLI, militar) | `3f64d3223b74d477` | `92ec9aa95374a429` | **bit-idéntico** |
| `skirmish_eco` (test) | `d610feef89ed9c65` | `5e1527e0921edf27` | **bit-idéntico** |
| G1 selftest (sintético) | `fefa48125dd35736` | — | **bit-idéntico** |
| G3 savetest (sintético) | `969199722657b853` | `6145075498b2fb7d` | **bit-idéntico** |
| G4 savetest (sintético+IA) | `774316057e5667fb` | `d52ac0019700684f` | **bit-idéntico** |
| `test_economy.cpp` (economía 0.3) | `a221f5271ba57219` → `94b41581127166c5` | — | **CAMBIA — deviación D3, ver abajo** |

## Desviaciones numeradas

**D1 — `ai_find_trainer_type` gana el parámetro `civ` (fix no pedido
literalmente, pero indispensable).** El brief no menciona explícitamente
este cambio, pero sin él el escenario del DoD es estructuralmente imposible
para el jugador de la civ con ids de catálogo más altos (rome, en el
catálogo real): la IA jamás encontraría un edificio propio y quedaría
económica/militarmente paralizada para siempre, sin ningún rechazo visible
que lo delate (ver "Qué se implementó — §19"). Justificación: SPEC-004 §19
dice literalmente "ahora posible: `trains` poblado + `civ_id`" — esta es
precisamente la pieza que lo hace posible; K1 tipó `civ_id` en los datos
pero no tocó `ai_stub.hpp` (fuera de su alcance). Bump `AI_ALGO_VERSION`
2→3 documentado en el commit `e5d29ee` y en el header de `ai_stub.hpp`.

**D2 — Rome (no egipto) es el actor que recorre el camino completo del
DoD.** Ambos jugadores arrancan simétricos (centro + 3 aldeanos, catálogo
real); el brief no fija cuál de los dos debe ser el "atacante" que
construye/entrena/ataca. Se eligió Rome porque TODO su contenido real
(`forum_center`/`castra_barracks`/`legionary`/`ballista_crew`/
`camp_work_crew`) vive en una única época (5, que coincide con su época
inicial derivada de `gs_init_epoch_from_catalog_per_player`) — la IA nunca
necesita depender de `EPOCH_UP` para desbloquear su cuartel. Egipto, en
cambio, tiene `chariotry_stable` en `epoch_window [3,4]` pero
`chariot_warrior` exige época 4 exacta mientras su época inicial es 3 (el
mínimo de su catálogo, fijado por `settlement_center`/`work_crew`): sigue
siendo jugable por la IA v1 (el edificio SÍ se puede construir a época 3;
`EPOCH_UP` se auto-satisface una vez completado, gate (a) — 2 edificios
propios en la ventana de época actual — y basta esperar el gate (b) de
tiempo), pero entrelazaría la demostración de §18/§19 con la mecánica de
épocas de SPEC-004 §12, fuera del alcance de este brief. Egipto SÍ demuestra
la fase de recolección (economía autónoma, humano-scripted, sin comandos
tras el setup) — la "asimetría deliberada" documentada es el mismo
precedente que `skirmish.hpp`/`skirmish_eco.hpp` (defensor humano-scripted
vs IA atacante), NO una ventaja injusta: ambos arrancan con exactamente lo
mismo.

**D3 — `test_economy.cpp` (escenario sintético de recolección, Sprint 0.3)
cambia su checksum informativo tras el endurecimiento de §18.** Ese
escenario ejercita deliberadamente el agotamiento de depósitos (es su razón
de ser); con el endurecimiento de `eco_find_nearest_deposit` (preferir el
MISMO recurso al reasignar), un ciudadano cuyo depósito cercano se agota
ahora puede viajar hasta OTRO depósito lejano del mismo recurso en vez del
más cercano de recurso distinto — cambia la trayectoria (posiciones,
`stock0` final), y por tanto el checksum de estado. **Esto es la
consecuencia literal y esperada de implementar §18 tal como está escrito**,
no una regresión: (a) el test NO tiene un checksum dorado hardcodeado —
solo autoconsistencia `checksum1==checksum2` (dos corridas frescas con el
código nuevo, que siguen coincidiendo entre sí) más los `CHECK` funcionales
(`stock>0`, `deposit0.remaining<500`, `alive_citizens==N`) — todos siguen en
verde, `economy: OK`; (b) `skirmish`/`skirmish_eco`/G1/G3/G4 (los
escenarios que el brief nombra explícitamente como objetivo de bit-
identidad) SÍ quedan bit-idénticos porque, dentro de su presupuesto de
ticks, ningún depósito llega a agotarse por completo — la ruta nueva de
código nunca se ejercita ahí. No se bumpeó ningún dominio de checksum por
esto (`CHUNSA_STATE_V7` intacto): no se añadió NINGÚN campo nuevo al
estado, solo cambió el ALGORITMO de reasignación sobre campos ya
existentes — un bump de dominio habría sido cosmético, no habría corregido
la divergencia de trayectoria (que es real y correcta), así que no
correspondía.

## Cómo se garantizó la regla de oro (SPEC-005 §0) en la capa económica nueva

1. **Cero `g.tick`/reloj real**: `ai_scan_economy`/`ai_find_deposit_for_
   resource`/la capa 1.5 completa solo leen `GameState` (posiciones, stock,
   `eco_assigned_deposit`, `build_target`, `player_civ`) y el `AiProfileV1`
   ya resuelto en el paso 0 — verificado por grep (`g\.tick` no aparece en
   código, solo en comentarios).
2. **Cero float/double**: todas las cantidades (umbral, déficit, dist_sq)
   son enteras (`int32_t`/`int64_t`/`uint64_t`); el umbral usa basis points
   con división entera, mismo patrón que el resto de `ai_stub.hpp`.
3. **Cero heap/STL dinámico**: `AiEcoStateV1`/`AiDepositPickV1` son structs
   POD con arrays fijos (`[3]`); ningún `new`/`malloc`/`std::vector`/
   `std::string` en el archivo (grep dedicado, 0 coincidencias).
4. **Cero RNG**: todo desempate es por menor índice (recurso, depósito,
   ciudadano) — igual que el resto de capas ya auditadas de Sprint 1.4;
   `AI_TIEBREAK` no se usa ni se necesita.
5. **Orden de emisión canónico**: la capa 1.5 se ejecuta en un punto FIJO
   del cuerpo de `ai_execute` (entre el paso 1 y el paso 2), emite como
   mucho 1 comando, y usa el mismo `emit()`/contador `count` compartido que
   el resto de capas — el presupuesto `AI_MAX_COMMANDS` sigue siendo un
   único contador monótono, nunca se resetea ni se bifurca.
6. **Determinismo verificado por test**: `test_ai_execute_deterministic_
   with_economic_layer` (dos llamadas a `ai_execute` con el MISMO
   `GameState` congelado) exige `result[]` byte-idéntico, incluyendo el
   `GATHER` nuevo — en verde.
7. **Sin romper el invariante del scheduler** (auditoría Opus, Sprint 1.4,
   P2, documentado en el header de `ai_stub.hpp`): la capa nueva no toca
   `ai_dispatch`/`ai_execute`/`ai_commit` como lifecycle, solo añade cuerpo
   de decisión dentro de `ai_execute` ya existente.

## Notas para Opus (auditoría del determinismo)

- El punto más delicado de esta pieza es la capa 1.5: revisar que
  `ai_scan_economy` es un barrido puro (sin mutación de `g`, `const
  GameState&`) y que `ai_find_deposit_for_resource` tampoco muta nada.
- El fix de `ai_find_trainer_type` (D1) es un cambio de COMPORTAMIENTO no
  trivial (afecta qué construye/entrena la IA) — merece revisión aparte de
  si el filtro por civ es exactamente el que describe SPEC-004 §17
  (`civ != INVALID_CIV_ID && bdef.civ_id != civ` → excluye, igual que el
  gate de `TRAIN_UNIT`/`PLACE_BUILDING` en `step.hpp`).
- El bump `AI_ALGO_VERSION` 2→3 es el criterio correcto per SPEC-005 §7;
  confirmar que no hace falta ningún otro bump (save/checksum) — mi análisis
  es que NO, porque ningún campo nuevo de `GameState` se serializa/
  checksummea en esta pieza (todo GATHER/economía reutiliza estado
  existente de K1/Sprint 0.3).
- D3 (test_economy.cpp cambia de trayectoria) es la decisión más discutible
  de todo el reporte — agradezco especialmente el escrutinio de si el
  Arquitecto/Opus la consideran aceptable o si prefieren que reescriba ese
  test para fijar un catálogo/depósitos que NUNCA agoten (evitando el
  cambio de trayectoria por completo, a costa de perder cobertura real del
  agotamiento en ese archivo — la cobertura de agotamiento SÍ existe ahora,
  completa, en `test_gather.cpp`).

## El punto del sprint, confirmado

`gs_init_economy_from_catalog` — opt-in desde K1, nunca llamada desde un
escenario — se invoca en `skirmish_apertura.hpp` y carga los **12 depósitos
reales** del mapa `base:demo_desert_basin` (verificado: sin ella, `n_deposits`
se queda en 6, el patrón legacy fijo; con ella, `n_deposits==12`, posiciones
y `amount` exactos del YAML real). El escenario corre de punta a punta con
datos 100% reales (civs, centros, aldeanos, depósitos) y cero mutaciones
privilegiadas fuera del batch de `t==0`.
