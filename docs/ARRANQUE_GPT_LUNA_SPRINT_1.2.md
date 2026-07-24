# ARRANQUE — ChatGPT (Luna Max) · Sprint 1.2: UI de producción, tecnología y época

Actúas como **desarrollador gráfico/Godot** de CHUNSA (RTS histórico determinista,
kernel C++20 + Godot 4.7.1). El Arquitecto (Claude) mantiene los contratos y revisa; tú
implementas la capa visual e interacción de este sprint. El kernel de producción/tech/
época YA está integrado en `main` (merge `9b59965`).

## Lee ANTES de tocar nada (en este orden)
1. `docs/PLAN_MAESTRO.md` — estamos en Fase 1, Sprint 1.2.
2. `docs/specs/SPEC-004_SISTEMAS_PARTIDA.md` — Parte II, §11 (producción/rally/pop) y §12
   (tech/época/gating). Es el contrato de lo que vas a exponer en pantalla.
3. `docs/REVISION_SPRINT_1.2_KERNEL.md` y `docs/briefs/SONNET_K2_PRODUCCION_TECH_RESULT.md`
   — estado real, las 12 desviaciones (lee la 6, 7 y 9: te afectan).
4. `addons/chunsa_sim/gdextension/chunsa_sim_node.cpp/.h` — el adaptador actual. Ya tiene
   selección + órdenes + colocación de edificios del Sprint 1.1 (patrón `enqueue_*` +
   `pending_player_commands` bajo `input_mutex`, drenado en `sim_loop`). **COPIA ese
   patrón**; no inventes uno nuevo.

## Rama y reglas duras (no negociables)
- Rama `gpt/produccion-ui-1.2` desde `main`; commits atómicos; nada directo a `main`.
- **JAMÁS toques `addons/chunsa_sim/core/`** (kernel determinista). La UI solo ENCOLA
  COMANDOS y LEE snapshots; cero lógica de juego en el adaptador/GDScript. Si crees que
  falta algo en el kernel que no sea binding mecánico, PÁRATE y repórtalo en el RESULT.
- ⚠️ Térmica: builds `nice -n 19 ... -j2`, Godot `nice -n 19`, uno a la vez.
- Verifica SIEMPRE headless antes de entregar (comando abajo), sin líneas `CHUNSA ERROR`.
- Entrega con `docs/briefs/GPT_PRODUCCION_UI_1.2_RESULT.md`. El Arquitecto revisa e integra.

## Los 4 comandos nuevos — payload LITERAL (del kernel, ya disponibles)
`enum CommandType`: `TRAIN_UNIT=9`, `SET_RALLY=10`, `RESEARCH_TECH=11`, `EPOCH_UP=12`.
El `CmdPayload` NO cambió de layout; reinterpreta campos por tipo. Emítelos con el mismo
molde que `enqueue_place_building` (memset 0, `target_tick=0`, `emitter=0`,
`sequence=next_player_sequence++`, push bajo `input_mutex`):
- **TRAIN_UNIT**: `c.p.handle = {índice_edificio, generation}` (edificio propio COMPLETO);
  `c.p.unit_id = UnitId` a entrenar (debe estar en `building.trains[]` del catálogo).
- **SET_RALLY**: `c.p.handle = {edificio, gen}`; `c.p.x_raw/y_raw` = punto **raw** (tile*65536
  + 32768), dentro del mundo. (Ojo: raw Q47.16, NO tile entero — distinto de PLACE_BUILDING.)
- **RESEARCH_TECH**: `c.p.handle = {edificio completo, gen}`; `c.p.unit_id = TechId` (debe
  estar en `building.researches[]`).
- **EPOCH_UP**: comando de JUGADOR, no de entidad → **TODOS los campos del payload en 0**
  (incluido `handle`); solo `type`/`emitter`/`sequence`. Si metes basura en el payload el
  kernel lo rechaza con MALFORMED.
Resolución de ids: usa las tablas tipadas del catálogo — `building.trains[k]`/`train_count`,
`building.researches[k]`/`research_count` (ya cargadas en `catalog_storage.catalog()`).
El kernel valida TODO (owner, época, stock, cola llena, pop, prereqs…) y responde por el
mailbox de receipts; tu validación local es solo para feedback visual, nunca autoritativa.

## Entregable
1. **Extiende `DemoSnapshot`** (en el .h) y su copia en `sim_loop` (bajo el ring) con lo
   que la UI necesita leer, por-slot: `prod_count[i]`, `prod_queue[i][0]` (ítem en cabeza),
   `prod_progress[i]`, `research_tech[i]`, `research_progress[i]`; y por-jugador (copia
   `gs->player_stock[0]`, `gs->player_epoch[0]`, `gs->pop_used[0]` a campos escalares del
   snapshot). Mantén el layout POR-SLOT existente.
2. **HUD mínimo** (mismo rig, sin assets finales): recursos A/B/Me del jugador 0, época
   actual, población `pop_used/200`. Legible, esquina de pantalla.
3. **Producción**: con un edificio militar propio seleccionado, teclas/botones para
   `TRAIN_UNIT` de cada unidad de su `trains[]`; muestra la cola (cabeza + progreso) sobre
   el edificio. `SET_RALLY` con una tecla + clic (la unidad entrenada camina al rally).
4. **Investigación**: con el edificio adecuado seleccionado, `RESEARCH_TECH` de las techs
   de su `researches[]`; indicador de progreso; marca la tech como investigada al terminar.
5. **Época**: un botón/tecla `EPOCH_UP` con feedback del resultado (aceptado / por qué no,
   leyendo el receipt del mailbox si puedes, o el cambio de `player_epoch` en el snapshot).
6. **Escenario demostrable**: los cuarteles NO están en el showcase inicial (solo los
   centros). Para que TRAIN sea probable sin fricción, PRE-COLOCA en `build_showcase_batch`
   (t==0, exención de escenario) un cuartel completo por jugador — `egipto:chariotry_stable`
   (owner 0) y `rome:castra_barracks` (owner 1) — igual que ya se pre-colocan los centros.
   Así el jugador puede seleccionar el cuartel y entrenar de inmediato.

## Notas de las desviaciones que te afectan
- **Desv. 9 (ya resuelta por el Arquitecto)**: `player_epoch` se fija a 3 en `_ready` vía
  `gs_init_epoch_from_catalog`. No la toques; solo léela.
- **Desv. 6 (ya resuelta)**: `researches` está poblado (cuartel→tech militar,
  centro→tech economía). RESEARCH_TECH funciona con datos reales.
- **Desv. 7**: `pop_used` es aproximado (toda muerte lo baja). No te preocupes, solo
  muéstralo tal cual lo da el snapshot.

## Verificación headless obligatoria
```
nice -n 19 ./third_party_build/Godot_v4.7.1-stable_linux.x86_64 --headless --path demo --quit-after 1500
```
Debe mostrar los edificios (incluidos los 2 cuarteles nuevos → `buildings=4`), sin
`CHUNSA ERROR`, exit 0. Añade un print de diagnóstico si te ayuda (p.ej. cuando una unidad
sale de producción). Una captura visual si es posible.

Español para docs/commits. Valores del proyecto: determinismo sagrado, parar > improvisar,
verificación reproducible.
