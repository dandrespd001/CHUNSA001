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
#include <cmath>
#include <cstring>
#include <iterator>
#include <vector>

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/theme_db.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <chunsa/driver.hpp>
#include <chunsa/step.hpp>

namespace {
constexpr uint64_t DEMO_SEED = 20260716ull;
constexpr auto TICK_PERIOD = std::chrono::milliseconds(50);  // 20 Hz
constexpr int64_t MAP_TILES = 256;
const godot::Color UNIT_COLOR(0.2, 0.9, 0.9);
const godot::Color WALL_COLOR(0.5, 0.5, 0.55);

const char* receipt_result_text(uint16_t result) {
    using chunsa::RejectReason;
    switch (static_cast<RejectReason>(result)) {
        case RejectReason::ACCEPTED: return "aceptado";
        case RejectReason::MALFORMED: return "payload malformado";
        case RejectReason::INVALID_ENTITY: return "entidad inválida";
        case RejectReason::NOT_OWNER: return "no es propio";
        case RejectReason::ILLEGAL_STATE: return "estado ilegal (época/stock/cola)";
        case RejectReason::OUT_OF_WINDOW: return "fuera de ventana";
        case RejectReason::POOL_EXHAUSTED: return "pool agotado";
        case RejectReason::RATE_LIMITED: return "rate limited";
        case RejectReason::SEQUENCE_REJECTED: return "secuencia rechazada";
        default: return "resultado desconocido";
    }
}
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
    // Sprint 1.2 (SPEC-004 §10.3): delay vuelve a 1 (valor de PRODUCCIÓN). Ya
    // no hace falta el hack delay=0 de Sprint 1.1: command_effective_tick
    // ahora da eff=0 a cualquier comando con target_tick==0 ingerido en el
    // PRIMER Step (t==0) SIN importar el delay — los PLACE_BUILDING de setup
    // (los dos centros del showcase, build_showcase_batch en t==0) siguen
    // entrando con target_tick=0 y activando la exención de SPEC-004 §4.1.2/
    // §4.3 exactamente igual que antes. La UI sigue usando el mismo command
    // stream; no hay camino privilegiado (el contrato del host es: NUNCA
    // ingerir input de jugador en la llamada a step() de t==0 — ver
    // command_effective_tick en step.hpp).
    const chunsa::MatchConfig01A cfg{demo_units + 16, 2, 1, 20, 20, 256, 256,
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
        // Sprint 1.2 (SPEC-004 §12.2, desviación 9 del RESULT de K2): fijar
        // player_epoch a la época inicial del catálogo. Sin esto queda en 0 y
        // el gate §12.4 rechazaría todo PLACE_BUILDING/TRAIN manual del jugador
        // (los datos reales del slice son época >= 3) — regresión de la demo de
        // colocación del Sprint 1.1. El adaptador enlaza gs->catalog a mano
        // (no vía gs_bind_catalog), así que la llamada va aquí explícita.
        chunsa::gs_init_epoch_from_catalog(*gs);
        uid_cavalry = chunsa::catalog_find_unit(
            *gs->catalog, "egipto:chariot_warrior", std::strlen("egipto:chariot_warrior"));
        uid_citizen = chunsa::catalog_find_unit(
            *gs->catalog, "egipto:work_crew", std::strlen("egipto:work_crew"));
        uid_artillery = chunsa::catalog_find_unit(
            *gs->catalog, "rome:ballista_crew", std::strlen("rome:ballista_crew"));
        bid_settlement_center = chunsa::catalog_find_building(
            *gs->catalog, "egipto:settlement_center",
            std::strlen("egipto:settlement_center"));
        bid_forum_center = chunsa::catalog_find_building(
            *gs->catalog, "rome:forum_center", std::strlen("rome:forum_center"));
        bid_chariotry_stable = chunsa::catalog_find_building(
            *gs->catalog, "egipto:chariotry_stable",
            std::strlen("egipto:chariotry_stable"));
        bid_castra_barracks = chunsa::catalog_find_building(
            *gs->catalog, "rome:castra_barracks",
            std::strlen("rome:castra_barracks"));
        bid_buildable = chunsa::catalog_find_building(
            *gs->catalog, "egipto:shena_granary",
            std::strlen("egipto:shena_granary"));
        if (bid_buildable == chunsa::INVALID_BUILDING_ID) {
            for (uint32_t i = 0; i < gs->catalog->building_count; ++i) {
                if (gs->catalog->buildings[i].constructible != 0u) {
                    bid_buildable = i;
                    break;
                }
            }
        }
        godot::UtilityFunctions::print(
            "CHUNSA catálogo OK: cav_id=", static_cast<int64_t>(uid_cavalry),
            " cit_id=", static_cast<int64_t>(uid_citizen),
            " art_id=", static_cast<int64_t>(uid_artillery),
            " building_count=", static_cast<int64_t>(gs->catalog->building_count),
            " settlement_id=", static_cast<int64_t>(bid_settlement_center),
            " forum_id=", static_cast<int64_t>(bid_forum_center),
            " stable_id=", static_cast<int64_t>(bid_chariotry_stable),
            " barracks_id=", static_cast<int64_t>(bid_castra_barracks),
            " buildable_id=", static_cast<int64_t>(bid_buildable));
        if (bid_settlement_center == chunsa::INVALID_BUILDING_ID ||
            bid_forum_center == chunsa::INVALID_BUILDING_ID ||
            bid_chariotry_stable == chunsa::INVALID_BUILDING_ID ||
            bid_castra_barracks == chunsa::INVALID_BUILDING_ID ||
            bid_buildable == chunsa::INVALID_BUILDING_ID) {
            godot::UtilityFunctions::print(
                "CHUNSA ERROR: catálogo sin centros iniciales o edificio construible");
            return;
        }
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
        // La primera llamada a step(t==0) solo admite setup generado por el
        // escenario. Una orden humana capturada durante ese arranque se
        // conserva para t==1 y no usa accidentalmente la exención de setup.
        if (t != 0u) {
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
                s->generation[i] = gs->entities.generation[i];
                s->x[i] = static_cast<float>(gs->pos_x[i]) / 65536.0f;
                s->y[i] = static_cast<float>(gs->pos_y[i]) / 65536.0f;
                s->owner[i] = gs->owner[i];
                s->unit_class[i] = gs->unit_class[i];
                s->fleeing[i] = gs->fleeing[i];
                s->hp[i] = gs->hp[i];
                s->max_hp[i] = gs->max_hp[i];
                s->entity_kind[i] = gs->entity_kind[i];
                s->building_id[i] = gs->building_id[i];
                s->build_progress[i] = gs->build_progress[i];
                s->bld_anchor_tx[i] = gs->bld_anchor_tx[i];
                s->bld_anchor_ty[i] = gs->bld_anchor_ty[i];
                s->build_target[i] = gs->build_target[i];
                for (uint32_t k = 0; k < chunsa::PROD_QUEUE_CAP; ++k) {
                    s->prod_queue[i][k] = gs->prod_queue[i][k];
                }
                s->prod_count[i] = gs->prod_count[i];
                s->prod_progress[i] = gs->prod_progress[i];
                s->rally_x[i] = gs->rally_x[i];
                s->rally_y[i] = gs->rally_y[i];
                s->rally_set[i] = gs->rally_set[i];
                s->research_tech[i] = gs->research_tech[i];
                s->research_progress[i] = gs->research_progress[i];
            }
            s->stock_a = gs->player_stock[0][0];
            s->stock_b = gs->player_stock[0][1];
            s->stock_me = gs->player_stock[0][2];
            s->player_epoch = gs->player_epoch[0];
            s->pop_used = gs->pop_used[0];
            const chunsa::ReceiptMailbox& mailbox = gs->mailbox[0];
            if (mailbox.count > 0u) {
                const uint32_t last =
                        (mailbox.head + mailbox.count - 1u) % chunsa::MAILBOX_CAP;
                s->last_receipt_sequence = mailbox.ring[last].sequence;
                s->last_receipt_tick = mailbox.ring[last].processed_tick;
                s->last_receipt_result =
                        static_cast<uint16_t>(mailbox.ring[last].result);
            } else {
                s->last_receipt_sequence = UINT64_MAX;
                s->last_receipt_tick = 0;
                s->last_receipt_result =
                        static_cast<uint16_t>(chunsa::RejectReason::ACCEPTED);
            }
            ring->publish();
        }

        // Diagnóstico del showcase (Sprint 0.3) cada 100 ticks, desde el hilo
        // de simulación: vivos por clase + stock de Alimentos del owner 0.
        if (t % 100u == 0u) {
            uint32_t n_cav_alive = 0, n_art_alive = 0, n_cit_alive = 0,
                     n_buildings_alive = 0;
            for (uint32_t i = 0; i < gs->entities.capacity; ++i) {
                if (gs->entities.alive[i] == 0u) continue;
                if (gs->entity_kind[i] == 1u) ++n_buildings_alive;
                else if (gs->unit_class[i] == 1u) ++n_cav_alive;
                else if (gs->unit_class[i] == 2u) ++n_art_alive;
                else if (gs->unit_class[i] == 3u) ++n_cit_alive;
            }
            godot::UtilityFunctions::print("CHUNSA cav=", n_cav_alive,
                                           " art=", n_art_alive,
                                           " citizens=", n_cit_alive,
                                           " buildings=", n_buildings_alive,
                                           " stock_A=", gs->player_stock[0][0]);
        }

        next_tick += TICK_PERIOD;
        std::this_thread::sleep_until(next_tick);
    }
}

