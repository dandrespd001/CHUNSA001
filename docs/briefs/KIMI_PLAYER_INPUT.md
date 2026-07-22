# BRIEF — Selección y órdenes del jugador con clic (Kimi K3)

Eres ingeniero Godot/C++ senior en CHUNSA001. **Rama `kimi/player-input` desde main (ya creada, estás en ella). Jamás toques main.** Objetivo: el jugador (owner 0) puede seleccionar su caballería con el ratón y ordenarle mover con clic derecho, sobre la demo showcase ya existente (combate+moral+economía).

## ⚠️ RECURSOS (el equipo se apaga por térmica): builds `nice -n 19 ... -j2`, runs de Godot `nice -n 19`, UNO a la vez.

## Archivos a tocar (SOLO estos): `addons/chunsa_sim/gdextension/chunsa_sim_node.h` y `.cpp`. NO toques el core (`addons/chunsa_sim/core/`).

## Contexto que ya existe (no lo repitas, ya está en el código actual)
- `sim_loop()` corre en su propio hilo a 20 Hz, llama `build_showcase_batch` y luego `chunsa::step(*gs, batch.data(), n)`.
- `_process` (hilo principal) lee el último snapshot publicado (`ring->acquire_latest()`), lo guarda en `snap_curr`/`snap_prev`, y `render_interpolated()` dibuja con lerp + color por bando/clase/pánico.
- La cámara (`cam3d`, campo ya existente) es `Camera3D` ORTOGRÁFICA, `set_size(MAP_PX)` (1024), posición `(512, -512, 2000)`, **sin rotación** (mirando -Z por defecto). El mapeo mundo↔render ya usado en `render_interpolated` es: `world = Vector3(px, -py, py)` donde `px = tile_x*4`, `py = tile_y*4`.
- Slots de la demo showcase (ver `build_showcase_batch`): `[0, n_cav)` = caballería owner 0 (seleccionable); `[n_cav, n_cav+n_cit)` = ciudadanos owner 0 (NO seleccionables, su IA es autónoma vía `economy_system`); resto = artillería owner 1 (enemigo, NO seleccionable).
- Comandos: `emitter` 0 = owner 0. `chunsa::step` exige `sequence` estrictamente creciente por emitter (`g.last_seq[emitter]`); el emitter 0 ya usa secuencias hasta ~600 (spawns + orden t=1) con `demo_units` hasta 1024 — así que las órdenes del jugador deben usar secuencias que EMPIECEN muy por encima, p. ej. desde `1000000`, para no colisionar nunca.
- `target_tick` se recorta internamente a `t + human_input_delay_ticks` como mínimo (§6.2 del kernel) — para una orden emitida "ahora", basta con poner `target_tick = 0` (igual que ya hace `build_showcase_batch` en sus SPAWN); el kernel la agenda al tick correcto solo.

## API EXACTA a usar (cópiala literal)
```cpp
// chunsa/commands.hpp (ya usado en el adaptador)
enum class CommandType : uint16_t { SPAWN_DEBUG=1, MOVE_TO=2, DESTROY_DEBUG=3, FLOW_MOVE=4, SPAWN_UNIT=5, SPAWN_CITIZEN=6 };
struct CmdPayload { EntityHandle handle; int64_t x_raw,y_raw; int32_t speed_mtpt;
                    int32_t hp; int32_t attack; int32_t range_mt; uint8_t unit_class; };
struct RawCommand { uint32_t target_tick; uint16_t emitter; CommandType type; uint64_t sequence; CmdPayload p; };
struct EntityHandle { uint32_t index; uint32_t generation; };
```
Los slots de caballería viven con `generation = 1` toda la demo (nunca se reciclan: no hay más SPAWN tras t=1). Usa siempre `EntityHandle{slot, 1u}`.

## 1. Cambios en `chunsa_sim_node.h`

Añade estos includes:
```cpp
#include <mutex>
#include <vector>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/variant/vector2.hpp>
```
Añade estos miembros privados (junto a los demás campos de estado, antes de `void sim_loop();`):
```cpp
// Selección/órdenes del jugador (Sprint 0.3+): el input llega en el hilo
// principal (_input); sim_loop corre en su propio hilo. `pending_player_commands`
// es la única sección compartida entre hilos → protegida por `input_mutex`.
// `is_selected` SOLO la tocan el hilo principal (_input escribe, render lee):
// no necesita mutex.
std::mutex input_mutex;
std::vector<chunsa::RawCommand> pending_player_commands;
uint64_t next_player_sequence = 1000000ull;
bool is_selected[1024] = {};
bool dragging = false;
godot::Vector2 drag_start;
```
Añade la declaración del override (junto a `_ready`/`_process`/`_exit_tree`):
```cpp
void _input(const godot::Ref<godot::InputEvent>& event) override;
```

