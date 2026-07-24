# BRIEF K2 — La IA de 3 capas + escenario skirmish + gate de fase (Sonnet · Sprint 1.4, pieza 2)

Implementa **SPEC-005 §4, §5 y §8.3/§8.4** (`docs/specs/SPEC-005_IA_OPONENTE.md`): el cuerpo
real de `ai_execute` (la IA de 3 capas), el escenario CLI de skirmish, y los tests del gate de
fase. Es el NÚCLEO del sprint y del gate de la Fase 1. El `main` del que partes YA incluye K1
(condición de victoria `game_over`/`winner`/`participants_mask`, perfil `AiProfileV1` tipado,
save v11/checksum v6). Lee §0 (la regla de oro), §2 (andamiaje), §4, §5, §6 y §8 ENTEROS antes
de tocar nada.

## Rama y alcance
- Rama `sonnet/k2-ia-execute` desde `main` (HEAD, con K1). Jamás toques `main`.
- Archivos esperados: `ai_stub.hpp` (el cuerpo de `ai_execute` — el corazón), `driver.hpp`
  (solo si el escenario de skirmish lo requiere: un modo de partida real IA-vs-IA o
  humano-scripted-vs-IA para el gate §8.3; con cuidado, es código sensible a la trayectoria
  golden), `cli_run.hpp`/CLI (un subcomando `skirmish` para §8.3 si hace falta), tests nuevos.
  Puedes leer el `AiProfileV1` del catálogo enlazado (K1) para parametrizar la estrategia.

## La regla de oro (criterio de rechazo #1 — SPEC-005 §0)
`ai_execute(box, g)` es función PURA de `(g, box.source_tick, box.runtime_before)`:
- **CERO** `g.tick`, reloj de pared, `float`/`double`, o entropía fuera de `rng_draw`/
  `rng_range` con `RngStream::AI_TIEBREAK` y las coordenadas canónicas
  `(seed, stream, source_tick, entity_index, draw_index)`.
- **CERO** heap/STL dinámico: todo en `box.result[AI_MAX_COMMANDS]` y pila acotada.
- Orden de emisión CANÓNICO y determinista (recorrido ascendente por índice; desempates
  explícitos por `AI_TIEBREAK` o por menor índice, documentado).
Opus auditará esto línea a línea. Una sola violación rompe G4/G5.

## Diseño de la IA v1 (SPEC-005 §4) — resumen operativo
Emite hasta `AI_MAX_COMMANDS` (64) comandos por ciclo, todos con `emitter=box.ai_player`,
`target_tick=source_tick+AI_INPUT_DELAY_TICKS`, `sequence` creciente desde
`runtime_before.ai_sequence`. Tres capas en orden fijo, presupuesto repartido
estratégica→reactiva→táctica:
1. **Estratégica (§4.1)**: cuenta el estado macro del jugador IA (ciudadanos, soldados,
   edificios por tipo, stock, época, pop) por barrido ascendente; elige intención por utilidad
   entera parametrizada por los pesos `_bp` del perfil. Acciones: entrenar ciudadanos/soldados
   (`TRAIN_UNIT`), construir cuartel (`PLACE_BUILDING` en celda válida por barrido determinista
   desde un ancla fija por jugador + `ASSIGN_BUILD`), investigar/subir época si los gates de
   SPEC-004 §12 se cumplen.
2. **Reactiva (§4.3)**: si hay enemigos cerca del ancla de la base IA → el ejército defiende
   (MOVE_TO a la base) en vez de atacar.
3. **Táctica (§4.2)**: si el ejército ≥ umbral (`expansion_aggressiveness_bp`) → objetivo = el
   edificio productor enemigo más cercano al centroide del ejército (dist_sq entera, empate por
   menor índice), `FLOW_MOVE`/`MOVE_TO` del bloque hacia él.
La IA es OPTIMISTA (§5): emite lo que cree válido, lee receipts el ciclo siguiente, no entra en
bucles. Nunca supera el presupuesto de 64.
**Bump `AI_ALGO_VERSION` 1→2** (el procedimiento cambió por completo).

## El gate de fase (SPEC-005 §8.3 — el corazón, no lo trates como un test más)
Un escenario CLI de skirmish determinista donde la partida TERMINA en victoria:
- Corre hasta `game_over==1` con `winner != 0xFF` en **< 36000 ticks** (30 min).
- **Dos corridas idénticas** → mismo `winner` y mismo tick de fin (determinismo).
- **Save a mitad + continuar** → mismo resultado. **Replay** → bit-exacto.
Diséñalo para que la IA REALMENTE gane/pierda (no que se estanque 36000 ticks — eso es fallo).
Un setup simétrico por comandos (ambos jugadores IA con el mismo perfil, o asimétrico
Egipto/Roma) que converja. Documenta el tick de fin y el ganador observados.

## Tests obligatorios (§8.4)
- Cada capa en un fixture pequeño: la IA construye un cuartel (verifica que emite
  PLACE_BUILDING+ASSIGN_BUILD válidos), entrena (TRAIN_UNIT), y ataca (MOVE_TO hacia el enemigo).
- Presupuesto de comandos respetado (nunca > AI_MAX_COMMANDS).
- **Determinismo de ai_execute**: dos ejecuciones con el mismo (g, source_tick, runtime) dan
  `result[]` idéntico byte a byte.
- El gate §8.3 completo (partida termina, determinista, save/replay).
- GameState SIEMPRE en heap en los tests (make_unique).
- G1/G3/G4/G5 con IA REAL: G4 `ai_executions>0` (la IA sí ejecuta), G5 feed-mode
  `ai_executions==0` `schedule_mismatches==0`; idénticos gcc/portable.

## Reglas duras
Append-only en enums/formatos; iteración ascendente; cero float/heap en `ai_execute` y en
cualquier ruta nueva de `step`; térmica `nice -n 19 -j2` un build a la vez; trayectoria golden
de escenarios SIN IA bit-idéntica (dump pre/post); conservador ante huecos + desviación
numerada. Si tocas `driver.hpp` para el skirmish, NO alteres el escenario `build_human_batch`
existente (crea uno nuevo). NO merges a main.

## Entrega
Commits atómicos + `docs/briefs/SONNET_K2_IA_EXECUTE_RESULT.md` (desviaciones numeradas, gates
completos, el tick de fin y ganador del skirmish, checksums, y una nota explícita de cómo
garantizaste la regla de oro del determinismo en cada capa). El Arquitecto revisa (+ Opus
audita el determinismo de ai_execute) e integra.
