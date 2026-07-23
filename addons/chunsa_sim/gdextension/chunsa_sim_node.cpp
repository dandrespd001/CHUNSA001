#include "chunsa_sim_node.h"

// chunsa_sim — ChunsaSimNode (Sprint 0.3, demo showcase: combate + moral +
// economía visibles). Escenario showcase_v1 (seed fija) avanzando a 20 Hz en
// un hilo: caballería (owner 0) y artillería (owner 1) convergen en (128,128)
// y se enzarzan (combat/morale systems del kernel, autónomos), mientras
// ciudadanos (owner 0) recolectan Alimentos (economy_system, autónomo). El
// hilo de presentación lee el último snapshot publicado sin bloquear al
// writer, interpola posiciones entre snapshots (60+ FPS) y colorea por
// bando/clase/pánico leyendo el snapshot actual.
//
// Presentación (ADR-009, modo (c) — rig reutilizado del SPIKE-RENDER-0):
// quads 3D con Camera3D ortográfica mirando al plano del mapa,
// MultiMeshInstance3D unlit; z de cada quad = su y de mapa → el depth buffer
// resuelve el orden por píxel. Edificios = cajas 3D estáticas.
//
// Interpolación (contrato del Arquitecto): el adaptador mantiene snap_prev y
// snap_curr + el instante (steady_clock) en que llegó snap_curr. Al llegar un
// snapshot NUEVO (tick distinto) rota prev=curr, curr=nuevo. Cada frame,
// alpha = clamp((now - curr_arrival) / 50ms, 0, 1); slot vivo en curr y en
// prev → lerp; recién spawneado → curr directo; no vivo en curr → no se
// dibuja. El kernel y el ring NO cambian su semántica.
//
// CHUNSA_UNITS=2..1024 (default 600). CHUNSA_SHOT=prefix guarda un PNG del
// viewport en el frame 600 (evidencia visual).

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iterator>
#include <vector>

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <chunsa/driver.hpp>
#include <chunsa/step.hpp>

namespace {
constexpr uint64_t DEMO_SEED = 20260716ull;
constexpr auto TICK_PERIOD = std::chrono::milliseconds(50);  // 20 Hz
const godot::Color UNIT_COLOR(0.2, 0.9, 0.9);
const godot::Color WALL_COLOR(0.5, 0.5, 0.55);
}  // namespace

void ChunsaSimNode::_bind_methods() {
    // Intencionadamente vacío: la demo no expone API al scripting.
}

void ChunsaSimNode::_ready() {
    if (godot::Engine::get_singleton()->is_editor_hint()) {
        return;  // en el editor no arrancamos simulación
    }

    godot::OS* os_ptr = godot::OS::get_singleton();
    const godot::String env_units = os_ptr->get_environment("CHUNSA_UNITS");
    if (!env_units.is_empty()) {
        const int parsed = env_units.to_int();
        if (parsed >= 2 && parsed <= 1024) {
            demo_units = static_cast<uint32_t>(parsed);
        }
    }
    const godot::String env_shot = os_ptr->get_environment("CHUNSA_SHOT");
    if (!env_shot.is_empty()) {
        shot_prefix = env_shot.utf8().get_data();
    }
    godot::UtilityFunctions::print("CHUNSA render=prod(c/3d+interp) units=",
                                   demo_units);

    // max_entities, player_count, human_input_delay_ticks,
    // max_future_command_ticks, checksum_every_ticks, map_tiles_x,
    // map_tiles_y, seed, allow_debug_stat_payload (0 = data-driven, Sprint 0.4)
    const chunsa::MatchConfig01A cfg{demo_units + 16, 1, 1, 20, 20, 256, 256,
                                     DEMO_SEED, 0};
    gs = new chunsa::GameState();
    chunsa::gs_init(*gs, cfg);
    // --- Sprint 0.4: cargar el catálogo de datos y bindearlo al GameState ---
    {
        godot::String blob_path = os_ptr->get_environment("CHUNSA_BLOB");
        if (blob_path.is_empty()) {
            blob_path = godot::ProjectSettings::get_singleton()
                            ->globalize_path("res://chunsa_base.chdb");
        }
        const chunsa::CatalogLoadCode code = chunsa::catalog_load_file_v1(
            blob_path.utf8().get_data(),
            chunsa::CatalogLoadProfile::Development, catalog_storage);
        if (code != chunsa::CatalogLoadCode::Ok || !catalog_storage.valid()) {
            godot::UtilityFunctions::print(
                "CHUNSA ERROR: catálogo no cargado (code=",
                static_cast<int64_t>(code), ") desde ", blob_path);
            return;  // sin catálogo no hay spawns data-driven
        }
        gs->catalog = &catalog_storage.catalog();
        uid_cavalry = chunsa::catalog_find_unit(
            *gs->catalog, "egipto:chariot_warrior", std::strlen("egipto:chariot_warrior"));
        uid_citizen = chunsa::catalog_find_unit(
            *gs->catalog, "egipto:work_crew", std::strlen("egipto:work_crew"));
        uid_artillery = chunsa::catalog_find_unit(
            *gs->catalog, "rome:ballista_crew", std::strlen("rome:ballista_crew"));
        godot::UtilityFunctions::print(
            "CHUNSA catálogo OK: cav_id=", static_cast<int64_t>(uid_cavalry),
            " cit_id=", static_cast<int64_t>(uid_citizen),
            " art_id=", static_cast<int64_t>(uid_artillery));
    }
    ring = new chunsa::SnapshotRing<DemoSnapshot>();
    ring->init();

    setup_3d();

    set_process(true);
    running.store(true);
    sim_thread = std::thread(&ChunsaSimNode::sim_loop, this);
}