void ChunsaSimNode::_process(double delta) {
    ++frame_count;
    pan_camera_from_keyboard(delta);
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
                    if (is_selected[i] &&
                        (snap_curr.alive[i] == 0u ||
                         snap_curr.generation[i] != selection_generation[i])) {
                        is_selected[i] = false;
                    }
                }
                if (snap_curr.tick % 100u == 0u &&
                    snap_curr.tick != last_reported_tick) {
                    last_reported_tick = snap_curr.tick;
                    godot::UtilityFunctions::print("CHUNSA tick=", snap_curr.tick,
                                                   " units=", alive_in_curr);
                }
                if (snap_curr.last_receipt_sequence != UINT64_MAX &&
                    snap_curr.last_receipt_sequence >= 1000000ull &&
                    snap_curr.last_receipt_sequence != last_feedback_sequence) {
                    last_feedback_sequence = snap_curr.last_receipt_sequence;
                    godot::UtilityFunctions::print(
                            "CHUNSA comando seq=", last_feedback_sequence,
                            " resultado=",
                            receipt_result_text(snap_curr.last_receipt_result),
                            " tick=", snap_curr.last_receipt_tick);
                }
                if (last_feedback_epoch != 0u &&
                    snap_curr.player_epoch != last_feedback_epoch) {
                    godot::UtilityFunctions::print(
                            "CHUNSA EPOCH_UP aceptado: época=",
                            static_cast<int64_t>(snap_curr.player_epoch));
                }
                last_feedback_epoch = snap_curr.player_epoch;
            }
        }
    }
    for (uint32_t i = 0; i < ORDER_MARKERS_MAX; ++i) {
        if (order_marker_ttl[i] > 0.0f) {
            order_marker_ttl[i] -= static_cast<float>(delta);
        }
    }
    render_interpolated();
    queue_redraw();
    maybe_screenshot();
}

// HUD de Sprint 1.3. Es una capa de presentación: lee únicamente el snapshot
// publicado y nunca consulta/muta GameState desde el hilo principal.
void ChunsaSimNode::_draw() {
    if (!have_curr || godot::ThemeDB::get_singleton() == nullptr) return;
    const godot::Ref<godot::Font> font =
            godot::ThemeDB::get_singleton()->get_fallback_font();
    if (font.is_null()) return;

    const godot::Color text(0.95, 0.98, 1.0, 1.0);
    const godot::Color muted(0.72, 0.82, 0.9, 1.0);
    draw_world_overlay(font, text);

    draw_rect(godot::Rect2(godot::Vector2(14, 14), godot::Vector2(720, 118)),
              godot::Color(0.02, 0.04, 0.08, 0.9));

    godot::String resources = godot::String("CHUNSA  A ") +
            godot::String::num_int64(snap_curr.stock_a) + "  B " +
            godot::String::num_int64(snap_curr.stock_b) + "  Me " +
            godot::String::num_int64(snap_curr.stock_me) + "    Época " +
            godot::String::num_int64(snap_curr.player_epoch) + "    Población " +
            godot::String::num_int64(snap_curr.pop_used) + "/200";
    draw_string(font, godot::Vector2(28, 42), resources,
                 static_cast<godot::HorizontalAlignment>(0), -1, 18, text);

    godot::String controls =
            "1..9 grupos | WASD/flechas: cámara | rueda: zoom | MMB: pan";
    draw_string(font, godot::Vector2(28, 68), controls,
                 static_cast<godot::HorizontalAlignment>(0), -1, 15, muted);

    if (snap_curr.last_receipt_sequence != UINT64_MAX &&
        snap_curr.last_receipt_sequence >= 1000000ull) {
        const godot::String feedback = "Último comando #" +
                godot::String::num_int64(
                        static_cast<int64_t>(snap_curr.last_receipt_sequence)) +
                ": " + receipt_result_text(snap_curr.last_receipt_result);
        draw_string(font, godot::Vector2(28, 94), feedback,
                     static_cast<godot::HorizontalAlignment>(0), -1, 14,
                     snap_curr.last_receipt_result ==
                                     static_cast<uint16_t>(chunsa::RejectReason::ACCEPTED)
                             ? godot::Color(0.45, 1.0, 0.55, 1.0)
                             : godot::Color(1.0, 0.65, 0.35, 1.0));
    }

    const godot::Rect2 epoch_rect = epoch_button_rect();
    draw_rect(epoch_rect, godot::Color(0.12, 0.3, 0.5, 0.95));
    draw_string(font, epoch_rect.position + godot::Vector2(12, 21), "E: SUBIR ÉPOCA",
                static_cast<godot::HorizontalAlignment>(0), -1, 14, text);

    draw_selection_panel(font, text, muted);
    draw_minimap(font, text);
}

// Selección, colocación y órdenes del jugador (Sprint 1.1). Todo lo que sale
// de aquí es un RawCommand: la validación real y la mutación siguen siendo del
// kernel. El ghost solo hace una validación local de mapa/muros/solape para
// feedback inmediato.
void ChunsaSimNode::_input(const godot::Ref<godot::InputEvent>& event) {
    if (cam3d == nullptr) return;

    godot::Ref<godot::InputEventKey> key = event;
    if (!key.is_null()) {
        const godot::Key physical = key->get_physical_keycode();
        const godot::Key logical = key->get_keycode();
        const godot::Key code = physical != godot::KEY_NONE ? physical : logical;
        update_pan_key(code, key->is_pressed());
        if (!key->is_pressed() || key->is_echo()) return;

        const int32_t group_number = static_cast<int32_t>(code) -
                static_cast<int32_t>(godot::KEY_1) + 1;
        if (group_number >= 1 && group_number <= 9) {
            if (key->is_ctrl_pressed()) {
                assign_control_group(static_cast<uint32_t>(group_number));
            } else {
                recover_control_group(static_cast<uint32_t>(group_number));
            }
            return;
        }
        if (code == godot::KEY_B) {
            placement_mode = !placement_mode;
            placement_input_captured = false;
            godot::UtilityFunctions::print(
                placement_mode ? "CHUNSA construir: ghost activo"
                               : "CHUNSA construir: ghost cancelado");
            return;
        }
        if (code == godot::KEY_ESCAPE &&
            (placement_mode || rally_mode || research_mode)) {
            placement_mode = false;
            placement_input_captured = false;
            rally_mode = false;
            research_mode = false;
            godot::UtilityFunctions::print("CHUNSA acción contextual cancelada");
            return;
        }
        if (code == godot::KEY_N && placement_mode) {
            cycle_buildable_building();
            return;
        }
        if (code == godot::KEY_T) {
            research_mode = !research_mode;
            godot::UtilityFunctions::print(
                    research_mode ? "CHUNSA panel TECH activo"
                                   : "CHUNSA panel TRAIN activo");
            return;
        }
        if (code == godot::KEY_R) {
            rally_mode = !rally_mode;
            godot::UtilityFunctions::print(
                    rally_mode ? "CHUNSA rally: selecciona un punto con clic"
                                : "CHUNSA rally cancelado");
            return;
        }
        if (code == godot::KEY_E) {
            enqueue_epoch_up();
            return;
        }
    }

    godot::Ref<godot::InputEventMouseMotion> motion = event;
    if (!motion.is_null()) {
        cursor_screen = motion->get_position();
        have_cursor = true;
        if (minimap_dragging) {
            recenter_from_minimap(cursor_screen);
            return;
        }
        if (camera_dragging) {
            const godot::Vector2 viewport_size =
                    get_viewport()->get_visible_rect().size;
            if (viewport_size.y > 0.0f) {
                const float units_per_pixel = cam3d->get_size() / viewport_size.y;
                const godot::Vector2 delta_screen = cursor_screen - camera_drag_start;
                set_camera_center(camera_drag_origin_px -
                                          delta_screen.x * units_per_pixel,
                                  camera_drag_origin_py -
                                          delta_screen.y * units_per_pixel);
            }
            return;
        }
        return;
    }

    godot::Ref<godot::InputEventMouseButton> mb = event;
    if (mb.is_null()) return;
    cursor_screen = mb->get_position();
    have_cursor = true;

    const godot::MouseButton button = mb->get_button_index();
    if (button == godot::MouseButton::MOUSE_BUTTON_WHEEL_UP ||
        button == godot::MouseButton::MOUSE_BUTTON_WHEEL_DOWN) {
        if (mb->is_pressed() && !minimap_rect().has_point(cursor_screen)) {
            const float factor =
                    button == godot::MouseButton::MOUSE_BUTTON_WHEEL_UP ? 0.85f : 1.15f;
            set_camera_zoom(cam3d->get_size() * factor, &cursor_screen);
        }
        return;
    }

    if (button == godot::MouseButton::MOUSE_BUTTON_MIDDLE) {
        if (mb->is_pressed()) {
            camera_dragging = true;
            camera_drag_start = cursor_screen;
            camera_drag_origin_px = camera_center_px;
            camera_drag_origin_py = camera_center_py;
        } else {
            camera_dragging = false;
        }
        return;
    }

    if (button != godot::MouseButton::MOUSE_BUTTON_LEFT &&
        button != godot::MouseButton::MOUSE_BUTTON_RIGHT) {
        return;
    }

    const uint32_t cap = snap_curr.capacity < 1024u ? snap_curr.capacity : 1024u;

    if (button == godot::MouseButton::MOUSE_BUTTON_LEFT) {
        if (mb->is_pressed()) {
            if (handle_hud_press(mb->get_position())) return;
            if (placement_mode) {
                placement_input_captured = true;
                int64_t tx = 0;
                int64_t ty = 0;
                const bool local_valid = screen_to_tile(mb->get_position(), tx, ty) &&
                        placement_valid(bid_buildable, tx, ty);
                if (local_valid) {
                    enqueue_place_building(tx, ty);
                    placement_mode = false;
                    godot::UtilityFunctions::print(
                        "CHUNSA PLACE_BUILDING enqueued tile=", tx, ",", ty,
                        " building_id=", static_cast<int64_t>(bid_buildable));
                } else {
                    godot::UtilityFunctions::print(
                        "CHUNSA PLACE_BUILDING local reject tile=", tx, ",", ty);
                }
                return;
            }
            if (rally_mode) {
                int64_t tx = 0;
                int64_t ty = 0;
                if (screen_to_tile(mb->get_position(), tx, ty)) {
                    enqueue_rally(tx, ty);
                    rally_mode = false;
                } else {
                    godot::UtilityFunctions::print(
                            "CHUNSA SET_RALLY local reject: punto fuera del mundo");
                }
                return;
            }
            dragging = true;
            drag_start = mb->get_position();
            return;
        }
        if (minimap_dragging) {
            recenter_from_minimap(mb->get_position());
            minimap_dragging = false;
            return;
        }
        if (placement_input_captured) {
            placement_input_captured = false;
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
            if (snap_curr.alive[i] == 0u) continue;
            if (snap_curr.owner[i] != 0u) continue;
            if (snap_curr.entity_kind[i] == 0u && snap_curr.unit_class[i] > 3u) continue;

            float px = snap_curr.x[i] * 4.0f;
            float py = snap_curr.y[i] * 4.0f;
            float hit_radius = 20.0f;
            if (snap_curr.entity_kind[i] == 1u &&
                snap_curr.building_id[i] < catalog_storage.catalog().building_count) {
                const chunsa::BuildingDefinitionV1& def =
                        catalog_storage.catalog().buildings[snap_curr.building_id[i]];
                px = (static_cast<float>(snap_curr.bld_anchor_tx[i]) +
                      static_cast<float>(def.footprint_w) * 0.5f) * 4.0f;
                py = (static_cast<float>(snap_curr.bld_anchor_ty[i]) +
                      static_cast<float>(def.footprint_h) * 0.5f) * 4.0f;
                hit_radius = std::max(hit_radius,
                                      static_cast<float>(std::max(def.footprint_w,
                                                                  def.footprint_h)) * 2.0f);
            }
            const godot::Vector3 world_pos(px, -py, py);
            const godot::Vector2 screen_pos = cam3d->unproject_position(world_pos);

            if (is_click) {
                const float dx = screen_pos.x - drag_end.x;
                const float dy = screen_pos.y - drag_end.y;
                const float d2 = dx * dx + dy * dy;
                if (d2 < hit_radius * hit_radius && d2 < best_d2) {
                    best_d2 = d2;
                    best_i = i;
                }
            } else {
                if (screen_pos.x >= rmin.x && screen_pos.x <= rmax.x &&
                    screen_pos.y >= rmin.y && screen_pos.y <= rmax.y) {
                    is_selected[i] = true;
                    selection_generation[i] = snap_curr.generation[i];
                }
            }
        }
        if (is_click && best_i != UINT32_MAX) {
            is_selected[best_i] = true;
            selection_generation[best_i] = snap_curr.generation[best_i];
        }
        return;
    }

    if (button == godot::MouseButton::MOUSE_BUTTON_RIGHT && mb->is_pressed()) {
        int64_t tile_x = 0;
        int64_t tile_y = 0;
        if (screen_to_tile(mb->get_position(), tile_x, tile_y) &&
            enqueue_build_assignments(tile_x, tile_y) > 0u) {
            return;
        }

        float world_px = 0.0f;
        float world_py = 0.0f;
        if (!screen_to_map(mb->get_position(), world_px, world_py)) return;
        const int64_t x_raw = static_cast<int64_t>((world_px / 4.0f) * 65536.0f);
        const int64_t y_raw = static_cast<int64_t>((world_py / 4.0f) * 65536.0f);

        bool issued = false;
        std::lock_guard<std::mutex> lock(input_mutex);
        for (uint32_t i = 0; i < cap; ++i) {
            if (!selected_slot_is_current(i)) continue;
            if (snap_curr.owner[i] != 0u) continue;  // pudo morir
            if (snap_curr.entity_kind[i] != 0u || snap_curr.unit_class[i] > 2u) continue;
            chunsa::RawCommand c;
            std::memset(&c, 0, sizeof(c));
            c.target_tick = 0;
            c.emitter = 0;
            c.type = chunsa::CommandType::MOVE_TO;
            c.sequence = next_player_sequence++;
            c.p.handle = chunsa::EntityHandle{i, snap_curr.generation[i]};
            c.p.x_raw = x_raw;
            c.p.y_raw = y_raw;
            pending_player_commands.push_back(c);
            issued = true;
        }
        if (issued) add_order_marker(world_px, world_py);
    }
}

