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
            " buildable_id=", static_cast<int64_t>(bid_buildable));
        if (bid_settlement_center == chunsa::INVALID_BUILDING_ID ||
            bid_forum_center == chunsa::INVALID_BUILDING_ID ||
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
                s->generation[i] = gs->entities.generation[i];
                s->x[i] = static_cast<float>(gs->pos_x[i]) / 65536.0f;
                s->y[i] = static_cast<float>(gs->pos_y[i]) / 65536.0f;
                s->owner[i] = gs->owner[i];
                s->unit_class[i] = gs->unit_class[i];
                s->fleeing[i] = gs->fleeing[i];
                s->entity_kind[i] = gs->entity_kind[i];
                s->building_id[i] = gs->building_id[i];
                s->build_progress[i] = gs->build_progress[i];
                s->bld_anchor_tx[i] = gs->bld_anchor_tx[i];
                s->bld_anchor_ty[i] = gs->bld_anchor_ty[i];
                s->build_target[i] = gs->build_target[i];
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

// Selección, colocación y órdenes del jugador (Sprint 1.1). Todo lo que sale
// de aquí es un RawCommand: la validación real y la mutación siguen siendo del
// kernel. El ghost solo hace una validación local de mapa/muros/solape para
// feedback inmediato.
void ChunsaSimNode::_input(const godot::Ref<godot::InputEvent>& event) {
    if (cam3d == nullptr) return;

    godot::Ref<godot::InputEventKey> key = event;
    if (!key.is_null() && key->is_pressed() && !key->is_echo()) {
        const godot::Key physical = key->get_physical_keycode();
        const godot::Key logical = key->get_keycode();
        const godot::Key code = physical != godot::KEY_NONE ? physical : logical;
        if (code == godot::KEY_B) {
            placement_mode = !placement_mode;
            placement_input_captured = false;
            godot::UtilityFunctions::print(
                placement_mode ? "CHUNSA construir: ghost activo"
                               : "CHUNSA construir: ghost cancelado");
            return;
        }
        if (code == godot::KEY_ESCAPE && placement_mode) {
            placement_mode = false;
            placement_input_captured = false;
            godot::UtilityFunctions::print("CHUNSA construir: ghost cancelado");
            return;
        }
        if (code == godot::KEY_N && placement_mode) {
            cycle_buildable_building();
            return;
        }
    }

    godot::Ref<godot::InputEventMouseMotion> motion = event;
    if (!motion.is_null()) {
        cursor_screen = motion->get_position();
        have_cursor = true;
        return;
    }

    godot::Ref<godot::InputEventMouseButton> mb = event;
    if (mb.is_null()) return;
    cursor_screen = mb->get_position();
    have_cursor = true;

    const uint32_t cap = snap_curr.capacity < 1024u ? snap_curr.capacity : 1024u;

    if (mb->get_button_index() == godot::MouseButton::MOUSE_BUTTON_LEFT) {
        if (mb->is_pressed()) {
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
            dragging = true;
            drag_start = mb->get_position();
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
                }
            }
        }
        if (is_click && best_i != UINT32_MAX) {
            is_selected[best_i] = true;
        }
        return;
    }

    if (mb->get_button_index() == godot::MouseButton::MOUSE_BUTTON_RIGHT && mb->is_pressed()) {
        int64_t tile_x = 0;
        int64_t tile_y = 0;
        if (screen_to_tile(mb->get_position(), tile_x, tile_y) &&
            enqueue_build_assignments(tile_x, tile_y) > 0u) {
            return;
        }

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
        if (is_selected[i])                     c = godot::Color(0.3, 1.0, 0.3);    // seleccionado: verde brillante
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
            if (is_selected[i]) {
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

// t==0: PLACE_BUILDING de los dos centros iniciales + spawns — 40% caballería
// (owner 0), 40% artillería (owner 1), 20% ciudadanos (owner 0). Los centros
// usan la exención de escenario de SPEC-004 y nacen completos. Orden de
// emisión = orden canónico de aplicación (emitter asc, sequence asc), así la
// tabla queda determinista:
// cav en slots [0,n_cav), ciudadanos [n_cav,n_cav+n_cit), artillería
// [n_cav+n_cit,demo_units), centros en los dos slots siguientes — todos con
// generación 1.
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
    } else if (t == 1u) {
        // Cada emisor ya consumió una secuencia adicional para su centro.
        uint64_t seq0 = static_cast<uint64_t>(n_cav + n_cit) + 2u;
        uint64_t seq1 = static_cast<uint64_t>(n_art) + 2u;
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
            c.p.handle = EntityHandle{n_cav + n_cit + 1u + i, 1u};  // tras centro egipcio, gen 1
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
    if (cam3d == nullptr) return false;
    const godot::Vector3 origin = cam3d->project_ray_origin(screen);
    const float world_px = origin.x;
    const float world_py = -origin.y;
    if (!std::isfinite(world_px) || !std::isfinite(world_py)) return false;
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
        if (!is_selected[i] || snap_curr.alive[i] == 0u ||
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