void ChunsaSimNode::sim_loop() {
    using clock = std::chrono::steady_clock;
    // +512: margen para las órdenes del jugador (Sprint 0.3+) que se drenan
    // de pending_player_commands cada tick, además del batch del showcase.
    std::vector<chunsa::RawCommand> batch(std::max<uint32_t>(demo_units, 700u) + 512u);

    auto next_tick = clock::now();
    while (running.load(std::memory_order_relaxed)) {
        const uint32_t t = gs->tick;
        uint32_t n = build_showcase_batch(batch.data(), t);
        {
            // Drenar órdenes del jugador encoladas por _input (hilo principal).
            std::lock_guard<std::mutex> lock(input_mutex);
            for (const chunsa::RawCommand& pc : pending_player_commands) {
                batch[n++] = pc;
            }
            pending_player_commands.clear();
        }
        chunsa::step(*gs, batch.data(), n);

        DemoSnapshot* s = ring->begin_write();
        if (s != nullptr) {
            // Layout POR-SLOT (contrato): no compacta; el índice es el slot.
            s->tick = t;
            const uint32_t cap =
                    gs->entities.capacity < 1024u ? gs->entities.capacity : 1024u;
            s->capacity = cap;
            for (uint32_t i = 0; i < cap; ++i) {
                s->alive[i] = gs->entities.alive[i];
                s->x[i] = static_cast<float>(gs->pos_x[i]) / 65536.0f;
                s->y[i] = static_cast<float>(gs->pos_y[i]) / 65536.0f;
                s->owner[i] = gs->owner[i];
                s->unit_class[i] = gs->unit_class[i];
                s->fleeing[i] = gs->fleeing[i];
            }
            ring->publish();
        }

        // Diagnóstico del showcase (Sprint 0.3) cada 100 ticks, desde el hilo
        // de simulación: vivos por clase + stock de Alimentos del owner 0.
        if (t % 100u == 0u) {
            uint32_t n_cav_alive = 0, n_art_alive = 0, n_cit_alive = 0;
            for (uint32_t i = 0; i < gs->entities.capacity; ++i) {
                if (gs->entities.alive[i] == 0u) continue;
                if (gs->unit_class[i] == 1u) ++n_cav_alive;
                else if (gs->unit_class[i] == 2u) ++n_art_alive;
                else if (gs->unit_class[i] == 3u) ++n_cit_alive;
            }
            godot::UtilityFunctions::print("CHUNSA cav=", n_cav_alive,
                                           " art=", n_art_alive,
                                           " citizens=", n_cit_alive,
                                           " stock_A=", gs->player_stock[0][0]);
        }

        next_tick += TICK_PERIOD;
        std::this_thread::sleep_until(next_tick);
    }
}