bool ChunsaSimNode::screen_to_map(const godot::Vector2& screen, float& px,
                                  float& py) const {
    if (cam3d == nullptr) return false;
    const godot::Vector3 origin = cam3d->project_ray_origin(screen);
    px = origin.x;
    py = -origin.y;
    return std::isfinite(px) && std::isfinite(py);
}

void ChunsaSimNode::clamp_camera_center() {
    if (cam3d == nullptr) return;
    const godot::Vector2 viewport_size = get_viewport()->get_visible_rect().size;
    if (viewport_size.x <= 0.0f || viewport_size.y <= 0.0f) return;
    const float half_h = cam3d->get_size() * 0.5f;
    const float half_w = half_h * viewport_size.x / viewport_size.y;
    camera_center_px = half_w >= MAP_PX * 0.5f
            ? MAP_PX * 0.5f
            : std::clamp(camera_center_px, half_w, MAP_PX - half_w);
    camera_center_py = half_h >= MAP_PX * 0.5f
            ? MAP_PX * 0.5f
            : std::clamp(camera_center_py, half_h, MAP_PX - half_h);
}

void ChunsaSimNode::set_camera_center(float px, float py) {
    if (cam3d == nullptr) return;
    camera_center_px = px;
    camera_center_py = py;
    clamp_camera_center();
    const godot::Vector3 old_position = cam3d->get_position();
    cam3d->set_position(godot::Vector3(camera_center_px, -camera_center_py,
                                       old_position.z));
}

void ChunsaSimNode::set_camera_zoom(float size,
                                    const godot::Vector2* anchor_screen) {
    if (cam3d == nullptr) return;
    float anchor_px = 0.0f;
    float anchor_py = 0.0f;
    const bool have_anchor = anchor_screen != nullptr &&
            screen_to_map(*anchor_screen, anchor_px, anchor_py);
    cam3d->set_size(std::clamp(size, ZOOM_MIN, ZOOM_MAX));
    if (have_anchor) {
        float after_px = 0.0f;
        float after_py = 0.0f;
        if (screen_to_map(*anchor_screen, after_px, after_py)) {
            camera_center_px += anchor_px - after_px;
            camera_center_py += anchor_py - after_py;
        }
    }
    set_camera_center(camera_center_px, camera_center_py);
}

void ChunsaSimNode::pan_camera_from_keyboard(double delta) {
    if (cam3d == nullptr || delta <= 0.0) return;
    const float x = (pan_right ? 1.0f : 0.0f) - (pan_left ? 1.0f : 0.0f);
    const float y = (pan_down ? 1.0f : 0.0f) - (pan_up ? 1.0f : 0.0f);
    if (x == 0.0f && y == 0.0f) return;
    constexpr float PAN_SPEED = 560.0f;
    set_camera_center(camera_center_px + x * PAN_SPEED * static_cast<float>(delta),
                      camera_center_py + y * PAN_SPEED * static_cast<float>(delta));
}

godot::Rect2 ChunsaSimNode::minimap_rect() const {
    const godot::Vector2 viewport_size = get_viewport()->get_visible_rect().size;
    const float map_side = std::clamp(
            std::min(viewport_size.x * 0.25f, viewport_size.y * 0.38f), 160.0f,
            230.0f);
    const godot::Vector2 panel_size(map_side + 20.0f, map_side + 42.0f);
    return godot::Rect2(godot::Vector2(viewport_size.x - panel_size.x - 16.0f,
                                      viewport_size.y - panel_size.y - 16.0f),
                        panel_size);
}

godot::Rect2 ChunsaSimNode::minimap_world_rect() const {
    const godot::Rect2 outer = minimap_rect();
    const float side = outer.size.x - 20.0f;
    return godot::Rect2(outer.position + godot::Vector2(10.0f, 26.0f),
                        godot::Vector2(side, side));
}

godot::Rect2 ChunsaSimNode::epoch_button_rect() const {
    const godot::Vector2 viewport_size = get_viewport()->get_visible_rect().size;
    const float width = 165.0f;
    const float x = std::max(28.0f, std::min(550.0f, viewport_size.x - width - 16.0f));
    return godot::Rect2(godot::Vector2(x, 48.0f), godot::Vector2(width, 30.0f));
}

godot::Rect2 ChunsaSimNode::selection_panel_rect() const {
    const godot::Vector2 viewport_size = get_viewport()->get_visible_rect().size;
    const float width = std::min(560.0f, std::max(340.0f, viewport_size.x - 28.0f));
    const float height = 236.0f;
    return godot::Rect2(godot::Vector2(14.0f,
                                      std::max(128.0f, viewport_size.y - height - 16.0f)),
                        godot::Vector2(width, height));
}

int32_t ChunsaSimNode::selected_count() const {
    if (!have_curr) return 0;
    const uint32_t cap = snap_curr.capacity < 1024u ? snap_curr.capacity : 1024u;
    int32_t count = 0;
    for (uint32_t i = 0; i < cap; ++i) {
        if (selected_slot_is_current(i)) ++count;
    }
    return count;
}

