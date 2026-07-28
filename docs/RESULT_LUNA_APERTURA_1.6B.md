# Resultado — Adaptador Godot de la apertura económica (Sprint 1.6B)

## Alcance

Trabajo realizado en la rama `gpt/apertura-economica-1.6b`. Solo se modificaron:

- `addons/chunsa_sim/gdextension/chunsa_sim_node.h`
- `addons/chunsa_sim/gdextension/chunsa_sim_node.cpp`

`addons/chunsa_sim/core/` no fue modificado.

## Implementación

- `DemoSnapshot` ahora transporta `n_deposits`, la posición/stock/recurso de
  cada depósito y el estado económico por slot (`eco_carry`, recurso de carga,
  `eco_state` y depósito asignado). `sim_loop()` copia esos datos junto al
  resto del snapshot publicado.
- El clic derecho conserva la precedencia de `enqueue_build_assignments()`.
  Después resuelve el depósito vivo más cercano dentro de
  `GATHER_PICK_RADIUS_RAW`, con empate por índice menor, y emite `GATHER` con
  el handle actual y las coordenadas raw exactas del depósito. Si no hay un
  aldeano seleccionado, no se emite `GATHER` y el flujo continúa al movimiento
  militar.
- La rama de `MOVE_TO` sigue filtrando `unit_class <= 2`; los ciudadanos no se
  agregan a ese comando. Su control sigue siendo construcción y `GATHER`, de
  acuerdo con la restricción 0.2.
- El HUD muestra `A/B/Me`, aldeanos imputados por depósito o carga, ciudadanos
  construyendo y ociosos. El panel de un aldeano muestra `SEEK`, `HARVEST` o
  `RETURN`, depósito asignado y carga `carry/50` con `A/B/Me` como nombres de
  recurso (el catálogo no contiene nombres de recursos v1).
- Los depósitos vivos se dibujan en el mundo con formas y colores distintos por
  recurso y texto de `remaining`. También aparecen como puntos en el minimapa;
  ambos caminos aplican `presentation_tile_visible()` y respetan la niebla.

## Verificación

Todos los comandos terminaron con código 0:

- `cmake --build build-godot --clean-first -j2` — build limpio del `.so`, sin
  warnings ni errores.
- `cmake --build build-gcc -j2` — correcto.
- `ctest --test-dir build-gcc` — **25/25**.
- `./build-gcc/chunsa_sim_cli run --selftest-g1` —
  `G1 selftest: alloc_delta=0 OK checksum=fefa48125dd35736`.
- `./build-gcc/chunsa_sim_cli savetest --ai` —
  `state=774316057e5667fb cont=d52ac0019700684f`.
- Godot headless con `--log-file` — cargó la extensión, el catálogo y arrancó
  el escenario: `units=600`, `citizens=8`, `buildings=4`, `tick=0`.

## Limitación de verificación visual

El entorno disponible no tiene una sesión gráfica utilizable para interacción
manual. El arranque headless confirma carga y ejecución, pero el renderer dummy
devuelve una imagen vacía, por lo que no fue posible realizar aquí la prueba
visual de seleccionar con ratón y observar el ciclo completo de recolección.
La ruta de input y presentación quedó compilada y el kernel de `GATHER` conserva
su suite completa en verde; no hay desviaciones funcionales deliberadas del
contrato.

No se hizo merge a `main`.
