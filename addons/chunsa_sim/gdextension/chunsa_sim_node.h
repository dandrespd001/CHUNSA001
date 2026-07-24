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
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include <chunsa/game_state.hpp>
#include <chunsa/snapshot_ring.hpp>

namespace godot {
class MultiMeshInstance3D;
class Camera3D;
class Font;
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
        uint32_t generation[1024]; // generación del handle del slot
        uint8_t owner[1024];    // 0..7
        uint8_t unit_class[1024]; // 0=infantry 1=cavalry 2=artillery 3=citizen
        uint8_t fleeing[1024];  // 1 = en pánico
        int32_t hp[1024];
        int32_t max_hp[1024];
        uint8_t entity_kind[1024]; // 0=unidad, 1=edificio
        uint32_t building_id[1024];
        uint32_t build_progress[1024];
        uint16_t bld_anchor_tx[1024];
        uint16_t bld_anchor_ty[1024];
        uint32_t build_target[1024]; // BUILD_NO_TARGET si el ciudadano está libre
        // Sprint 1.2: estado de producción/tech por slot. Se conserva el
        // layout POR-SLOT; la UI solo necesita la cabeza de la cola, pero se
        // copia la cola completa para no dejar basura en el snapshot.
        uint32_t prod_queue[1024][chunsa::PROD_QUEUE_CAP];
        uint8_t prod_count[1024];
        uint32_t prod_progress[1024];
        int64_t rally_x[1024];
        int64_t rally_y[1024];
        uint8_t rally_set[1024];
        uint32_t research_tech[1024];
        uint32_t research_progress[1024];

        // Sprint 1.2: estado escalar del jugador 0 para el HUD.
        int64_t stock_a;
        int64_t stock_b;
        int64_t stock_me;
        uint8_t player_epoch;
        int32_t pop_used;

        // Último receipt del mailbox del jugador 0. Es feedback de
        // presentación; la aceptación/rechazo autoritativa sigue en kernel.
        uint64_t last_receipt_sequence;
        uint32_t last_receipt_tick;
        uint16_t last_receipt_result;
    };

private:
    static constexpr float MAP_PX = 1024.0f;  // 256 tiles × 4 px
    static constexpr float ZOOM_MIN = 300.0f;
    static constexpr float ZOOM_MAX = 1200.0f;
    static constexpr uint32_t CONTROL_GROUPS = 10;
    static constexpr uint32_t ORDER_MARKERS_MAX = 32;

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
    chunsa::BuildingId bid_settlement_center = chunsa::INVALID_BUILDING_ID;
    chunsa::BuildingId bid_forum_center = chunsa::INVALID_BUILDING_ID;
    chunsa::BuildingId bid_chariotry_stable = chunsa::INVALID_BUILDING_ID;
    chunsa::BuildingId bid_castra_barracks = chunsa::INVALID_BUILDING_ID;
    chunsa::BuildingId bid_buildable = chunsa::INVALID_BUILDING_ID;
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
    godot::MultiMeshInstance3D* mmi_buildings3d = nullptr;
    godot::MultiMeshInstance3D* mmi_ghost3d = nullptr;
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
    uint32_t selection_generation[1024] = {};
    uint32_t control_group_slots[CONTROL_GROUPS][1024] = {};
    uint32_t control_group_generations[CONTROL_GROUPS][1024] = {};
    uint16_t control_group_counts[CONTROL_GROUPS] = {};
    int32_t last_group_number = -1;
    std::chrono::steady_clock::time_point last_group_activation{};
    bool dragging = false;
    bool camera_dragging = false;
    godot::Vector2 camera_drag_start;
    float camera_drag_origin_px = MAP_PX / 2.0f;
    float camera_drag_origin_py = MAP_PX / 2.0f;
    float camera_center_px = MAP_PX / 2.0f;
    float camera_center_py = MAP_PX / 2.0f;
    bool pan_up = false;
    bool pan_down = false;
    bool pan_left = false;
    bool pan_right = false;
    bool minimap_dragging = false;
    godot::Vector2 order_marker_pos[ORDER_MARKERS_MAX] = {};
    float order_marker_ttl[ORDER_MARKERS_MAX] = {};
    godot::Vector2 drag_start;
    godot::Vector2 cursor_screen;
    bool have_cursor = false;
    bool placement_mode = false;
    bool placement_input_captured = false;
    bool rally_mode = false;
    bool research_mode = false;
    uint64_t last_feedback_sequence = 0;
    uint8_t last_feedback_epoch = 0;

    void sim_loop();  // cuerpo del hilo de simulación (20 Hz)

    void setup_3d();               // rig del modo (c), reutilizado del spike
    void render_interpolated();    // cada frame: lerp(prev, curr, alpha)
    void maybe_screenshot();
    bool screen_to_tile(const godot::Vector2& screen, int64_t& tx,
                        int64_t& ty) const;
    bool is_static_wall(int64_t tx, int64_t ty) const;
    bool placement_valid(chunsa::BuildingId building_id, int64_t tx,
                         int64_t ty) const;
    void enqueue_place_building(int64_t tx, int64_t ty);
    uint32_t enqueue_build_assignments(int64_t tx, int64_t ty);
    int32_t selected_building_slot() const;
    void enqueue_selected_action(uint32_t action_index, bool research);
    void enqueue_rally(int64_t tx, int64_t ty);
    void enqueue_epoch_up();
    void cycle_buildable_building();
    bool screen_to_map(const godot::Vector2& screen, float& px, float& py) const;
    void clamp_camera_center();
    void set_camera_center(float px, float py);
    void set_camera_zoom(float size, const godot::Vector2* anchor_screen = nullptr);
    void pan_camera_from_keyboard(double delta);
    void recenter_from_minimap(const godot::Vector2& screen);
    godot::Rect2 minimap_rect() const;
    godot::Rect2 minimap_world_rect() const;
    godot::Rect2 epoch_button_rect() const;
    godot::Rect2 selection_panel_rect() const;
    int32_t selected_count() const;
    int32_t selected_single_building_slot() const;
    bool selected_slot_is_current(uint32_t slot) const;
    godot::String slot_display_name(uint32_t slot) const;
    godot::String catalog_name(const char* name, uint16_t bytes) const;
    bool handle_hud_press(const godot::Vector2& screen);
    void recover_control_group(uint32_t group_number);
    void assign_control_group(uint32_t group_number);
    void add_order_marker(float px, float py);
    void draw_minimap(const godot::Ref<godot::Font>& font,
                      const godot::Color& text);
    void draw_selection_panel(const godot::Ref<godot::Font>& font,
                              const godot::Color& text,
                              const godot::Color& muted);
    void draw_world_overlay(const godot::Ref<godot::Font>& font,
                            const godot::Color& text);
    void update_pan_key(godot::Key code, bool pressed);

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
    void _draw() override;
    void _process(double delta) override;
    void _input(const godot::Ref<godot::InputEvent>& event) override;
    void _exit_tree() override;
};
