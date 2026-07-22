# RESULT — Selección y órdenes del jugador con clic (Kimi K3 + Arquitecto)

**Nota de proceso**: Kimi implementó el código del contrato completo, pero agotó
su cuota mientras intentaba automatizar la prueba de ratón del paso 2 (llegó a
diseñar un inyector de eventos `uinput` — sobreingeniería: el brief pedía prueba
manual). No llegó a escribir este RESULT ni a comitear. El Arquitecto retomó:
revisó el diff línea a línea, verificó builds/gates, y la prueba manual la hizo
el Director en vivo.

## Qué hizo Kimi (contrato cumplido al 100% en código)

- `chunsa_sim_node.h`: `_input` override + estado de selección (`is_selected`,
  `dragging`, `drag_start`) + cola `pending_player_commands` protegida por
  `input_mutex` + `next_player_sequence` desde 1000000 (sin colisión con las
  secuencias del showcase).
- `_input`: clic izquierdo (radio 20 px, la más cercana) o arrastre (rectángulo
  en pantalla) selecciona SOLO caballería propia (owner 0, unit_class 1);
  clic derecho convierte pantalla→mundo (`project_ray_origin` — cámara
  ortográfica sin rotación: `world_px=origin.x`, `world_py=-origin.y`) y encola
  un `MOVE_TO` por unidad seleccionada viva.
- `sim_loop`: drena la cola bajo el mutex tras `build_showcase_batch`, mismo
  batch (+512 de margen).
- `render_interpolated`: seleccionado = verde brillante, prioridad máxima sobre
  ciudadano/pánico/bando.

Sin desviaciones del contrato.

## Verificación

1. Build adaptador (`nice -19 -j2`): limpio. Kernel: `addons/chunsa_sim/core/`
   sin diff; golden 1074/1074; ctest 9/9 (verificado por el Arquitecto).
2. Prueba manual (Director, ventana 1280×800): selección por clic/arrastre pinta
   la caballería de verde; clic derecho la mueve al punto indicado "similar a
   Age of Empires". Confirmado en vivo.

## Veredicto del Arquitecto

**ACEPTADO sin cambios de código.** Observación del Director derivada de la
prueba: el combate se estanca (los supervivientes no se rematan) — no es un bug
del input sino la ausencia de persecución/aggro en `combat_system` (las
unidades solo disparan dentro de `range_mt` y nadie se re-acerca). Se aborda
como siguiente tarea de kernel.