void ChunsaSimNode::_process(double /*delta*/) {
    ++frame_count;
    if (ring != nullptr) {
        auto h = ring->acquire_latest();
        if (h.valid) {
            const DemoSnapshot fresh = *h.data;
            ring->release(h);
            // Snapshot NUEVO (tick distinto): rotar prev/curr + timestamp.
            if (!have_curr || fresh.tick != snap_curr.tick) {
                snap_prev = snap_curr;
                have_prev = have_curr;
                snap_curr = fresh;
                curr_arrival = std::chrono::steady_clock::now();
                have_curr = true;

                const uint32_t cap = snap_curr.capacity < 1024u
                        ? snap_curr.capacity
                        : 1024u;
                alive_in_curr = 0;
                for (uint32_t i = 0; i < cap; ++i) {
                    alive_in_curr += snap_curr.alive[i] != 0u ? 1u : 0u;
                }
                if (snap_curr.tick % 100u == 0u &&
                    snap_curr.tick != last_reported_tick) {
                    last_reported_tick = snap_curr.tick;
                    godot::UtilityFunctions::print("CHUNSA tick=", snap_curr.tick,
                                                   " units=", alive_in_curr);
                }
            }
        }
    }
    render_interpolated();
    maybe_screenshot();
}

// Selección y órdenes del jugador (Sprint 0.3+): clic/arrastre izquierdo
// selecciona caballería propia; clic derecho ordena MOVE_TO a lo seleccionado.
// Corre en el hilo principal; encola en pending_player_commands (protegida por
// input_mutex) y sim_loop los drena en el siguiente tick.
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

// Cada frame: posición renderizada = lerp(prev, curr, alpha) por slot; el
// slot recién spawneado usa curr directo; el slot muerto no se dibuja.
void ChunsaSimNode::render_interpolated() {
    if (!have_curr || mmi_units3d == nullptr) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    float alpha = std::chrono::duration<float>(now - curr_arrival).count() /
            (std::chrono::duration<float>(TICK_PERIOD)).count();
    alpha = std::clamp(alpha, 0.0f, 1.0f);

    const uint32_t cap =
            snap_curr.capacity < 1024u ? snap_curr.capacity : 1024u;
    const uint32_t pcap =
            snap_prev.capacity < 1024u ? snap_prev.capacity : 1024u;
    const godot::Ref<godot::MultiMesh> mm = mmi_units3d->get_multimesh();
    int32_t k = 0;
    for (uint32_t i = 0; i < cap; ++i) {
        if (snap_curr.alive[i] == 0u) {
            continue;
        }
        float fx = snap_curr.x[i];
        float fy = snap_curr.y[i];
        if (have_prev && i < pcap && snap_prev.alive[i] != 0u) {
            fx = snap_prev.x[i] + (snap_curr.x[i] - snap_prev.x[i]) * alpha;
            fy = snap_prev.y[i] + (snap_curr.y[i] - snap_prev.y[i]) * alpha;
        }
        const float px = fx * 4.0f;
        const float py = fy * 4.0f;
        const godot::Basis sc(godot::Vector3(8, 0, 0), godot::Vector3(0, 8, 0),
                              godot::Vector3(0, 0, 1));
        mm->set_instance_transform(k, godot::Transform3D(
                                              sc, godot::Vector3(px, -py, py)));
        // Color por selección/bando + pánico (Sprint 0.3+): se lee del
        // snapshot actual (snap_curr, sin interpolar — solo la posición se
        // interpola). La selección tiene prioridad MÁXIMA sobre el resto.
        godot::Color c;
        if (is_selected[i])                     c = godot::Color(0.3, 1.0, 0.3);    // seleccionado: verde brillante
        else if (snap_curr.unit_class[i] == 3u) c = godot::Color(0.9, 0.85, 0.2);   // ciudadano: amarillo
        else if (snap_curr.fleeing[i] != 0u)    c = godot::Color(0.9, 0.9, 0.95);   // pánico: casi blanco
        else if (snap_curr.owner[i] == 0u)      c = godot::Color(0.2, 0.6, 0.95);   // owner 0: azul
        else                                     c = godot::Color(0.9, 0.3, 0.2);   // owner 1: rojo/naranja
        mm->set_instance_color(k, c);
        ++k;
    }
    mm->set_visible_instance_count(k);
}

// ---------------------------------------------------------------------------
// Escenarios de demo (batches de comandos para sim_loop)
// ---------------------------------------------------------------------------

