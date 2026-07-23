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
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/variant/vector2.hpp>

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
    // Sprint 0.3: owner/unit_class/fleeing por slot para el coloreado por
    // bando + pánico (se interpolan posiciones; el color se lee de curr).
    struct DemoSnapshot {
        uint32_t tick;
        uint32_t capacity;      // gs->entities.capacity (cap a 1024)
        float x[1024];
        float y[1024];
        uint8_t alive[1024];    // 1 = slot vivo este snapshot
        uint8_t owner[1024];    // 0..7
        uint8_t unit_class[1024]; // 0=infantry 1=cavalry 2=artillery 3=citizen
        uint8_t fleeing[1024];  // 1 = en pánico
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

    // Sprint 0.4: catálogo de datos (CHDB). `catalog_storage` posee el catálogo
    // (RAII) y vive tanto como el nodo; `gs->catalog` apunta a su interior. Los
    // uid_* se resuelven una vez en _ready() (record_id → UnitId).
    chunsa::DataCatalogStorageV1 catalog_storage;
    chunsa::UnitId uid_cavalry = chunsa::INVALID_UNIT_ID;
    chunsa::UnitId uid_citizen = chunsa::INVALID_UNIT_ID;
    chunsa::UnitId uid_artillery = chunsa::INVALID_UNIT_ID;
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
    godot::MultiMeshInstance3D* mmi_wall3d = nullptr;
    godot::Camera3D* cam3d = nullptr;

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

    void sim_loop();  // cuerpo del hilo de simulación (20 Hz)

    void setup_3d();               // rig del modo (c), reutilizado del spike
    void render_interpolated();    // cada frame: lerp(prev, curr, alpha)
    void maybe_screenshot();

protected:
    static void _bind_methods();

public:
    ChunsaSimNode() = default;
    ~ChunsaSimNode() override = default;

    // Escenario de demo Sprint 0.2 (FlowField): unidades marchando a un goal
    // rodeando el muro. Ya no se usa en sim_loop (reemplazado por el showcase
    // del Sprint 0.3); se conserva como referencia del patrón rng/comandos.
    uint32_t build_flow_batch(chunsa::RawCommand* batch, uint32_t t);

    // Escenario de demo Sprint 0.3 (showcase): dos ejércitos (caballería
    // owner 0 vs artillería owner 1) convergen en (128,128) — combate RPS +
    // pánico visibles — mientras ciudadanos owner 0 recolectan (economía).
    uint32_t build_showcase_batch(chunsa::RawCommand* batch, uint32_t t);

    void _ready() override;
    void _process(double delta) override;
    void _input(const godot::Ref<godot::InputEvent>& event) override;
    void _exit_tree() override;
};
