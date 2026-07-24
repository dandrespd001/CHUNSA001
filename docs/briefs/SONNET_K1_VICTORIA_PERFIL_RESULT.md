# RESULT — K1: condición de victoria + perfil de IA tipado (Sprint 1.4)

Autor: sonnet-5. Brief: `docs/briefs/SONNET_K1_VICTORIA_PERFIL_SPRINT_1.4.md`.
Spec: `docs/specs/SPEC-005_IA_OPONENTE.md` §0/§3/§6/§7/§8.4.
Rama: `sonnet/k1-victoria-perfil` (desde `main`, HEAD `400b921`). **NO se hizo
merge a `main`.**

## Commits

Atómicos, en este orden (ver `git log sonnet/k1-victoria-perfil`):
1. `feat: condición de victoria/derrota (SPEC-005 §6) + save v11/checksum v6`
   — `game_state.hpp`, `step.hpp`, `checksum.hpp`, `serialize.hpp`, `save_io.hpp`.
2. `feat: perfil de IA tipado ai-profile (SPEC-005 §3)` — `data_catalog.hpp`.
3. `test: victoria + perfil de IA (Sprint 1.4 K1)` — `CMakeLists.txt`,
   `tests/unit/test_victory_ai_profile.cpp`.
4. `docs: RESULT K1 victoria/perfil (Sprint 1.4)` — este documento.

`ai_execute`/`driver.hpp` **NO se tocaron** (fuera de alcance, K2).
`AI_ALGO_VERSION` se mantiene en 1 (el bump a 2 es de K2, cuando cambie el
cuerpo de `ai_execute`).

## Mi definición exacta de "jugador activo" (contrato no negociable #1)

