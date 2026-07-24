# BRIEF K1 — Condición de victoria + perfil de IA tipado (Sonnet · Sprint 1.4, pieza 1)

Implementa **SPEC-005 §3, §6 y §7** (`docs/specs/SPEC-005_IA_OPONENTE.md`). Es la
infraestructura sobre la que K2 construirá la IA: la condición de victoria en el kernel, el
perfil de IA tipado en el loader, y los bumps de save/checksum. Pieza acotada y bien definida.
NO toques `ai_execute` todavía (eso es K2). Lee §3, §6, §7 y §8.4 antes de empezar.

## Rama y alcance
- Rama `sonnet/k1-victoria-perfil` desde `main` (HEAD). Jamás toques `main`.
- Archivos esperados: `data_catalog.hpp` (§3: `AiProfileV1`, `catalog_find_ai_profile`,
  parsing tipado de `kind=ai-profile` con el patrón endurecido de tech/unit/building),
  `game_state.hpp` (§6: `game_over`/`winner` + init en `gs_init`/`zero_components` — nota:
  son escalares del estado, no por-slot), `step.hpp` (§6: evaluación de victoria al final de
  `step`, tras el destroy batch, barrido ascendente), `checksum.hpp`/`serialize.hpp`/
  `save_io.hpp` (§7: save v11, dominio `CHUNSA_STATE_V6`), tests.

## Puntos de contrato no negociables
1. **Victoria (§6)**: `game_over`/`winner` en `GameState`; regla v1 = "derrotado = 0 edificios
   vivos propios Y 0 ciudadanos vivos propios"; un solo no-derrotado ⇒ gana; todos derrotados
   el mismo tick ⇒ `winner=0xFF`; una vez `game_over==1`, congela (no re-evalúa). Determinista:
   sin RNG/float, conteo por índice ascendente. Considera SOLO jugadores que tuvieron entidades
   iniciales (no marques "ganador" a un emisor que nunca jugó — define el conjunto de jugadores
   activos de forma determinista, p.ej. los que tienen `player_count` de la config o los que
   alguna vez tuvieron una entidad; documenta la elección).
2. **Perfil tipado (§3)**: `AiProfileV1` con los pesos `_bp` y `difficulty_params`, parseado
   como espejo EXACTO del patrón de `TechDefinitionV1` (unique_ptr, reserve, rechazo del
   catálogo entero, `catalog_find_ai_profile` bytewise). El blob NO cambia de formato.
3. **Versiones (§7)**: `SAVE_FORMAT_VERSION` 10→11, `CHECKSUM_ALGO_VERSION` 5→6
   (`CHUNSA_STATE_V6`), `game_over`/`winner` al final del stream y del dominio. Sin migración
   (precedente D7). Regen golden por el procedimiento del bump anterior; dump pre/post
   bit-idéntico de un escenario que NO alcance game_over (la trayectoria no cambia).

## Tests obligatorios (§8.4, subconjunto K1)
- Perfil de IA: carga del `base:demo_normal` real (pesos correctos) + rechazo de un perfil
  con referencia/rango inválido (fixture).
- Victoria: (a) jugador owner 1 se queda sin edificios ni ciudadanos → `game_over==1`,
  `winner==0`; (b) empate simultáneo → `winner==0xFF`; (c) partida en curso → `game_over==0`;
  (d) tras game_over, un cambio de estado NO reabre la partida (congelado).
- `game_over`/`winner` sobreviven save/load v11 (round-trip).
- GameState SIEMPRE en heap en los tests (make_unique — lección de sprints previos: en pila
  segfaultea bajo ctest).

## Reglas duras
Append-only en formatos; cero float/heap en `step`; iteración ascendente; térmica
`nice -n 19 -j2` un build a la vez; conservador ante huecos del SPEC + desviación numerada en
el RESULT. NO toques `ai_execute` ni `driver.hpp` (la IA es K2). La demo headless debe seguir
verde. NO merges a main.

## Entrega
Commits atómicos + `docs/briefs/SONNET_K1_VICTORIA_PERFIL_RESULT.md` (desviaciones numeradas,
gates golden/G1/G3/G4/G5/ctest, checksums nuevos, y tu definición exacta de "jugador activo"
para la condición de victoria). El Arquitecto revisa (+ Opus audita el parsing) e integra.
