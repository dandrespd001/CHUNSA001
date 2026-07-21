# RESULT — KIMI_RENDER_PROD (Sprint 0.2: render de producción modo (c) + interpolación)

Generado por: kimi-k3. Rama: `kimi/render-prod` (desde main). Pendiente revisión Arquitecto.

## Qué se hizo

1. **`DemoSnapshot` → layout por-slot (contrato tal cual)**
   (`chunsa_sim_node.h`): `tick`, `capacity` (= `gs->entities.capacity`, cap a
   1024), `x[1024]`, `y[1024]` en tiles (pos_raw/65536), `alive[1024]`.
   El `sim_loop` ya no compacta por vivos: recorre `i=0..capacity-1` y escribe
   `x[i]/y[i]/alive[i]` por slot. Índice = slot de entidad (identidad estable).

2. **Interpolación 60 FPS (contrato tal cual)**: el adaptador mantiene
   `snap_prev`/`snap_curr` + `curr_arrival` (`steady_clock`). Cuando `_process`
   adquiere un snapshot NUEVO (tick distinto) rota `prev=curr; curr=nuevo;
   curr_arrival=now`. Cada frame `render_interpolated()` calcula
   `alpha = clamp((now - curr_arrival)/50ms, 0, 1)` y por slot vivo en curr:
   vivo también en prev → `lerp(prev, curr, alpha)`; recién spawneado → curr
   directo; no vivo → no se dibuja. Las transforms 3D se reescriben CADA frame
   (ya no solo por snapshot nuevo): movimiento fluido a 60+ FPS con sim a 20 Hz.

3. **Escena modo (c) reutilizada del spike** (`git show
   kimi/spike-render0:.../chunsa_sim_node.cpp`): mismo rig — `Camera3D`
   ortográfica top-down (mundo `(u,-v,v)` ← mapa px, z=y de mapa → depth
   resuelve el orden), `MultiMeshInstance3D` de quads 8×8 unlit para unidades,
   cajas 3D estáticas para 20 edificios falsos, clear color como fondo. Se
   eliminaron los candidatos (a)/(b) y la telemetría del spike: producción es
   solo (c). `MultiMesh` de unidades dimensionado a `entities.capacity`
   (cualquier slot puede estar vivo).

4. **Conservado**: print cada 100 ticks (`CHUNSA tick=... units=...`, ahora con
   el recuento de vivos por slot), `CHUNSA_UNITS` (default 600), semilla y
   escenario. `CHUNSA_SHOT=prefix` vuelca PNG del viewport en el frame 600.

5. **Kernel y ring intactos**: ningún archivo de `addons/chunsa_sim/core/`
   tocado (`git diff main -- addons/chunsa_sim/core` vacío). La interpolación
   es presentación pura; el determinismo no cambia.

## Verificación

1. **Build** (nice -n 19, -j2): limpio, sin warnings nuevos;
   `demo/bin/libchunsa_godot.so` regenerado.
   ```
   [100%] Linking CXX shared library .../demo/bin/libchunsa_godot.so
   [100%] Built target chunsa_godot
   ```

2. **Godot headless** (`nice -n 19 .../Godot_v4.7.1-stable_linux.x86_64
   --headless --path demo --quit-after N`): SIN crash (EXIT=0). Salida:
   ```
   CHUNSA render=prod(c/3d+interp) units=600
   CHUNSA tick=0 units=0
   CHUNSA tick=100 units=600
   ```
   **Desviación mínima documentada**: el brief fija `--quit-after 300`, pero en
   esta máquina headless corre deslimitado (~130 fps → 300 frames ≈ 2,3 s de
   reloj ≈ 46 ticks de sim), así que solo se alcanza a imprimir tick=0. Además
   en tick=0 hay 0 unidades vivas por semántica del kernel (spawns emitidos en
   t=0 se ejecutan en t=1 por `human_input_delay_ticks=1` — igual que con el
   adaptador anterior). Para que aparezca `units=600` se corrió el MISMO
   binario con `--quit-after 1200` (8,6 s): imprime `CHUNSA tick=100
   units=600` y sale limpio. El comando literal de 300 frames también se corrió:
   EXIT=0, sin errores. No se tocó kernel ni ring para forzar el número.

3. **Golden del kernel**: `./build-gcc/chunsa_sim_cli golden --vectors
   tests/determinism/golden` → `GOLDEN backend=int128 casos=1074 fallos=0 [OK]`
   (sin recompilar: el core no se tocó).

## Screenshot

`CHUNSA_SHOT=render_prod --quit-after 700`: el mecanismo dispara en el frame
600, pero el renderer headless es *dummy* → `get_texture()->get_image()` vacío
(mensaje `CHUNSA SHOT: imagen vacía (renderer dummy?)`, mismo fallback del
spike). En este entorno no hay Xvfb ni display, así que no se pudo generar el
PNG. Queda pendiente correrlo con display (o Xvfb) para la evidencia visual;
el código está listo (`maybe_screenshot()`, frame 600, `<prefix>.f600.png`).

## Archivos en el commit

- `addons/chunsa_sim/gdextension/chunsa_sim_node.h`
- `addons/chunsa_sim/gdextension/chunsa_sim_node.cpp`
- `demo/bin/libchunsa_godot.so` (recompilado)
- `docs/briefs/KIMI_RENDER_PROD.md` (brief del sprint)
- `docs/briefs/KIMI_RENDER_PROD_RESULT.md` (este informe)

---

## Revisión del Arquitecto (2026-07-21)

**Veredicto: ACEPTADO sin cambios.** Verifiqué yo mismo: build 0-warnings, demo headless sin crash (`tick=100 units=600`), **kernel/ring intactos** (`git diff main -- addons/chunsa_sim/core` vacío — confirmado), golden 1074/1074. Revisión de código: `_process` copia el snapshot y libera el slot del ring de inmediato (sin retención → sin race); `render_interpolated` interpola solo entre slots vivos en ambos snapshots (spawns nuevos usan curr), mapea `z=y` para el depth-order del modo (c); float solo en presentación (legítimo). Las 2 desviaciones de Kimi son del entorno (sin display para PNG; semántica de `--quit-after`), no del código.

Caso borde anotado (no bloquea, demo no destruye): si un slot se reciclara (muerte+spawn en el mismo slot entre dos snapshots de 50 ms), el lerp mezclaría dos unidades distintas un frame. Trivial de blindar en 0.3 con un id de generación en el snapshot si el reciclaje se vuelve frecuente.

**Reparto:** Kimi K3 en su nicho (render/frontend) — reutilizó su propio modo (c) del spike, implementó el contrato de snapshot+interpolación al 100%, esta vez sin agotar cuota. Cierra el Sprint 0.2.
