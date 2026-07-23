# SPEC-004 — Sistemas de partida · Partes I–II

Versión 0.2 (Parte I EJECUTADA en Sprint 1.1; Parte II DRAFT ejecutable para Sprint 1.2) · 2026-07-23 · Autor: Arquitecto (Claude)
Jerarquía: INDICE_MAESTRO → SPEC_ARQUITECTURA_BASE v1.1.1 → SPEC-001 v1.1 → SPEC-002 → **este documento**.
Partes futuras (III win-conditions y efectos de stats por tech, IV+) se añadirán en sprints posteriores **append-only**.

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