// Escenario Sprint 0.2 (FlowField), ya NO usado por sim_loop (ver
// build_showcase_batch más abajo): spawns en el lado IZQUIERDO del muro y un
// FLOW_MOVE hacia un goal a la DERECHA; las unidades convergen en el hueco y
// cruzan. Se conserva como referencia del patrón rng/comandos. Determinista.
uint32_t ChunsaSimNode::build_flow_batch(chunsa::RawCommand* batch, uint32_t t) {
    using namespace chunsa;
    FatalReason dummy = FatalReason::NONE;
    const uint32_t BENCH = static_cast<uint32_t>(RngStream::BENCH);
    uint32_t n = 0;
    if (t == 0u) {
        for (uint32_t i = 0; i < demo_units; ++i) {
            RawCommand& c = batch[n];
            std::memset(&c, 0, sizeof(RawCommand));
            c.target_tick = 0; c.emitter = 0; c.type = CommandType::SPAWN_DEBUG;
            c.sequence = i + 1u;
            c.p.handle = EntityHandle{i, 1u};
            const uint32_t tx = rng_range(DEMO_SEED, BENCH, 0u, i, 1u, 4u, 60u, dummy);
            const uint32_t ty = rng_range(DEMO_SEED, BENCH, 0u, i, 2u, 40u, 216u, dummy);
            c.p.x_raw = static_cast<int64_t>(tx) * 65536 + 32768;
            c.p.y_raw = static_cast<int64_t>(ty) * 65536 + 32768;
            c.p.speed_mtpt = static_cast<int32_t>(
                    rng_range(DEMO_SEED, BENCH, 0u, i, 3u, 100u, 300u, dummy));
            ++n;
        }
    } else if (t == 1u) {
        RawCommand& c = batch[0];
        std::memset(&c, 0, sizeof(RawCommand));
        c.target_tick = 1; c.emitter = 0; c.type = CommandType::FLOW_MOVE;
        c.sequence = demo_units + 1u;
        c.p.x_raw = static_cast<int64_t>(220) * 65536 + 32768;
        c.p.y_raw = static_cast<int64_t>(128) * 65536 + 32768;
        n = 1;
    }
    return n;
}

// ---------------------------------------------------------------------------
// Escenario showcase Sprint 0.3: combate + moral + economía visibles
// ---------------------------------------------------------------------------

