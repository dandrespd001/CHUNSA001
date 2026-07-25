# BRIEF K1 — Identidad de civilización + depósitos desde el mapa (Sonnet · Sprint 1.6B, pieza 1)

Implementa **SPEC-004 §15.3, §16, §17 y §20** (`docs/specs/SPEC-004_SISTEMAS_PARTIDA.md`,
Parte III). Es la infraestructura de la apertura económica: el kernel deja de inventar los
depósitos y empieza a saber de qué civilización es cada dato. Lee §15–§17 y §20 enteros antes
de tocar nada. **NO implementes el comando GATHER ni la IA económica** — eso es K2.

## Rama y alcance
- Rama `sonnet/k1-civ-depositos` desde `main` (HEAD). Jamás toques `main`.
- Archivos esperados: `data_catalog.hpp` (§15.3 `CivId`/`civ_id` tipado + tabla de civs +
  `ResourceSpawnV1` del mapa), `game_state.hpp` (§17 `player_civ` + variante por-jugador de
  `gs_init_epoch_from_catalog`; §16 `gs_init_economy` desde el mapa), `step.hpp` (§17 gates de
  civ en PLACE_BUILDING/TRAIN_UNIT/RESEARCH_TECH), `checksum/serialize/save_io` (§20 save v12,
  dominio `CHUNSA_STATE_V7`), tests.

## Los 4 bloques
1. **`civ_id` tipado (§15.3)**: hoy el loader NO tipa `civ_id` (solo lo valida el compilador
   Python). Añade `CivId`/`INVALID_CIV_ID`, la tabla tipada de `kind=civ` con su índice por
   record_id (`catalog_find_civ`), y el campo `civ_id` resuelto en `UnitDefinitionV1`,
   `BuildingDefinitionV1` y `TechDefinitionV1`. **Patrón endurecido EXACTO** de tech/ai-profile
   (CveValue acotado, unique_ptr, reserve exacto, resolución diferida de referencias, rechazo del
   catálogo entero, búsqueda binaria bytewise) — lo auditará Opus, que ya audita este archivo
   desde el Sprint 0.4 y comparará con el patrón existente.
2. **`ResourceSpawnV1` + depósitos desde el mapa (§16)**: tipifica los `resource_spawns` del
   mapa (`kind`/`id`/`x_millitiles`/`y_millitiles`/`amount`; conversión exacta
   `raw = mt * FX_ONE_RAW / 1000`; `id` textual A/B/Me → `resource_idx` 0/1/2, cualquier otro
   valor rechaza el catálogo). `gs_init_economy` puebla `deposits[]` desde el mapa activo en
   orden canónico hasta `ECO_MAX_DEPOSITS` (si el mapa trae más ⇒ rechazo de carga, no truncar
   en silencio). **CRÍTICO — fallback legacy exacto**: sin catálogo o sin spawns, se conserva el
   patrón fijo de 6 depósitos BIT-IDÉNTICO (los tests de economía del 0.3 y los sintéticos deben
   quedar exactos; misma disciplina que el fallback del dropoff-edificio de §6).
   Nota: los datos del mapa los está poblando MiniMax en paralelo (`mm/datos-apertura-1.6b`, 12
   depósitos simétricos). NO dependas de esa rama: tipifica el formato del schema y usa un
   fixture propio en tus tests; el Arquitecto integra ambos.
3. **Civ y época por jugador (§17)**: `player_civ[MAX_EMITTERS]` (estado, serializado+
   checksummeado, init `INVALID_CIV_ID`); una forma explícita de asignarla en el setup (init del
   host o comando — elige y documenta, sin inferencias en caliente); **época inicial por
   jugador** = mínimo `epoch_window[0]` de los datos de SU civ (cierra la deuda del 1.2 que la
   calculaba catálogo-ancha; conserva la variante catálogo-ancha para escenarios sin civ);
   **gates de civ** en PLACE_BUILDING/TRAIN_UNIT/RESEARCH_TECH → **ILLEGAL_STATE** si
   `def.civ_id != player_civ[emitter]`, con dos exenciones obligatorias: (a) si el jugador tiene
   `INVALID_CIV_ID` el gate NO aplica (compatibilidad con todos los escenarios/tests
   existentes), (b) la ventana de setup del tick 0, como los demás gates.
4. **Persistencia (§20)**: save v12, dominio `CHUNSA_STATE_V7`, campos nuevos al final (append,
   sin migración — precedente D7). Regen por el procedimiento establecido.

## Tests obligatorios
Carga del `civ_id` real del blob (las 2 civs del repo resuelven) + rechazo de una referencia civ
inválida (fixture) · depósitos poblados desde un mapa fixture con spawns (posiciones/cantidades
exactas, conversión mt→raw) + rechazo si excede `ECO_MAX_DEPOSITS` · **fallback legacy
bit-idéntico** (sin spawns ⇒ los 6 depósitos exactos de siempre) · época inicial por jugador
distinta entre dos civs · gate de civ: rechaza construir/entrenar/investigar contenido de otra
civ, y NO rechaza cuando `player_civ` es INVALID · save/load v12 round-trip con `player_civ`.
GameState SIEMPRE en heap (`make_unique`) — en pila segfaultea bajo ctest.

## Reglas duras
Append-only en formatos/enums; iteración ascendente; cero float/heap en `step`; térmica
`nice -n 19 -j2` un build a la vez; **trayectoria bit-idéntica** de los escenarios que no usan
civ ni spawns de mapa (dump pre/post obligatorio: sintéticos, economía 0.3, skirmish militar y
eco deben dar los MISMOS checksums que en main); conservador ante huecos + desviación numerada.
NO toques el adaptador Godot. NO merges a main.

## Entrega
Commits atómicos + `docs/briefs/SONNET_K1_CIV_DEPOSITOS_RESULT.md`: desviaciones numeradas, gates
completos (golden, G1/G3/G4/G5, ctest N/N, skirmish militar + eco como regresión), checksums
nuevos, cómo asignas `player_civ` en el setup, y confirmación de la bit-identidad del fallback.
El Arquitecto revisa (+ Opus audita el parsing) e integra.
