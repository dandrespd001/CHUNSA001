#include "chunsa_sim_node.h"

// chunsa_sim — ChunsaSimNode (Sprint 0.2, render de producción). Escenario
// synthetic_movement_v1@1 (seed fija) avanzando a 20 Hz en un hilo; el hilo
// de presentación lee el último snapshot publicado sin bloquear al writer e
// interpola entre snapshots para movimiento fluido a 60+ FPS.
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
#include <vector>

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/os.hpp>
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
    // map_tiles_y, seed
    const chunsa::MatchConfig01A cfg{demo_units + 16, 1, 1, 20, 20, 256, 256,
                                     DEMO_SEED};
    gs = new chunsa::GameState();
    chunsa::gs_init(*gs, cfg);
    ring = new chunsa::SnapshotRing<DemoSnapshot>();
    ring->init();

    setup_3d();

    set_process(true);
    running.store(true);
    sim_thread = std::thread(&ChunsaSimNode::sim_loop, this);
}

void ChunsaSimNode::sim_loop() {
    using clock = std::chrono::steady_clock;
    std::vector<chunsa::RawCommand> batch(std::max<uint32_t>(demo_units, 700u));

    auto next_tick = clock::now();
    while (running.load(std::memory_order_relaxed)) {
        const uint32_t t = gs->tick;
        const uint32_t n =
                chunsa::build_human_batch(batch, t, demo_units, DEMO_SEED, false);
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
            }
            ring->publish();
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
        ++k;
    }
    mm->set_visible_instance_count(k);
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