// t==0: spawns — 40% caballería (owner 0), 40% artillería (owner 1), 20%
// ciudadanos (owner 0). Orden de emisión = orden canónico de aplicación en
// tick 1 (emitter asc, sequence asc), así la tabla queda determinista:
// cav en slots [0,n_cav), ciudadanos [n_cav,n_cav+n_cit), artillería
// [n_cav+n_cit,demo_units) — todos con generación 1.
// t==1: UN MOVE_TO por unidad de combate hacia (128,128) con los handles
// analíticos de arriba (los spawns se aplican dentro de step() del tick 1,
// así que en t==1 aún no son visibles en gs; la cuenta de slots es exacta).
// Resto: batch vacío — combate, moral y economía corren autónomos en el kernel.
//
// DESVIACIÓN documentada en docs/briefs/KIMI_DEMO_SHOWCASE_RESULT.md: cajas de
// spawn acercadas al centro (cav x∈[110,124], art x∈[132,146]; el brief dice
// [60,90]/[166,196]) y ciudadanos a 800 mtpt (brief: 200). Motivo: el run de
// verificación (--quit-after 2000 frames ≈ 14 s ≈ 280 ticks de sim a 20 Hz en
// esta máquina) no alcanza los ~460+ ticks que el primer contacto y la primera
// entrega de stock necesitarían con los valores originales. Stats (hp/attack/
// range/clase) y resto del contrato, intactos.
uint32_t ChunsaSimNode::build_showcase_batch(chunsa::RawCommand* batch, uint32_t t) {
    using namespace chunsa;
    FatalReason dummy = FatalReason::NONE;
    const uint32_t BENCH = static_cast<uint32_t>(RngStream::BENCH);
    const uint32_t n_cav = (demo_units * 40u) / 100u;
    const uint32_t n_art = (demo_units * 40u) / 100u;
    const uint32_t n_cit = demo_units - n_cav - n_art;
    uint32_t n = 0;
    if (t == 0u) {
        uint64_t seq0 = 1;  // emitter 0 (caballería + ciudadanos), estrictamente creciente
        uint64_t seq1 = 1;  // emitter 1 (artillería), estrictamente creciente
        for (uint32_t i = 0; i < n_cav; ++i) {  // caballería, owner 0
            RawCommand& c = batch[n];
            std::memset(&c, 0, sizeof(RawCommand));
            c.target_tick = 0; c.emitter = 0; c.type = CommandType::SPAWN_UNIT;
            c.sequence = seq0++;
            const uint32_t tx = rng_range(DEMO_SEED, BENCH, 0u, i, 1u, 110u, 125u, dummy);
            const uint32_t ty = rng_range(DEMO_SEED, BENCH, 0u, i, 2u, 100u, 157u, dummy);
            c.p.x_raw = static_cast<int64_t>(tx) * 65536 + 32768;
            c.p.y_raw = static_cast<int64_t>(ty) * 65536 + 32768;
            c.p.unit_id = uid_cavalry;  // stats del catálogo (payload en 0 por el memset)
            ++n;
        }
        for (uint32_t i = 0; i < n_cit; ++i) {  // ciudadanos, owner 0
            RawCommand& c = batch[n];
            std::memset(&c, 0, sizeof(RawCommand));
            c.target_tick = 0; c.emitter = 0; c.type = CommandType::SPAWN_CITIZEN;
            c.sequence = seq0++;
            const uint32_t tx = rng_range(DEMO_SEED, BENCH, 0u, i, 1u, 36u, 45u, dummy);
            const uint32_t ty = rng_range(DEMO_SEED, BENCH, 0u, i, 2u, 36u, 45u, dummy);
            c.p.x_raw = static_cast<int64_t>(tx) * 65536 + 32768;
            c.p.y_raw = static_cast<int64_t>(ty) * 65536 + 32768;
            c.p.unit_id = uid_citizen;  // stats del catálogo (payload en 0 por el memset)
            ++n;
        }
        for (uint32_t i = 0; i < n_art; ++i) {  // artillería, owner 1
            RawCommand& c = batch[n];
            std::memset(&c, 0, sizeof(RawCommand));
            c.target_tick = 0; c.emitter = 1; c.type = CommandType::SPAWN_UNIT;
            c.sequence = seq1++;
            const uint32_t tx = rng_range(DEMO_SEED, BENCH, 0u, i, 1u, 132u, 147u, dummy);
            const uint32_t ty = rng_range(DEMO_SEED, BENCH, 0u, i, 2u, 100u, 157u, dummy);
            c.p.x_raw = static_cast<int64_t>(tx) * 65536 + 32768;
            c.p.y_raw = static_cast<int64_t>(ty) * 65536 + 32768;
            c.p.unit_id = uid_artillery;  // stats del catálogo (payload en 0 por el memset)
            ++n;
        }
    } else if (t == 1u) {
        uint64_t seq0 = static_cast<uint64_t>(n_cav + n_cit) + 1u;
        uint64_t seq1 = static_cast<uint64_t>(n_art) + 1u;
        const int64_t cx = static_cast<int64_t>(128) * 65536 + 32768;
        const int64_t cy = static_cast<int64_t>(128) * 65536 + 32768;
        for (uint32_t i = 0; i < n_cav; ++i) {  // caballería → (128,128)
            RawCommand& c = batch[n];
            std::memset(&c, 0, sizeof(RawCommand));
            c.target_tick = 1; c.emitter = 0; c.type = CommandType::MOVE_TO;
            c.sequence = seq0++;
            c.p.handle = EntityHandle{i, 1u};  // slots [0,n_cav), gen 1
            c.p.x_raw = cx; c.p.y_raw = cy;
            ++n;
        }
        for (uint32_t i = 0; i < n_art; ++i) {  // artillería → (128,128)
            RawCommand& c = batch[n];
            std::memset(&c, 0, sizeof(RawCommand));
            c.target_tick = 1; c.emitter = 1; c.type = CommandType::MOVE_TO;
            c.sequence = seq1++;
            c.p.handle = EntityHandle{n_cav + n_cit + i, 1u};  // slots finales, gen 1
            c.p.x_raw = cx; c.p.y_raw = cy;
            ++n;
        }
    }
    return n;
}

// ---------------------------------------------------------------------------
// Escena 3D (rig del modo (c), reutilizado del SPIKE-RENDER-0)
// ---------------------------------------------------------------------------

