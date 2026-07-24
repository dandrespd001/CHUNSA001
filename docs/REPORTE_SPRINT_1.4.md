# REPORTE — Sprint 1.4: IA oponente v1 + partida concluyente

- Fecha de cierre técnico: 2026-07-24
- Kernel integrado en `main`: `afbe66f`
- Godot: rama `gpt/skirmish-jugable-1.4`, commits `e03da59` y `b9b8afa`

## Objetivo y veredicto
Cerrar el bucle de partida con una IA determinista de tres capas, condición de
victoria/derrota, aldeanos vulnerables y un skirmish humano contra IA en Godot.

**Gate técnico superado.** Existe una partida concluyente, ganable/perdible, reproducible y
con economía real antes del límite de 30 minutos. Esto **no** acredita un playtest humano de
30+ minutos: la demo Godot actual termina sin input humano en el tick 2554, aproximadamente
2 min 08 s a 20 Hz. La duración y el pacing quedan como deuda de producto.

## Entregables consolidados

### Kernel
- K1 (`ba0a2f8`): `game_over`/`winner`/`participants_mask`, perfil `AiProfileV1`, save v11 y
  checksum v6.
- K2 (`39bdb66`): IA estratégica, reactiva y táctica que emite Commands por el mailbox
  determinista; `AI_ALGO_VERSION=2`; skirmish militar concluido en el tick 1236.
- K3 (merge final `afbe66f`): aldeanos enemigos como objetivos vulnerables, sin convertirlos
  en atacantes; escenario `skirmish_eco` con economía real, concluido por conquista en el tick
  1827 tras recolectar 300 de recurso B.

La auditoría y revisión independiente no encontraron P0 de determinismo. El skirmish previo
permaneció bit-idéntico y dos corridas de `skirmish_eco` produjeron el mismo ganador, tick y
checksum.

### Godot
- El adaptador bombea `AiJobBox` para owner 1 antes de cada `step`, siguiendo el ciclo del
  driver del kernel.
- Owner 0 y owner 1 reciben centro, cuartel, ocho unidades militares y cuatro aldeanos; owner
  0 queda controlable mediante el HUD existente.
- `DemoSnapshot` expone `game_over` y `winner`.
- La presentación muestra `VICTORIA`, `DERROTA` o `EMPATE`, registra
  `CHUNSA game_over winner=N tick=T` y detiene la simulación al finalizar.
- No se modificó `addons/chunsa_sim/core/` desde la pieza Godot.

## Verificación trazable
- Golden: **1074/1074**.
- Suite integrada: **20/20** tests.
- G1: `alloc_delta=0`; G4 con IA real bit-exacto entre lanes.
- Save a mitad + continuación: mismo resultado que la corrida continua.
- Replay feed-mode: bit-exacto, sin re-ejecutar la IA y sin desajustes de agenda.
- `skirmish_eco`: ganador 1, tick 1827, checksum de estado
  `8ea5bc60ca1a48cd` en dos corridas.
- Build del adaptador Godot: objetivo `chunsa_godot` completo, sin warnings reportados.
- Headless corto (`--quit-after 4000`): exit 0, catálogo cargado, `ai_seq` creciente y sin
  `CHUNSA ERROR`.
- Headless extendido (`--quit-after 20000`): exit 0,
  `CHUNSA game_over winner=1 tick=2554`.

Fuentes: `docs/REVISION_SPRINT_1.4_KERNEL.md`,
`docs/briefs/SONNET_K1_VICTORIA_PERFIL_RESULT.md`,
`docs/briefs/SONNET_K2_IA_EXECUTE_RESULT.md`,
`docs/briefs/SONNET_K3_ALDEANOS_VULNERABLES_RESULT.md` y
`docs/briefs/GPT_SKIRMISH_JUGABLE_1.4_RESULT.md`.

## Desviaciones y lectura honesta del gate
1. El PLAN_MAESTRO decía “partida de 30+ min”; SPEC-005 usa 36.000 ticks como límite máximo
   para exigir que la partida concluya. La evidencia disponible valida **conclusión antes de
   30 min**, no duración mínima ni playtest humano prolongado.
2. La demo parte con ejército y edificios militares ya disponibles; prioriza demostrar IA,
   combate y victoria sobre un arco económico largo.
3. El ciudadano de owner 1 usa provisionalmente `egipto:work_crew`; todavía no existe una
   unidad ciudadana romana propia en el catálogo del slice.
4. La experiencia Godot está entregada en `gpt/skirmish-jugable-1.4`; su integración a `main`
   y el cierre administrativo deben conservar los commits citados.

## Deuda y siguiente incremento
- **Pacing**: diseñar y validar más adelante una partida con apertura económica y duración
  objetivo mediante playtests; no alterar el gate técnico ya reproducible.
- **Fog de presentación**: el mundo y minimapa aún muestran toda entidad. Sprint 1.5A debe
  exponer `visible`/`explored` de owner 0 por snapshot, ocultar enemigos fuera de visión y
  mantener intactos kernel, IA, save, replay y checksum, según SPEC-006 Parte II.
- Se mantienen las deudas del kernel ya registradas: balance RPS contra aldeanos, efectos de
  stats por tecnología, época/civ por entidad y precisión de `pop_used`.
