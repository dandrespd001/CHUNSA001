# Revisión del kernel de edificios — Sprint 1.1 (implementó: Sonnet · revisó: Arquitecto + Opus)

Fecha: 2026-07-23 · Rama revisada: `sonnet/buildings-1.1` (4 commits + RESULT) → integrada vía `arch/sprint-1.1-integration`

## Veredicto
**ACEPTADO CON 1 ENDURECIMIENTO.** Implementación fiel de SPEC-004 Parte I con 9
desviaciones documentadas, todas razonables (destacan D3 —no-solape como corolario del
FF_WALL, correcto— y D5 —locomoción propia del constructor para no tocar `movement_v1`
CONGELADA, comportamiento observable idéntico—). La calidad del RESULT y la disciplina
de gates (dump de trayectoria pre/post bit-idéntico, no solo checksums) fueron ejemplares.

## Endurecimiento del Arquitecto (en revisión)
La exención de escenario omitía el **chequeo** de stock pero ejecutaba igualmente la
**deducción**: pre-colocar en tick 0 un edificio con coste habría dejado el stock en
negativo. Corregido (deducción condicionada a `!scenario_exempt`) + test `2a-bis`; el
camino feliz del test 1a pasó a `eff=1` para ejercitar el camino normal con deducción.

## Auditoría del parsing CHDB de edificios (Opus)
**SIN P0.** El parsing opera solo sobre el árbol CVE ya acotado (sin offsets crudos
nuevos); patrón memoria/fallo idéntico al de unidades endurecido en 0.4 (unique_ptr +
release solo en éxito, reserve exacto, búsqueda binaria coherente con el orden bytewise).
**200k blobs adversariales limpios bajo ASan/UBSan.** 3 P2 documentados y aceptados:
sin gate de claves desconocidas (delegado al compilador, coherente con tech/civ/map),
correlación `build_time==0 ⇔ constructible:false` diferida al compilador, duplicados
inocuos en `dropoff_resources` (OR idempotente).

## Verificación independiente del Arquitecto (rama de integración)
- Golden **1074/1074** · **G1** alloc_delta=0 `1b2dc94a0dd12439` · **G3** `2a457d7dcf1d48d8` ·
  **G4** `795c204be40a4e28` · **G5** schedule_mismatches=0 — checksums idénticos a los del
  RESULT de Sonnet (dominio v3 intencional).
- **ctest 14/14** (con el test endurecido) · build `-Werror` limpio · adaptador Godot
  compila 0 warnings y demo headless intacta (spawns data-driven, sin `CHUNSA ERROR`).

## Deuda registrada (prioridad para sprints próximos)
1. **Replay v3 (ALTA, sube de prioridad por D8)**: el replay v2 no serializa
   `CmdPayload::unit_id`, campo que ahora también transporta el `BuildingId` de
   PLACE_BUILDING — un replay real con edificios de id != 0 divergiría. Candidata a
   pieza temprana del Sprint 1.2 junto con el envelope save v8→literal.
2. Migración de saves entre versiones (v7 no carga en v8; precedente aceptado del 0.4).
3. Restauración del cost_grid a valor fijo 1 al destruir (pierde costes de terreno
   variables futuros; hoy el mapa base es uniforme — refinar cuando exista terreno real).
