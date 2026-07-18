# BRIEF — Adaptador GDExtension + demo visual del ring (Kimi K3)

Eres un ingeniero C++/Godot senior trabajando en el repo CHUNSA001 (rama `kimi/gdext-adapter` — **jamás toques main**). Objetivo: el kernel determinista de CHUNSA corriendo en un hilo propio dentro de Godot, visible en pantalla vía el ring de snapshots (SPEC-001 §9 ya implementado en `addons/chunsa_sim/core/include/chunsa/snapshot_ring.hpp`).

## ⚠️ RESTRICCIÓN DE RECURSOS (el equipo del dueño se apaga por térmica)
TODO build: `nice -n 19 cmake --build <dir> -j2`. JAMÁS `-j` sin número. Una compilación a la vez.

## Reglas
- Solo AÑADES archivos; los existentes no se tocan salvo APPEND al final de `CMakeLists.txt`.
- No tocar `third_party_build/` (ahí están: Godot editor `Godot_v4.7.1-stable_linux.x86_64`, y godot-cpp con headers en `include/`, `build/gen/include/`, `gdextension/`, lib en `build/bin/libgodot-cpp.linux.template_debug.x86_64.a`).
- El core es header-only puro; NO incluyas `chunsa/cli_run.hpp` (requiere un símbolo del CLI). Usa `chunsa/driver.hpp` (función `build_human_batch`), `chunsa/step.hpp`, `chunsa/game_state.hpp`, `chunsa/snapshot_ring.hpp`.

## Entregables

1. `addons/chunsa_sim/gdextension/register_types.h/.cpp` — init GDExtension estándar (nivel SCENE), registra `ChunsaSimNode`. Entry symbol: `chunsa_gdext_init`.

2. `addons/chunsa_sim/gdextension/chunsa_sim_node.h/.cpp` — `class ChunsaSimNode : public godot::Node2D`:
   - `struct DemoSnapshot { uint32_t tick; uint32_t count; float x[1024]; float y[1024]; };`
   - Miembros: `std::thread sim_thread; std::atomic<bool> running{false}; chunsa::SnapshotRing<DemoSnapshot>* ring; chunsa::GameState* gs; DemoSnapshot last{};`  (gs y ring en HEAP con new — GameState pesa ~10MB).
   - `_ready()`: si `godot::Engine::get_singleton()->is_editor_hint()` → return. Inicializa gs (`gs_init`) con `MatchConfig01A{604, 1, 1, 20, 20, 256, 256, 20260716ull}` (orden de campos: max_entities, player_count, human_input_delay_ticks, max_future_command_ticks, checksum_every_ticks, map_tiles_x, map_tiles_y, seed), `ring->init()`. `set_process(true)`. Lanza el hilo:
     bucle a 20 Hz con `std::this_thread::sleep_until` (steady_clock, TICK = 50ms): `n = chunsa::build_human_batch(batch, t, 600, seed, false)` (batch = std::vector<chunsa::RawCommand> prealocado de 700) → `chunsa::step(*gs, batch.data(), n)` → `DemoSnapshot* s = ring->begin_write()` → llenar (tick; recorrer `gs->entities.capacity`, para vivos: `x[k] = float(gs->pos_x[i]) / 65536.0f;` ídem y; count=k; máx 1024) → `ring->publish()`. Mientras `running`.
   - `_exit_tree()`: `running=false`; join si joinable; delete gs/ring.
   - `_process(double)`: `auto h = ring->acquire_latest(); if (h.valid) { last = *h.data; ring->release(h); queue_redraw(); }`
   - `_draw()`: fondo `draw_rect(Rect2(0,0,1024,1024), Color(0.05,0.05,0.08))`; por unidad `draw_circle(Vector2(last.x[k]*4.0f, last.y[k]*4.0f), 2.0f, Color(0.2,0.9,0.9))` (mapa de 256 tiles × 4 px). Cada 100 ticks `godot::UtilityFunctions::print("CHUNSA tick=", last.tick, " units=", last.count);` (hazlo en _process comparando tick%100==0 y tick cambiado).
   - `_bind_methods()` puede quedar vacío. Overrides con las firmas godot-cpp: `void _ready() override; void _process(double delta) override; void _draw() override; void _exit_tree() override;`.

3. APPEND a `CMakeLists.txt`: bloque `option(CHUNSA_BUILD_GODOT "" OFF)` + `if(CHUNSA_BUILD_GODOT)` que crea `add_library(chunsa_godot SHARED ...)` con los 2 .cpp; include dirs: `addons/chunsa_sim/core/include` + los 3 de godot-cpp listados arriba; linkea la .a de godot-cpp (ruta absoluta vía `${CMAKE_CURRENT_SOURCE_DIR}`) y `Threads::Threads`; `set_target_properties`: `LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/demo/bin`, `OUTPUT_NAME "chunsa_godot"`. Si el link manual de la .a falla por definiciones que faltan, alternativa permitida: `add_subdirectory(third_party_build/godot-cpp <binary_dir dentro de tu build>)` con el target que exporte (míralo con grep en su CMakeLists) — SIEMPRE compilando con -j2 nice.

4. `demo/project.godot` (application/config/name="CHUNSA Demo", run/main_scene="res://main.tscn", rendering/renderer/rendering_method="gl_compatibility") · `demo/main.tscn` (nodo raíz ChunsaSimNode) · `demo/chunsa_sim.gdextension`:
   ```
   [configuration]
   entry_symbol = "chunsa_gdext_init"
   compatibility_minimum = "4.7"
   [libraries]
   linux.debug.x86_64 = "res://bin/libchunsa_godot.so"
   linux.release.x86_64 = "res://bin/libchunsa_godot.so"
   ```

## Verificación OBLIGATORIA antes de commitear
1. `nice -n 19 cmake -B build-godot -DCMAKE_BUILD_TYPE=Debug -DCHUNSA_BUILD_GODOT=ON && nice -n 19 cmake --build build-godot -j2 --target chunsa_godot` sin errores y `demo/bin/libchunsa_godot.so` existe.
2. `./third_party_build/Godot_v4.7.1-stable_linux.x86_64 --headless --path demo --quit-after 300` termina SIN crash y en su salida aparecen líneas `CHUNSA tick=... units=600`.
3. Los tests existentes siguen verdes: `./build-gcc/chunsa_sim_cli golden --vectors tests/determinism/golden` (no debiste romper nada — no tocaste el core).
4. `git add` SOLO de los archivos nuevos + CMakeLists.txt; commit en esta rama: mensaje "Sprint 0.2: adaptador GDExtension + demo visual del ring (generado: kimi-k3, pendiente revisión Arquitecto)".
5. Escribe `docs/briefs/KIMI_GDEXT_RESULT.md`: qué hiciste, salida de la verificación 2 (recorta 5 líneas), problemas encontrados. Inclúyelo en el commit.

Si algo del contrato de arriba es imposible tal cual (p. ej. una firma de godot-cpp cambió), documenta la desviación en el RESULT.md y resuelve con el mínimo cambio — NO rediseñes.
