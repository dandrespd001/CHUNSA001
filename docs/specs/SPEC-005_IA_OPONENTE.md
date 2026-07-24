# SPEC-005 — IA oponente v1 + partida completa (Sprint 1.4)

Versión 0.1 (DRAFT ejecutable) · 2026-07-24 · Autor: Arquitecto (Claude)
Jerarquía: INDICE_MAESTRO → SPEC_ARQUITECTURA_BASE v1.1.1 → **SPEC-001 §7 (contrato de IA)** →
SPEC-004 (sistemas de partida) → SPEC-002 (datos) → **este documento**.
Referencia de diseño: doc 30 (IA 3 capas) del corpus. Es el **GATE DE FASE 1**: al cerrarlo,
CHUNSA debe ser una partida de 30+ min jugable y ganable, Egipto vs Roma, humano vs IA.

---
## §0 Principio inviolable — la regla de oro del determinismo
La decisión de la IA es un **emisor de comandos más**, idéntico en trato a un humano
(SPEC-001 §7). El punto de decisión `ai_execute(box, g)` es una **función pura** de
`(g, box.source_tick, box.runtime_before)`:
- **PROHIBIDO** leer `g.tick`, cualquier reloj de pared, cualquier float/double, cualquier
  fuente de entropía que no sea `rng_draw`/`rng_range` con `RngStream::AI_TIEBREAK` y las
  coordenadas canónicas `(seed, stream, source_tick, entity_index, draw_index)`.
- **PROHIBIDO** asignar memoria (heap) o usar contenedores STL dinámicos dentro de
  `ai_execute` (mismo régimen que `step`): todo cabe en `AiJobBox::result[AI_MAX_COMMANDS]`
  y en variables locales de pila acotadas.
- La IA lee el `GameState` directamente, que es **enteros deterministas** (Q47.16/enteros
  canónicos): eso satisface "nunca decidir sobre un snapshot float" de SPEC-001 §7.
- Cualquier cambio del procedimiento de decisión ⇒ **bump `AI_ALGO_VERSION`** (los replays
  viejos se re-ejecutan con su versión; §7 de este doc).
Si esta regla se viola, G4/G5 divergen y el replay deja de ser bit-exacto. **Es el criterio
de rechazo #1 en la revisión y la auditoría de Opus.**

## §1 Objetivo del incremento
Sustituir el stub de `ai_execute` (hoy emite MOVE_TO aleatorios) por una **IA competente v1
de 3 capas** que juega una partida real: recolecta, construye, entrena, investiga, sube de
época y ataca; y añadir la **condición de victoria/derrota** (SPEC-004 Parte III mínima) para
que la partida termine de forma determinista. NO se busca una IA fuerte ni "justa" en
información (v1 puede leer todo el estado); se busca una IA **determinista, competente y que
gana o pierde**, suficiente para el gate de fase.

## §2 Andamiaje existente (NO reinventar — SPEC-001 §7, Sprint 0.1B)
Ya implementado y verde en G4/G5 (hoy con un stub):
- `AiJobBox` con máquina EMPTY→DISPATCHED→RUNNING→COMPLETED y su serialización
  (`ai_serialize`/`ai_deserialize`), `AiRuntimeV1{decision_epoch, ai_sequence}`.
- Ciclo en `driver.hpp::drive`: `ai_should_dispatch` (fase `tick%20==7`), `ai_dispatch`
  (congela `source_tick`+`runtime_before`), `ai_execute` (calcula), `ai_due`
  (`source_tick+AI_INPUT_DELAY_TICKS`, delay=4), `ai_commit` (avanza `ai_sequence`/epoch),
  `ai_stalled` (watchdog). Constantes: `AI_DECISION_PHASE=7`, `AI_INPUT_DELAY_TICKS=4`,
  `AI_MAX_COMMANDS=64`.
- `continuation_checksum` incluye el `AiJobBox`+runtime (G4). El replay graba los comandos de
  IA ya materializados (ADR-019): en feed-mode `ai_executions==0` (G5).
