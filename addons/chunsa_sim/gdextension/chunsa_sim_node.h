#pragma once

// chunsa_sim — ChunsaSimNode (Sprint 0.2): kernel determinista en un hilo
// propio (20 Hz), snapshots publicados vía SnapshotRing (SPEC-001 §9) y
// pintados en un Node2D. El core es header-only puro; este adaptador es la
// única capa que conoce Godot.

#include <atomic>
#include <cstdint>
#include <thread>

#include <godot_cpp/classes/node2d.hpp>

#include <chunsa/game_state.hpp>
#include <chunsa/snapshot_ring.hpp>

class ChunsaSimNode : public godot::Node2D {
    GDCLASS(ChunsaSimNode, godot::Node2D);

public:
    // Snapshot para presentación: posiciones ya en tiles (pos raw Q47.16 / 65536).
    struct DemoSnapshot {
        uint32_t tick;
        uint32_t count;
        float x[1024];
        float y[1024];
    };

private:
    std::thread sim_thread;
    std::atomic<bool> running{false};
    // gs y ring en HEAP: GameState pesa ~10MB. nullptr hasta _ready() (modo
    // juego); en el editor (_ready con editor_hint) quedan a nullptr y
    // _exit_tree() los deletea sin problema.
    chunsa::SnapshotRing<DemoSnapshot>* ring = nullptr;
    chunsa::GameState* gs = nullptr;
    DemoSnapshot last{};
    uint32_t last_reported_tick = UINT32_MAX;  // para el print "cada 100 ticks"

    void sim_loop();  // cuerpo del hilo de simulación (20 Hz)

protected:
    static void _bind_methods();

public:
    ChunsaSimNode() = default;
    ~ChunsaSimNode() override = default;

    void _ready() override;
    void _process(double delta) override;
    void _draw() override;
    void _exit_tree() override;
};
