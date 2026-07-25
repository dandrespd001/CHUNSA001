# Revisión K1 — civ tipada + depósitos desde el mapa (Sprint 1.6B)

Implementó: Sonnet · Auditó: Opus · Revisó e integró: Arquitecto (Claude) · 2026-07-25

## Veredicto
**ACEPTADO CON 4 ENDURECIMIENTOS + 1 FIX DE TEST.** La implementación es sólida (patrón
endurecido espejo fiel, fallback legacy preservado, época por jugador cerrando la deuda del
1.2), y la auditoría destapó **un P1 real y explotable** que se corrigió y se probó.

## Lo entregado (Sonnet, `sonnet/k1-civ-depositos`, 5 commits)
- `civ_id` tipado (`CivId`/`CivNameIndexV1`, `catalog_find_civ`) en unit/building; en tech se
  **deriva de `available_to`** porque el schema no declara un `civ_id` escalar (D1, decisión
  correcta: falla cerrado ante 0, >1 o civ inexistente).
- `ResourceSpawnV1` + `gs_init_economy_from_catalog` **opt-in**: el fallback legacy de 6
  depósitos queda bit-idéntico para todo el que no la llame (protege los goldens).
- `player_civ[]` + `gs_set_player_civ` + **época inicial por jugador** (cierra la deuda de K2
  del 1.2, que la calculaba catálogo-ancha) + gates de civilización en PLACE_BUILDING/
  TRAIN_UNIT/RESEARCH_TECH, exentos si `player_civ == INVALID_CIV_ID` (compatibilidad) y en la
  ventana de setup.
- Save v12 / checksum v7 (`CHUNSA_STATE_V7`).

## Auditoría Opus: SIN P0, pero 1 P1 real
**P1 — `resource_spawns` no se validaban contra la cota del mundo.** El schema permite hasta
2^31-1 mili-tiles (≈262× el mundo de 8192 tiles). Opus demostró empíricamente que un blob con
un depósito fuera de cota **congelaba el kernel**: `dist_sq_raw` marca
`FatalReason::WORLD_BOUNDS` en el primer tick que un aldeano lo evalúa y `step()` queda muerto
para siempre. Entrada no confiable que pasa el trust boundary y deja el juego inservible.
→ **Corregido por el Arquitecto** (rechazo del catálogo entero, política SPEC-002 §7) **y
probado**: parcheé los bytes de un blob válido para poner un depósito en 32768 tiles; el loader
devuelve `InvalidMap` (antes devolvía `Ok`), y el blob legítimo sigue cargando sin falso
positivo. (Nota: el compilador Python **también** rechaza esos datos — la defensa del lado de
datos ya existía; el guard cubre el blob manipulado que evita el pipeline, que es el modelo de
amenaza real de ADR-018/mods.)

**P2-2 — falta de defensa en profundidad en el consumidor.** `gs_init_economy_from_catalog`
copiaba `map_resource_spawn_count` sin re-clampar, confiando en que el cap del loader lo hace
irrepresentable. Cierto hoy, pero es un único punto de fallo. → **Clamp defensivo añadido.**

**P3-1 — `civ_names` era la única tabla nombre-índice sin `reserve()`.** → Añadido (paridad
exacta del patrón).

**P3-3 — `kind: material` rechazaba el catálogo entero.** El enum del schema admite
`{resource, material}` y los materiales son contenido **legítimo** de Fase 2 (recetas),
excluidos del alcance de este sprint por el PLAN_MAESTRO. Rechazar por un valor válido del
schema es una bomba de compatibilidad. → **Semántica cambiada: los spawns de material se
IGNORAN** (documentado en código y test). El test 3e de Sonnet, que codificaba el
comportamiento viejo, se actualizó al contrato nuevo, y se añadió el test 3f del guard del P1.

## P2-1 — hallazgo de PROCESO (ya neutralizado, pero importante)
La rama de Sonnet **revertía los 12 depósitos del mapa** que MiniMax había añadido a main: se
ramificó de un main anterior al merge de datos y su copia del YAML/blob era la vieja. Opus lo
detectó y avisó de que ningún test lo cazaría (los tests de spawns usan fixtures propios).
**Mi merge lo neutralizó** (git resolvió a favor de main; verificado: 12 spawns y
`content_hash 45b652bf` intactos en la integración, más la corrida end-to-end).
**Lección de proceso**: cuando dos agentes trabajan en paralelo y uno ramifica de un main que
luego avanza, la integración DEBE verificar los datos explícitamente, no solo que los tests
pasen. Añadido a la doctrina de delegación.

## Verificación independiente del Arquitecto
- **End-to-end con el blob real** (lo que ningún agente podía hacer solo: MiniMax no tenía
  kernel, Sonnet no tenía los datos): los 12 depósitos llegan al kernel (6 legacy → 12 del
  mapa), la conversión mili-tiles→Q47.16 es **exacta sin deriva** (122500 mt → 122.500 tiles),
  la **simetría del mapa sobrevive el pipeline completo** (12.5↔243.5, 28.5↔227.5, 114↔142, y
  los dos del eje en 128.0), `civ_id` resuelve (egipto=0, roma=1) y el centro egipcio
  referencia su civ con 1 unidad entrenable.
- **ctest 22/22** · build `-Werror` 0 warnings · golden/G1/G3/G4/G5 verdes (verificados por
  Sonnet contra main real, con la trayectoria de ambos skirmish idéntica salvo el bump de
  dominio v6→v7 intencional).

## Estado y siguiente
`gs_init_economy_from_catalog` es opt-in y **nadie la llama todavía** — el escenario de K2 será
el primero, y ahí se verá la apertura económica real. Siguiente: **K2** (`GATHER` + la IA
jugando la apertura + el escenario del DoD), brief en
`docs/briefs/SONNET_K2_GATHER_APERTURA_SPRINT_1.6B.md`.