**El trabajo de este sprint es el CUERPO de `ai_execute` + la condición de victoria + el
perfil tipado + el escenario de partida; el lifecycle NO se toca.**

## §3 Perfil de IA tipado (extiende el loader CHDB — SPEC-002)
Hoy los records `kind=ai-profile` solo se validan estructuralmente. Tipificar en
`data_catalog.hpp` (mismo patrón endurecido de unit/building/tech), leyendo del
`ai-profile.schema.json` existente (`base:demo_normal`):
```cpp
struct AiProfileV1 {
    AiProfileId id;
    // strategic_weights_bp (basis points 0..10000):
    int32_t economy_focus_bp, military_focus_bp, tech_focus_bp,
            expansion_aggressiveness_bp, risk_tolerance_bp;
    // difficulty_params:
    uint32_t decision_period_ticks;   // debe ser divisor coherente con AI_DECISION_PHASE
    uint32_t reaction_latency_ticks;  // informativo v1 (el delay real es AI_INPUT_DELAY_TICKS)
    // tactical_behaviors[0] (v1 usa el primero):
    int32_t retreat_hp_threshold_bp, retreat_morale_threshold_bp;
};
catalog_find_ai_profile(cat, "base:demo_normal", ...);
```
Los pesos `_bp` PARAMETRIZAN la capa estratégica (§4.1): son datos, no código. La IA v1 usa
`base:demo_normal` para ambos jugadores IA. Referencias no resolubles ⇒ rechazo del catálogo
entero (patrón §7 de SPEC-002). El blob NO cambia de formato (el record ya viaja).

## §4 La IA de 3 capas (doc 30) — diseño v1
`ai_execute` emite hasta `AI_MAX_COMMANDS` comandos por ciclo (uno cada 20 ticks = 1 s),
todos con `emitter = box.ai_player`, `target_tick = source_tick + AI_INPUT_DELAY_TICKS`,
`sequence` tomada de `runtime_before.ai_sequence` (creciente estricto por emisor). Orden de
emisión canónico y determinista. Las tres capas se recorren en orden fijo; el presupuesto de
64 comandos se reparte con prioridad estratégica→reactiva→táctica.

### §4.1 Capa estratégica (qué hacer) — utility sobre enteros
Evalúa el estado macro del jugador IA (contadores derivados del `GameState` por recorrido
ascendente: nº de ciudadanos, soldados, edificios por tipo, stock A/B/Me, época, pop_used) y
elige una **intención** por utilidad entera (basis points del perfil, sin float):
- **Expandir economía**: si ciudadanos < objetivo o stock bajo → entrenar ciudadanos en el
  centro (`TRAIN_UNIT`), asignar ociosos a recolectar (los ciudadanos ya recolectan solos por
  la máquina económica; la IA solo garantiza que existan y no estén parados).
- **Construir**: si falta cuartel y hay stock → `PLACE_BUILDING` de un cuartel en una celda
  válida cerca de la base (búsqueda determinista de celda libre: barrido ascendente desde un
  ancla fija por jugador); `ASSIGN_BUILD` de un ciudadano al sitio.
- **Militarizar**: si hay cuartel completo y stock → `TRAIN_UNIT` de unidades de combate
  (según `military_focus_bp`).
- **Tecnología/época**: si se cumplen los gates (§SPEC-004 §12) y `tech_focus_bp` lo favorece
  → `RESEARCH_TECH` / `EPOCH_UP`.
La elección es **determinista**: ante empate de utilidad, menor índice de intención (o
`AI_TIEBREAK` si el diseño lo requiere explícitamente). Los pesos del perfil desempatan de
forma reproducible.

### §4.2 Capa táctica (cómo ejecutar el ataque)
Cuando la intención estratégica es atacar (ejército ≥ umbral de `expansion_aggressiveness_bp`):
- Selecciona el objetivo: el **edificio productor enemigo más cercano** al centroide del
  ejército IA (métrica `dist_sq` entera, empate por menor índice — igual que combat/aggro), o
  la unidad enemiga más cercana si no hay edificios.
