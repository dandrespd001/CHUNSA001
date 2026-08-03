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
#include <chunsa/ai_stub.hpp>
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
        uint32_t map_w;         // gs->vision.w (presentación, <= VIS_AXIS)
        uint32_t map_h;         // gs->vision.h (presentación, <= VIS_AXIS)
        uint64_t visible[chunsa::VIS_WORDS];   // visión de owner 0
        uint64_t explored[chunsa::VIS_WORDS];  // exploración acumulada de owner 0
        float x[1024];
        float y[1024];
        int64_t x_raw[1024];
        int64_t y_raw[1024];
        uint8_t alive[1024];    // 1 = slot vivo este snapshot
        uint32_t generation[1024]; // generación del handle del slot
        uint8_t owner[1024];    // 0..7
        uint8_t unit_class[1024]; // 0=infantry 1=cavalry 2=artillery 3=citizen
        uint8_t fleeing[1024];  // 1 = en pánico
        int32_t hp[1024];
        int32_t max_hp[1024];
        int32_t attack[1024];       // daño base de la unidad
        int32_t range_mt[1024];     // alcance en mili-tiles (1000 = 1 tile)
        int32_t speed_mtpt[1024];   // velocidad en mili-tiles por tick
        uint8_t entity_kind[1024]; // 0=unidad, 1=edificio
        uint32_t unit_id[1024];
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

        // Sprint 1.6B: economía visible para la presentación. Los depósitos
        // conservan el layout del kernel (slots fijos hasta ECO_MAX_DEPOSITS)
        // y el resto es estado económico POR-SLOT.
        uint32_t n_deposits;
        int64_t dep_x_raw[chunsa::ECO_MAX_DEPOSITS];
        int64_t dep_y_raw[chunsa::ECO_MAX_DEPOSITS];
        int32_t dep_remaining[chunsa::ECO_MAX_DEPOSITS];
        uint8_t dep_resource_idx[chunsa::ECO_MAX_DEPOSITS];
        int32_t eco_carry[1024];
        uint8_t eco_carry_resource[1024];
        uint8_t eco_state[1024]; // 0=SEEK 1=HARVEST 2=RETURN
        // Sprint 1.8H: tarea explicita del ciudadano (SPEC-004 §22). Hacia
        // falta para contar OCIOSOS de verdad: con solo eco_state, un aldeano
        // andando hacia un sitio se contaba como ocioso.
        uint8_t citizen_task[1024];
        uint32_t eco_assigned_deposit[1024];

        // Sprint 1.8C: stock completo del jugador 0. El índice es el slot
        // independiente del recurso que expone el catálogo.
        int64_t stock[chunsa::RESOURCE_COUNT];
        uint8_t player_epoch;
        uint8_t epoch_initial;
        int32_t pop_used;
        uint64_t player_techs[chunsa::TECH_WORDS];
        uint64_t player_caps[chunsa::CAP_WORDS];
        chunsa::CivId player_civ;
        uint8_t game_over;
        uint8_t winner;

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
    static constexpr uint32_t FOG_BLOCK_TILES = 8;
    static constexpr uint32_t FOG_BLOCK_AXIS = 32;
    static constexpr uint32_t FOG_BLOCK_COUNT = FOG_BLOCK_AXIS * FOG_BLOCK_AXIS;
    static constexpr float TILE_PX = 4.0f;
    static constexpr float ENTITY_Z_BIAS =
            static_cast<float>(FOG_BLOCK_TILES) * TILE_PX;

    std::thread sim_thread;
    std::atomic<bool> running{false};
    // gs y ring en HEAP: GameState pesa ~10MB. nullptr hasta _ready() (modo
    // juego); en el editor (_ready con editor_hint) quedan a nullptr y
    // _exit_tree() los deletea sin problema.
    chunsa::SnapshotRing<DemoSnapshot>* ring = nullptr;
    chunsa::GameState* gs = nullptr;
    chunsa::AiJobBox ai_box;
    chunsa::AiRuntimeV1 ai_rt{0, 0};

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
    godot::MultiMeshInstance3D* mmi_fog3d = nullptr;
    godot::Camera3D* cam3d = nullptr;

    // Selección/órdenes del jugador (Sprint 0.3+): el input llega en el hilo
    // principal (_input); sim_loop corre en su propio hilo. `pending_player_commands`
    // es la única sección compartida entre hilos → protegida por `input_mutex`.
    // `is_selected` SOLO la tocan el hilo principal (_input escribe, render lee):
    // no necesita mutex.
    std::mutex input_mutex;
    std::vector<chunsa::RawCommand> pending_player_commands;
    struct CommandPresentationPrediction {
        uint64_t sequence;
        std::string detail_utf8;
    };
    std::vector<CommandPresentationPrediction> command_predictions;
    uint64_t next_player_sequence = 1000000ull;
    bool is_selected[1024] = {};
    uint32_t selection_generation[1024] = {};
    uint32_t control_group_slots[CONTROL_GROUPS][1024] = {};
    uint32_t control_group_generations[CONTROL_GROUPS][1024] = {};
    uint16_t control_group_counts[CONTROL_GROUPS] = {};
    int32_t last_group_number = -1;
    std::chrono::steady_clock::time_point last_group_activation{};
    bool dragging = false;
    // Sprint 1.15: ranura del ultimo aldeano ocioso al que salto la tecla
    // PUNTO. Es lo que hace que pulsar repetidamente RECORRA los ociosos en
    // vez de volver siempre al primero.
    uint32_t last_idle_slot = 0;
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
    // Página visible de la barra de comandos. La aritmética y las reglas de
    // borde viven en command_panel_view.hpp; aquí solo se conserva el estado
    // de interacción del adaptador.
    uint32_t command_page = 0u;
    // Índice por ResourceFamilyV1; el valor 0 (Invalid) no se usa.
    bool resource_family_expanded[8] = {};
    uint64_t last_feedback_sequence = 0;
    uint8_t last_feedback_epoch = 0;

    void sim_loop();  // cuerpo del hilo de simulación (20 Hz)

    void setup_3d();               // rig del modo (c), reutilizado del spike
    void update_fog_from_snapshot();  // colores del velo sólo en snapshot nuevo
    void render_interpolated();    // cada frame: lerp(prev, curr, alpha)
    void maybe_screenshot();
    bool screen_to_tile(const godot::Vector2& screen, int64_t& tx,
                        int64_t& ty) const;
    bool is_static_wall(int64_t tx, int64_t ty) const;
    bool placement_valid(chunsa::BuildingId building_id, int64_t tx,
                         int64_t ty) const;
    void enqueue_place_building(int64_t tx, int64_t ty);
    uint32_t enqueue_build_assignments(int64_t tx, int64_t ty);
    uint32_t enqueue_gather_orders(int64_t x_raw, int64_t y_raw);
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
    static constexpr uint32_t LABEL_CLAIMS_MAX = 256;
    godot::Vector2 label_claims[LABEL_CLAIMS_MAX] = {};
    uint32_t label_claims_used = 0;
    bool claim_label_slot(const godot::Vector2& pos);
    float ui_scale() const;
    godot::Rect2 minimap_rect() const;
    godot::Rect2 minimap_world_rect() const;
    godot::Rect2 resource_hud_rect() const;
    godot::Rect2 building_catalog_rect() const;
    godot::Rect2 epoch_button_rect() const;
    godot::Rect2 selection_panel_rect() const;
    bool building_available(chunsa::BuildingId building_id) const;
    uint32_t available_building_count() const;
    int32_t selected_count() const;
    int32_t selected_single_building_slot() const;
    bool presentation_entity_visible(uint32_t slot) const;
    bool presentation_tile_visible(int64_t x_raw, int64_t y_raw) const;
    bool selected_slot_is_current(uint32_t slot) const;
    godot::String slot_display_name(uint32_t slot) const;
    godot::String catalog_name(const char* name, uint16_t bytes) const;
    godot::String resource_display_name(uint32_t resource_id) const;
    godot::String resource_family_display_name(
            chunsa::ResourceFamilyV1 family) const;
    godot::String resource_nature_display_name(
            chunsa::ResourceNatureV1 nature) const;
    uint32_t resource_id_for_stock_index(uint8_t stock_index) const;
    godot::String unit_display_name(uint32_t unit_id) const;
    godot::String tech_display_name(uint32_t tech_id) const;
    godot::String unit_class_display_name(uint8_t unit_class) const;
    godot::String cost_summary(const int32_t* costs) const;
    godot::String missing_summary(const int32_t* costs) const;
    godot::String presentation_rejection_explanation(
            chunsa::CommandType type, uint32_t building_slot,
            uint32_t item_id) const;
    void append_stock_details(godot::String& details,
                              const int32_t* costs,
                              const int64_t* stock) const;
    void remember_command_prediction(uint64_t sequence,
                                     const godot::String& detail);
    const CommandPresentationPrediction* command_prediction(
            uint64_t sequence) const;
    bool handle_hud_press(const godot::Vector2& screen);
    void recover_control_group(uint32_t group_number);
    void assign_control_group(uint32_t group_number);
    void add_order_marker(float px, float py);
    void draw_minimap(const godot::Ref<godot::Font>& font,
                      const godot::Color& text);
    void draw_resource_hud(const godot::Ref<godot::Font>& font,
                           const godot::Color& text,
                           const godot::Color& muted);
    void draw_building_catalog(const godot::Ref<godot::Font>& font,
                               const godot::Color& text,
                               const godot::Color& muted);
    void draw_selection_panel(const godot::Ref<godot::Font>& font,
                              const godot::Color& text,
                              const godot::Color& muted);
    void draw_world_overlay(const godot::Ref<godot::Font>& font,
                            const godot::Color& text);

    // Sprint 1.8H (SPEC-006 Parte V): la barra de comandos de abajo, rejilla
    // 3x5 con las teclas por posición (Q W E R T / A S D F G / Z X C V B).
    // Construir, entrenar e investigar comparten panel, botón y tooltip.
    enum class PanelKind : uint8_t { Build = 0, Train = 1, Research = 2, EpochUp = 3 };
    struct PanelSlot {
        PanelKind kind;
        uint32_t id;
    };
    static constexpr uint32_t PANEL_COLS = 5u;
    static constexpr uint32_t PANEL_ROWS = 3u;
    static constexpr uint32_t PANEL_SLOTS = PANEL_COLS * PANEL_ROWS;

    uint32_t selected_citizen_count() const;
    uint32_t issue_move_orders(int64_t x_raw, int64_t y_raw);
    mutable int32_t epoch_cost_cache[chunsa::RESOURCE_COUNT] = {};
    godot::Rect2 command_bar_rect() const;
    void collect_command_items(std::vector<PanelSlot>& out) const;
    uint32_t command_item_count() const;
    uint32_t command_page_count() const;
    void normalize_command_page();
    uint32_t collect_command_slots(PanelSlot* out, uint32_t max);
    void draw_command_bar(const godot::Ref<godot::Font>& font,
                          const godot::Color& text,
                          const godot::Color& muted);
    void activate_command_slot(const PanelSlot& slot);
    const int32_t* slot_costs(const PanelSlot& slot) const;
    godot::String slot_title(const PanelSlot& slot) const;
    int64_t slot_time_ticks(const PanelSlot& slot) const;
    godot::String slot_blocker(const PanelSlot& slot) const;

    // Sprint 1.8H (SPEC-006 Parte V). El icono de recurso pasa SIEMPRE por
    // aquí: es el único punto que hay que cambiar el día que exista arte.
    void draw_resource_icon(const godot::Ref<godot::Font>& font,
                            const godot::Vector2& center,
                            float radius,
                            uint32_t resource_id);
    // Tooltip al estilo AoE2: un renglón por recurso, icono y número, con el
    // que falta en rojo. Se coloca solo para no salirse de la pantalla.
    void draw_cost_tooltip(const godot::Ref<godot::Font>& font,
                           const godot::Vector2& anchor,
                           const godot::String& title,
                           const godot::String& hotkey,
                           const int32_t* costs,
                           int64_t time_ticks,
                           const godot::String& blocker);
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

    // Escenario jugable Sprint 1.4: owner 0 humano contra owner 1 IA, ambos
    // con centro, cuartel, ejército y aldeanos reales del catálogo.
    uint32_t build_skirmish_batch(chunsa::RawCommand* batch, uint32_t t);

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
