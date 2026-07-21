#pragma once

// chunsa_sim — ChunsaSimNode (Sprint 0.2, render de producción): kernel
// determinista en un hilo propio (20 Hz), snapshots publicados vía
// SnapshotRing (SPEC-001 §9). Presentación: modo (c) de ADR-009 — mundo 3D
// minimal con Camera3D ortográfica + depth buffer (rig reutilizado del
// SPIKE-RENDER-0) — con interpolación suave a 60 FPS entre ticks. El core
// sigue sin conocer Godot; kernel y ring conservan su semántica (solo el
// consumidor interpola: presentación pura, no toca determinismo).

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

#include <godot_cpp/classes/node2d.hpp>

#include <chunsa/game_state.hpp>
#include <chunsa/snapshot_ring.hpp>

namespace godot {
class MultiMeshInstance3D;
class Camera3D;
}  // namespace godot

class ChunsaSimNode : public godot::Node2D {
    GDCLASS(ChunsaSimNode, godot::Node2D);

public:
    // Snapshot para presentación, layout POR-SLOT (contrato del Arquitecto):
    // el índice ES el slot de entidad (identidad estable mientras la unidad
    // vive) → permite interpolar entre snapshots. Posiciones en tiles
    // (pos raw Q47.16 / 65536); válidas si alive[i] != 0.
    struct DemoSnapshot {
        uint32_t tick;
        uint32_t capacity;      // gs->entities.capacity (cap a 1024)
        float x[1024];
        float y[1024];
        uint8_t alive[1024];    // 1 = slot vivo este snapshot
    };

private:
    static constexpr float MAP_PX = 1024.0f;  // 256 tiles × 4 px

    std::thread sim_thread;
    std::atomic<bool> running{false};
    // gs y ring en HEAP: GameState pesa ~10MB. nullptr hasta _ready() (modo
    // juego); en el editor (_ready con editor_hint) quedan a nullptr y
    // _exit_tree() los deletea sin problema.
    chunsa::SnapshotRing<DemoSnapshot>* ring = nullptr;
    chunsa::GameState* gs = nullptr;

    // Interpolación (contrato): dos snapshots + instante de llegada de curr.
    DemoSnapshot snap_prev{};
    DemoSnapshot snap_curr{};
    bool have_prev = false;
    bool have_curr = false;
    std::chrono::steady_clock::time_point curr_arrival{};
    uint32_t alive_in_curr = 0;  // para el print "cada 100 ticks"
    uint32_t last_reported_tick = UINT32_MAX;

    uint32_t demo_units = 600;   // CHUNSA_UNITS (2..1024, default 600)
    uint64_t frame_count = 0;
    std::string shot_prefix;     // CHUNSA_SHOT: prefijo para el PNG
    bool shot_f600_done = false;

    godot::MultiMeshInstance3D* mmi_units3d = nullptr;
    godot::Camera3D* cam3d = nullptr;

    void sim_loop();  // cuerpo del hilo de simulación (20 Hz)

    void setup_3d();               // rig del modo (c), reutilizado del spike
    void render_interpolated();    // cada frame: lerp(prev, curr, alpha)
    void maybe_screenshot();

protected:
    static void _bind_methods();

public:
    ChunsaSimNode() = default;
    ~ChunsaSimNode() override = default;

    void _ready() override;
    void _process(double delta) override;
    void _exit_tree() override;
};