## 2. Cambios en `chunsa_sim_node.cpp`

### 2.1. Includes nuevos
```cpp
#include <godot_cpp/classes/input_event_mouse_button.hpp>
```

### 2.2. `_input` — selección (clic/arrastre izquierdo) y orden (clic derecho)

Implementa así (usa exactamente esta lógica; los umbrales en píxeles son ajustables si algo no se siente bien, documenta el cambio):

```cpp
void ChunsaSimNode::_input(const godot::Ref<godot::InputEvent>& event) {
    if (cam3d == nullptr) return;
    godot::Ref<godot::InputEventMouseButton> mb = event;
    if (mb.is_null()) return;

    const uint32_t cap = snap_curr.capacity < 1024u ? snap_curr.capacity : 1024u;

    if (mb->get_button_index() == godot::MouseButton::MOUSE_BUTTON_LEFT) {
        if (mb->is_pressed()) {
            dragging = true;
            drag_start = mb->get_position();
            return;
        }
        // Soltar botón izquierdo: cerrar selección.
        dragging = false;
        const godot::Vector2 drag_end = mb->get_position();
        std::fill(std::begin(is_selected), std::end(is_selected), false);

        const bool is_click = (drag_start - drag_end).length() < 6.0f;
        godot::Vector2 rmin(std::min(drag_start.x, drag_end.x), std::min(drag_start.y, drag_end.y));
        godot::Vector2 rmax(std::max(drag_start.x, drag_end.x), std::max(drag_start.y, drag_end.y));

        uint32_t best_i = UINT32_MAX;
        float best_d2 = 1.0e30f;
        for (uint32_t i = 0; i < cap; ++i) {
            // Solo caballería propia (owner 0, unit_class 1) es seleccionable:
            // los ciudadanos tienen IA autónoma y la artillería es del rival.
            if (snap_curr.alive[i] == 0u) continue;
            if (snap_curr.owner[i] != 0u) continue;
            if (snap_curr.unit_class[i] != 1u) continue;

            const float px = snap_curr.x[i] * 4.0f;
            const float py = snap_curr.y[i] * 4.0f;
            const godot::Vector3 world_pos(px, -py, py);
            const godot::Vector2 screen_pos = cam3d->unproject_position(world_pos);

            if (is_click) {
                const float dx = screen_pos.x - drag_end.x;
                const float dy = screen_pos.y - drag_end.y;
                const float d2 = dx * dx + dy * dy;
                if (d2 < 20.0f * 20.0f && d2 < best_d2) {
                    best_d2 = d2;
                    best_i = i;
                }
            } else {
                if (screen_pos.x >= rmin.x && screen_pos.x <= rmax.x &&
                    screen_pos.y >= rmin.y && screen_pos.y <= rmax.y) {
                    is_selected[i] = true;
                }
            }
        }
        if (is_click && best_i != UINT32_MAX) {
            is_selected[best_i] = true;
        }
        return;
    }

    if (mb->get_button_index() == godot::MouseButton::MOUSE_BUTTON_RIGHT && mb->is_pressed()) {
        // Screen → mundo: cámara ortográfica SIN rotación mirando -Z, con el
        // mapeo world=(px,-py,py) ya usado en render_interpolated. El origen
        // del rayo en cada píxel YA da directamente (px, world_y=-py) en sus
        // componentes X/Y (no hace falta intersección de plano ni la dirección
        // del rayo): world_px = origin.x ; world_py = -origin.y.
        const godot::Vector3 origin = cam3d->project_ray_origin(mb->get_position());
        const float world_px = origin.x;
        const float world_py = -origin.y;
        const int64_t x_raw = static_cast<int64_t>((world_px / 4.0f) * 65536.0f);
        const int64_t y_raw = static_cast<int64_t>((world_py / 4.0f) * 65536.0f);

        std::lock_guard<std::mutex> lock(input_mutex);
        for (uint32_t i = 0; i < cap; ++i) {
            if (!is_selected[i]) continue;
            if (snap_curr.alive[i] == 0u || snap_curr.owner[i] != 0u) continue;  // pudo morir
            chunsa::RawCommand c;
            std::memset(&c, 0, sizeof(c));
            c.target_tick = 0;
            c.emitter = 0;
            c.type = chunsa::CommandType::MOVE_TO;
            c.sequence = next_player_sequence++;
            c.p.handle = chunsa::EntityHandle{i, 1u};
            c.p.x_raw = x_raw;
            c.p.y_raw = y_raw;
            pending_player_commands.push_back(c);
        }
    }
}
```
Añade `#include <algorithm>` si `std::fill`/`std::min`/`std::max` no están ya cubiertos (revisa, `<algorithm>` ya está incluido para `std::clamp`).