- Emite `FLOW_MOVE`/`MOVE_TO` del ejército hacia el objetivo (el `aggro_system` y el combate
  ya resuelven el enfrentamiento local; la IA solo dirige el grueso). Agrupa por proximidad;
  v1 puede mandar todo el ejército como un bloque (formaciones/posición fina = combate v2,
  fuera de alcance, registrado en memoria del Director).

### §4.3 Capa reactiva (defensa)
Antes de gastar presupuesto en ataque: si hay enemigos dentro de un radio de la base IA (ancla
fija) → redirige el ejército a defender (MOVE_TO a la base). Umbrales de retirada por unidad
(`retreat_hp_threshold_bp`/`retreat_morale_threshold_bp`) los aplica el kernel de moral ya
existente; la capa reactiva solo decide "defender vs atacar".

## §5 Comandos que emite y validación
La IA emite exclusivamente comandos ya existentes: `TRAIN_UNIT`, `PLACE_BUILDING`,
`ASSIGN_BUILD`, `RESEARCH_TECH`, `EPOCH_UP`, `SET_RALLY`, `MOVE_TO`, `FLOW_MOVE`. **El kernel
valida cada uno** (owner, época, stock, cola, pop, footprint…); la IA v1 es **optimista**:
emite lo que cree válido y lee el resultado por su mailbox de receipts en el siguiente ciclo
(no reintenta agresivamente; evita bucles). La IA NUNCA usa la exención de escenario
(`effective_tick==0` es inalcanzable para ella: sus comandos llevan delay 4). **Presupuesto**:
nunca más de `AI_MAX_COMMANDS`; si se alcanza, se corta el ciclo (determinista).

## §6 Condición de victoria/derrota (SPEC-004 Parte III mínima)
Estado nuevo en `GameState` (ESTADO: serializado + checksummeado):
```cpp
uint8_t  game_over;   // 0 = en curso, 1 = terminado
uint8_t  winner;      // emisor ganador si game_over==1; 0xFF = sin decidir/empate
```
Regla v1 **determinista**, evaluada al final de `step` (tras el destroy batch), barrido
ascendente:
- Un jugador está **derrotado** cuando no tiene ninguna entidad viva que sea (a) un edificio
  productor (centro o cuartel, `entity_kind==1` con `train_count>0` o el centro) **ni** (b) un
  ciudadano capaz de reconstruir. Definición v1 concreta y testeable: **derrotado = 0 edificios
  vivos propios Y 0 ciudadanos vivos propios** (no puede producir ni reconstruir).
- Si exactamente un jugador con entidades iniciales queda no-derrotado → `game_over=1`,
  `winner = ese jugador`. Si todos derrotados en el mismo tick → `game_over=1`, `winner=0xFF`
  (empate). Una vez `game_over==1`, `step` deja de evaluar (congela el resultado).
- **Determinismo**: sin RNG, sin float; puro conteo por índice ascendente. `game_over`/`winner`
  entran al checksum y al save (bump correspondiente).
El límite de tiempo (partida de 30 min = 36000 ticks) NO es una condición del kernel: es del
escenario/driver (si nadie gana en 36000 ticks, el gate lo trata como "no concluyente" y es un
fallo de diseño de la IA, no del kernel).

## §7 Persistencia, checksum y versiones
- **Save v11**: `game_over`/`winner` al final del stream (append, precedente D7 sin migración).
  El `AiJobBox`+`AiRuntimeV1` YA se serializan (0.1B); nada nuevo ahí.
- **Checksum**: `game_over`/`winner` entran al dominio → bump a `CHUNSA_STATE_V6` con regen
  golden por el procedimiento establecido (dump pre/post bit-idéntico de un escenario previo).
