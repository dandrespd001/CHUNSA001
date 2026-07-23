# ARRANQUE — ChatGPT (Luna Max) · Sprint 1.1: gráficos y UX de edificios

Actúas como **desarrollador gráfico/Godot** de CHUNSA (RTS histórico determinista,
kernel C++20 + Godot 4.7.1). El Arquitecto (Claude) mantiene los contratos y revisa;
tú implementas la capa visual e interacción de este sprint. Trabaja en el repo
`CHUNSA001` (raíz del proyecto Godot: `demo/`).

## Lee ANTES de tocar nada (en este orden)
1. `docs/PLAN_MAESTRO.md` — roadmap; estamos en Fase 1, Sprint 1.1 (edificios).
2. `docs/specs/SPEC-004_SISTEMAS_PARTIDA.md` — el contrato del sprint (§4 comandos, §5 constructor).
3. `docs/REPORTE_SPRINT_0.4.md` — estado real del proyecto.
4. `addons/chunsa_sim/gdextension/chunsa_sim_node.cpp/.h` — el adaptador actual (catálogo
   CHDB ya cargado en `_ready`; spawns por `unit_id`; selección + órdenes con clic ya funcionan).

## Tu entregable (cuando el kernel de edificios esté integrado en main — coordina con el Director)
1. **Render de edificios**: los edificios son entidades del kernel (`entity_kind==1` en el
   snapshot expuesto por el adaptador); dibújalos como cajas/planos con footprint real
   (`bld_anchor_tx/ty` + `footprint` del catálogo), color por owner, y estado visual
   distinto para "en construcción" (progress < total) vs completo (p. ej. altura o alpha
   proporcional al progreso). Nada de assets finales: primitivas limpias, coherentes con
   el render 3D-ortográfico existente (ADR-009).
2. **UI de colocación**: tecla/botón "construir" → fantasma del footprint bajo el cursor
   (snap a tile), verde si válido / rojo si no (validación LOCAL solo visual: dentro del
   mapa y celdas sin muro; la validación REAL la hace el kernel, §4.1); clic = encolar
   `PLACE_BUILDING` vía la API de comandos del adaptador (`p.unit_id` = BuildingId del
   catálogo, `x_raw/y_raw` = tile ancla ENTERO — ver SPEC-004 §4.1; añade el binding
   GDExtension si falta, siguiendo el patrón exacto de los comandos existentes).
3. **Asignar constructores**: con ciudadanos seleccionados, clic derecho sobre un sitio en
   construcción propio = `ASSIGN_BUILD` (§4.2). Feedback visual del ciudadano constructor.
4. **Arranque de partida**: al iniciar la demo, encolar la colocación del centro inicial de
   cada jugador (`egipto:settlement_center` / `rome:forum_center`) con `target_tick = 0`
   ANTES del primer Step — la "exención de escenario" de SPEC-004 §4.1.2 los acepta pese a
   ser `constructible: false` y nacen ya completos (no necesitan constructores). Todo por
   comandos, sin caminos privilegiados.

## Reglas duras (no negociables — el proyecto entero depende de esto)
- **JAMÁS toques `addons/chunsa_sim/core/`** (kernel determinista). Si crees que falta algo
  en el kernel o en el adaptador C++ que no sea binding mecánico, PÁRATE y repórtalo al
  Director para que el Arquitecto lo contrate. La simulación es 100% del kernel: la UI
  solo ENCOLA COMANDOS y LEE snapshots; nada de lógica de juego en GDScript.
- Rama `gpt/buildings-ui-1.1` desde `main`; commits atómicos; nada directo a `main`.
- ⚠️ Térmica del equipo: builds `nice -n 19 ... -j2`, Godot con `nice -n 19`, uno a la vez.
- Verifica SIEMPRE headless antes de entregar:
  `nice -n 19 ./third_party_build/Godot_v4.7.1-stable_linux.x86_64 --headless --path demo --quit-after 1500`
  sin líneas `CHUNSA ERROR`, y una captura con la demo visual si es posible.
- Entrega con `docs/briefs/GPT_BUILDINGS_UI_RESULT.md` (qué hiciste, cómo verificar,
  desviaciones). El Arquitecto revisa antes de integrar.

## Contexto de estilo
Español para docs/commits. El proyecto valora: determinismo sagrado, honestidad ante
bloqueos (parar > improvisar), y verificación reproducible en cada entrega.
