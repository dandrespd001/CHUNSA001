# ARRANQUE — ChatGPT (Luna Max) · Sprint 1.3: UI/HUD v1

Actúas como **desarrollador gráfico/Godot** de CHUNSA (RTS histórico determinista, kernel
C++20 + Godot 4.7.1). El Arquitecto (Claude) mantiene el contrato y revisa; tú implementas
TODA la capa de interfaz de este sprint (es frontend Godot puro — no hay trabajo de kernel).

## Lee ANTES de tocar nada (en este orden)
1. `docs/specs/SPEC-006_UI_HUD.md` — el CONTRATO de este sprint. §0 (principio inviolable),
   §2 cámara, §3 minimapa, §4 panel de selección, §5 **resolución del conflicto de teclas**
   (numeración = grupos de control; producción pasa a botones/letras), §7 exposición nueva,
   §8 gates. Síguelo al pie de la letra; §5 es una decisión de diseño ya tomada, no la
   re-litigues.
2. `docs/REPORTE_SPRINT_1.2.md` — qué existe ya.
3. `addons/chunsa_sim/gdextension/chunsa_sim_node.cpp/.h` — TU archivo. Lo que ya hay:
   render 3D ortográfico + interpolación 60fps, selección por arrastre (`is_selected[]`,
   `snap_curr`), MOVE_TO (clic derecho), colocación de edificios (ghost, `B`/`N`), producción/
   tech/época/rally (hoy en teclas `1..8`/`T`/`R`/`E`), HUD `_draw` (recursos/época/población/
   feedback de receipts), cámara ortográfica FIJA. Reutiliza toda esa maquinaria.

## Rama y reglas duras (no negociables)
- Rama `gpt/ui-hud-1.3` desde `main`; commits atómicos; nada directo a `main`.
- **JAMÁS toques `addons/chunsa_sim/core/`**. La UI solo LEE el snapshot y ENCOLA comandos.
  Si crees que necesitas algo del kernel que no sea binding mecánico en el adaptador, PÁRATE
  y repórtalo en el RESULT.
- ⚠️ Térmica: builds `nice -n 19 ... -j2`, Godot `nice -n 19`, uno a la vez.
- Verifica SIEMPRE headless antes de entregar (comando abajo), exit 0, sin `CHUNSA ERROR`,
  `buildings=4`. Una captura visual si puedes.
- Entrega con `docs/briefs/GPT_UI_HUD_1.3_RESULT.md`. El Arquitecto revisa e integra.

## Trabajo concreto (mapea a las § del SPEC)
1. **Cámara movible (§2)**: pan (WASD/flechas + arrastre botón central) con clamp al mapa
   (256 tiles × 4 px/tile = 1024 px de lado); zoom con rueda sobre `cam3d->set_size(...)`,
   límites ~300..1200, anclado al cursor. La cámara es `PROJECTION_ORTHOGONAL`, mirando -Z,
   con el mapeo mundo `(px, -py, py)` ya usado en el render y en `screen_to_tile`. NO rotar.
2. **Minimapa v1 (§3)**: un `_draw` de un rectángulo (esquina) a escala del mapa; muros del
   `cost_grid` (patrón fijo: muro en x=128, y∈[32,224) con hueco y∈[124,132) — ya conocido en
   `is_static_wall`), entidades del snapshot como puntos por owner, rectángulo del viewport, y
   clic→recentrar cámara. SIN fog en v1 (documenta que llega con la visión más adelante).
3. **Panel de selección (§4)**: al haber selección, panel con tipo/owner/vida; barra de vida
   sobre las entidades seleccionadas. Requiere añadir `hp[i]`/`max_hp[i]` al `DemoSnapshot` y
   copiarlos en `sim_loop` (§7) — es lo único "nuevo" que expones, y es del adaptador, no del
   kernel (`gs->hp[i]`/`gs->max_hp[i]` ya existen).
4. **Grupos de control (§5)**: `Ctrl+N` asigna `is_selected[]` actual al grupo N (guarda
   índices de slot); `N` recupera (re-selecciona los slots vivos del grupo, verificando
   `alive` + `generation` del snapshot para no seleccionar un slot reciclado); doble-`N`
   centra cámara. Son estado de presentación, arrays en el nodo.
5. **Producción/tech a botones (§5)**: MUEVE TRAIN/RESEARCH de las teclas `1..8` a botones
   clicables del panel contextual (un botón por `trains[k]`/`researches[k]` del edificio
   seleccionado, resueltos del catálogo como ya haces). Puedes conservar hotkeys de LETRA
   (Q/W/E… no numéricas). `EPOCH_UP` a un botón dedicado (o `E`). `SET_RALLY` sigue `R`+clic.
   El resultado se lee del mailbox de receipts (ya lo tienes). LIBERA los números para §5.4.
6. **Marcadores de orden (§6)**: marcador breve en el destino de MOVE_TO/rally; el rally fijado
   se dibuja persistente (`rally_set`/`rally_x/y` ya en el snapshot).

## Detalles del adaptador que ya existen (úsalos, no reinventes)
- `screen_to_tile(screen, tx, ty)` convierte pantalla→tile. `cam3d->unproject_position(...)`
  y `project_ray_origin(...)` para el mapeo inverso.
- `DemoSnapshot` (en el .h) es POR-SLOT; añade tus campos ahí y cópialos en `sim_loop` bajo
  `ring->begin_write()`. Ya trae x/y/owner/unit_class/entity_kind/building_id/build_progress/
  prod_*/research_*/generation/stock/época/pop/receipt.
- Toda emisión de comando: `enqueue_*` con `next_player_sequence++`, push bajo `input_mutex`.
- El HUD ya usa `_draw` + `queue_redraw()`; extiéndelo, no lo dupliques.

## Verificación headless obligatoria
```
nice -n 19 ./third_party_build/Godot_v4.7.1-stable_linux.x86_64 --headless --path demo --quit-after 1500
```
exit 0, `buildings=4`, sin `CHUNSA ERROR`. (Headless no ejercita ratón/teclado, así que
además compila limpio `-Werror` y, si puedes, deja una captura con el HUD/minimapa visibles.)

Español para docs/commits. Valores del proyecto: determinismo sagrado, la UI NUNCA decide
nada del juego (solo lo pide), parar > improvisar, verificación reproducible.
