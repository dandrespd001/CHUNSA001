# SPEC-004 — Sistemas de partida · Partes I–III

Versión 0.3 (Partes I–II EJECUTADAS en Sprints 1.1/1.2; Parte III DRAFT ejecutable para Sprint 1.6B) · 2026-07-24 · Autor: Arquitecto (Claude)
Jerarquía: INDICE_MAESTRO → SPEC_ARQUITECTURA_BASE v1.1.1 → SPEC-001 v1.1 → SPEC-002 → **este documento**.
Partes futuras (efectos de stats por tech, recetas M2–M3, IV+) se añadirán en sprints posteriores **append-only**.

---
# PARTE I — Construcción (Sprint 1.1, EJECUTADA — ver REPORTE_SPRINT_1.1)

## §1 Objetivo y alcance del incremento
Los edificios entran al kernel como **entidades** de la misma `EntityTable` (comparten
pos/hp/owner/serialización), con: colocación validada por comando, sitio en
construcción que los ciudadanos completan, colisión real en el `cost_grid` de
navegación, destrucción por combate, y el **dropoff económico deja de ser un punto
mágico** — pasa a ser un edificio (con fallback legacy exacto si el jugador no tiene
ninguno, para no alterar la semántica de los escenarios golden existentes).

**Fuera de alcance en 1.1** (Partes II+): entrenar unidades desde edificios, tech,
recetas/`trains`/`researches` del schema (se ignoran al compilar la tabla tipada),
edificios que atacan, reparación, y hp proporcional al progreso (v1: hp completo
desde la colocación — simplificación documentada, refinar en Parte II).

## §2 Datos — tabla tipada de edificios en el catálogo (extiende SPEC-002)
El loader CHDB (`data_catalog.hpp`) añade una tabla tipada para `kind=building`
(hoy solo se validan estructuralmente), con el mismo rigor que `UnitDefinitionV1`:

```cpp
using BuildingId = uint32_t;                       // índice en la tabla
inline constexpr BuildingId INVALID_BUILDING_ID = 0xFFFFFFFFu;

struct BuildingDefinitionV1 {
    BuildingId id;                 // == índice
    int32_t  hp;                   // > 0
    uint8_t  footprint_w;          // tiles, 1..8
    uint8_t  footprint_h;          // tiles, 1..8
    uint32_t build_time_ticks;     // >= 1
    int32_t  cost_a, cost_b, cost_me;  // >= 0 (deducidos al aceptar PLACE_BUILDING)
    uint8_t  dropoff_mask;         // bit0=A bit1=B bit2=Me (de dropoff_resources)
    uint8_t  constructible;        // 0/1 (schema `constructible`)
    // resto de campos del schema (trains/researches/recipes/...) NO tipados en Parte I
};
// API espejo de la de unidades:
//   catalog.building_count / catalog.buildings[]
//   BuildingId catalog_find_building(cat, name, name_len);  // record_id textual
```
Validación en carga (perfil Verified y Development por igual): rangos anteriores;
violación ⇒ código de error de carga (el catálogo entero se rechaza, política
SPEC-002 §7). El `content_hash` y el formato del blob **no cambian** (los records
building ya viajan en el blob; solo cambia qué parsea el loader).

## §3 Estado nuevo en `GameState` (ESTADO: serializado + checksummeado)
```cpp
uint8_t   entity_kind[ENTITY_HARD_CAP];    // 0=unidad (default), 1=edificio
BuildingId building_id[ENTITY_HARD_CAP];   // INVALID_BUILDING_ID en unidades
uint32_t  build_progress[ENTITY_HARD_CAP]; // ticks acumulados; >= T ⇒ funcional
uint16_t  bld_anchor_tx[ENTITY_HARD_CAP];  // tile ancla (esquina superior-izq del footprint)
uint16_t  bld_anchor_ty[ENTITY_HARD_CAP];
uint32_t  build_target[ENTITY_HARD_CAP];   // índice de entidad edificio objetivo del
                                           // ciudadano constructor; BUILD_NO_TARGET=0xFFFFFFFFu
```
`gs_init`/`zero_components`: `building_id=INVALID_BUILDING_ID`, `build_target=BUILD_NO_TARGET`,
resto 0 — misma convención "todos los slots hasta capacity" que `unit_id` (checksum/serialize
los recorren sin gate de alive). Un edificio **completo** es aquel con
`build_progress >= def.build_time_ticks`. La posición (`pos_x/pos_y`) de un edificio es el
**centro geométrico** del footprint: `anchor*T + (w*T)/2` (raw, exacto en Q47.16 con T=FX_ONE_RAW).
Los edificios tienen `vel=0`, `speed_mtpt=0`, `unit_class=255` (nunca 0..3), `morale=0`,
`fleeing=0`, `eco_state` intacto en SEEK (los sistemas de unidades los saltan por `entity_kind`).

## §4 Comandos nuevos (append-only en `CommandType`)
```cpp
PLACE_BUILDING = 7,
ASSIGN_BUILD   = 8,
```
El `CmdPayload` **no cambia de layout** (compatibilidad replay v2): los campos se
reinterpretan por tipo de comando.

### §4.1 PLACE_BUILDING (emitter = jugador)
Payload: `p.unit_id` = **BuildingId** del catálogo · `p.x_raw`/`p.y_raw` = **tile ancla
en unidades ENTERAS de tile** (no raw; elimina toda ambigüedad de redondeo).
Validación, en este orden (primera que falle decide el receipt):
1. `g.catalog != nullptr` y `p.unit_id < building_count`, si no **MALFORMED**.
2. `def.constructible == 1`, si no **ILLEGAL_STATE**. **Exención de escenario**: si
   `effective_tick == 0` este paso y el 6 (costes) se OMITEN — es la ventana de setup
   de partida (los comandos de jugador nunca ejecutan en el tick 0 porque
   `human_input_delay_ticks >= 1` en producción; los edificios iniciales los encola el
   driver/adaptador con `target_tick = 0` antes del primer Step, y quedan grabados en
   el replay como cualquier comando). Motivación: el schema de SPEC-002 exige, con
   razón, que todo `constructible: true` tenga coste positivo; los centros iniciales
   son `constructible: false` + `build_time_ticks: 0` (nacen completos por la
   definición de §3: `progress 0 >= T 0`), como edificio pre-colocado de escenario.
3. Resto de campos de stats del payload (hp/attack/range_mt/unit_class/speed_mtpt/handle) == 0,
   si no **MALFORMED** (misma disciplina payload-limpio que SPAWN_UNIT).
4. Footprint dentro del mapa: `x + w <= map_tiles_x` y `y + h <= map_tiles_y`, si no **MALFORMED**.
5. Todas las celdas del footprint transitables en `cost_grid` (valor != FF_WALL) **y** sin
   footprint de otro edificio vivo, si no **ILLEGAL_STATE**.
6. `player_stock[emitter]` cubre `cost_a/b/me`, si no **ILLEGAL_STATE**.
7. Slot libre en `EntityTable`, si no **POOL_EXHAUSTED**.

Efecto (atómico dentro de la ejecución del comando): deducir costes del stock; spawn
entidad con `entity_kind=1`, `building_id`, anclas, `hp = max_hp = def.hp` (v1 §1),
`build_progress = 0` (o `= T` si `build_time_ticks == 0` es ilegal — el schema exige ≥1;
un edificio "inicial" de los datos con `build_time_ticks == 1` se completa con 1 tick de
un ciudadano, o puede spawnearse ya completo vía §4.3); marcar las celdas del footprint
`cost_grid = FF_WALL`; `flow_dirty = 1`.