- **`AI_ALGO_VERSION`**: 1 → 2 (el procedimiento de `ai_execute` cambió por completo). Los
  replays v3 que se graben ahora fijan esta versión implícitamente (los comandos de IA van
  materializados en el stream; el feed-mode no re-ejecuta la IA).
- **Replay v3**: sin cambios de formato (los comandos de IA viajan como cualquier comando; ya
  se graban por ADR-019).

## §8 Gates del sprint (DoD binario — es el gate de FASE)
1. **G1** alloc_delta=0 con IA activa (la IA no asigna en Step) · **G3** save/continuar ==
   corrida continua con IA · **G4** matriz con IA real (`ai_executions>0`, state+continuation
   bit-exactos idénticos gcc/portable) · **G5** record+replay con IA (feed-mode
   `ai_executions==0`, `schedule_mismatches==0`, checksum coincide).
2. **Golden 1074/1074** · trayectoria pre/post bit-idéntica de un escenario SIN IA (la IA no
   cambia el comportamiento de los escenarios que no la usan).
3. **Partida completa determinista y GANABLE**: un escenario CLI de skirmish (IA vs IA o
   humano-scripted vs IA) corre hasta `game_over==1` con `winner != 0xFF` en **< 36000 ticks**;
   **dos corridas idénticas** dan el mismo `winner` y el mismo tick de fin (determinismo); un
   **save a mitad + continuar** produce el mismo resultado; un **replay** reproduce bit-exacto.
   Este es el corazón del gate de fase.
4. Tests unitarios: perfil de IA tipado (carga + rechazo) · condición de victoria (jugador sin
   edificios ni ciudadanos → derrota; el otro gana; empate simultáneo) · cada capa de la IA
   ejercitada en un fixture pequeño (la IA construye un cuartel, entrena, ataca) · presupuesto
   de comandos respetado.
5. `ctest` completo verde · build `-Werror` · cero float/heap en `ai_execute` y en la nueva
   ruta de `step` (grep + contador CI).
6. **Experiencia jugable (Godot)**: un modo skirmish humano (owner 0) vs IA (owner 1) con
   setup simétrico por comandos, y pantalla de victoria/derrota leyendo `game_over`/`winner`
   del snapshot. (La verificación DETERMINISTA del gate es la del CLI §8.3; Godot es la cara
   jugable para el Director.)

## §9 Reparto (buenas prácticas: el determinismo crítico NO se fragmenta)
- **Arquitecto**: este contrato, revisión, integración, auditoría (delegada a Opus).
- **Sonnet K1**: perfil de IA tipado (§3) + condición de victoria (§6) + save v11/checksum v6
  (§7). Infraestructura, se integra ANTES de K2.
- **Sonnet K2**: el cuerpo de `ai_execute` (§4/§5, las 3 capas) + el escenario CLI de skirmish
  y los tests del gate (§8.3/§8.4). El núcleo determinista — indelegable a un modelo de bajo
  razonamiento por el riesgo de divergencia.
- **Opus**: auditoría del determinismo de `ai_execute` (¿alguna ruta usa `g.tick`/float/heap/
  RNG fuera de `AI_TIEBREAK`? ¿el orden de emisión es canónico?) y del parsing del perfil.
- **Codex/Luna Max**: modo skirmish jugable + UI de victoria/derrota en Godot (§8.6). Al final,
  contra el main con K2.
- **MiniMax** (opcional): afinar los pesos del `ai-profile` / un perfil "agresivo" adicional
  como datos, validado por el gate `data_compile`.

## §10 Notas de alcance (v1 explícito, para no sobre-diseñar)
- La IA v1 puede leer todo el estado (sin fog para la IA); "IA justa en información" es
  post-1.0. La reactividad es básica (defender vs atacar). Formaciones/posición fina de combate
  = combate v2 (candidato Fase 2, directriz del Director en memoria). La diplomacia
  (`diplomacy_openness_bp`) se ignora en v1 (1v1). El objetivo es el **gate**: una partida que
  termina, es ganable y es bit-exacta reproducible.
