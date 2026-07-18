#include "chunsa_sim_node.h"

// chunsa_sim — ChunsaSimNode (Sprint 0.2). Escenario synthetic_movement_v1@1
// (600 unidades, seed fija) avanzando a 20 Hz en un hilo; el hilo de
// presentación lee el último snapshot publicado sin bloquear al writer.

#include <chrono>
#include <vector>

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <chunsa/driver.hpp>
#include <chunsa/step.hpp>

namespace {
constexpr uint32_t DEMO_UNITS = 600;
constexpr uint64_t DEMO_SEED = 20260716ull;
constexpr auto TICK_PERIOD = std::chrono::milliseconds(50);  // 20 Hz
}  // namespace

void ChunsaSimNode::_bind_methods() {
    // Intencionadamente vacío: la demo no expone API al scripting.
}

void ChunsaSimNode::_ready() {
    if (godot::Engine::get_singleton()->is_editor_hint()) {
        return;  // en el editor no arrancamos simulación
    }

    // max_entities, player_count, human_input_delay_ticks,
    // max_future_command_ticks, checksum_every_ticks, map_tiles_x,
    // map_tiles_y, seed
    const chunsa::MatchConfig01A cfg{604, 1, 1, 20, 20, 256, 256, DEMO_SEED};
    gs = new chunsa::GameState();
    chunsa::gs_init(*gs, cfg);
    ring = new chunsa::SnapshotRing<DemoSnapshot>();
    ring->init();

    set_process(true);
    running.store(true);
    sim_thread = std::thread(&ChunsaSimNode::sim_loop, this);
}

void ChunsaSimNode::sim_loop() {
    using clock = std::chrono::steady_clock;
    std::vector<chunsa::RawCommand> batch(700);  // prealocado: fuera del bucle

    auto next_tick = clock::now();
    while (running.load(std::memory_order_relaxed)) {
        const uint32_t t = gs->tick;
        const uint32_t n = chunsa::build_human_batch(batch, t, DEMO_UNITS, DEMO_SEED, false);
        chunsa::step(*gs, batch.data(), n);

        DemoSnapshot* s = ring->begin_write();
        if (s != nullptr) {
            s->tick = t;
            uint32_t k = 0;
            for (uint32_t i = 0; i < gs->entities.capacity && k < 1024u; ++i) {
                if (gs->entities.alive[i] == 0u) {
                    continue;
                }
                s->x[k] = static_cast<float>(gs->pos_x[i]) / 65536.0f;
                s->y[k] = static_cast<float>(gs->pos_y[i]) / 65536.0f;
                ++k;
            }
            s->count = k;
            ring->publish();
        }

        next_tick += TICK_PERIOD;
        std::this_thread::sleep_until(next_tick);
    }
}

void ChunsaSimNode::_process(double /*delta*/) {
    if (ring == nullptr) {
        return;
    }
    auto h = ring->acquire_latest();
    if (h.valid) {
        last = *h.data;
        ring->release(h);
        queue_redraw();
        if (last.tick % 100u == 0u && last.tick != last_reported_tick) {
            last_reported_tick = last.tick;
            godot::UtilityFunctions::print("CHUNSA tick=", last.tick, " units=", last.count);
        }
    }
}

void ChunsaSimNode::_draw() {
    // Mapa de 256 tiles × 4 px = 1024×1024.
    draw_rect(godot::Rect2(0, 0, 1024, 1024), godot::Color(0.05, 0.05, 0.08));
    for (uint32_t k = 0; k < last.count; ++k) {
        draw_circle(godot::Vector2(last.x[k] * 4.0f, last.y[k] * 4.0f), 2.0f,
                    godot::Color(0.2, 0.9, 0.9));
    }
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
