# BRIEF K1 — Replay v3 + save v9 + ventana de setup (Sonnet · Sprint 1.2, pieza 1)

Implementa **SPEC-004 §10 completo** (`docs/specs/SPEC-004_SISTEMAS_PARTIDA.md`, Parte II).
Pieza acotada pero DELICADA: tocas `replay.hpp`, `serialize.hpp` (agenda) y
`command_effective_tick` — código compartido sensible a la trayectoria golden. Lee §10
entero y el §13 (tests) antes de tocar nada.

## Rama y alcance
- Rama `sonnet/k1-replay-v3` desde `main` (HEAD). Jamás toques `main`.
- Archivos esperados: `replay.hpp` (v3, §10.1), `serialize.hpp` (agenda + unit_id, §10.2),
  `save_io.hpp` (v9), `step.hpp` (solo `command_effective_tick`, §10.3), driver/cli si el
  contrato de "no ingerir input pre-Step" exige documentarlo ahí (comentario, no lógica),
  `gdextension/chunsa_sim_node.cpp` (UNA línea: `human_input_delay_ticks` de 0 a 1 en la
  MatchConfig01A de la demo — §10.3 la devuelve a producción; el resto del adaptador NO
  se toca), tests.

## Puntos de contrato no negociables
1. Replay v3: registro = layout v2 + `u32 unit_id` AL FINAL. Loader v1/v2/v3;
   `legacy_payload_loss` según §10.1. Grabación siempre v3.
2. Save v9: agenda gana `u32 unit_id` tras `unit_class`. Sin migración v8→v9 (precedente
   D7). VERIFICA si el dominio del checksum ya cubre `pending.items[].p.unit_id` — si no
   lo cubre, inclúyelo (bump `CHUNSA_STATE_V4` + regen por el procedimiento del v3; si sí
   lo cubre, NO bumpees y dilo en el RESULT).
3. Setup window: `command_effective_tick(target, t, delay)` gana el caso
   `target == 0 && t == 0 → 0`. TODO lo demás idéntico. Documenta en el header el
   contrato del host (no ingerir input de jugador antes del primer Step).
4. La demo vuelve a delay=1 y DEBE seguir funcionando: los centros del showcase entran
   con target_tick=0 en el primer Step → eff=0 → exención (verifícalo con la corrida
   headless: `buildings=2`, sin CHUNSA ERROR).

## Tests obligatorios (§13, subconjunto K1)
- Replay v3 round-trip con PLACE_BUILDING de BuildingId != 0: DEBE reproducir bit-exacto
  (y demuestra en el test que el formato v2 lo perdía: carga un stream v2 sintético del
  mismo escenario y comprueba `legacy_payload_loss == 1`).
- Save v9 con un SPAWN_UNIT de unit_id != 0 PENDIENTE en la agenda: load + continuar ==
  corrida continua.
- `command_effective_tick`: (0,0,1)→0 · (0,1,1)→2 · (5,0,1)→5 · casos existentes intactos.
- Suite completa + golden + G1/G3/G4/G5 + trayectoria pre/post de un escenario sin
  edificios (dump, no solo checksum).

## Reglas duras
- Append-only en formatos; térmica `nice -n 19 -j2`, un build a la vez; cero float/heap
  en Step; si el SPEC tiene un hueco, conservador + desviación en el RESULT.

## Entrega
Commits atómicos + `docs/briefs/SONNET_K1_REPLAY_V3_RESULT.md` (desviaciones numeradas,
salida de gates, checksums, veredicto sobre el punto 2). NO merges; el Arquitecto integra.
