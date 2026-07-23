# RESULT — UI y render de edificios · Sprint 1.1

Fecha: 2026-07-23

Rama: `gpt/buildings-ui-1.1` desde `main` @ `9997d95`

Rol: desarrollador gráfico/Godot (Luna)

## Estado

Implementado el binding visual e interactivo de edificios sobre el kernel ya
integrado. No se modificó `addons/chunsa_sim/core/`. La simulación continúa
siendo propiedad del kernel: Godot solo lee snapshots y encola comandos.

## Entregado

- `DemoSnapshot` ahora expone por slot generación, `entity_kind`, `building_id`,
  progreso, ancla y `build_target`.
- Edificios renderizados como cajas 3D ortográficas con footprint del catálogo,
  color por owner y estado en construcción diferenciado por alpha/profundidad.
  Las unidades no vuelven a dibujar slots de edificios.
- Feedback del constructor: ciudadanos con `build_target` activo se muestran en
  naranja.
- `B` activa/desactiva construcción; `N` rota al siguiente edificio
  construible; `Esc` cancela. El ghost hace snap a tile y se muestra verde/rojo
  según validación local de límites, muro y solape.
- Clic izquierdo válido en el ghost encola `PLACE_BUILDING` con
  `p.unit_id = BuildingId` y `p.x_raw/y_raw` como tiles enteros.
- Ciudadanos propios seleccionables; clic derecho sobre una obra propia y
  seleccionada encola `ASSIGN_BUILD` con handles generacionales y tile entero.
  Las órdenes MOVE_TO existentes conservan handles generacionales.
- El arranque encola por comandos `PLACE_BUILDING` para
  `egipto:settlement_center` (owner 0) y `rome:forum_center` (owner 1) antes del
  primer `Step`; ambos aparecen completos desde tick 0.

## Verificación reproducible

Build del adaptador, serializado y térmico:

```text
nice -n 19 cmake --build /tmp/chunsa-godot-build -j2 --target chunsa_godot
```

Resultado: compilación limpia, `-Werror`, 0 warnings.

Build y tests del repo:

```text
nice -n 19 cmake --build build-gcc -j2
nice -n 19 ctest --test-dir build-gcc --output-on-failure
```

Resultado: `100% tests passed out of 14`, incluidos `buildings` y
`data_compile`.

Verificación headless obligatoria:

```text
nice -n 19 ./third_party_build/Godot_v4.7.1-stable_linux.x86_64 --headless --path demo --quit-after 1500
```

Resultado: exit code 0; catálogo cargado con `building_count=4`,
`settlement_id=0`, `forum_id=2`, `buildable_id=1`; diagnóstico del adaptador
con `buildings=2` en tick 0, 100 y 200; ninguna línea `CHUNSA ERROR`.

Se intentó además `CHUNSA_SHOT=/tmp/chunsa_buildings` para la captura visual.
El renderer dummy de Godot headless no produjo imagen (`imagen vacía`) y el
entorno no dispone de `xvfb-run`/`Xvfb`; por eso no se entrega PNG.

## Desviaciones y límites reportados

1. El adaptador usa `human_input_delay_ticks=0` en este escenario para que los
   comandos de setup con `target_tick=0` lleguen con `effective_tick=0` y
   cumplan la exención de escenario de SPEC-004 §4.1.2. Los centros siguen
   entrando exclusivamente como `PLACE_BUILDING`; no hay camino privilegiado.
2. La revisión del kernel ya registra una deuda no mecánica: replay v2 no
   serializa `CmdPayload::unit_id`, campo que también transporta `BuildingId`.
   Un replay real con `BuildingId != 0` puede divergir al recargarlo. No se
   tocó `replay.hpp` ni se inventó un replay v3; queda para contratación del
   Arquitecto en un sprint posterior.
3. La validación verde/roja del ghost es deliberadamente local (mapa fijo,
   muros, footprints del snapshot). Stock, ownership y toda validación de
   juego siguen siendo responsabilidad del kernel.
4. `demo/bin/libchunsa_godot.so` ya estaba modificado al iniciar el trabajo y
   `.claude/` ya era no rastreado; ambos se conservaron sin incorporarlos al
   cambio.

## Archivos del cambio

- `addons/chunsa_sim/gdextension/chunsa_sim_node.cpp`
- `addons/chunsa_sim/gdextension/chunsa_sim_node.h`
- `docs/briefs/GPT_BUILDINGS_UI_RESULT.md`