int32_t ChunsaSimNode::selected_single_building_slot() const {
    if (selected_count() != 1) return -1;
    const uint32_t cap = snap_curr.capacity < 1024u ? snap_curr.capacity : 1024u;
    for (uint32_t i = 0; i < cap; ++i) {
        if (selected_slot_is_current(i) && snap_curr.entity_kind[i] == 1u &&
            snap_curr.owner[i] == 0u) {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

bool ChunsaSimNode::selected_slot_is_current(uint32_t slot) const {
    return slot < 1024u && is_selected[slot] && have_curr &&
            snap_curr.alive[slot] != 0u &&
            snap_curr.generation[slot] == selection_generation[slot];
}

godot::String ChunsaSimNode::catalog_name(const char* name, uint16_t bytes) const {
    return name == nullptr ? godot::String() : godot::String::utf8(name, bytes);
}

godot::String ChunsaSimNode::slot_display_name(uint32_t slot) const {
    if (slot >= 1024u || !have_curr) return "desconocido";
    const chunsa::DataCatalogV1& catalog = catalog_storage.catalog();
    if (snap_curr.entity_kind[slot] == 1u) {
        if (snap_curr.building_id[slot] < catalog.building_count &&
            catalog.building_names != nullptr) {
            return catalog_name(catalog.building_names[snap_curr.building_id[slot]].record_id_utf8,
                                catalog.building_names[snap_curr.building_id[slot]].record_id_bytes);
        }
        return "edificio";
    }
    switch (snap_curr.unit_class[slot]) {
        case 0u: return "infantería";
        case 1u: return "caballería";
        case 2u: return "artillería";
        case 3u: return "ciudadano";
        default: return "unidad";
    }
}

void ChunsaSimNode::update_pan_key(godot::Key code, bool pressed) {
    switch (code) {
        case godot::KEY_W:
        case godot::KEY_UP: pan_up = pressed; break;
        case godot::KEY_S:
        case godot::KEY_DOWN: pan_down = pressed; break;
        case godot::KEY_A:
        case godot::KEY_LEFT: pan_left = pressed; break;
        case godot::KEY_D:
        case godot::KEY_RIGHT: pan_right = pressed; break;
        default: break;
    }
}

void ChunsaSimNode::recenter_from_minimap(const godot::Vector2& screen) {
    const godot::Rect2 world_rect = minimap_world_rect();
    if (!world_rect.has_point(screen)) return;
    const float nx = std::clamp((screen.x - world_rect.position.x) / world_rect.size.x,
                                0.0f, 1.0f);
    const float ny = std::clamp((screen.y - world_rect.position.y) / world_rect.size.y,
                                0.0f, 1.0f);
    set_camera_center(nx * MAP_PX, ny * MAP_PX);
}

bool ChunsaSimNode::handle_hud_press(const godot::Vector2& screen) {
    if (minimap_rect().has_point(screen)) {
        recenter_from_minimap(screen);
        minimap_dragging = true;
        return true;
    }
    if (epoch_button_rect().has_point(screen)) {
        enqueue_epoch_up();
        return true;
    }
    if (selected_count() == 0) return false;
    const godot::Rect2 panel = selection_panel_rect();
    if (!panel.has_point(screen)) return false;
    const int32_t selected = selected_single_building_slot();
    if (selected < 0) return true;
    const float tab_y = panel.position.y + 101.0f;
    if (screen.y >= tab_y && screen.y <= tab_y + 26.0f) {
        if (screen.x >= panel.position.x + 16.0f &&
            screen.x <= panel.position.x + 91.0f) {
            research_mode = false;
        } else if (screen.x >= panel.position.x + 97.0f &&
                   screen.x <= panel.position.x + 172.0f) {
            research_mode = true;
        }
        return true;
    }
    const chunsa::BuildingDefinitionV1& def =
            catalog_storage.catalog().buildings[snap_curr.building_id[selected]];
    const uint32_t count = research_mode ? def.research_count : def.train_count;
    const float button_y = panel.position.y + 136.0f;
    const float button_w = 126.0f;
    const float button_h = 30.0f;
    const float gap = 6.0f;
    if (screen.y >= button_y && screen.y <= button_y + button_h) {
        const float rel_x = screen.x - panel.position.x - 16.0f;
        const int32_t column = static_cast<int32_t>(std::floor(rel_x / (button_w + gap)));
        if (column >= 0 && column < 4) {
            const float local_x = rel_x - static_cast<float>(column) * (button_w + gap);
            if (local_x >= 0.0f && local_x <= button_w) {
                const uint32_t action = static_cast<uint32_t>(column);
                if (action < count) enqueue_selected_action(action, research_mode);
                return true;
            }
        }
    }
    if (screen.y >= button_y + button_h + gap &&
        screen.y <= button_y + button_h * 2.0f + gap) {
        const float rel_x = screen.x - panel.position.x - 16.0f;
        const int32_t column = static_cast<int32_t>(std::floor(rel_x / (button_w + gap)));
        if (column >= 0 && column < 4) {
            const float local_x = rel_x - static_cast<float>(column) * (button_w + gap);
            if (local_x >= 0.0f && local_x <= button_w) {
                const uint32_t action = static_cast<uint32_t>(column + 4);
                if (action < count) enqueue_selected_action(action, research_mode);
                return true;
            }
        }
    }
    return true;
}

void ChunsaSimNode::assign_control_group(uint32_t group_number) {
    if (group_number == 0u || group_number >= CONTROL_GROUPS) return;
    control_group_counts[group_number] = 0;
    const uint32_t cap = snap_curr.capacity < 1024u ? snap_curr.capacity : 1024u;
    for (uint32_t i = 0; i < cap; ++i) {
        if (!selected_slot_is_current(i)) continue;
        const uint16_t count = control_group_counts[group_number];
        if (count >= 1024u) break;
        control_group_slots[group_number][count] = i;
        control_group_generations[group_number][count] = snap_curr.generation[i];
        control_group_counts[group_number] = static_cast<uint16_t>(count + 1u);
    }
    godot::UtilityFunctions::print("CHUNSA grupo ", static_cast<int64_t>(group_number),
                                   " asignado slots=",
                                   static_cast<int64_t>(control_group_counts[group_number]));
}

void ChunsaSimNode::recover_control_group(uint32_t group_number) {
    if (group_number == 0u || group_number >= CONTROL_GROUPS) return;
    const auto now = std::chrono::steady_clock::now();
    const bool double_tap = last_group_number == static_cast<int32_t>(group_number) &&
            std::chrono::duration<float>(now - last_group_activation).count() < 0.45f;
    last_group_number = static_cast<int32_t>(group_number);
    last_group_activation = now;

    std::fill(std::begin(is_selected), std::end(is_selected), false);
    float center_x = 0.0f;
    float center_y = 0.0f;
    uint32_t valid = 0;
    const uint16_t count = control_group_counts[group_number];
    for (uint16_t k = 0; k < count; ++k) {
        const uint32_t slot = control_group_slots[group_number][k];
        if (slot >= 1024u || !have_curr || snap_curr.alive[slot] == 0u ||
            snap_curr.generation[slot] != control_group_generations[group_number][k]) {
            continue;
        }
        is_selected[slot] = true;
        selection_generation[slot] = snap_curr.generation[slot];
        center_x += snap_curr.entity_kind[slot] == 1u
                ? static_cast<float>(snap_curr.bld_anchor_tx[slot]) * 4.0f
                : snap_curr.x[slot] * 4.0f;
        center_y += snap_curr.entity_kind[slot] == 1u
                ? static_cast<float>(snap_curr.bld_anchor_ty[slot]) * 4.0f
                : snap_curr.y[slot] * 4.0f;
        ++valid;
    }
    if (double_tap && valid > 0u) {
        set_camera_center(center_x / static_cast<float>(valid),
                          center_y / static_cast<float>(valid));
    }
    godot::UtilityFunctions::print("CHUNSA grupo ", static_cast<int64_t>(group_number),
                                   " recuperado vivos=", static_cast<int64_t>(valid));
}

void ChunsaSimNode::add_order_marker(float px, float py) {
    uint32_t target = ORDER_MARKERS_MAX;
    for (uint32_t i = 0; i < ORDER_MARKERS_MAX; ++i) {
        if (order_marker_ttl[i] <= 0.0f) {
            target = i;
            break;
        }
    }
    if (target == ORDER_MARKERS_MAX) target = 0;
    order_marker_pos[target] = godot::Vector2(px, py);
    order_marker_ttl[target] = 1.0f;
}

void ChunsaSimNode::draw_minimap(const godot::Ref<godot::Font>& font,
                                 const godot::Color& text) {
    const godot::Rect2 panel = minimap_rect();
    const godot::Rect2 map = minimap_world_rect();
    const float scale = map.size.x / MAP_PX;
    draw_rect(panel, godot::Color(0.02, 0.04, 0.08, 0.94));
    draw_string(font, panel.position + godot::Vector2(12, 19), "MINIMAPA · sin fog",
                static_cast<godot::HorizontalAlignment>(0), -1, 14, text);
    draw_rect(map, godot::Color(0.13, 0.17, 0.2, 1.0));

    for (int64_t y = 32; y < 224; ++y) {
        if (!is_static_wall(128, y)) continue;
        draw_rect(godot::Rect2(map.position +
                                       godot::Vector2(128.0f * scale,
                                                      static_cast<float>(y) * 4.0f * scale),
                                   godot::Vector2(std::max(1.0f, 4.0f * scale),
                                                  std::max(1.0f, 4.0f * scale))),
                               godot::Color(0.45, 0.48, 0.52, 1.0));
    }

    const uint32_t cap = snap_curr.capacity < 1024u ? snap_curr.capacity : 1024u;
    const chunsa::DataCatalogV1& catalog = catalog_storage.catalog();
    for (uint32_t i = 0; i < cap; ++i) {
        if (snap_curr.alive[i] == 0u) continue;
        const godot::Color color = snap_curr.entity_kind[i] == 1u
                ? (snap_curr.owner[i] == 0u ? godot::Color(0.15, 0.55, 0.95, 1.0)
                                            : godot::Color(0.9, 0.25, 0.16, 1.0))
                : (snap_curr.unit_class[i] == 3u
                           ? godot::Color(1.0, 0.85, 0.2, 1.0)
                           : (snap_curr.owner[i] == 0u
                                      ? godot::Color(0.25, 0.7, 1.0, 1.0)
                                      : godot::Color(1.0, 0.35, 0.25, 1.0)));
        if (snap_curr.entity_kind[i] == 1u &&
            snap_curr.building_id[i] < catalog.building_count) {
            const chunsa::BuildingDefinitionV1& def =
                    catalog.buildings[snap_curr.building_id[i]];
            const godot::Rect2 b(
                    map.position + godot::Vector2(
                            static_cast<float>(snap_curr.bld_anchor_tx[i]) * 4.0f * scale,
                            static_cast<float>(snap_curr.bld_anchor_ty[i]) * 4.0f * scale),
                    godot::Vector2(static_cast<float>(def.footprint_w) * 4.0f * scale,
                                   static_cast<float>(def.footprint_h) * 4.0f * scale));
            draw_rect(b, color);
            if (selected_slot_is_current(i)) draw_rect(b, godot::Color(0.35, 1.0, 0.35, 1.0), false, 2.0f);
        } else {
            const godot::Vector2 p = map.position + godot::Vector2(
                    snap_curr.x[i] * 4.0f * scale, snap_curr.y[i] * 4.0f * scale);
            draw_circle(p, std::max(1.0f, 1.5f * scale), color);
        }
    }

    if (cam3d != nullptr) {
        const godot::Vector2 viewport_size = get_viewport()->get_visible_rect().size;
        if (viewport_size.y > 0.0f) {
            const float half_h = cam3d->get_size() * 0.5f;
            const float half_w = half_h * viewport_size.x / viewport_size.y;
            const float left = std::max(0.0f, camera_center_px - half_w);
            const float top = std::max(0.0f, camera_center_py - half_h);
            const float right = std::min(MAP_PX, camera_center_px + half_w);
            const float bottom = std::min(MAP_PX, camera_center_py + half_h);
            draw_rect(godot::Rect2(map.position + godot::Vector2(left * scale, top * scale),
                                   godot::Vector2((right - left) * scale,
                                                  (bottom - top) * scale)),
                      godot::Color(0.8, 0.95, 1.0, 0.95), false, 2.0f);
        }
    }
}

void ChunsaSimNode::draw_selection_panel(const godot::Ref<godot::Font>& font,
                                         const godot::Color& text,
                                         const godot::Color& muted) {
    const int32_t count = selected_count();
    if (count == 0) return;
    const godot::Rect2 panel = selection_panel_rect();
    draw_rect(panel, godot::Color(0.02, 0.04, 0.08, 0.94));
    draw_string(font, panel.position + godot::Vector2(16, 24),
                "SELECCIÓN · " + godot::String::num_int64(count),
                static_cast<godot::HorizontalAlignment>(0), -1, 17, text);

    const uint32_t cap = snap_curr.capacity < 1024u ? snap_curr.capacity : 1024u;
    int32_t first = -1;
    int64_t hp_sum = 0;
    int64_t max_hp_sum = 0;
    uint32_t class_counts[6] = {};
    uint32_t building_count = 0;
    for (uint32_t i = 0; i < cap; ++i) {
        if (!selected_slot_is_current(i)) continue;
        if (first < 0) first = static_cast<int32_t>(i);
        hp_sum += snap_curr.hp[i];
        max_hp_sum += snap_curr.max_hp[i];
        if (snap_curr.entity_kind[i] == 1u) ++building_count;
        else if (snap_curr.unit_class[i] < 6u) ++class_counts[snap_curr.unit_class[i]];
    }

    if (count == 1 && first >= 0) {
        draw_string(font, panel.position + godot::Vector2(16, 49),
                    slot_display_name(static_cast<uint32_t>(first)) + "  · owner " +
                            godot::String::num_int64(snap_curr.owner[first]),
                    static_cast<godot::HorizontalAlignment>(0), -1, 15, muted);
    } else {
        godot::String summary = "unidades";
        if (class_counts[1] > 0u) summary += "  caballería " + godot::String::num_int64(class_counts[1]);
        if (class_counts[2] > 0u) summary += "  artillería " + godot::String::num_int64(class_counts[2]);
        if (class_counts[3] > 0u) summary += "  ciudadanos " + godot::String::num_int64(class_counts[3]);
        if (class_counts[0] > 0u) summary += "  infantería " + godot::String::num_int64(class_counts[0]);
        if (building_count > 0u) summary += "  edificios " + godot::String::num_int64(building_count);
        draw_string(font, panel.position + godot::Vector2(16, 49), summary,
                    static_cast<godot::HorizontalAlignment>(0), -1, 14, muted);
    }

    const float hp_ratio = max_hp_sum > 0
            ? std::clamp(static_cast<float>(hp_sum) / static_cast<float>(max_hp_sum), 0.0f, 1.0f)
            : 0.0f;
    const godot::Rect2 hp_bar(panel.position + godot::Vector2(16, 61),
                              godot::Vector2(panel.size.x - 32.0f, 12.0f));
    draw_rect(hp_bar, godot::Color(0.18, 0.12, 0.15, 1.0));
    draw_rect(godot::Rect2(hp_bar.position,
                           godot::Vector2(hp_bar.size.x * hp_ratio, hp_bar.size.y)),
              godot::Color(0.25, 0.85, 0.35, 1.0));
    draw_string(font, panel.position + godot::Vector2(16, 87),
                "HP " + godot::String::num_int64(hp_sum) + "/" +
                        godot::String::num_int64(max_hp_sum),
                static_cast<godot::HorizontalAlignment>(0), -1, 13, text);

    const int32_t selected_building = selected_single_building_slot();
    if (selected_building >= 0 &&
        snap_curr.building_id[selected_building] < catalog_storage.catalog().building_count) {
        const chunsa::BuildingDefinitionV1& def =
                catalog_storage.catalog().buildings[snap_curr.building_id[selected_building]];
        if (def.build_time_ticks > 0u &&
            snap_curr.build_progress[selected_building] < def.build_time_ticks) {
            draw_string(font, panel.position + godot::Vector2(130, 87),
                        "Construcción " +
                                godot::String::num_int64(snap_curr.build_progress[selected_building]) +
                                "/" + godot::String::num_int64(def.build_time_ticks),
                        static_cast<godot::HorizontalAlignment>(0), -1, 13,
                        godot::Color(1.0, 0.7, 0.25, 1.0));
        }
        const godot::Rect2 train_tab(panel.position + godot::Vector2(16, 101),
                                     godot::Vector2(75, 26));
        const godot::Rect2 tech_tab(panel.position + godot::Vector2(97, 101),
                                    godot::Vector2(75, 26));
        draw_rect(train_tab, research_mode ? godot::Color(0.08, 0.12, 0.17, 1.0)
                                           : godot::Color(0.14, 0.35, 0.55, 1.0));
        draw_rect(tech_tab, research_mode ? godot::Color(0.55, 0.35, 0.12, 1.0)
                                          : godot::Color(0.08, 0.12, 0.17, 1.0));
        draw_string(font, train_tab.position + godot::Vector2(12, 18), "TRAIN",
                    static_cast<godot::HorizontalAlignment>(0), -1, 13, text);
        draw_string(font, tech_tab.position + godot::Vector2(14, 18), "TECH",
                    static_cast<godot::HorizontalAlignment>(0), -1, 13, text);

        const uint32_t action_count = research_mode ? def.research_count : def.train_count;
        const float button_w = 126.0f;
        const float button_h = 30.0f;
        const float gap = 6.0f;
        for (uint32_t k = 0; k < action_count && k < 8u; ++k) {
            const float x = panel.position.x + 16.0f +
                    static_cast<float>(k % 4u) * (button_w + gap);
            const float y = panel.position.y + 136.0f +
                    static_cast<float>(k / 4u) * (button_h + gap);
            const godot::Color button_color = research_mode
                    ? godot::Color(0.35, 0.24, 0.1, 1.0)
                    : godot::Color(0.1, 0.25, 0.4, 1.0);
            draw_rect(godot::Rect2(godot::Vector2(x, y),
                                   godot::Vector2(button_w, button_h)), button_color);
            const uint32_t id = research_mode ? def.researches[k] : def.trains[k];
            godot::String label = research_mode ? "Investigar " : "Entrenar ";
            if (research_mode && id < catalog_storage.catalog().tech_count &&
                catalog_storage.catalog().tech_names != nullptr) {
                label += catalog_name(catalog_storage.catalog().tech_names[id].record_id_utf8,
                                      catalog_storage.catalog().tech_names[id].record_id_bytes);
            } else if (!research_mode && id < catalog_storage.catalog().unit_count &&
                       catalog_storage.catalog().unit_names != nullptr) {
                label += catalog_name(catalog_storage.catalog().unit_names[id].record_id_utf8,
                                      catalog_storage.catalog().unit_names[id].record_id_bytes);
            } else {
                label += "#" + godot::String::num_int64(id);
            }
            draw_string(font, godot::Vector2(x + 7, y + 20), label.left(20),
                        static_cast<godot::HorizontalAlignment>(0), -1, 12, text);
        }
        const bool has_train = snap_curr.prod_count[selected_building] > 0u;
        const bool has_research = snap_curr.research_tech[selected_building] != chunsa::INVALID_TECH_ID;
        if (has_train || has_research) {
            godot::String queue = has_train ? "Cola: " : "Investigación: ";
            if (has_train) {
                queue += slot_display_name(selected_building) + "  " +
                        godot::String::num_int64(snap_curr.prod_count[selected_building]);
            }
            if (has_research) {
                if (has_train) queue += "  ·  ";
                queue += "tech #" + godot::String::num_int64(
                        snap_curr.research_tech[selected_building]);
            }
            draw_string(font, panel.position + godot::Vector2(16, 224), queue.left(72),
                        static_cast<godot::HorizontalAlignment>(0), -1, 12, muted);
        }
    } else {
        draw_string(font, panel.position + godot::Vector2(16, 116),
                    "Acciones: clic derecho mueve · B construye · R rally",
                    static_cast<godot::HorizontalAlignment>(0), -1, 13, muted);
    }
}

void ChunsaSimNode::draw_world_overlay(const godot::Ref<godot::Font>& font,
                                       const godot::Color& text) {
    if (cam3d == nullptr) return;
    const uint32_t cap = snap_curr.capacity < 1024u ? snap_curr.capacity : 1024u;
    const chunsa::DataCatalogV1& catalog = catalog_storage.catalog();
    for (uint32_t i = 0; i < ORDER_MARKERS_MAX; ++i) {
        if (order_marker_ttl[i] <= 0.0f) continue;
        const godot::Vector2 p = cam3d->unproject_position(godot::Vector3(
                order_marker_pos[i].x, -order_marker_pos[i].y, order_marker_pos[i].y + 18.0f));
        const float alpha = std::clamp(order_marker_ttl[i], 0.0f, 1.0f);
        const godot::Color marker(1.0, 0.9, 0.25, alpha);
        draw_circle(p, 7.0f, marker, false, 2.0f);
        draw_line(p - godot::Vector2(10, 0), p + godot::Vector2(10, 0), marker, 2.0f);
        draw_line(p - godot::Vector2(0, 10), p + godot::Vector2(0, 10), marker, 2.0f);
    }

    for (uint32_t i = 0; i < cap; ++i) {
        if (snap_curr.alive[i] == 0u || snap_curr.entity_kind[i] != 1u ||
            snap_curr.rally_set[i] == 0u || snap_curr.building_id[i] >= catalog.building_count) {
            continue;
        }
        const chunsa::BuildingDefinitionV1& def = catalog.buildings[snap_curr.building_id[i]];
        const float bx = (static_cast<float>(snap_curr.bld_anchor_tx[i]) +
                          static_cast<float>(def.footprint_w) * 0.5f) * 4.0f;
        const float by = (static_cast<float>(snap_curr.bld_anchor_ty[i]) +
                          static_cast<float>(def.footprint_h) * 0.5f) * 4.0f;
        const float rx = static_cast<float>(snap_curr.rally_x[i]) / 65536.0f * 4.0f;
        const float ry = static_cast<float>(snap_curr.rally_y[i]) / 65536.0f * 4.0f;
        const godot::Vector2 from = cam3d->unproject_position(godot::Vector3(bx, -by, by + 16.0f));
        const godot::Vector2 to = cam3d->unproject_position(godot::Vector3(rx, -ry, ry + 16.0f));
        const godot::Color rally_color = snap_curr.owner[i] == 0u
                ? godot::Color(0.25, 0.95, 0.45, 0.9)
                : godot::Color(1.0, 0.35, 0.25, 0.75);
        draw_line(from, to, rally_color, 2.0f);
        draw_line(to, to + godot::Vector2(0, -16), rally_color, 2.0f);
        draw_circle(to, 5.0f, rally_color);
    }

    for (uint32_t i = 0; i < cap; ++i) {
        if (!selected_slot_is_current(i) || snap_curr.max_hp[i] <= 0) continue;
        float px = snap_curr.x[i] * 4.0f;
        float py = snap_curr.y[i] * 4.0f;
        float width = 28.0f;
        if (snap_curr.entity_kind[i] == 1u && snap_curr.building_id[i] < catalog.building_count) {
            const chunsa::BuildingDefinitionV1& def = catalog.buildings[snap_curr.building_id[i]];
            px = (static_cast<float>(snap_curr.bld_anchor_tx[i]) +
                  static_cast<float>(def.footprint_w) * 0.5f) * 4.0f;
            py = (static_cast<float>(snap_curr.bld_anchor_ty[i]) +
                  static_cast<float>(def.footprint_h) * 0.5f) * 4.0f;
            width = std::clamp(static_cast<float>(def.footprint_w) * 4.0f, 28.0f, 72.0f);
        }
        const godot::Vector2 center = cam3d->unproject_position(
                godot::Vector3(px, -py, py + 20.0f));
        const godot::Rect2 bar(center + godot::Vector2(-width * 0.5f, -16.0f),
                               godot::Vector2(width, 5.0f));
        draw_rect(bar, godot::Color(0.12, 0.05, 0.06, 0.95));
        const float ratio = std::clamp(static_cast<float>(snap_curr.hp[i]) /
                                               static_cast<float>(snap_curr.max_hp[i]),
                                       0.0f, 1.0f);
        draw_rect(godot::Rect2(bar.position, godot::Vector2(width * ratio, bar.size.y)),
                  ratio > 0.5f ? godot::Color(0.25, 0.9, 0.35, 1.0)
                               : godot::Color(0.95, 0.35, 0.2, 1.0));
    }

    // Colas y research sobre edificios propios relevantes. El estado viene del
    // snapshot; el catálogo solo aporta nombres y tiempos estáticos.
    for (uint32_t i = 0; i < cap; ++i) {
        if (snap_curr.alive[i] == 0u || snap_curr.owner[i] != 0u ||
            snap_curr.entity_kind[i] != 1u || snap_curr.building_id[i] >= catalog.building_count) {
            continue;
        }
        const chunsa::BuildingDefinitionV1& def = catalog.buildings[snap_curr.building_id[i]];
        const bool has_train = snap_curr.prod_count[i] > 0u;
        const bool has_research = snap_curr.research_tech[i] != chunsa::INVALID_TECH_ID;
        if (!has_train && !has_research) continue;
        const float px = (static_cast<float>(snap_curr.bld_anchor_tx[i]) +
                          static_cast<float>(def.footprint_w) * 0.5f) * 4.0f;
        const float py = (static_cast<float>(snap_curr.bld_anchor_ty[i]) +
                          static_cast<float>(def.footprint_h) * 0.5f) * 4.0f;
        const godot::Vector2 label_pos =
                cam3d->unproject_position(godot::Vector3(px, -py, py + 12.0f));
        draw_rect(godot::Rect2(label_pos - godot::Vector2(5, 32),
                               godot::Vector2(230, has_research ? 48 : 26)),
                  godot::Color(0.02, 0.04, 0.08, 0.8));
        if (has_train) {
            const uint32_t uid = snap_curr.prod_queue[i][0];
            uint32_t total = 0;
            if (uid < catalog.unit_count) total = catalog.units[uid].build_time_ticks;
            godot::String line = "TRAIN " + godot::String::num_int64(uid) + " " +
                    godot::String::num_int64(snap_curr.prod_progress[i]) + "/" +
                    godot::String::num_int64(total);
            draw_string(font, label_pos - godot::Vector2(0, 14), line,
                        static_cast<godot::HorizontalAlignment>(0), -1, 13, text);
        }
        if (has_research) {
            uint32_t total = 0;
            if (snap_curr.research_tech[i] < catalog.tech_count) {
                total = catalog.techs[snap_curr.research_tech[i]].research_time_ticks;
            }
            godot::String line = "TECH " + godot::String::num_int64(
                    snap_curr.research_tech[i]) + " " +
                    godot::String::num_int64(snap_curr.research_progress[i]) + "/" +
                    godot::String::num_int64(total);
            draw_string(font, label_pos + godot::Vector2(0, 2), line,
                        static_cast<godot::HorizontalAlignment>(0), -1, 13,
                        godot::Color(1.0, 0.86, 0.35, 1.0));
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
        if (snap_curr.alive[i] == 0u || snap_curr.entity_kind[i] != 0u) {
            continue;
        }
        float fx = snap_curr.x[i];
        float fy = snap_curr.y[i];
        if (have_prev && i < pcap && snap_prev.alive[i] != 0u &&
            snap_prev.entity_kind[i] == 0u) {
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
        if (selected_slot_is_current(i))        c = godot::Color(0.3, 1.0, 0.3);    // seleccionado: verde brillante
        else if (snap_curr.unit_class[i] == 3u &&
                 snap_curr.build_target[i] != chunsa::BUILD_NO_TARGET)
                                                    c = godot::Color(1.0, 0.55, 0.1); // constructor: naranja
        else if (snap_curr.unit_class[i] == 3u) c = godot::Color(0.9, 0.85, 0.2);   // ciudadano: amarillo
        else if (snap_curr.fleeing[i] != 0u)    c = godot::Color(0.9, 0.9, 0.95);   // pánico: casi blanco
        else if (snap_curr.owner[i] == 0u)      c = godot::Color(0.2, 0.6, 0.95);   // owner 0: azul
        else                                     c = godot::Color(0.9, 0.3, 0.2);   // owner 1: rojo/naranja
        mm->set_instance_color(k, c);
        ++k;
    }
    mm->set_visible_instance_count(k);

    // Edificios: cajas con footprint real. La altura y alpha distinguen un
    // sitio en construcción de uno completo; las posiciones/anclas vienen del
    // snapshot y el catálogo aporta únicamente geometría estática.
    if (mmi_buildings3d != nullptr) {
        const godot::Ref<godot::MultiMesh> mm_buildings =
                mmi_buildings3d->get_multimesh();
        const chunsa::DataCatalogV1& catalog = catalog_storage.catalog();
        int32_t building_k = 0;
        for (uint32_t i = 0; i < cap; ++i) {
            if (snap_curr.alive[i] == 0u || snap_curr.entity_kind[i] != 1u) continue;
            if (snap_curr.building_id[i] >= catalog.building_count) continue;
            const chunsa::BuildingDefinitionV1& def =
                    catalog.buildings[snap_curr.building_id[i]];
            const float progress = def.build_time_ticks == 0u
                    ? 1.0f
                    : std::clamp(static_cast<float>(snap_curr.build_progress[i]) /
                                         static_cast<float>(def.build_time_ticks),
                                 0.0f, 1.0f);
            const float width = static_cast<float>(def.footprint_w) * 4.0f;
            const float height = static_cast<float>(def.footprint_h) * 4.0f;
            const float box_depth = 2.0f + 8.0f * progress;
            const float px = (static_cast<float>(snap_curr.bld_anchor_tx[i]) +
                              static_cast<float>(def.footprint_w) * 0.5f) * 4.0f;
            const float py = (static_cast<float>(snap_curr.bld_anchor_ty[i]) +
                              static_cast<float>(def.footprint_h) * 0.5f) * 4.0f;
            const godot::Basis sc(godot::Vector3(width, 0, 0),
                                  godot::Vector3(0, height, 0),
                                  godot::Vector3(0, 0, box_depth));
            mm_buildings->set_instance_transform(
                    building_k, godot::Transform3D(
                                       sc, godot::Vector3(px, -py, py + box_depth * 0.5f)));

            godot::Color color;
            if (selected_slot_is_current(i)) {
                color = godot::Color(0.3, 1.0, 0.3, 0.95);
            } else if (progress < 1.0f) {
                color = godot::Color(1.0, 0.68, 0.12, 0.68);
            } else if (snap_curr.owner[i] == 0u) {
                color = godot::Color(0.15, 0.55, 0.95, 1.0);
            } else {
                color = godot::Color(0.9, 0.25, 0.16, 1.0);
            }
            mm_buildings->set_instance_color(building_k, color);
            ++building_k;
        }
        mm_buildings->set_visible_instance_count(building_k);
    }

    // Ghost de colocación: solo feedback local, nunca muta GameState.
    if (mmi_ghost3d != nullptr) {
        const godot::Ref<godot::MultiMesh> mm_ghost = mmi_ghost3d->get_multimesh();
        int32_t visible = 0;
        int64_t tx = 0;
        int64_t ty = 0;
        if (placement_mode && have_cursor && bid_buildable != chunsa::INVALID_BUILDING_ID &&
            screen_to_tile(cursor_screen, tx, ty) &&
            bid_buildable < catalog_storage.catalog().building_count) {
            const chunsa::BuildingDefinitionV1& def =
                    catalog_storage.catalog().buildings[bid_buildable];
            const bool valid = placement_valid(bid_buildable, tx, ty);
            const float width = static_cast<float>(def.footprint_w) * 4.0f;
            const float height = static_cast<float>(def.footprint_h) * 4.0f;
            const godot::Basis sc(godot::Vector3(width, 0, 0),
                                  godot::Vector3(0, height, 0),
                                  godot::Vector3(0, 0, 3.0f));
            const float px = (static_cast<float>(tx) +
                              static_cast<float>(def.footprint_w) * 0.5f) * 4.0f;
            const float py = (static_cast<float>(ty) +
                              static_cast<float>(def.footprint_h) * 0.5f) * 4.0f;
            mm_ghost->set_instance_transform(
                    0, godot::Transform3D(sc, godot::Vector3(px, -py, py + 2.0f)));
            mm_ghost->set_instance_color(
                    0, valid ? godot::Color(0.2, 1.0, 0.35, 0.45)
                             : godot::Color(1.0, 0.15, 0.1, 0.45));
            visible = 1;
        }
        mm_ghost->set_visible_instance_count(visible);
    }
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

// t==0: PLACE_BUILDING de los dos centros y los dos cuarteles iniciales +
// spawns — 40% caballería (owner 0), 40% artillería (owner 1), 20% ciudadanos
// (owner 0). Los edificios de escenario usan la exención de SPEC-004 y nacen
// completos. Orden de
// emisión = orden canónico de aplicación (emitter asc, sequence asc), así la
// tabla queda determinista:
// cav en slots [0,n_cav), ciudadanos [n_cav,n_cav+n_cit), centro/stable del
// owner 0, artillería del owner 1 y foro/castra del owner 1 — todos con
// generación 1. El showcase arranca así con `buildings=4` y TRAIN/RESEARCH
// se pueden demostrar sin construir primero un cuartel.
// t==1: UN MOVE_TO por unidad de combate hacia (128,128) con los handles
// analíticos de arriba (con delay 0 los spawns y centros ya están vivos tras
// el primer Step; la cuenta de slots sigue siendo exacta).
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
        // Setup por comandos: la coordenada es el tile ancla ENTERO, no raw.
        {
            RawCommand& c = batch[n];
            std::memset(&c, 0, sizeof(RawCommand));
            c.target_tick = 0; c.emitter = 0;
            c.type = CommandType::PLACE_BUILDING;
            c.sequence = seq0++;
            c.p.x_raw = 20;
            c.p.y_raw = 128;
            c.p.unit_id = bid_settlement_center;
            ++n;
        }
        {
            RawCommand& c = batch[n];
            std::memset(&c, 0, sizeof(RawCommand));
            c.target_tick = 0; c.emitter = 1;
            c.type = CommandType::PLACE_BUILDING;
            c.sequence = seq1++;
            c.p.x_raw = 233;
            c.p.y_raw = 128;
            c.p.unit_id = bid_forum_center;
            ++n;
        }
        {
            RawCommand& c = batch[n];
            std::memset(&c, 0, sizeof(RawCommand));
            c.target_tick = 0; c.emitter = 0;
            c.type = CommandType::PLACE_BUILDING;
            c.sequence = seq0++;
            c.p.x_raw = 24;
            c.p.y_raw = 124;
            c.p.unit_id = bid_chariotry_stable;
            ++n;
        }
        {
            RawCommand& c = batch[n];
            std::memset(&c, 0, sizeof(RawCommand));
            c.target_tick = 0; c.emitter = 1;
            c.type = CommandType::PLACE_BUILDING;
            c.sequence = seq1++;
            c.p.x_raw = 229;
            c.p.y_raw = 124;
            c.p.unit_id = bid_castra_barracks;
            ++n;
        }
    } else if (t == 1u) {
        // Cada emisor ya consumió una secuencia adicional para sus edificios
        // de escenario (centro+stable para owner 0, foro+castra para owner 1).
        uint64_t seq0 = static_cast<uint64_t>(n_cav + n_cit) + 3u;
        uint64_t seq1 = static_cast<uint64_t>(n_art) + 3u;
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
            c.p.handle = EntityHandle{n_cav + n_cit + 2u + i, 1u};  // tras centro+stable, gen 1
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

    // Edificios y ghost: una caja unidad se escala por footprint en cada
    // instancia. El material acepta alpha para que los sitios en construcción
    // y el ghost sean legibles sobre el mapa.
    godot::Ref<godot::StandardMaterial3D> building_mat;
    building_mat.instantiate();
    building_mat->set_shading_mode(godot::BaseMaterial3D::SHADING_MODE_UNSHADED);
    building_mat->set_flag(godot::BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
    building_mat->set_albedo(godot::Color(1, 1, 1));
    building_mat->set_cull_mode(godot::BaseMaterial3D::CULL_DISABLED);
    building_mat->set_transparency(godot::BaseMaterial3D::TRANSPARENCY_ALPHA);

    godot::Ref<godot::BoxMesh> building_box;
    building_box.instantiate();
    building_box->set_size(godot::Vector3(1, 1, 1));
    building_box->set_material(building_mat);

    godot::Ref<godot::MultiMesh> mm_buildings;
    mm_buildings.instantiate();
    mm_buildings->set_transform_format(godot::MultiMesh::TRANSFORM_3D);
    mm_buildings->set_use_colors(true);
    mm_buildings->set_mesh(building_box);
    mm_buildings->set_instance_count(static_cast<int32_t>(cap));
    mm_buildings->set_visible_instance_count(0);
    mmi_buildings3d = memnew(godot::MultiMeshInstance3D);
    mmi_buildings3d->set_multimesh(mm_buildings);
    add_child(mmi_buildings3d);

    godot::Ref<godot::MultiMesh> mm_ghost;
    mm_ghost.instantiate();
    mm_ghost->set_transform_format(godot::MultiMesh::TRANSFORM_3D);
    mm_ghost->set_use_colors(true);
    mm_ghost->set_mesh(building_box);
    mm_ghost->set_instance_count(1);
    mm_ghost->set_visible_instance_count(0);
    mmi_ghost3d = memnew(godot::MultiMeshInstance3D);
    mmi_ghost3d->set_multimesh(mm_ghost);
    add_child(mmi_ghost3d);

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

bool ChunsaSimNode::screen_to_tile(const godot::Vector2& screen, int64_t& tx,
                                   int64_t& ty) const {
    float world_px = 0.0f;
    float world_py = 0.0f;
    if (!screen_to_map(screen, world_px, world_py)) return false;
    tx = static_cast<int64_t>(std::floor(world_px / 4.0f));
    ty = static_cast<int64_t>(std::floor(world_py / 4.0f));
    return true;
}

bool ChunsaSimNode::is_static_wall(int64_t tx, int64_t ty) const {
    // Es el patrón de gs_init_cost_grid() del escenario actual. Los edificios
    // vivos se comprueban aparte desde el snapshot para no leer GameState
    // concurrentemente desde el hilo de presentación.
    return tx == 128 && ty >= 32 && ty < 224 && !(ty >= 124 && ty < 132);
}

bool ChunsaSimNode::placement_valid(chunsa::BuildingId building_id, int64_t tx,
                                    int64_t ty) const {
    if (!have_curr || building_id == chunsa::INVALID_BUILDING_ID) return false;
    const chunsa::DataCatalogV1& catalog = catalog_storage.catalog();
    if (building_id >= catalog.building_count) return false;
    const chunsa::BuildingDefinitionV1& def = catalog.buildings[building_id];
    const int64_t fw = static_cast<int64_t>(def.footprint_w);
    const int64_t fh = static_cast<int64_t>(def.footprint_h);
    if (tx < 0 || ty < 0 || tx + fw > MAP_TILES || ty + fh > MAP_TILES) {
        return false;
    }
    for (int64_t y = ty; y < ty + fh; ++y) {
        for (int64_t x = tx; x < tx + fw; ++x) {
            if (is_static_wall(x, y)) return false;
        }
    }

    const uint32_t cap = snap_curr.capacity < 1024u ? snap_curr.capacity : 1024u;
    for (uint32_t i = 0; i < cap; ++i) {
        if (snap_curr.alive[i] == 0u || snap_curr.entity_kind[i] != 1u) continue;
        if (snap_curr.building_id[i] >= catalog.building_count) continue;
        const chunsa::BuildingDefinitionV1& other =
                catalog.buildings[snap_curr.building_id[i]];
        const int64_t ox = static_cast<int64_t>(snap_curr.bld_anchor_tx[i]);
        const int64_t oy = static_cast<int64_t>(snap_curr.bld_anchor_ty[i]);
        const int64_t ow = static_cast<int64_t>(other.footprint_w);
        const int64_t oh = static_cast<int64_t>(other.footprint_h);
        if (tx < ox + ow && ox < tx + fw && ty < oy + oh && oy < ty + fh) {
            return false;
        }
    }
    return true;
}

void ChunsaSimNode::enqueue_place_building(int64_t tx, int64_t ty) {
    std::lock_guard<std::mutex> lock(input_mutex);
    chunsa::RawCommand c;
    std::memset(&c, 0, sizeof(c));
    c.target_tick = 0;
    c.emitter = 0;
    c.type = chunsa::CommandType::PLACE_BUILDING;
    c.sequence = next_player_sequence++;
    c.p.x_raw = tx; // SPEC-004 §4.1: tile ancla ENTERO, no raw Q47.16.
    c.p.y_raw = ty;
    c.p.unit_id = bid_buildable;
    pending_player_commands.push_back(c);
}

uint32_t ChunsaSimNode::enqueue_build_assignments(int64_t tx, int64_t ty) {
    if (!have_curr) return 0;
    const chunsa::DataCatalogV1& catalog = catalog_storage.catalog();
    const uint32_t cap = snap_curr.capacity < 1024u ? snap_curr.capacity : 1024u;
    bool has_site = false;
    for (uint32_t i = 0; i < cap; ++i) {
        if (snap_curr.alive[i] == 0u || snap_curr.entity_kind[i] != 1u) continue;
        if (snap_curr.owner[i] != 0u || snap_curr.building_id[i] >= catalog.building_count) continue;
        const chunsa::BuildingDefinitionV1& def =
                catalog.buildings[snap_curr.building_id[i]];
        const int64_t bx = static_cast<int64_t>(snap_curr.bld_anchor_tx[i]);
        const int64_t by = static_cast<int64_t>(snap_curr.bld_anchor_ty[i]);
        if (snap_curr.build_progress[i] < def.build_time_ticks &&
            tx >= bx && tx < bx + static_cast<int64_t>(def.footprint_w) &&
            ty >= by && ty < by + static_cast<int64_t>(def.footprint_h)) {
            has_site = true;
            break;
        }
    }
    if (!has_site) return 0;

    uint32_t count = 0;
    std::lock_guard<std::mutex> lock(input_mutex);
    for (uint32_t i = 0; i < cap; ++i) {
        if (!selected_slot_is_current(i) ||
            snap_curr.owner[i] != 0u || snap_curr.entity_kind[i] != 0u ||
            snap_curr.unit_class[i] != 3u) {
            continue;
        }
        chunsa::RawCommand c;
        std::memset(&c, 0, sizeof(c));
        c.target_tick = 0;
        c.emitter = 0;
        c.type = chunsa::CommandType::ASSIGN_BUILD;
        c.sequence = next_player_sequence++;
        c.p.handle = chunsa::EntityHandle{i, snap_curr.generation[i]};
        c.p.x_raw = tx; // SPEC-004 §4.2: tile ENTERO contenido en el footprint.
        c.p.y_raw = ty;
        pending_player_commands.push_back(c);
        ++count;
    }
    if (count > 0u) {
        godot::UtilityFunctions::print("CHUNSA ASSIGN_BUILD enqueued citizens=", count,
                                       " tile=", tx, ",", ty);
    }
    return count;
}

int32_t ChunsaSimNode::selected_building_slot() const {
    return selected_single_building_slot();
}

void ChunsaSimNode::enqueue_selected_action(uint32_t action_index, bool research) {
    const int32_t selected = selected_building_slot();
    if (selected < 0) {
        godot::UtilityFunctions::print(
                "CHUNSA acción local reject: selecciona un edificio propio");
        return;
    }
    const chunsa::BuildingId building_id = snap_curr.building_id[selected];
    const chunsa::DataCatalogV1& catalog = catalog_storage.catalog();
    if (building_id >= catalog.building_count) return;
    const chunsa::BuildingDefinitionV1& def = catalog.buildings[building_id];
    const uint32_t count = research ? def.research_count : def.train_count;
    if (action_index >= count) {
        godot::UtilityFunctions::print(
                research ? "CHUNSA RESEARCH local reject: índice sin tech"
                         : "CHUNSA TRAIN local reject: índice sin unidad");
        return;
    }

    const uint32_t item_id = research ? def.researches[action_index]
                                      : def.trains[action_index];
    chunsa::RawCommand c;
    std::memset(&c, 0, sizeof(c));
    c.target_tick = 0;
    c.emitter = 0;
    c.type = research ? chunsa::CommandType::RESEARCH_TECH
                      : chunsa::CommandType::TRAIN_UNIT;
    c.sequence = next_player_sequence++;
    c.p.handle = chunsa::EntityHandle{
            static_cast<uint32_t>(selected), snap_curr.generation[selected]};
    c.p.unit_id = item_id;
    {
        std::lock_guard<std::mutex> lock(input_mutex);
        pending_player_commands.push_back(c);
    }
    godot::UtilityFunctions::print(
            research ? "CHUNSA RESEARCH_TECH enqueued tech_id="
                     : "CHUNSA TRAIN_UNIT enqueued unit_id=",
            static_cast<int64_t>(item_id), " building_slot=", selected,
            " sequence=", static_cast<int64_t>(c.sequence));
}

void ChunsaSimNode::enqueue_rally(int64_t tx, int64_t ty) {
    const uint32_t cap = snap_curr.capacity < 1024u ? snap_curr.capacity : 1024u;
    uint32_t count = 0;
    std::lock_guard<std::mutex> lock(input_mutex);
    for (uint32_t i = 0; i < cap; ++i) {
        if (!selected_slot_is_current(i) ||
            snap_curr.owner[i] != 0u || snap_curr.entity_kind[i] != 1u) {
            continue;
        }
        chunsa::RawCommand c;
        std::memset(&c, 0, sizeof(c));
        c.target_tick = 0;
        c.emitter = 0;
        c.type = chunsa::CommandType::SET_RALLY;
        c.sequence = next_player_sequence++;
        c.p.handle = chunsa::EntityHandle{i, snap_curr.generation[i]};
        // SET_RALLY usa raw Q47.16: centro del tile, distinto de
        // PLACE_BUILDING que usa la coordenada tile entera.
        c.p.x_raw = tx * 65536 + 32768;
        c.p.y_raw = ty * 65536 + 32768;
        pending_player_commands.push_back(c);
        ++count;
    }
    if (count == 0u) {
        godot::UtilityFunctions::print(
                "CHUNSA SET_RALLY local reject: selecciona un edificio propio");
    } else {
        godot::UtilityFunctions::print("CHUNSA SET_RALLY enqueued tile=", tx, ",",
                                       ty, " edificios=", count);
        add_order_marker(static_cast<float>(tx) * 4.0f + 2.0f,
                         static_cast<float>(ty) * 4.0f + 2.0f);
    }
}

void ChunsaSimNode::enqueue_epoch_up() {
    chunsa::RawCommand c;
    std::memset(&c, 0, sizeof(c));
    c.target_tick = 0;
    c.emitter = 0;
    c.type = chunsa::CommandType::EPOCH_UP;
    c.sequence = next_player_sequence++;
    // EPOCH_UP es de jugador: todos los campos de c.p permanecen en cero.
    {
        std::lock_guard<std::mutex> lock(input_mutex);
        pending_player_commands.push_back(c);
    }
    godot::UtilityFunctions::print("CHUNSA EPOCH_UP enqueued sequence=",
                                   static_cast<int64_t>(c.sequence));
}

void ChunsaSimNode::cycle_buildable_building() {
    const chunsa::DataCatalogV1& catalog = catalog_storage.catalog();
    if (catalog.building_count == 0u) return;
    const uint32_t start = bid_buildable == chunsa::INVALID_BUILDING_ID ||
                                   bid_buildable >= catalog.building_count
            ? 0u
            : (bid_buildable + 1u) % catalog.building_count;
    for (uint32_t offset = 0; offset < catalog.building_count; ++offset) {
        const uint32_t candidate = (start + offset) % catalog.building_count;
        if (catalog.buildings[candidate].constructible != 0u) {
            bid_buildable = candidate;
            godot::UtilityFunctions::print(
                "CHUNSA construir: building_id=", static_cast<int64_t>(bid_buildable));
            return;
        }
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