El brief ofrecía dos opciones de partida ("los que tienen `player_count` de
la config, o los que alguna vez tuvieron una entidad") y pedía elegir y
documentar. Elegí una versión **más estricta** que la segunda opción, por un
motivo concreto descubierto al analizar los fixtures existentes:

**Un jugador `p` es "activo" ⟺ en algún momento del partido tuvo AL MENOS UN
edificio (`entity_kind==1`) o UN ciudadano (`unit_class==3`) vivo,
simultáneamente con `game_over==0`.** Se registra de forma **monótona** (solo
se enciende, nunca se apaga) en un nuevo campo `GameState::participants_mask`
(`uint16_t`, un bit por emisor, `MAX_EMITTERS==16`). La condición de
victoria/derrota **solo se evalúa si hay ≥ 2 jugadores activos**; con 0 o 1,
`game_over` nunca se activa por este mecanismo.

Por qué NO bastaba con "`cfg.player_count`" ni con "alguna vez tuvo
**cualquier** entidad":

- **`cfg.player_count` a secas es peligroso**: `cli_run.hpp::run_synthetic`
  fija `player_count=1` (el escenario sintético de movimiento estándar del
  CLI/G1/G3). Si se cuenta ese único jugador configurado como "activo" sin
  más, y no tiene ni edificios ni ciudadanos (usa `SPAWN_DEBUG`, unidades
  genéricas), la regla literal ("0 no derrotados ⇒ empate") declararía
  `game_over=1, winner=0xFF` en el primer tick de **todo** el corpus de
  escenarios sintéticos existentes — rompiendo exactamente la garantía que
  el propio brief pide proteger ("la trayectoria golden previa no cambia").
  Peor aún: los fixtures de `test_production_tech.cpp`/`test_buildings.cpp`/
  `test_replay_v3.cpp`/`test_state.cpp` usan `player_count=2` pero **solo
  pueblan al jugador 0** (el jugador 1 nunca recibe ninguna entidad en toda
  la vida del test) — con `cfg.player_count` puro, el jugador 0 "ganaría"
  espuriamente desde el primer tick en docenas de sub-tests ya existentes.
- **"Alguna vez tuvo cualquier entidad" tampoco basta**: la matriz G4 de
  `driver.hpp` (con `--ai`, `player_count=2`) da a AMBOS jugadores unidades
  `SPAWN_DEBUG` (índices pares/impares). Si "activo" se basara en "tuvo
  cualquier entidad" en vez de "tuvo edificio/ciudadano", ambos jugadores
  entrarían al cómputo desde tick 0 y, como sus unidades debug no son ni
  edificios ni ciudadanos, se declararía empate espurio de inmediato en esa
  matriz también.

La definición elegida (edificio-o-ciudadano, no "cualquier entidad") hace
que **ninguno de los dos escenarios anteriores dispare la evaluación jamás**
— quedan excluidos por construcción, exactamente el mismo patrón ya usado en
`step.hpp` para `aggro_system` ("unidades con `attack>0` … excluye por
construcción a `SPAWN_DEBUG` … preserva los vectores golden sin cambio
alguno", comentario preexistente en el archivo). El gate adicional de `≥2
activos` cubre el caso restante: un `player_count` configurado a 1, o a 2
pero con un solo jugador real (el resto de fixtures citados).

**Regla de derrota** (instantánea, no monótona, sobre el subconjunto
activo): un jugador activo está derrotado si, EN ESE TICK, no tiene ningún
edificio vivo propio **ni** ningún ciudadano vivo propio. Empate si los ≥2
activos quedan derrotados en el mismo tick (`winner=0xFF`); victoria si
exactamente uno queda no-derrotado (`winner`=ese emisor). Congelado
(`game_over==1`) en cuanto se fija: `step()` deja de reevaluar para siempre
(verificado explícitamente en test, incluso destruyendo al "ganador"
después — el resultado no se revierte a empate).

Todo el cómputo es un único barrido ascendente por índice de entidad + un
segundo barrido por `[0, player_count)`, sin RNG/float/heap (arrays locales
fijos de 16 `bool`, en pila) — ver `step.hpp::detail::victory_check`.

## Puntos de contrato — resumen de la implementación

1. **`GameState`** gana 3 escalares del partido (no por-slot): `game_over`
   (u8), `winner` (u8, 0xFF=empate/pendiente), `participants_mask` (u16).
   `gs_init` los fija explícitamente (`winner=0xFF`; los otros dos ya quedan
   en 0 por el `memset`); `zero_components` NO se toca (son escalares del
   partido, no componentes por-entidad).
2. **`step.hpp::detail::victory_check`**, llamada al final de `step()` tras
   el destroy batch (dentro del mismo bloque `if (fatal==NONE)` que el resto
   del pipeline de ese tick, mismo criterio que el destroy batch).
3. **`AiProfileV1`** en `data_catalog.hpp`: espejo EXACTO del patrón
   endurecido de `TechDefinitionV1` — `unique_ptr` posee el `Impl` durante
   toda la construcción, `reserve` exacto antes de llenar, rechazo del
   catálogo entero ante cualquier campo fuera de rango o estructuralmente
   inválido, `catalog_find_ai_profile` por búsqueda binaria bytewise (idéntico
   a `catalog_find_tech`/`catalog_find_building`/`catalog_find_capability`).
   Sin referencias cruzadas a otras secciones (a diferencia de
   building/tech): todos los campos son valores inline del propio record.
   El código de error nuevo (`InvalidAiProfile`) es append-only al final del
   enum, sin renumerar los existentes.
4. **Save v11 / checksum v6**: `game_over`/`winner`/`participants_mask`
   añadidos AL FINAL del stream de `gs_serialize`/`gs_deserialize` y AL
   FINAL del dominio de `state_checksum_v1` (dominio `"CHUNSA_STATE_V6"`,
   15 bytes, mismo patrón de los 5 bumps anteriores). Sin migración v10→v11
   (precedente D7): un save v10 real falla el check `SAVE_FORMAT_VERSION`
   del envelope, no hay ruta de compatibilidad hacia atrás.

## Desviaciones numeradas (conservador ante huecos del SPEC)

**D1 — gate de ≥2 jugadores activos.** No está en la letra literal de
SPEC-005 §6 (que solo habla de "jugadores con entidades iniciales"). Lo
añadí porque, sin él, *cualquier* definición razonable de "activo" que
incluyera un único jugador real dispararía un "empate"/"victoria" espurio en
escenarios de un solo jugador real (ver la sección de arriba — el problema
NO es la elección entre las dos opciones que ofrecía el brief, sino que
ninguna de las dos, sola, alcanza para no romper el corpus de fixtures
existente sin este gate adicional). Impacto: una partida 1v1 real (K2) sigue
funcionando exactamente igual (siempre hay 2 jugadores activos en cuanto
ambos plantan su centro inicial); lo único que cambia es que un escenario
degenerado de 0 o 1 jugador real nunca "termina" por este mecanismo — que es
precisamente el comportamiento correcto (no puede haber "ganador" sin
adversario).

**D2 — criterio de "activo" = edificio-o-ciudadano, no "cualquier
entidad".** Ver justificación completa arriba. Es una interpretación MÁS
estricta de "entidades iniciales" que la sugerida literalmente por el
brief, elegida porque es la única que preserva sin cambios TODO el corpus de
fixtures/escenarios existentes (golden sintético G1/G3, matriz G4/G5 con
`SPAWN_DEBUG`, y los ~5 archivos de test con `player_count=2` pero un solo
jugador realmente poblado).

**D3 — `participants_mask` es estado nuevo no listado literalmente en el
brief.** El brief describía `game_state.hpp` como "`game_over`/`winner` +
init" — no mencionaba un tercer campo. Lo añadí porque es la única forma que
until encontré de implementar correctamente "no marques ganador a un emisor
que nunca jugó" (mandato explícito del propio brief) de forma que sobreviva
un save/load a mitad de partida: sin persistir "quién llegó a jugar", un
jugador legítimamente derrotado ANTES del save perdería su historial al
recargar, pudiendo producir un resultado distinto entre una corrida continua
y una corrida con save+resume (rompería G3/G4). Es un campo de 2 bytes,
serializado/checksummeado en el mismo punto que `game_over`/`winner` (mismo
bump v11/v6, no uno adicional).

**D4 — "referencia no resoluble" en el rechazo de `ai-profile`.** SPEC-005
§3 no tiene referencias cruzadas a otras secciones del catálogo (a
diferencia de building/tech), así que "rechazo por referencia" no aplica
literalmente. Interpreté el mandato del brief ("rechazo de un perfil con
referencia/rango inválido") como: (a) cualquier campo `_bp` fuera de
`0..10000` o `decision_period_ticks`/`reaction_latency_ticks` fuera de
`1..1000000` (rango, ver test `test_ai_profile_rejects_invalid_range`), y
(b) `tactical_behaviors` vacío — v1 usa el índice `[0]`, que en un array
vacío es una "referencia" (al primer elemento) que no resuelve a nada (ver
test `test_ai_profile_rejects_empty_tactical_behaviors`). Ambos casos
rechazan el catálogo entero con el código nuevo `InvalidAiProfile`.

**D5 — `diplomacy_openness_bp` se valida pero no se tipa.** El schema lo
exige (`required`); el loader lo LEE y valida su rango (rechazo si falta o
excede `0..10000`, mismo rigor que el resto de campos) pero no lo copia a
`AiProfileV1` — SPEC-005 §3/§10 excluye diplomacia explícitamente del struct
contratado y del alcance v1 (1v1). Igual con el resto de campos del schema
no listados en el struct literal (`personality`, `difficulty`,
`utility_curves`, `performance_lod`, `provenance`, `tactical_behaviors[1..]`,
y los demás campos de `difficulty_params` no contratados): se aceptan
estructuralmente sin tipar, mismo patrón que building/tech con sus campos no
tipados (`recipes`, `required_buildings`, etc.).

## Gates — salida real de esta sesión

- **Golden**: `casos=1074 fallos=0 [OK]` en backend `int128` (gcc) Y en
  backend `portable` (build con `-DCHUNSA_WIDE128_FORCE_PORTABLE=ON`) —
  no se tocó `fixed64.hpp`/`wide128.hpp`, resultado esperado.
- **G1** (`run --selftest-g1`): `alloc_delta=0 OK checksum=2defd6416796e3d8`
  — idéntico en gcc y portable (verificado, mismo checksum en ambos lanes).
- **G3** (`savetest`, sin `--ai`): `OK state=794d43a2dd8333a8
  cont=8b3a30f0b0eb11f6`.
- **G4** (`savetest --ai`): `OK state=18b88495356cc556
  cont=1d6e6f40638a2450`.
- **G5** (`record` + `verify`): `OK ai_executions=0 schedule_mismatches=0
  replay_v=3 checksum=18b88495356cc556` (coincide con G4, mismo seed/units).
- **ctest**: **17/17** (`props, golden, state, ring, flow_field, flow_move,
  combat, morale, economy, aggro, replay_v2, data_blob, buildings,
  replay_v3, production_tech, victory_ai_profile, data_compile`), verde en
  ambos lanes (gcc nativo y portable).
- **Build**: `-Wall -Wextra -Wshadow -Werror`, cero warnings, en ambos
  lanes.
- **Cero float/heap en la ruta nueva de `step`**: `grep` sobre el cuerpo de
  `victory_check` — cero coincidencias de `float`, `double`, `new`,
  `malloc`, `std::vector`, `std::string` (verificado, no solo por
  inspección).
- **Trayectoria golden previa bit-idéntica**: construí el binario ANTES de
  este cambio (`git stash` a `main` HEAD `400b921`, build aparte) y corrí
  `chunsa_sim_cli run --units 200 --ticks 2000 --checksum-every 20 --seed
  20260716` en ambos binarios. Resultado: `accepted=2200`, `rejected=0`,
  `fatal=NONE`, `checksums_seen=100` **idénticos** en ambos; el
  `final_checksum` difiere (`7ac7af6e05eec64c` viejo vs `c6b3c834dc81dd82`
  nuevo) — **esperado y documentado**, es el bump intencional de dominio
  v5→v6. `game_over` nunca se activa en este escenario (`player_count=1`,
  gate D1) — verificado también como test automatizado
  (`test_victory_single_configured_player_never_ends`, 100 ticks,
  `game_over==0` en todos).

## Checksums/valores nuevos

- `SAVE_FORMAT_VERSION`: 10 → **11**.
- `CHECKSUM_ALGO_VERSION`: 5 → **6**; dominio `"CHUNSA_STATE_V5"` →
  `"CHUNSA_STATE_V6"` (15 bytes, sin cambio de longitud).
- `AI_ALGO_VERSION`: sin cambio, **1** (fuera de alcance K1).
- Sin golden-checksum de estado persistido en el repo (mismo precedente que
  los 5 bumps anteriores, ver comentario en `checksum.hpp`): todos los tests
  de estado comparan corridas en vivo entre sí; "regenerar" el golden no
  tocó ningún archivo aparte del propio bump de versión/dominio.

## Tests obligatorios (§8.4, subconjunto K1) — mapeo a `test_victory_ai_profile.cpp`

- Perfil de IA: `test_ai_profile_real_golden` (carga `base:demo_normal` real
  del golden compilado, verifica los 9 campos tipados contra
  `data/ai_profiles/base_demo_normal.yaml`) + `test_ai_profile_rejects_invalid_range`
  + `test_ai_profile_rejects_empty_tactical_behaviors` (rechazo) +
  `test_ai_profile_accepts_valid_fixture` (control positivo).
- Victoria: `test_victory_single_winner` (a) · `test_victory_simultaneous_tie`
  (b, ambos edificios destruidos en el MISMO batch/step) ·
  `test_victory_in_progress_citizen_alone` (c, y de paso prueba que el
  ciudadano solo también cuenta) · `test_victory_frozen_after_game_over` (d,
  destruye también al "ganador" después y confirma que NO se reabre ni
  cambia a empate).
- Extra (D1/D2 arriba): `test_victory_never_populated_player_no_spurious_result`
  (jugador nunca poblado, mismo patrón que `test_production_tech.cpp`) y
  `test_victory_single_configured_player_never_ends` (escenario tipo
  `cli_run.hpp`, `player_count=1`, `SPAWN_DEBUG`, 100 ticks).
- Save/load: `test_victory_save_v11_roundtrip` (round-trip completo +
  congelado sobrevive el load).
- GameState siempre en heap (`std::make_unique`) en todo el archivo.

## Lo que NO se tocó (confirmado)

`ai_execute`, `driver.hpp`, `AI_ALGO_VERSION`, formato de replay v3 (sin
cambios de formato — los comandos de IA ya viajaban como cualquier comando).
Ningún merge a `main`.