### §4.2 ASSIGN_BUILD (emitter = jugador)
Payload: `p.handle` = ciudadano propio · `p.x_raw`/`p.y_raw` = **tile entero** contenido
en el footprint del sitio objetivo.
Validación: handle vivo y del emitter (**INVALID_ENTITY**/**NOT_OWNER**); `unit_class == 3`
(**ILLEGAL_STATE**); resolver el edificio: entidad viva propia con `entity_kind==1`,
`build_progress < T` y cuyo footprint contiene el tile — si hay varias (imposible por
no-solape §4.1.5, pero por robustez) la de **menor índice**; si ninguna ⇒ **INVALID_ENTITY**.
Efecto: `build_target[ciudadano] = índice del edificio`. Mientras `build_target` esté
activo el ciudadano queda **fuera** del pipeline económico (el sistema económico lo salta).

### §4.3 Edificios iniciales (arranque de partida)
El **driver/adaptador** (fuera de Step) coloca los edificios iniciales encolando
PLACE_BUILDING con `target_tick = 0` antes del primer Step (exención de escenario,
§4.1.2): los centros (`egipto:settlement_center` / `rome:forum_center`, datos
`constructible: false`, `build_time_ticks: 0`, sin coste) nacen completos y funcionan
como dropoff desde el tick 0. No existe camino privilegiado dentro del kernel: **todo
pasa por comandos** (SPEC-001 §6, sin excepciones) y queda grabado en el replay.

## §5 Sistema constructor (nueva fase de Step)
Se inserta como fase propia **después de economía y antes del destroy batch**, iteración
ascendente por índice (regla canónica):
```
para cada i vivo con unit_class==3 y build_target != BUILD_NO_TARGET:
    b = build_target[i]
    si b no vivo, o entity_kind[b]!=1, o build_progress[b] >= T(b):
        build_target[i] = BUILD_NO_TARGET; continue     // vuelve a economía
    p_cerca = clamp(pos[i] al rectángulo raw del footprint de b)   // punto más cercano, exacto
    si dist_sq(pos[i], p_cerca) > BUILD_ARRIVE_RADIUS_RAW^2 (= ECO_ARRIVE_RADIUS_RAW):
        tgt[i] = p_cerca                                  // el movement system lo lleva
    si no:
        build_progress[b] += 1                            // 1 punto/tick/ciudadano; varios suman
        si build_progress[b] > T(b): build_progress[b] = T(b)
```
Determinismo: sin RNG, sin floats, sin asignaciones; el clamp al rectángulo es aritmética
entera exacta. Al completarse (`>= T`) el edificio se vuelve funcional (dropoff §6); no hay
evento — los sistemas consultan la condición.

## §6 Dropoff como edificio (modifica RETURN de economía)
En RETURN, el destino del ciudadano que carga el recurso `r` es:
- el edificio **propio, completo**, con `dropoff_mask` incluyendo `r`, **más cercano**
  (métrica `dist_sq` existente; empate ⇒ menor índice de entidad); el punto de entrega es
  el clamp de la posición del ciudadano al rectángulo del footprint (llegada con el mismo
  radio `ECO_ARRIVE_RADIUS_RAW`);
- si el jugador **no tiene ninguno** ⇒ **fallback legacy exacto**: `dropoff_x/y[emitter]`
  como hasta hoy. Los escenarios sin edificios (golden 0.1–0.4) conservan así su
  comportamiento **bit-exacto** (los checksums cambian solo por el dominio §8).
La búsqueda va en el wiring de step.hpp (no en `economy.hpp`, que sigue autocontenido y
sin conocer GameState: se le pasa el punto de dropoff resuelto, como hoy).

## §7 Combate contra edificios
- **Targeting** (ataque y aggro): las entidades enemigas con `entity_kind==1` son objetivos
  válidos, con la misma métrica de distancia/desempate (menor índice). La distancia a un
  edificio se mide **pos a pos** (centro), no al borde del footprint — simplificación v1
  documentada (los ranges del slice ≫ medio footprint).
- Los edificios **no atacan**, no persiguen, no tienen moral ni pánico: `combat`, `aggro`,
  `morale` saltan `entity_kind==1` como atacantes/sujetos, solo los admiten como objetivo.
- RPS: multiplicador contra edificios = tabla existente con clase defensora "edificio"
  (nueva fila: artillery/siege ×2.0 en bp, resto ×1.0 — valores en bp enteros como el resto).
- **Muerte** (`hp<=0`): mismo destroy batch (paso 6); al reciclar el slot: restaurar las
  celdas del footprint a `cost_grid = 1` y `flow_dirty = 1`. Todo ciudadano con
  `build_target == ese índice` lo limpia en su siguiente fase §5 (no hace falta barrido).

### §7.1 ENMIENDA — Aldeanos vulnerables en combate (Sprint 1.4-cierre, decisión del Director 2026-07-24)
Hasta el Sprint 1.3 los aldeanos (`unit_class==3`) eran **intocables** en combate (excluidos
como objetivo por el guard `unit_class[j] > 2 && entity_kind[j] != 1`). Esto era una
simplificación temporal del Sprint 1.1, no una decisión de diseño: hacía que una partida con
economía **nunca terminara por conquista** (quien conservara un aldeano no podía perder — la
derrota v1 de SPEC-005 §6 exige "0 edificios Y 0 aldeanos"). El Director resuelve, alineado con
la visión AoE2: **los aldeanos pasan a ser objetivo válido de combate**.
- **Targeting** (`combat_system` y `aggro_system`): un aldeano enemigo vivo es objetivo válido,
  con la misma métrica/desempate (dist_sq, menor índice). El guard de exclusión de aldeanos como
  OBJETIVO se elimina en ambos sistemas.
- **Los aldeanos SIGUEN sin atacar/perseguir**: el guard de ATACANTE `unit_class[i] > 2` se
  mantiene intacto (son vulnerables, no combatientes). Sin moral/pánico (unit_class 3 fuera de
  `morale_system` como hasta ahora).
- **RPS contra aldeano defensor**: la tabla `rps_mult_bp` es 3×3 (clases 0..2); un aldeano
  (unit_class 3) haría OOB. Se añade una rama análoga a la de edificios: multiplicador **×1.0
  (10000 bp) neutro** de cualquier atacante contra un aldeano (balance v1 ajustable; documentar).
- **Consecuencia de trayectoria**: cambia el comportamiento de TODO escenario donde un aldeano
  quede al alcance de un enemigo (showcase, tests de economía+combate). NO es un bump de dominio
  de checksum (los campos hasheados no cambian): es un cambio de TRAYECTORIA — los checksums
  golden de los escenarios afectados se regeneran por el procedimiento establecido (dump del
  diff, sin cambios de trayectoria en escenarios SIN aldeanos-en-peligro). La economía en sí
  (recolección) no cambia salvo que un aldeano muera antes de entregar.
- **Habilita** la partida completa con economía que termina por conquista: la condición de
  victoria v1 (SPEC-005 §6) queda ahora alcanzable con aldeanos presentes.

## §8 Persistencia y checksum
- **Save**: bump de versión (v7 → v8): los arrays de §3 entran al stream canónico en el
  orden en que aparecen en §3, tras los campos del v7. El loader v8 acepta v7 (arrays
  nuevos = valores de init §3) siguiendo el patrón de migración existente.
- **Checksum**: los arrays de §3 entran al dominio (mismo orden). Es un **cambio de dominio
  intencional**: los checksums golden se regeneran una vez por el procedimiento establecido
  (regen + revisión del diff de checksums en el commit, sin cambios de trayectoria).
- **Replay**: v2 sin cambios de formato (los comandos nuevos viajan como cualquier otro
  `CommandType`; el payload no cambió de layout).

## §9 Gates del sprint (DoD binario)
1. Golden 1074/1074 tras regen de dominio; **la trayectoria (pos/hp/stock por tick) de los
   escenarios previos es idéntica** — se verifica con un dump comparativo pre/post en al
   menos 2 escenarios (movement y economía), no solo con el checksum.
2. G1 alloc_delta=0 · G3/G4/G5 verdes (replay con PLACE_BUILDING/ASSIGN_BUILD incluido).
3. Tests nuevos mínimos: colocación válida/ inválida (cada rechazo de §4.1) · construcción
   completa por 1 y por N ciudadanos (progreso suma) · dropoff a edificio vs fallback
   legacy · destrucción libera cost_grid (una unidad atraviesa donde estaba el edificio) ·
   save/load v8 round-trip con edificio a medio construir · catálogo: edificio inválido
   rechazado (hp 0, footprint 9).
4. ctest completo verde, `-Werror`, sin floats/heap en Step (grep + contador CI).

## §9b Reparto Parte I (ejecutado)
Arquitecto: contrato/revisión/integración · Sonnet: kernel · MiniMax: datos · Codex/Luna Max: UI.

---
# PARTE II — Fidelidad de comandos, producción, tecnología y épocas (Sprint 1.2)

## §10 Fidelidad de comandos: replay v3 + save v9 (PRIMERA pieza del sprint, bloqueante)
Cierra los dos gaps de identidad de datos (D8 del Sprint 1.1 + gap gemelo del save):
`CmdPayload::unit_id` — que transporta UnitId en SPAWN_UNIT/SPAWN_CITIZEN y BuildingId en
PLACE_BUILDING — **no se serializa** ni en el replay (v2) ni en la agenda pendiente del
save. Cualquier partida real con ids != 0 pierde fidelidad al recargar/reproducir.

### §10.1 Replay v3
- `REPLAY_WRITE_VERSION = 3`. Registro por comando = el layout v2 **+ `u32 unit_id` al
  final**. Cabecera y semántica de `effective_tick`/`schedule_mismatches` sin cambios.
- El loader acepta v1/v2/v3. En v1/v2 el campo se reconstruye como 0 con una advertencia
  contable en `ReplayData` (`legacy_payload_loss = 1` si el stream contiene algún comando
  de un tipo que use unit_id) — el verificador puede así distinguir "replay antiguo
  potencialmente infiel" de "replay v3 fiel". Grabación siempre en v3.

### §10.2 Save v9 (agenda con unit_id)
- `SAVE_FORMAT_VERSION = 9`: la serialización de cada `ScheduledCommand` de la agenda
  pendiente gana `u32 unit_id` (tras `unit_class`, mismo patrón append). El resto del
  stream v8 no cambia. Sin migración v8→v9 (precedente D7).
- El checksum NO cambia de dominio por esto (la agenda ya estaba en el dominio y el
  dominio hashea los structs en memoria, no el stream — verificar que efectivamente el
  checksum de la agenda incluye unit_id; si no lo incluye, añadirlo = bump a
  `CHUNSA_STATE_V4` con regen, mismo procedimiento).

### §10.3 Ventana de setup alcanzable con delay ≥ 1 (refina la enmienda §4.1.2)
Hallazgo del Sprint 1.1: con `human_input_delay_ticks >= 1`, `effective_tick == 0` es
inalcanzable (`eff = max(target, t+delay) >= 1`) y la exención de escenario obligaba a
la demo a usar delay=0. Refinamiento contractual:
- `command_effective_tick` gana un caso explícito de setup: **si `target_tick == 0` y
  `t == 0`, `eff = 0`** (los comandos con destino "antes de que el mundo empiece",
  ingeridos en el primer Step, ejecutan en el tick 0 sin sumar delay).
- Contrato del host (driver/adaptador, documentado en el header): NO ingerir input de
  jugador antes del primer Step; los comandos pre-Step son exclusivamente de setup.
- El replay no necesita nada nuevo: v2+ persiste `effective_tick` por comando.
- La demo vuelve a `human_input_delay_ticks = 1` (el valor de producción).

## §11 Producción (colas de entrenamiento)
### §11.1 Datos (extiende la tabla tipada §2)
`UnitDefinitionV1` gana campos tipados desde el schema unit existente: `cost_a/cost_b/
cost_me` (de resource_costs; ausente = 0), `build_time_ticks` (de stats, >= 1),
`pop_cost = 1` (constante v1, no viene de datos). `BuildingDefinitionV1` gana
`trains[PROD_TRAINS_MAX=8]` (UnitId resueltos desde los record_id del campo `trains`
del schema en carga; referencia no resoluble ⇒ error de carga, catálogo rechazado) y
`train_count`, más `researches[PROD_TECHS_MAX=8]`/`research_count` (TechId, §12).
### §11.2 Estado nuevo en GameState (ESTADO: serializado + checksummeado)
```cpp
inline constexpr uint32_t PROD_QUEUE_CAP = 5;
uint32_t prod_queue[ENTITY_HARD_CAP][PROD_QUEUE_CAP]; // UnitId; slots >= count = INVALID_UNIT_ID
uint8_t  prod_count[ENTITY_HARD_CAP];
uint32_t prod_progress[ENTITY_HARD_CAP];  // ticks del ítem en cabeza
int64_t  rally_x[ENTITY_HARD_CAP], rally_y[ENTITY_HARD_CAP]; // raw; 0,0 = sin rally
uint8_t  rally_set[ENTITY_HARD_CAP];
int32_t  pop_used[MAX_EMITTERS];          // pop_cap fijo v1: POP_CAP_V1 = 200
```
### §11.3 Comandos (append-only)
- **TRAIN_UNIT = 9**: `p.handle` = edificio propio COMPLETO; `p.unit_id` = UnitId.
  Validación en orden: handle vivo/propio (**INVALID_ENTITY/NOT_OWNER**) · es edificio
  completo (**ILLEGAL_STATE**) · catálogo y unit_id válidos y `unit_id ∈ trains` del
  edificio (**MALFORMED**) · época: `player_epoch[emitter] ∈ epoch_window` de la unidad
  (**ILLEGAL_STATE**) · cola no llena (**ILLEGAL_STATE**) · `pop_used + pop_cost <=
  POP_CAP_V1` (**ILLEGAL_STATE**) · stock cubre costes (**ILLEGAL_STATE**). Efecto:
  deducir costes, encolar, `pop_used += pop_cost` (la población se reserva al encolar;
  se libera al morir la unidad o nunca si se entrena — sin cancel en Parte II,
  desviación documentada).
- **SET_RALLY = 10**: `p.handle` = edificio propio (completo o no); `p.x_raw/p.y_raw` =
  punto raw dentro de la cota del mundo. Efecto: rally_x/y + rally_set=1.
### §11.4 production_system (fase de Step, tras construction_system, antes de DESTROY)
Iteración ascendente sobre edificios vivos completos con `prod_count > 0`:
`prod_progress += 1`; al alcanzar `build_time_ticks` de la unidad en cabeza: spawn
determinista de la unidad (misma inicialización que SPAWN_UNIT data-driven; posición =
punto medio del lado inferior del footprint + medio tile, exacto en raw), `tgt` = rally
si `rally_set` (la unidad camina con movement_v1), desplazar la cola una posición,
`prod_progress = 0`. Si no hay slot de entidad libre: el ítem espera (reintenta cada
tick, sin perder progreso — determinista). La muerte de una unidad reduce `pop_used`
(en el reciclaje del destroy batch, por owner).
La muerte del edificio pierde su cola (los costes ya pagados se pierden; pop reservada
de los ítems no entrenados se libera en el reciclaje).

## §12 Tecnología y épocas (ADR-015)
### §12.1 Datos
Tabla tipada `TechDefinitionV1`: `id`, `cost_a/b/me`, `research_time_ticks (>=1)`,
`epoch` (1..15), `prerequisites[4]`/`prereq_count` (TechId), `grants[4]`/`grant_count`
(CapabilityId = índice en la tabla de capacidades declaradas del blob),
`mutually_exclusive_with[4]`/`mutex_count` (TechId). Referencias no resolubles ⇒ error
de carga. **Las techs son paquetes de capacidad (base v1.1): NO modifican stats en
Parte II** — los efectos sobre stats llegan en Parte III; en 1.2 las capacidades
gatean contenido (units/buildings/techs con `required_capabilities`) y el epoch-up.
### §12.2 Estado
```cpp
uint64_t player_techs[MAX_EMITTERS][TECH_WORDS];   // bitmask TechId investigadas
uint64_t player_caps[MAX_EMITTERS][CAP_WORDS];     // bitmask capacidades adquiridas
uint8_t  player_epoch[MAX_EMITTERS];               // época actual (init: época del slice)
uint32_t research_tech[ENTITY_HARD_CAP];           // INVALID_TECH_ID = ocioso
uint32_t research_progress[ENTITY_HARD_CAP];
```
### §12.3 Comandos
- **RESEARCH_TECH = 11**: `p.handle` = edificio propio completo con `tech ∈ researches`;
  `p.unit_id` = TechId. Validación: análoga a TRAIN (handle/completo/catálogo/lista) ·
  no investigada ya ni en curso por este jugador (**ILLEGAL_STATE**) · prerequisites
  cumplidos y ningún mutex investigado (**ILLEGAL_STATE**) · `tech.epoch <=
  player_epoch` (**ILLEGAL_STATE**) · edificio ocioso de investigación (**ILLEGAL_STATE**)
  · stock (**ILLEGAL_STATE**). Efecto: deducir, `research_tech/progress`.
  Al completar (research_system, misma fase que production): set bit en player_techs,
  OR de grants en player_caps.
- **EPOCH_UP = 12**: `p.handle` = 0 (comando de jugador, no de entidad). Doble gate
  ADR-015: (a) el jugador posee >= 2 edificios COMPLETOS cuya `epoch_window` incluye su
  época actual; (b) `g.tick >= EPOCH_MIN_TICKS * (player_epoch - época_inicial + 1)`
  (constante v1: 20 ticks/s * 300 s = 6000). Además coste fijo v1 (EPOCH_COST_A/B/ME,
  constantes) y `player_epoch < EPOCH_MAX_V1` (época máxima del slice, de los datos del
  match — v1: constante 7). Efecto: deducir, `player_epoch += 1`. Rechazos: ILLEGAL_STATE.
### §12.4 Gating por época y capacidades (retro-aplica a Parte I)
PLACE_BUILDING (camino normal, no exento) y TRAIN_UNIT validan además:
`player_epoch ∈ epoch_window` del def y `required_capabilities ⊆ player_caps`
(**ILLEGAL_STATE**). Los defs iniciales del slice no requieren capacidades (compat).

## §13 Persistencia, checksum y gates (Parte II)
- Save v9 (§10.2) y, tras §11–§12, **save v10** con los arrays nuevos (§11.2, §12.2) al
  final; dominio de checksum → `CHUNSA_STATE_V4` (o V5 si §10.2 ya lo bumpeó) con regen
  golden por el procedimiento establecido; trayectoria de escenarios previos bit-idéntica
  (dump pre/post obligatorio, mismo estándar que §9.1).
- Replay v3 desde §10.1; los comandos 9–12 viajan como cualquier CommandType.
- Tests mínimos: replay v3 round-trip con PLACE_BUILDING de id != 0 (el caso que v2
  pierde — DEBE fallar con v2 forzado y pasar con v3) · save v9 con SPAWN_UNIT pendiente
  en agenda · setup window con delay=1 (target 0 en t=0 ⇒ eff 0; target 0 en t=1 ⇒
  eff 2) · TRAIN feliz + cada rechazo · cola llena/pop llena · producción multi-ítem con
  slot exhausto intermedio · rally · RESEARCH con prereq/mutex/época · EPOCH_UP doble
  gate (falla por edificios, falla por tiempo, pasa) · gating de época en TRAIN/PLACE.
- ctest completo + golden + G1/G3/G4/G5 verdes, `-Werror`, cero float/heap en Step.

## §14 Reparto Parte II
- **Arquitecto**: este contrato, revisión, integración, enmiendas.
- **Sonnet K1**: §10 completo (replay v3 + save v9 + setup window) — PRIMERO, se integra
  antes de empezar K2.
- **Sonnet K2**: §11 + §12 + §13 (producción, tech, épocas, save v10) sobre el main que
  ya incluye K1.
- **MiniMax**: datos YAML (cuarteles con `trains`, techs M1 de Egipto/Roma con
  procedencia ADR-014) — en paralelo, validados por el gate `data_compile`.
- **Codex/Luna Max**: UI de producción/tech/época — al final, contra main con K2.

---
# PARTE III — Apertura económica completa (Sprint 1.6B)

Versión 0.3 · 2026-07-24 · Autor: Arquitecto (Claude). Append-only sobre las Partes I–II.
Objetivo del sprint (PLAN_MAESTRO v1.2 §4): **empezar con centro + aldeanos y llegar a
producción militar sin nada inyectado tras el setup**. Hoy la economía arranca con 6 depósitos
hardcodeados (`gs_init_economy`), los centros no producen aldeanos (`trains: []`), y el kernel
no sabe de qué civilización es cada dato (`civ_id` no tipado) — esta parte cierra los tres.

## §15 Datos: lo que el blob debe traer (y el loader tipificar)
### §15.1 `resource_spawns` del mapa (el schema YA lo exige; hoy va vacío)
`data/maps/base_demo_desert_basin.yaml` tiene `resource_spawns: []`. Debe poblarse con los
depósitos reales del escenario: `{kind: resource, id: <A|B|Me>, x_millitiles, y_millitiles,
amount}`. Diseño del slice: depósitos simétricos alrededor de las dos `starting_positions`
existentes (slot 0 en 20.5/128.5 tiles, slot 1 en 235.5/128.5) para que la apertura sea justa,
más algunos neutrales al centro. **La simetría es requisito de balance, no estético.**
**Regla de simetría (enmienda del Arquitecto, 2026-07-24)**: un recurso con conteo PAR entre los
neutrales se coloca como pares espejados (`x_der = 256000 - x_izq`, misma Y, mismo `amount`,
**mismo `id`** — espejar la posición pero no el recurso NO es simétrico: daría a cada jugador un
recurso distinto más cerca). Un recurso con conteo IMPAR se coloca **sobre el eje central**
(`x = 128000`, con la Y dentro del hueco del muro `y ∈ [124000, 132000)`), donde es equidistante
de ambos jugadores. Así la composición deseada nunca fuerza una asimetría de balance.
### §15.2 Los centros producen aldeanos
`egipto:settlement_center` y `rome:forum_center` tienen `trains: []`. Deben entrenar el aldeano
de su civilización (`egipto:work_crew`, `rome:camp_work_crew`) — igual que poblé `researches` en
el Sprint 1.2. Sin esto no hay apertura económica posible.
### §15.3 `civ_id` tipado en el loader (deuda de Fase 1, ahora bloqueante)
El schema declara `civ_id` en unit/building/tech, pero el loader C++ **no lo tipa** (solo lo
valida el compilador Python). Se añade al patrón endurecido:
```cpp
using CivId = uint32_t;  inline constexpr CivId INVALID_CIV_ID = 0xFFFFFFFFu;
// tabla de civilizaciones (kind=civ, hoy solo estructural) + índice por record_id
CivId catalog_find_civ(cat, name, name_len);
// UnitDefinitionV1 / BuildingDefinitionV1 / TechDefinitionV1 ganan: CivId civ_id;
```
Referencia `civ_id` no resoluble ⇒ rechazo del catálogo entero (patrón SPEC-002 §7). El formato
del blob NO cambia (los records civ ya viajan).

## §16 Depósitos desde el mapa (reemplaza el patrón fijo)
- El catálogo tipifica los `resource_spawns` del mapa activo:
  `struct ResourceSpawnV1 { uint8_t resource_idx; /*0=A,1=B,2=Me*/ int64_t x_raw, y_raw; int32_t amount; };`
  (`x_millitiles → raw` = `mt * FX_ONE_RAW / 1000`, exacto en enteros).
- `gs_init_economy` pasa a poblar `deposits[]` **desde el mapa**, en el orden canónico del blob
  (ascendente, ya garantizado), hasta `ECO_MAX_DEPOSITS`; si el mapa trae más, se rechaza la
  carga (no se truncan datos en silencio).
- **Compatibilidad**: si no hay catálogo enlazado o el mapa no trae spawns, se conserva el
  patrón fijo de 6 depósitos **exacto** como fixture legacy — los escenarios golden previos
  (sintéticos, tests de economía de 0.3) deben quedar **bit-idénticos**. Es la misma disciplina
  de fallback que el dropoff-edificio de §6.
- El `dropoff` sigue resolviéndose por §6 (edificio propio, fallback al punto fijo).

## §17 Identidad de civilización y época por jugador
Estado nuevo en `GameState` (ESTADO: serializado + checksummeado):
```cpp
CivId player_civ[MAX_EMITTERS];   // INVALID_CIV_ID = sin asignar
```
- Se fija en el setup del escenario (comando o init explícito del host, análogo a
  `gs_init_epoch_from_catalog`), NO se infiere en caliente.
- **Época inicial POR JUGADOR** (cierra la deuda de K2 del 1.2, que la calculaba
  catálogo-ancha): con `civ_id` tipado, la época inicial de un jugador es el mínimo
  `epoch_window[0]` de los datos **de SU civilización**. `gs_init_epoch_from_catalog` gana una
  variante por-jugador; la catálogo-ancha se conserva para escenarios sin civ asignada.
- **Gates de civilización** (retro-aplican a Partes I–II): `PLACE_BUILDING`, `TRAIN_UNIT` y
  `RESEARCH_TECH` rechazan con **ILLEGAL_STATE** si `def.civ_id != g.player_civ[emitter]`
  (cuando el jugador tiene civ asignada; si es `INVALID_CIV_ID`, el gate no aplica —
  compatibilidad con los escenarios/tests existentes). Exento en la ventana de setup del tick 0,
  igual que los demás gates (§4.1.2).

## §18 Recolección explícita: comando GATHER
Hoy los aldeanos recolectan **solos** (state machine autónoma SEEK/HARVEST/RETURN del Sprint
0.3). Para una apertura jugable el jugador debe poder dirigirlos.
- **`GATHER = 13`** (append-only): `p.handle` = ciudadano propio; `p.x_raw/p.y_raw` = punto raw
  del depósito objetivo. Validación en orden: handle vivo/propio (**INVALID_ENTITY**/
  **NOT_OWNER**) · `unit_class == 3` (**ILLEGAL_STATE**) · resolver depósito: el de índice más
  bajo cuyo `remaining > 0` y cuya distancia al punto sea ≤ `GATHER_PICK_RADIUS_RAW` (1 tile);
  si ninguno ⇒ **INVALID_ENTITY**. Efecto: `eco_assigned_deposit = ese índice`,
  `build_target = BUILD_NO_TARGET` (una orden de recolectar cancela la de construir — decisión
  explícita) y `eco_state` según la **regla de carga** de abajo.
- **Regla de carga (enmienda del Arquitecto, 2026-07-28)**: la redacción original fijaba
  `eco_state = SEEK` de forma incondicional. Eso era un defecto de integridad económica: un
  ciudadano que transportaba parcialmente el recurso A y recibía `GATHER` hacia B conservaba la
  cantidad de A, cosechaba B y `HARVEST` sobrescribía `eco_carry_resource` con B — acreditando
  **toda** la suma como B en el dropoff. Reproducido en la auditoría multimodelo del 2026-07-27
  (10 unidades de A → carga de 15 unidades de B tras una sola cosecha de 5 B). Rompía
  conservación de recursos y afectaba tanto a órdenes humanas como a la reasignación automática
  de la IA (§19). Semántica correcta, por casos:

  | Condición | `eco_assigned_deposit` | `eco_state` |
  |---|---|---|
  | `eco_carry == 0` | índice resuelto | `SEEK` |
  | `eco_carry > 0` y `deposits[idx].resource_idx == eco_carry_resource` | índice resuelto | `SEEK` |
  | `eco_carry > 0` y el recurso **difiere** | índice resuelto | **`RETURN`** |

  El tercer caso deposita primero y **no necesita campos nuevos**: `RETURN`, al llegar al
  dropoff, pone `carry = 0` y `eco_state = SEEK` conservando `eco_assigned_deposit`; en el `SEEK`
  siguiente `need_reassign` es falso si el depósito nuevo tiene `remaining > 0`, así que el
  aldeano marcha directo al que pidió el jugador. **`SAVE_FORMAT_VERSION` 12 y
  `CHECKSUM_ALGO_VERSION` 7 quedan intactos**: el formato serializado y el algoritmo de checksum
  no cambian; lo que cambia es el comportamiento del kernel, que es justo lo que la versión de
  checksum no cubre. El caso "mismo recurso" mantiene `SEEK` a propósito: la carga sigue siendo
  homogénea, no hay corrupción posible y forzar un viaje de vuelta sería una regresión de
  jugabilidad injustificada.
- **Órdenes de grupo**: el host emite un GATHER por ciudadano seleccionado (N comandos), como ya
  hace con MOVE_TO. El kernel no agrupa (mantiene un comando = una entidad).
- **Agotamiento y reasignación deterministas**: cuando `remaining <= 0`, el aldeano asignado
  reasigna al depósito **vivo más cercano del mismo recurso** (métrica `dist_sq` entera, empate
  por menor índice); si no queda ninguno de ese recurso, al más cercano de cualquier recurso; si
  no queda ninguno, queda ocioso (`ECO_NO_DEPOSIT`) sin consumir CPU. Esto ya existe
  parcialmente en `economy.hpp::eco_find_nearest_deposit` — se endurece para respetar la
  preferencia de recurso y se documenta.

## §19 La IA juega la apertura (extiende SPEC-005 §4.1)
La capa estratégica gana consciencia económica, **sin romper la regla de oro** (§0 de SPEC-005:
función pura, cero `g.tick`/float/heap/RNG fuera de `AI_TIEBREAK`):
- Cuenta aldeanos por recurso asignado y stock A/B/Me; si el stock de un recurso está por debajo
  del umbral que su intención requiere, emite GATHER redirigiendo aldeanos ociosos/excedentes a
  ese recurso (demanda adaptativa).
- **Repone aldeanos**: si `citizens < AI_ECON_TARGET_CITIZENS` y el centro puede entrenar,
  `TRAIN_UNIT` del aldeano de su civilización (ahora posible por §15.2).
- Progresa desde la apertura: aldeanos → recursos → edificio militar → ejército (el camino ya
  existe en las capas de K2; ahora arranca desde cero en vez de con ejército inyectado).

## §20 Persistencia, checksum y gates (Parte III)
- **Save v12** (`player_civ` + los campos nuevos al final, append; sin migración, precedente D7)
  y dominio de checksum → `CHUNSA_STATE_V7` con regen por el procedimiento establecido.
- **Trayectoria**: los escenarios sin civ asignada y sin spawns de mapa (sintéticos, economía
  0.3, skirmish militar) deben quedar **bit-idénticos** — dump pre/post obligatorio. Los que
  cambien (economía con mapa real) se documentan.
- **DoD del sprint** (PLAN_MAESTRO): un escenario `centro + 3 aldeanos → recolectar → construir
  edificio militar → entrenar ejército` **sin ninguna mutación privilegiada** (todo por
  comandos); la IA recorre el mismo camino; save/load/replay a mitad de recolección;
  golden 1074/1074, G1 alloc_delta=0, G3/G4/G5, ctest completo, `-Werror`, cero float/heap.

## §21 Reparto Parte III
- **Arquitecto**: este contrato, el gate de civ/época, revisión e integración.
- **MiniMax M3**: datos (§15.1 `resource_spawns` simétricos + §15.2 `trains` de los centros),
  brief cerrado, validado por el gate `data_compile`.
- **Sonnet K1**: §15.3 (`civ_id`/`CivId` tipado + `ResourceSpawnV1`) + §16 (depósitos desde el
  mapa con fallback legacy) + §17 (civ/época por jugador + gates) + save v12/checksum v7.
- **Sonnet K2**: §18 (GATHER + agotamiento/reasignación) + §19 (la IA juega la apertura) + el
  escenario del DoD + tests. Sobre el main con K1 integrado.
- **Opus**: auditoría del parsing nuevo (`resource_spawns`/`civ_id`, entrada no confiable) y del
  determinismo de la extensión de la IA.
- **Codex/Luna Max**: adaptador Godot (órdenes de recolección por clic, HUD de aldeanos por
  recurso, escenario de apertura jugable) — al final, contra main con K2.

# Parte IV — Control del ciudadano (Sprint 1.7)

## §22 Modelo de tareas del ciudadano

**Motivación (sesión de verificación del Director, 2026-07-28).** El ciudadano
es hoy el único cuerpo del juego que el jugador **no puede mandar**. Sus dos
modos son implícitos: si `build_target != BUILD_NO_TARGET` lo mueve
`construction_system`; si no, lo posee `economy_system`, que le reescribe la
posición cada tick. No existe hueco para «vete ahí». Un `MOVE_TO` sobre un
ciudadano es aceptado por `apply_command` y **no hace nada**: `movement_v1`
excluye `unit_class > 2` y economía le sobrescribe `pos` en la misma fase.

Referencia de género (Age of Empires II): el aldeano es **unidad primero y
trabajador después**. El clic derecho es contextual según el objetivo (suelo →
mover · recurso → recolectar · obra → construir) y la orden de moverse siempre
funciona y cancela lo anterior.

### §22.1 La tarea es explícita y única

Cada ciudadano tiene exactamente **una** tarea, en un campo nuevo
`citizen_task[ENTITY_HARD_CAP]` (`uint8_t`):

| Valor | Nombre | Quién le mueve | Se fija con |
|---|---|---|---|
| 0 | `IDLE` | nadie — quieto, `vel = 0` | al completar `MOVE`; al quedar sin depósito |
| 1 | `MOVE` | `citizen_move_system` (§22.3) | `MOVE_TO` |
| 2 | `GATHER` | `economy_system` | `GATHER` |
| 3 | `BUILD` | `construction_system` | `ASSIGN_BUILD` |

`citizen_task` es la **única** autoridad sobre qué sistema posee al ciudadano.
`build_target` y `eco_assigned_deposit` pasan a ser datos **de** la tarea, no
selectores de sistema.

**Invariante rector:** en un tick dado, a lo sumo un sistema escribe
`pos_x/pos_y` de un ciudadano. Cada sistema comprueba `citizen_task` al entrar y
sale si no es la suya. Esto deroga y sustituye al guard
`if (g.unit_class[i] > 2) continue;` de `movement_v1` como mecanismo de
exclusión — **`movement_v1` NO se modifica** (sigue CONGELADO, SPEC-001 §12) y
sigue sin tocar ciudadanos.

### §22.2 Transiciones

Fijar una tarea **cancela la anterior sin residuo**:

- `MOVE_TO` sobre ciudadano propio vivo, destino dentro de cota:
  `citizen_task = MOVE`, `tgt_x/tgt_y = destino`, `build_target = BUILD_NO_TARGET`.
  **`eco_assigned_deposit`, `eco_carry` y `eco_carry_resource` NO se tocan**: el
  ciudadano sigue transportando lo que llevaba mientras camina.
- `GATHER`: `citizen_task = GATHER`, más las reglas de §18 (incluida la **regla
  de carga**, que se mantiene íntegra).
- `ASSIGN_BUILD`: `citizen_task = BUILD`, `build_target = objetivo`.
- Al llegar al destino de `MOVE`: `citizen_task = IDLE`, `vel = 0`.
- Al completar o perder la obra: `citizen_task = IDLE`.
- En `GATHER`, si no queda ningún depósito alcanzable:
  `citizen_task = IDLE` (hoy queda en `SEEK` girando en vacío).

**Divergencia deliberada frente a AoE2 — se documenta y se conserva.** AoE2
**pierde** la carga al reasignar a un recurso distinto (comportamiento que su
propia comunidad reporta como defecto). CHUNSA **conserva** el recurso: la regla
de carga de §18 hace volver al dropoff primero. No se copia el defecto.

### §22.3 `citizen_move_system`

Sistema nuevo, fase propia, **antes** de `economy_system` y de
`construction_system`, iteración ascendente por índice.

- Actúa solo sobre ciudadanos vivos con `citizen_task == MOVE`.
- Locomoción: **idéntica en forma** a la rama de aproximación de
  `economy.hpp::try_move` (normalize + step entero, con snap si el paso cubre la
  distancia restante). Cero float, cero heap.
- Llegada: cuando la distancia al objetivo es `<= arrive_r_sq`,
  `citizen_task = IDLE` y `vel = 0`.

**Alcance de la navegación en este sprint: línea recta, sin pathfinding.** No es
una rebaja: `movement_v1` mueve a los militares con `MOVE_TO` **también en línea
recta**; el flow field solo interviene con `FLOW_MOVE`, y el motor mantiene **un
único** campo global (`g.flow`, `g.flow_goal_cell`), no uno por unidad, así que
no es reutilizable para destinos individuales. Dar `MOVE_TO` recto al ciudadano
lo deja **a la misma altura que el resto del ejército**, no por debajo.

**Deuda declarada:** navegación por obstáculos para órdenes individuales
(ciudadanos y militares por igual) es un problema de motor sin resolver, no un
agujero de este sprint. Va a su propio sprint. Hasta entonces, un ciudadano
enviado al otro lado del muro de `x=128` empujará la pared — igual que hoy
haría un soldado.

### §22.4 Auto-asignación al aparecer: se conserva

AoE2 hace que los aldeanos nuevos queden **parados**. CHUNSA los auto-asigna al
depósito más cercano. **En este sprint se conserva la auto-asignación.**

Razón: cambiarla altera la apertura de la IA y **todas** las trayectorias
económicas de golpe, incluido el gate de fase del Sprint 1.4. La libertad que el
Director reclama la aporta `MOVE` (§22.1), no este comportamiento por defecto.
Pasar a «nacen parados» es un cambio de diseño legítimo y va **después**, con el
reajuste de la apertura de la IA, para no mezclar dos fuentes de divergencia en
el mismo incremento.

### §22.5 Versionado

`citizen_task` es estado nuevo persistido y entra en el checksum:

- `SAVE_FORMAT_VERSION` **12 → 13**.
- `CHECKSUM_ALGO_VERSION` **7 → 8** (`CHUNSA_STATE_V8`).
- Los baselines de determinismo de escenarios **con ciudadanos** cambian y se
  re-registran. Los que **no** tienen ciudadanos deben quedar **bit-idénticos**;
  si alguno se mueve, es una regresión y se para.

`gs_init` inicializa `citizen_task = GATHER` para ciudadanos (coherente con
§22.4) e `IDLE` para el resto de clases, que no lo consultan.

---

## §23 Alcance de la auto-asignación económica (zona aliada)

**Origen:** defecto observado por el Director en sesión de juego real
(2026-07-28) y reproducido con instrumentación del kernel.

### §23.1 El defecto que corrige

La regla de preferencia de recurso introducida en §18 (Sprint 1.6B, arreglo de
F-01) —«al agotarse el depósito asignado, prefiere el MISMO recurso; si no hay,
cualquiera»— se especificó **sin cota de distancia**. Consecuencia medida en el
demo:

```text
CIT idx=10 state=SEEK dep=7 dep_rem=750 dep_res=2 carry=0 pos=45,132
CIT idx=11 state=SEEK dep=7 dep_rem=750 dep_res=2 carry=0 pos=46,133
CIT idx=12 state=SEEK dep=7 dep_rem=750 dep_res=2 carry=0 pos=38,131
CIT idx=13 state=SEEK dep=7 dep_rem=750 dep_res=2 carry=0 pos=39,131
```

Los cuatro aldeanos del jugador agotan el depósito de `Me` contiguo a su base
(tile 20,128) y la preferencia los envía al siguiente depósito de `Me`: un
neutral del centro del mapa, a más de 100 tiles. Marchan indefinidamente
mientras ignoran 2 depósitos de `A` y 1 de `B` situados a 8–20 tiles. Como todo
lo construible/entrenable/investigable cuesta `A`, el jugador queda sin
economía y **cada comando se rechaza con `ILLEGAL_STATE`**.

### §23.2 Regla

La **auto-asignación** de un ciudadano (la que hace `eco_find_nearest_deposit`
cuando el ciudadano no tiene una orden explícita del jugador) considera
**únicamente depósitos dentro de la zona aliada**.

Un depósito está en zona aliada del jugador `p` si su distancia a **algún
edificio COMPLETO propiedad de `p`** es menor o igual a
`ECO_AUTO_GATHER_RADIUS_RAW`. Se reutiliza el mismo recorrido de edificios que
`find_building_dropoff` (§6): `entity_kind == 1`, vivo, `owner == p`,
`build_progress >= build_time_ticks`.

Orden de resolución, todo con aritmética entera y recorrido ascendente:

1. Depósitos en zona aliada con `remaining > 0` **y** `resource_idx` igual al
   preferido (§18). Gana el más cercano; empate por índice más bajo.
2. Si no hay, depósitos en zona aliada con `remaining > 0`, cualquier recurso.
   Gana el más cercano; empate por índice más bajo.
3. Si no hay ninguno, el ciudadano pasa a `IDLE` (§22.2), **no** queda girando
   en `SEEK`.

### §23.3 La orden del jugador NO está acotada

`GATHER` emitido por el jugador asigna el depósito **directamente**, sin pasar
por `eco_find_nearest_deposit`. Por tanto **puede apuntar a cualquier depósito
del mapa**, dentro o fuera de la zona aliada, incluidos los neutrales
disputados. Es agencia del jugador y es deliberado: el patrón AoE2 es
«los aldeanos trabajan solos cerca de casa; expandir a recursos lejanos o
disputados es una decisión explícita del jugador».

Si un depósito asignado por el jugador se agota, la reasignación posterior
vuelve a ser auto-asignación y por tanto **sí** queda acotada por §23.2: el
aldeano regresa a trabajar cerca de casa en vez de seguir alejándose.

### §23.4 Constante

`ECO_AUTO_GATHER_RADIUS_RAW` se define en `economy.hpp` junto al resto de
constantes económicas. Valor inicial: **32 tiles**. Justificación: los
depósitos de base del mapa están a 8–20 tiles y los neutrales disputados a más
de 100, así que 32 separa ambos conjuntos con holgura y deja margen para
edificios de expansión.

Es un valor de balance. Cuando el catálogo gane un bloque de parámetros de
partida, debe migrar allí como dato; hasta entonces vive como constante con
nombre y comentario que explique el criterio.

### §23.5 Versionado

No introduce estado persistente nuevo: **`SAVE_FORMAT_VERSION` no cambia**. Sí
cambia la trayectoria de todo escenario con ciudadanos y depósitos, luego los
baselines de `ai_skirmish_eco` y `ai_skirmish_apertura` **se re-registran**; los
escenarios sin ciudadanos deben quedar **bit-idénticos**.

---

# PARTE V — Órdenes de combate (Sprint 1.13, cierre mecánico de Fase 1)

## §24 ATTACK, ATTACK_MOVE y proyectiles

**Origen**: este contenido estaba planificado como «Sprint 1.7» en
`PLAN_MAESTRO` §4 y **no se ejecutó**; el número se consumió con §22–§23. Su
DoD **es** el cierre mecánico de la Fase 1, y de él dependen arte y audio.

### §24.1 El problema que resuelve

Hoy el jugador solo puede dar `MOVE_TO`. El combate ocurre porque el sistema de
aggro (Sprint 0.3) engancha lo que tiene cerca. **El jugador no puede decidir a
quién ataca**, que es la carencia más visible que le queda a la partida.

### §24.2 Comandos

`CommandType` append-only. Tras `CRAFT = 14`:

- **`ATTACK = 15`** — `p.handle` = unidad propia viva; el objetivo va en
  **`p.unit_id` = índice del slot** y **`p.speed_mtpt` = generación**.

  **Corrección del contrato (2026-07-30, antes de implementar)**: escribí
  `p.target_handle`, un campo que **no existe** en `CmdPayload`. Añadirlo
  obligaría a **subir el formato de replay**, porque el replay serializa campo
  a campo y solo guarda un subconjunto (`handle`, `x_raw`, `y_raw`,
  `speed_mtpt`, `unit_id`). Reutilizar dos campos **ya serializados** evita ese
  bump y sigue el idioma que la base de código ya usa a propósito: `GATHER` usa
  `x_raw/y_raw` para el depósito, `RESEARCH_TECH` usa `unit_id` para el
  `TechId`, `CRAFT` para el `RecipeId`. La unidad persigue y ataca a **ese** objetivo hasta que
  muere, sale de alcance permanentemente, o llega otra orden.
- **`ATTACK_MOVE = 16`** — `p.handle` = unidad propia viva; `p.x_raw/y_raw` =
  destino en cota. La unidad avanza hacia el destino y **ataca lo que encuentre
  por el camino**, reanudando el avance al terminar.

**Orden de validación**, sin alterar el existente:

`ATTACK`: handle vivo · propio · es unidad (no edificio) · objetivo vivo ·
objetivo **no propio** (`NOT_OWNER` si lo es) ⇒ si no, el código
correspondiente.

`ATTACK_MOVE`: handle vivo · propio · es unidad · destino en cota
(`MALFORMED` si no).

### §24.3 Estado

```cpp
EntityHandle attack_target[ENTITY_HARD_CAP];  // INVALID si ninguno
uint8_t      order_mode[ENTITY_HARD_CAP];     // 0=NINGUNA 1=MOVE 2=ATTACK 3=ATTACK_MOVE
```

`order_mode` es a las unidades militares lo que `citizen_task` (§22) es a los
ciudadanos: **la autoridad única** sobre qué sistema decide su movimiento. El
patrón ya está probado y no se reinventa.

### §24.4 Interacción con aggro

`order_mode` **manda sobre el aggro automático**:

- `ATTACK`: el aggro **no** puede cambiar el objetivo. El jugador mandó.
- `ATTACK_MOVE`: el aggro elige objetivo, pero solo dentro del radio; al no
  haber nada, se reanuda el avance.
- `NINGUNA`: comportamiento actual, sin cambios.

**Esto es lo que hace la orden útil**: sin esta precedencia, `ATTACK` sería una
sugerencia que el aggro pisa, y el jugador lo notaría como que el juego no le
obedece.

### §24.5 Proyectiles

Unidades con `range_millitiles > 0` disparan proyectiles con **viaje**, no
daño instantáneo.

```cpp
struct Projectile {
    int64_t x_raw, y_raw;
    int64_t vel_x, vel_y;
    EntityHandle target;
    int32_t damage;
    uint8_t owner;
};
Projectile projectiles[PROJECTILE_HARD_CAP];  // 256
uint32_t n_projectiles;
```

- Movimiento entero por tick, misma aritmética que `movement_v1`. **Sin
  balística, sin gravedad, sin float.**

**CORRECCIÓN DEL DIRECTOR (2026-07-31): los proyectiles NO persiguen.**

Mi primera implementación los hacía perseguir al objetivo, y era un atajo, no
un diseño. En AoE2 la flecha vuela **al punto donde se predijo que estaría el
objetivo** y **falla** si éste se aparta — por eso moverse sirve de algo contra
los tiradores, y por eso existe una estadística de *precisión*.

Cómo queda:

1. Al disparar se estima el **tiempo de vuelo** (distancia dominante ÷
   velocidad) y se apunta a `posición + velocidad × vuelo` del objetivo. Todo
   entero.
2. La flecha **no corrige rumbo**. Cae donde se apuntó.
3. **Se resuelve en el punto de impacto**, no contra el objetivo en vuelo. Con
   paso discreto de 2 tiles/tick y radio de medio tile, una comprobación
   continua hacía que la flecha **saltara por encima** sin tocar nunca — nadie
   moría. Cae, y ahí se mira si el objetivo sigue en el radio.
4. Campo `guided` reservado: un **misil o un dron** de las edades tardías SÍ
   corrigen. Cuando existan, basta con ponerlo a 1 desde los datos del arma; la
   rama ya está escrita.

### §24.5.1 Progresión por edades — investigado (Empire Earth)

`docs/research/EMPIRE_EARTH_PROYECTILES.md`, con **12 marcas explícitas de NO
VERIFICADO** y citas a páginas concretas del manual. Cambia el plan que yo
tenía:

**1. NO existe una estadística de precisión, y no la vamos a inventar.** La
tabla oficial de atributos de EE enumera `Hit Points`, `Attack`, `Range`,
`Speed`, `Area Effect`, **cinco armaduras**, `Flight Time`, `Cargo` y `Power`
— y **no** enumera `Accuracy`, `Hit Chance` ni `Evasion` (manual pp. 103–104).
Yo había supuesto que la precisión mejoraría con tecnología; **no hay apoyo
para eso**. Nuestro fallo ya emerge de la predicción de tiro, que es
suficiente y no necesita una estadística nueva.

**2. El hallazgo aprovechable: la ARMADURA es por TIPO DE ARMA y cambia con la
edad.** EE separa `Arrow Armour`, `Gun Armour` y `Laser Armour`, así que
**cambiar de arma altera la matriz de contadores**, no solo el número de
ataque. Encaja exactamente con nuestros tres tipos de daño del §27, y sugiere
que la progresión de 15 edades debe expresarse **añadiendo tipos de daño**
(flecha → pólvora → láser) en vez de subiendo números.

**3. Cómo evita EE que 15 edades de armas sean inmanejables**, que era la
preocupación de fondo: **por reemplazo con conversión**. Avanzar de época
*habilita* el reemplazo; el jugador investiga la mejora en el edificio y **todas
las unidades viejas de esa línea se convierten**. No ocurre solo por pasar de
edad. Es el patrón a copiar.

**4. Guiado**: el misil verificado más temprano es de la época **XII**
(Atomic–Modern) y el láser de la **XIII**. Que el motor haga *homing* de verdad
es **NO VERIFICADO** en la fuente, así que nuestro campo `guided` se queda como
decisión propia, no como copia.

**Pendiente**: `Flight Time` es un atributo real de EE y aún no tenemos
equivalente; hoy la velocidad de proyectil es una constante global.
- Impacto cuando la distancia al objetivo `<= PROJECTILE_HIT_RADIUS_RAW`.
- Si el objetivo muere antes del impacto, el proyectil **se descarta**; no
  redirige ni cae al suelo.
- Cota dura: al llegar a `PROJECTILE_HARD_CAP`, los disparos nuevos **no se
  crean**. Determinista y documentado, nunca desbordamiento (SPEC-008 §3.3).

### §24.6 Versionado

`SAVE_FORMAT_VERSION` +1 · `CHECKSUM_ALGO_VERSION` +1. `attack_target`,
`order_mode`, el array de proyectiles y `n_projectiles` entran en serialización
y checksum.

### §24.7 Criterios de aceptación (= pruebas del sprint)

1. `ATTACK` sobre enemigo vivo es aceptado y la unidad se acerca hasta alcance.
2. `ATTACK` sobre **unidad propia** ⇒ `NOT_OWNER`.
3. `ATTACK` sobre entidad muerta ⇒ `INVALID_ENTITY`.
4. Con `ATTACK` activo, el aggro **no** cambia el objetivo aunque pase otro
   enemigo más cerca.
5. Al morir el objetivo, la unidad pasa a `NINGUNA` y el aggro vuelve a mandar.
6. `ATTACK_MOVE` avanza, se detiene a atacar lo que encuentra, y **reanuda** al
   despejar.
7. `ATTACK_MOVE` con destino fuera de cota ⇒ `MALFORMED`.
8. Un proyectil recorre la distancia en el número de ticks esperado y aplica el
   daño **en el tick del impacto**, no antes.
9. Si el objetivo muere en vuelo, el proyectil se descarta sin aplicar daño.
10. Con `PROJECTILE_HARD_CAP` proyectiles vivos, un disparo más **no se crea** y
    no hay desbordamiento.
11. `ATTACK`, `ATTACK_MOVE` y los proyectiles sobreviven save/load y replay
    con checksum idéntico.
12. Dos corridas del mismo escenario de combate producen impactos en los
    **mismos ticks** y el mismo checksum.
13. Mutar `order_mode` o `n_projectiles` cambia el checksum.
14. ~~Un escenario **sin** órdenes de combate es **bit-idéntico** al anterior,
    salvo el bump.~~

    **CRITERIO RETIRADO (2026-07-30, antes de implementar): era imposible.**
    §24.5 hace que toda unidad con `range_millitiles > 0` dispare proyectiles
    **con viaje** en vez de daño instantáneo. Eso cambia el momento en que se
    aplica el daño para **todas** las unidades a distancia, haya órdenes o no,
    así que ningún escenario con arqueros o artillería puede quedar
    bit-idéntico. Pedir las dos cosas a la vez es el mismo error de contrato
    que ya cometí en el 1.7 (identidad bit a bit **y** subida de checksum).

    **Lo sustituye**: la apertura sigue terminando con `winner=1` por debajo de
    36000 ticks y con las cuatro fases observadas. Las trayectorias se mueven a
    propósito y los baselines se re-registran con justificación.

---

# Parte VI — Estadísticas de unidad y combate por tipo de daño (Sprint 1.18)

**Origen**: petición del Director — *«cada una debe tener salud dependiendo de
su tipo y la unidad como tal, así como tecnologías, armadura para diversos
tipos de armas, velocidad, daño (con la característica del arma que se usa),
etc. similar a AoE2»*, y revisar también la reimplementación libre.

## §25 Lo que hay hoy, y un hallazgo

El daño actual es **un multiplicador de piedra-papel-tijera por clase**:

```cpp
dmg = attack[i] * rps_mult(clase_atacante, clase_objetivo) / 10000;
hp[best] -= dmg;                                  // step.hpp:1064-1072
```

**No existe armadura.** Ni una. Un aldeano y un legionario reciben el mismo
daño de la misma flecha, corregido solo por el multiplicador de clase.

**Y hay dato muerto**: `UnitDefinitionV1::bonus_vs_bp[6]` se declara en el
esquema, se valida, se compila al blob y se carga… y **no aparece ni una vez en
`step.hpp`**. Verificado con `grep`: cero usos en la simulación. Es decir,
llevamos sprints editando un campo que no cambia nada. Se resuelve aquí:
o se conecta, o se borra; lo que no puede seguir es fingiendo.

## §26 Los dos modelos de referencia

| | AoE2 | 0 A.D. |
|---|---|---|
| Tipos de daño | melee / pierce | **hack / pierce / crush** |
| Armadura | **resta plana** por tipo | **porcentaje** (nivel 1 = 10 %) |
| Clases de armadura | sí, muchas (caballería, edificio, camello…) | por tipo de daño |
| Suelo de daño | **mínimo 1** | **mínimo 1** |
| Bonos | suma por clase; la armadura base **no** los frena | — |

Fuentes: wiki de la serie AoE (vía búsqueda; la página directa da 402) y wiki
de 0 A.D.

### §26.1 Decisión: resta plana, y por qué

**Se adopta el modelo de AoE2 (resta plana), no el porcentual de 0 A.D.**

El motivo es del kernel, no de gusto: somos **enteros y deterministas, sin
float**. Una resta es exacta y no admite ambigüedad. Un porcentaje obliga a una
división y a **fijar para siempre una regla de redondeo** que además hay que
respetar en cada plataforma; es justo la clase de decisión que produce
divergencias entre máquinas y que ya nos costó trabajo cerrar con `Fixed64`.

El modelo porcentual es más elegante para equilibrar, y conviene decirlo. Pero
no compensa el riesgo.

## §27 El modelo

**Tres tipos de daño**, tomando la distinción de 0 A.D. porque separa el asedio
de lo demás y nosotros vamos a tener quince edades de artillería:

`CORTE` (cuerpo a cuerpo) · `PERFORACIÓN` (proyectiles) · `IMPACTO` (asedio).

Cada unidad y edificio gana:

```
armor[3]        // resta plana por tipo de daño, >= 0
attack_type     // el tipo que inflige SU arma
```

`hp`, `attack`, `speed` y `range` **ya existen** y ya vienen por unidad desde
datos: lo que falta es la armadura y el tipo.

### §27.1 Fórmula

```
base   = attack[atacante] - armor[objetivo][tipo_del_arma]
bono   = bonus_vs_class[atacante][clase_del_objetivo]
daño   = max(1, base + bono)
```

- **Nunca por debajo de 1**, como en ambos juegos: una unidad jamás es
  invulnerable a otra, solo muy resistente.
- **La armadura no frena el bono**, igual que AoE2. Es lo que hace que un
  contador sea un contador y no una mejora marginal.
- `bonus_vs_class` es `bonus_vs_bp` **conectado de verdad**, dejando de ser
  basis points para ser daño plano.

### §27.2 Lo que sustituye

`rps_mult_bp` desaparece del cálculo. La piedra-papel-tijera deja de ser un
multiplicador opaco por clase y pasa a ser **datos legibles**: armadura del que
recibe y bono del que ataca. El comportamiento cambiará; es intencionado.

## §28 Tecnologías que tocan estadísticas

Las tecnologías ya existen pero **solo conceden capacidades**. Aquí ganan
efectos sobre estadísticas —al estilo de la herrería de AoE2—: `+N` a un tipo
de armadura, `+N` al ataque, para un conjunto de clases de unidad.

Se aplican al **valor efectivo** en el momento del cálculo, no mutando la
definición del catálogo: la definición es inmutable y compartida, y mutarla
haría que la partida dependiera del orden de carga.

## §29 La interfaz de la selección múltiple

**Verificado en AoE2**: al seleccionar dos o más unidades, **los atributos
dejan de mostrarse** y el panel se llena con los **iconos de las unidades
seleccionadas**.

Nuestro panel hace hoy lo contrario: agrega HP en una sola barra y pierde de
vista quién está herido, que es justo lo que el jugador necesita saber.

1. **Una unidad seleccionada** → ficha completa: HP, ataque **con su tipo de
   daño**, las tres armaduras, velocidad y alcance.
2. **Dos o más** → **rejilla de iconos**, uno por unidad, cada uno con su
   **barra de vida individual**. Sin atributos agregados.
3. La rejilla se **ordena de forma determinista** (índice de slot ascendente)
   para que no baile entre fotogramas.
4. **Clic en un icono** selecciona solo esa unidad.
5. Si no caben todas, se indica **cuántas quedan fuera**; no se miente con un
   recuento parcial.

## §30 Criterios de aceptación

1. La armadura reduce el daño del **tipo correspondiente** y no de los otros.
2. El daño **nunca baja de 1**, ni con armadura muy superior al ataque.
3. El bono contra clase **no lo frena la armadura**.
4. `bonus_vs_bp` deja de ser dato muerto: **una prueba falla si se ignora**.
5. Dos unidades del mismo tipo con armaduras distintas reciben daños distintos
   de la misma fuente.
6. Una tecnología de armadura sube la efectiva **sin mutar el catálogo**.
7. Selección de **una** unidad: ficha completa con los tres valores de armadura.
8. Selección de **varias**: rejilla de iconos con barra de vida por unidad, en
   orden **determinista**.
9. Clic en un icono de la rejilla deja seleccionada esa unidad.
10. El escenario de la apertura sigue terminando con `winner=1` por debajo de
    36000 ticks: las trayectorias **cambian a propósito**, el resultado no.