### 2.3. `sim_loop` — drenar comandos del jugador cada tick

Amplía el tamaño del batch para que quepan las órdenes del jugador (puede seleccionar y ordenar a TODA su caballería en un tick):
```cpp
std::vector<chunsa::RawCommand> batch(std::max<uint32_t>(demo_units, 700u) + 512u);
```
Justo antes de `chunsa::step(*gs, batch.data(), n)`, añade el drenado (bajo el mismo `n` que ya devuelve `build_showcase_batch`):
```cpp
{
    std::lock_guard<std::mutex> lock(input_mutex);
    for (const chunsa::RawCommand& pc : pending_player_commands) {
        batch[n++] = pc;
    }
    pending_player_commands.clear();
}
```
(Ojo: esto va DESPUÉS de `const uint32_t n = build_showcase_batch(batch.data(), t);` y ANTES de `chunsa::step(*gs, batch.data(), n);` — mismo `n`, se sigue incrementando.)

### 2.4. `render_interpolated` — resaltar selección

En el bloque de color ya existente, añade la selección como prioridad MÁXIMA (antes que ciudadano/pánico/bando):
```cpp
godot::Color c;
if (is_selected[i])                     c = godot::Color(0.3, 1.0, 0.3);   // seleccionado: verde brillante
else if (snap_curr.unit_class[i] == 3u) c = godot::Color(0.9, 0.85, 0.2);
else if (snap_curr.fleeing[i] != 0u)    c = godot::Color(0.9, 0.9, 0.95);
else if (snap_curr.owner[i] == 0u)      c = godot::Color(0.2, 0.6, 0.95);
else                                     c = godot::Color(0.9, 0.3, 0.2);
mm->set_instance_color(k, c);
```

## Verificación OBLIGATORIA antes de commitear
1. `nice -n 19 cmake -B build-godot -DCMAKE_BUILD_TYPE=Debug -DCHUNSA_BUILD_GODOT=ON && nice -n 19 cmake --build build-godot -j2 --target chunsa_godot` — limpio, `demo/bin/libchunsa_godot.so` existe.
2. `nice -n 19 ./third_party_build/Godot_v4.7.1-stable_linux.x86_64 --path demo` (CON ventana, no headless — necesitas probar el ratón de verdad): haz clic izquierdo sobre un grupo de caballería (o arrastra un rectángulo sobre varias), confirma que se pintan de VERDE; haz clic derecho en otro punto del mapa y confirma que la caballería seleccionada se mueve hacia allí mientras el resto sigue su comportamiento normal. Prueba también: clic izquierdo en vacío limpia la selección; clic derecho sin nada seleccionado no hace nada (no debe crashear).
3. Kernel intacto: `nice -n 19 cmake --build build-gcc -j2 && ./build-gcc/chunsa_sim_cli golden --vectors tests/determinism/golden` (no debiste tocar `addons/chunsa_sim/core/`) — y `ctest` en `build-gcc` (9/9).
4. `git add` de los 2 archivos del adaptador + `demo/bin/libchunsa_godot.so`; commit en la rama: "Sprint 0.3+: selección y órdenes del jugador con clic (generado: kimi-k3, pendiente revisión Arquitecto)".
5. Escribe `docs/briefs/KIMI_PLAYER_INPUT_RESULT.md`: qué hiciste, qué probaste manualmente en el paso 2 (descríbelo, no hay forma de automatizar clics de ratón), desviaciones. Inclúyelo en el commit.

Si algo del contrato es imposible tal cual (p. ej. el nombre exacto de la constante `MOUSE_BUTTON_LEFT` en esta versión de godot-cpp, o `unproject_position`/`project_ray_origin` no existen con esa firma), ajusta con el mínimo cambio y documenta la desviación — no rediseñes el kernel ni cambies el contrato de comandos.