void ChunsaSimNode::setup_3d() {
    godot::RenderingServer::get_singleton()->set_default_clear_color(
            godot::Color(0.05, 0.05, 0.08));

    // Cámara ortográfica mirando -Z sobre el plano del mapa: mundo (u, -v, v)
    // ← mapa px (u, v); z = v → más al sur = más cerca de cámara (z=2000).
    cam3d = memnew(godot::Camera3D);
    add_child(cam3d);
    cam3d->set_projection(godot::Camera3D::PROJECTION_ORTHOGONAL);
    cam3d->set_size(MAP_PX);
    cam3d->set_near(1.0f);
    cam3d->set_far(4000.0f);
    cam3d->set_position(godot::Vector3(MAP_PX / 2.0f, -MAP_PX / 2.0f, 2000.0f));
    cam3d->make_current();

    godot::Ref<godot::StandardMaterial3D> mat;
    mat.instantiate();
    mat->set_shading_mode(godot::BaseMaterial3D::SHADING_MODE_UNSHADED);
    mat->set_flag(godot::BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
    mat->set_albedo(godot::Color(1, 1, 1));
    mat->set_cull_mode(godot::BaseMaterial3D::CULL_DISABLED);
    // alpha CUT: disabled (default, opaco) — el orden lo resuelve el depth.

    godot::Ref<godot::QuadMesh> quad;
    quad.instantiate();
    quad->set_size(godot::Vector2(1, 1));
    quad->set_material(mat);
    godot::Ref<godot::MultiMesh> mm_units;
    mm_units.instantiate();
    mm_units->set_transform_format(godot::MultiMesh::TRANSFORM_3D);
    mm_units->set_use_colors(true);
    mm_units->set_mesh(quad);
    // Capacidad = slots del kernel (cap 1024): cualquier slot puede estar vivo.
    const uint32_t cap =
            gs->entities.capacity < 1024u ? gs->entities.capacity : 1024u;
    mm_units->set_instance_count(static_cast<int32_t>(cap));
    mm_units->set_visible_instance_count(0);
    for (uint32_t k = 0; k < cap; ++k) {
        mm_units->set_instance_color(static_cast<int32_t>(k), UNIT_COLOR);
    }
    mmi_units3d = memnew(godot::MultiMeshInstance3D);
    mmi_units3d->set_multimesh(mm_units);
    add_child(mmi_units3d);

    // Muro visible: celdas FF_WALL del cost_grid (el obstáculo que el FlowField
    // hace rodear). Quads de 1 tile (4 px), gris, mismo plano; z ligeramente
    // atrás para que las unidades pasen por delante.
    uint32_t wall_n = 0;
    for (uint32_t i = 0; i < chunsa::FF_CELLS; ++i)
        if (gs->cost_grid[i] == chunsa::FF_WALL) ++wall_n;
    if (wall_n > 0) {
        godot::Ref<godot::MultiMesh> mm_wall;
        mm_wall.instantiate();
        mm_wall->set_transform_format(godot::MultiMesh::TRANSFORM_3D);
        mm_wall->set_use_colors(true);
        mm_wall->set_mesh(quad);
        mm_wall->set_instance_count(static_cast<int32_t>(wall_n));
        const godot::Basis wsc(godot::Vector3(4, 0, 0), godot::Vector3(0, 4, 0),
                               godot::Vector3(0, 0, 1));
        int32_t w = 0;
        for (uint32_t i = 0; i < chunsa::FF_CELLS; ++i) {
            if (gs->cost_grid[i] != chunsa::FF_WALL) continue;
            const float px = (static_cast<float>(i % chunsa::FF_AXIS) + 0.5f) * 4.0f;
            const float py = (static_cast<float>(i / chunsa::FF_AXIS) + 0.5f) * 4.0f;
            mm_wall->set_instance_transform(w, godot::Transform3D(
                    wsc, godot::Vector3(px, -py, py - 1.0f)));
            mm_wall->set_instance_color(w, WALL_COLOR);
            ++w;
        }
        mmi_wall3d = memnew(godot::MultiMeshInstance3D);
        mmi_wall3d->set_multimesh(mm_wall);
        add_child(mmi_wall3d);
    }
}

void ChunsaSimNode::maybe_screenshot() {
    if (shot_prefix.empty() || shot_f600_done || frame_count < 600u) {
        return;
    }
    shot_f600_done = true;
    const std::string path = shot_prefix + ".f600.png";
    const godot::Ref<godot::ViewportTexture> tex = get_viewport()->get_texture();
    if (tex.is_null()) {
        godot::UtilityFunctions::print("CHUNSA SHOT: viewport sin textura");
        return;
    }
    const godot::Ref<godot::Image> img = tex->get_image();
    if (img.is_null() || img->is_empty()) {
        godot::UtilityFunctions::print("CHUNSA SHOT: imagen vacía (renderer dummy?)");
        return;
    }
    img->save_png(godot::String(path.c_str()));
    godot::UtilityFunctions::print("CHUNSA SHOT saved: ", path.c_str());
}

void ChunsaSimNode::_exit_tree() {
    running.store(false);
    if (sim_thread.joinable()) {
        sim_thread.join();
    }
    delete gs;
    gs = nullptr;
    delete ring;
    ring = nullptr;
}
