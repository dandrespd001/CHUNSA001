# BRIEF — Render de producción: modo (c) 3D ortográfico + interpolación (Kimi K3)

Eres ingeniero Godot/C++ senior en CHUNSA001. **Rama `kimi/render-prod` desde main. Jamás toques main.** Conviertes la demo (hoy: puntos a saltos de 20 Hz en `_draw`) en el render de producción que decidió ADR-009: **mundo 3D minimal con cámara ortográfica + depth buffer**, con **interpolación suave a 60 FPS** entre ticks de simulación.

## ⚠️ RECURSOS (el equipo se apaga por térmica): builds `nice -n 19 ... -j2`, runs de Godot `nice -n 19`, UNO a la vez.

## Base de la que partes
- Adaptador actual en `addons/chunsa_sim/gdextension/chunsa_sim_node.{h,cpp}` (main): kernel a 20 Hz en hilo, `SnapshotRing<DemoSnapshot>`, `_draw` con círculos 2D.
- **La implementación del modo (c) 3D ortográfico ya existe** en la rama `kimi/spike-render0` (mismo adaptador, candidato `c`): cámara ortográfica, MultiMeshInstance3D de quads unlit, depth. Consúltala como REFERENCIA con `git show kimi/spike-render0:addons/chunsa_sim/gdextension/chunsa_sim_node.cpp`. Reutiliza ese setup 3D; NO reimplementes desde cero el rig de cámara si ya está resuelto ahí.
- Godot editor: `./third_party_build/Godot_v4.7.1-stable_linux.x86_64`. Demo en `demo/` (gl_compatibility, ADR-009).

## Contrato del snapshot (DEFINIDO por el Arquitecto — impleméntalo tal cual)
Para interpolar, cada unidad debe ser identificable entre frames. Cambia `DemoSnapshot` a **layout por-slot** (índice = slot de entidad, identidad estable mientras la unidad vive), en lugar del actual compactado-por-vivos:
```cpp
struct DemoSnapshot {
    uint32_t tick;
    uint32_t capacity;         // g->entities.capacity (<=1024 en la demo)
    float    x[1024];          // posición en TILES (pos_raw / 65536); válida si alive
    float    y[1024];
    uint8_t  alive[1024];      // 1 = slot vivo este snapshot
};
```
El `sim_loop` llena por slot `i` (no compacta): recorre `i=0..capacity-1`, escribe `x[i]/y[i]/alive[i]`. `capacity` = `gs->entities.capacity` (cap a 1024).

## Interpolación (contrato)
El adaptador mantiene DOS snapshots — `snap_prev` y `snap_curr` — y el instante (`steady_clock`) en que llegó `snap_curr`. Cuando `_process` adquiere un snapshot NUEVO del ring (tick distinto), rota `snap_prev = snap_curr; snap_curr = nuevo; curr_arrival = now`. Cada frame:
- `alpha = clamp((now - curr_arrival) / TICK_PERIOD_50ms, 0, 1)`.
- Para cada slot `i` vivo en `snap_curr`: si también vivo en `snap_prev`, posición renderizada = `lerp(prev, curr, alpha)`; si NO estaba en prev (recién spawneada), usa `curr` directo.
- Slots no vivos en curr: no se dibujan.
Esto da movimiento fluido a 60+ FPS aunque la simulación avance a 20 Hz. (El kernel y el ring NO cambian su semántica; solo el consumidor interpola — presentación pura, no toca determinismo.)

## Escena
- Modo (c) 3D: `Camera3D` ortográfica top-down, `MultiMeshInstance3D` de quads 8×8 unlit para unidades (z = y de mapa para el depth-order), fondo. Reutiliza el rig del spike.
- Mantén el print de diagnóstico cada 100 ticks (`CHUNSA tick=... units=...`).
- 600 unidades por defecto (`CHUNSA_UNITS` opcional, ya existe patrón en el spike).
- Opcional: un `CHUNSA_SHOT=prefix` que vuelque un PNG del viewport en el frame 600 para evidencia visual (si el spike ya lo tenía, reúsalo).

## Verificación OBLIGATORIA antes de commitear
1. `nice -n 19 cmake -B build-godot -DCMAKE_BUILD_TYPE=Debug -DCHUNSA_BUILD_GODOT=ON && nice -n 19 cmake --build build-godot -j2 --target chunsa_godot` — limpio, `demo/bin/libchunsa_godot.so` existe.
2. `nice -n 19 ./third_party_build/Godot_v4.7.1-stable_linux.x86_64 --headless --path demo --quit-after 300` — SIN crash, y aparece `CHUNSA tick=... units=600`. (Si el proyecto necesita `--import` la primera vez, córrelo antes.)
3. Los tests del kernel siguen verdes (no tocaste el core): `./build-gcc/chunsa_sim_cli golden --vectors tests/determinism/golden` (recompila build-gcc si hace falta, `nice -19 -j2`).
4. `git add` de los archivos tocados + `demo/bin/libchunsa_godot.so`; commit en la rama: "Sprint 0.2: render de producción modo (c) 3D + interpolación 60 FPS (generado: kimi-k3, pendiente revisión Arquitecto)".
5. Escribe `docs/briefs/KIMI_RENDER_PROD_RESULT.md`: qué hiciste, salida de la verificación 2 (5 líneas), y si tomaste una screenshot dónde quedó. Inclúyelo en el commit.

Si algo del contrato del snapshot o de la interpolación es imposible tal cual, documenta la desviación mínima en el RESULT y resuelve con el menor cambio — no rediseñes el kernel ni el ring.
