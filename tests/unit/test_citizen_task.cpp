// SPEC-004 §22 (Sprint 1.7): tarea explícita y única del ciudadano.
//
// Cubre las transiciones MOVE/GATHER/BUILD, conservación de carga, propiedad
// exclusiva de pos por sistema, auto-asignación al spawn, fin de tarea,
// save/load, replay y pertenencia de citizen_task al dominio de checksum.
//
// GameState siempre vive en heap: su tamaño excede una pila segura bajo ctest.
#include <cstdint>
#include <cstdio>
#include <memory>

#include "chunsa/replay.hpp"
#include "chunsa/save_io.hpp"

static int g_fails = 0;
#define CHECK(cond) do { \
    if (!(cond)) { \
        ++g_fails; \
        std::printf("CHECK L%d: %s\n", __LINE__, #cond); \
    } \
} while (0)

using namespace chunsa;

namespace {

constexpr int64_t TILE = FX_ONE_RAW;

struct TaskCatalog {
    UnitDefinitionV1 units[1]{};
    BuildingDefinitionV1 buildings[1]{};
    DataCatalogV1 catalog{};

    TaskCatalog() {
        units[0].id = 0u;
        units[0].unit_class = UnitClassV1::Citizen;
        units[0].hp = 25;
        units[0].speed_millitile_tick = 1000;
        units[0].morale = 100;
        units[0].build_time_ticks = 1;
        units[0].pop_cost = 1;
        units[0].epoch_min = 1u;
        units[0].epoch_max = 15u;
        units[0].civ_id = INVALID_CIV_ID;

        buildings[0].id = 0u;
        buildings[0].hp = 100;
        buildings[0].footprint_w = 2u;
        buildings[0].footprint_h = 2u;
        buildings[0].build_time_ticks = 10u;
        buildings[0].constructible = 1u;
        buildings[0].epoch_min = 1u;
        buildings[0].epoch_max = 15u;
        buildings[0].trains[0] = 0u;
        buildings[0].train_count = 1u;
        buildings[0].civ_id = INVALID_CIV_ID;

        catalog.unit_count = 1u;
        catalog.units = units;
        catalog.building_count = 1u;
        catalog.buildings = buildings;
    }
};

MatchConfig01A task_cfg() {
    MatchConfig01A cfg{};
    cfg.max_entities = 16u;
    cfg.player_count = 2u;
    cfg.human_input_delay_ticks = 0u;
    cfg.max_future_command_ticks = 20u;
    cfg.checksum_every_ticks = 1u;
    cfg.map_tiles_x = 256u;
    cfg.map_tiles_y = 256u;
    cfg.seed = 20260728ull;
    cfg.allow_debug_stat_payload = 1u;
    return cfg;
}

std::unique_ptr<GameState> make_state(const DataCatalogV1* catalog = nullptr) {
    auto g = std::make_unique<GameState>();
    gs_init(*g, task_cfg());
    if (catalog != nullptr) {
        gs_bind_catalog(*g, *catalog);
        gs_init_epoch_from_catalog(*g);
    }
    return g;
}

EntityHandle add_citizen(GameState& g, int64_t x_raw, int64_t y_raw,
                         uint8_t task = CITIZEN_TASK_GATHER) {
    const EntityHandle h = et_spawn(g.entities);
    CHECK(!handle_eq(h, NULL_HANDLE));
    if (handle_eq(h, NULL_HANDLE)) return h;
    const uint32_t i = h.index;
    g.pos_x[i] = x_raw;
    g.pos_y[i] = y_raw;
    g.tgt_x[i] = x_raw;
    g.tgt_y[i] = y_raw;
    g.vel_x[i] = 0;
    g.vel_y[i] = 0;
    g.speed_mtpt[i] = 1000;
    g.owner[i] = 0u;
    g.hp[i] = 25;
    g.max_hp[i] = 25;
    g.unit_class[i] = 3u;
    g.build_target[i] = BUILD_NO_TARGET;
    g.eco_state[i] = EcoState::SEEK;
    g.eco_assigned_deposit[i] = 0u;
    g.eco_carry[i] = 0;
    g.eco_carry_resource[i] = 0u;
    g.citizen_task[i] = task;
    return h;
}

EntityHandle add_building(GameState& g, uint16_t tx, uint16_t ty,
                          uint32_t progress = 0u) {
    const EntityHandle h = et_spawn(g.entities);
    CHECK(!handle_eq(h, NULL_HANDLE));
    if (handle_eq(h, NULL_HANDLE)) return h;
    const uint32_t i = h.index;
    g.owner[i] = 0u;
    g.entity_kind[i] = 1u;
    g.building_id[i] = 0u;
    g.build_progress[i] = progress;
    g.bld_anchor_tx[i] = tx;
    g.bld_anchor_ty[i] = ty;
    g.unit_class[i] = 255u;
    g.hp[i] = 100;
    g.max_hp[i] = 100;
    g.pos_x[i] = static_cast<int64_t>(tx + 1u) * TILE;
    g.pos_y[i] = static_cast<int64_t>(ty + 1u) * TILE;
    return h;
}

RawCommand move_to(uint32_t tick, uint64_t sequence, EntityHandle h,
                   int64_t x_raw, int64_t y_raw) {
    RawCommand c{};
    c.target_tick = tick;
    c.emitter = 0u;
    c.type = CommandType::MOVE_TO;
    c.sequence = sequence;
    c.p.handle = h;
    c.p.x_raw = x_raw;
    c.p.y_raw = y_raw;
    return c;
}

RawCommand gather_at(uint32_t tick, uint64_t sequence, EntityHandle h,
                     int64_t x_raw, int64_t y_raw) {
    RawCommand c{};
    c.target_tick = tick;
    c.emitter = 0u;
    c.type = CommandType::GATHER;
    c.sequence = sequence;
    c.p.handle = h;
    c.p.x_raw = x_raw;
    c.p.y_raw = y_raw;
    return c;
}

RawCommand assign_build(uint32_t tick, uint64_t sequence, EntityHandle h,
                        int64_t tx, int64_t ty) {
    RawCommand c{};
    c.target_tick = tick;
    c.emitter = 0u;
    c.type = CommandType::ASSIGN_BUILD;
    c.sequence = sequence;
    c.p.handle = h;
    c.p.x_raw = tx;
    c.p.y_raw = ty;
    return c;
}

void test_spawn_paths_default_to_gather() {
    TaskCatalog fixture;

    // SPAWN_CITIZEN data-driven.
    {
        auto g = make_state(&fixture.catalog);
        // §23: la auto-asignación solo puede persistir si existe una zona
        // aliada. Centro completo junto al depósito legacy 0.
        const EntityHandle center = add_building(*g, 39u, 39u, 10u);
        CHECK(center.index == 0u);
        RawCommand c{};
        c.target_tick = 0u;
        c.emitter = 0u;
        c.type = CommandType::SPAWN_CITIZEN;
        c.sequence = 1u;
        c.p.x_raw = g->deposits[0].x_raw;
        c.p.y_raw = g->deposits[0].y_raw;
        c.p.unit_id = 0u;
        CHECK(step(*g, &c, 1u).accepted == 1u);
        CHECK(g->unit_class[1] == 3u);
        CHECK(g->citizen_task[1] == CITIZEN_TASK_GATHER);
    }

    // SPAWN_UNIT de clase Citizen comparte la misma inicialización.
    {
        auto g = make_state(&fixture.catalog);
        const EntityHandle center = add_building(*g, 39u, 39u, 10u);
        CHECK(center.index == 0u);
        RawCommand c{};
        c.target_tick = 0u;
        c.emitter = 0u;
        c.type = CommandType::SPAWN_UNIT;
        c.sequence = 1u;
        c.p.x_raw = g->deposits[0].x_raw;
        c.p.y_raw = g->deposits[0].y_raw;
        c.p.unit_id = 0u;
        CHECK(step(*g, &c, 1u).accepted == 1u);
        CHECK(g->unit_class[1] == 3u);
        CHECK(g->citizen_task[1] == CITIZEN_TASK_GATHER);
    }

    // TRAIN_UNIT: production_system materializa el ciudadano de la cola.
    {
        auto g = make_state(&fixture.catalog);
        const EntityHandle building = add_building(*g, 20u, 20u, 10u);
        CHECK(building.index == 0u);
        g->prod_queue[building.index][0] = 0u;
        g->prod_count[building.index] = 1u;
        g->prod_progress[building.index] = 0u;
        detail::production_system(*g);
        CHECK(g->entities.alive[1] == 1u);
        CHECK(g->unit_class[1] == 3u);
        CHECK(g->citizen_task[1] == CITIZEN_TASK_GATHER);
    }
}

void test_move_to_moves_then_idles_and_preserves_carry() {
    auto g = make_state();
    const EntityHandle citizen = add_citizen(*g, 10 * TILE, 10 * TILE);
    g->eco_assigned_deposit[citizen.index] = 2u;
    g->eco_carry[citizen.index] = 17;
    g->eco_carry_resource[citizen.index] = 2u;
    g->build_target[citizen.index] = 9u;

    const int64_t start_x = g->pos_x[citizen.index];
    RawCommand move = move_to(g->tick, 1u, citizen, 20 * TILE, 10 * TILE);
    StepResult result = step(*g, &move, 1u);
    CHECK(result.accepted == 1u);
    CHECK(g->pos_x[citizen.index] > start_x);
    CHECK(g->citizen_task[citizen.index] == CITIZEN_TASK_MOVE);
    CHECK(g->build_target[citizen.index] == BUILD_NO_TARGET);
    CHECK(g->eco_assigned_deposit[citizen.index] == 2u);
    CHECK(g->eco_carry[citizen.index] == 17);
    CHECK(g->eco_carry_resource[citizen.index] == 2u);

    for (uint32_t k = 0; k < 20u
         && g->citizen_task[citizen.index] == CITIZEN_TASK_MOVE; ++k) {
        step(*g, nullptr, 0u);
    }
    CHECK(g->citizen_task[citizen.index] == CITIZEN_TASK_IDLE);
    CHECK(g->vel_x[citizen.index] == 0);
    CHECK(g->vel_y[citizen.index] == 0);
    CHECK(g->eco_carry[citizen.index] == 17);
    CHECK(g->eco_carry_resource[citizen.index] == 2u);
}

void test_gather_interrupts_move_and_honours_load_rule() {
    auto g = make_state();
    const EntityHandle citizen = add_citizen(*g, 100 * TILE, 100 * TILE,
                                             CITIZEN_TASK_MOVE);
    g->tgt_x[citizen.index] = 200 * TILE;
    g->tgt_y[citizen.index] = 100 * TILE;
    g->build_target[citizen.index] = 7u;
    g->eco_state[citizen.index] = EcoState::HARVEST;
    g->eco_assigned_deposit[citizen.index] = 0u;  // A
    g->eco_carry[citizen.index] = 10;
    g->eco_carry_resource[citizen.index] = 0u;
    CHECK(g->deposits[2].resource_idx == 1u);      // B

    const int64_t before_x = g->pos_x[citizen.index];
    RawCommand gather = gather_at(g->tick, 1u, citizen,
                                  g->deposits[2].x_raw,
                                  g->deposits[2].y_raw);
    CHECK(step(*g, &gather, 1u).accepted == 1u);
    CHECK(g->citizen_task[citizen.index] == CITIZEN_TASK_GATHER);
    CHECK(g->eco_state[citizen.index] == EcoState::RETURN);
    CHECK(g->eco_assigned_deposit[citizen.index] == 2u);
    CHECK(g->eco_carry[citizen.index] == 10);
    CHECK(g->eco_carry_resource[citizen.index] == 0u);
    CHECK(g->build_target[citizen.index] == BUILD_NO_TARGET);
    // La fase MOVE no actuó antes de RETURN: el avance observado apunta al
    // dropoff, no al target viejo de x=200.
    CHECK(g->pos_x[citizen.index] < before_x);
}

void test_assign_build_interrupts_move_and_finishes_idle() {
    TaskCatalog fixture;
    auto g = make_state(&fixture.catalog);
    const EntityHandle citizen = add_citizen(*g, 40 * TILE, 11 * TILE,
                                             CITIZEN_TASK_MOVE);
    const EntityHandle building = add_building(*g, 30u, 10u, 0u);
    g->tgt_x[citizen.index] = 60 * TILE;  // movimiento viejo, dirección opuesta
    g->tgt_y[citizen.index] = 11 * TILE;

    const int64_t before_x = g->pos_x[citizen.index];
    RawCommand assign = assign_build(g->tick, 1u, citizen, 30, 10);
    CHECK(step(*g, &assign, 1u).accepted == 1u);
    CHECK(g->citizen_task[citizen.index] == CITIZEN_TASK_BUILD);
    CHECK(g->build_target[citizen.index] == building.index);
    CHECK(g->pos_x[citizen.index] < before_x);

    // Completar la obra deja al constructor IDLE en el mismo tick.
    g->pos_x[citizen.index] = 32 * TILE;
    g->pos_y[citizen.index] = 11 * TILE;
    g->build_progress[building.index] = 9u;
    detail::construction_system(*g);
    CHECK(g->build_progress[building.index] == 10u);
    CHECK(g->citizen_task[citizen.index] == CITIZEN_TASK_IDLE);
    CHECK(g->build_target[citizen.index] == BUILD_NO_TARGET);

    // Perder el objetivo también termina en IDLE, sin velocidad residual.
    g->citizen_task[citizen.index] = CITIZEN_TASK_BUILD;
    g->build_target[citizen.index] = 15u;
    g->vel_x[citizen.index] = TILE;
    detail::construction_system(*g);
    CHECK(g->citizen_task[citizen.index] == CITIZEN_TASK_IDLE);
    CHECK(g->build_target[citizen.index] == BUILD_NO_TARGET);
    CHECK(g->vel_x[citizen.index] == 0);
    CHECK(g->vel_y[citizen.index] == 0);
}

void test_exactly_one_position_owner_per_task() {
    TaskCatalog fixture;
    auto g = make_state(&fixture.catalog);
    const EntityHandle citizen = add_citizen(*g, 10 * TILE, 10 * TILE);
    const EntityHandle building = add_building(*g, 30u, 10u, 0u);

    const uint8_t tasks[] = {
        CITIZEN_TASK_IDLE,
        CITIZEN_TASK_MOVE,
        CITIZEN_TASK_GATHER,
        CITIZEN_TASK_BUILD,
    };
    for (uint8_t task : tasks) {
        const uint32_t ci = citizen.index;
        g->citizen_task[ci] = task;
        g->pos_x[ci] = 10 * TILE;
        g->pos_y[ci] = 10 * TILE;
        g->tgt_x[ci] = 50 * TILE;
        g->tgt_y[ci] = 10 * TILE;
        g->vel_x[ci] = 0;
        g->vel_y[ci] = 0;
        g->eco_state[ci] = EcoState::SEEK;
        g->eco_assigned_deposit[ci] = 0u;
        g->build_target[ci] = building.index;  // dato obsoleto salvo en BUILD
        g->build_progress[building.index] = 0u;

        uint32_t writers = 0u;
        int64_t before_x = g->pos_x[ci];
        int64_t before_y = g->pos_y[ci];
        detail::citizen_move_system(*g);
        if (g->pos_x[ci] != before_x || g->pos_y[ci] != before_y) ++writers;

        before_x = g->pos_x[ci];
        before_y = g->pos_y[ci];
        detail::economy_system(*g);
        if (g->pos_x[ci] != before_x || g->pos_y[ci] != before_y) ++writers;

        before_x = g->pos_x[ci];
        before_y = g->pos_y[ci];
        detail::construction_system(*g);
        if (g->pos_x[ci] != before_x || g->pos_y[ci] != before_y) ++writers;

        CHECK(writers <= 1u);  // aserto directo del invariante §22.1
        if (task == CITIZEN_TASK_IDLE) {
            CHECK(writers == 0u);
        } else {
            CHECK(writers == 1u);
        }
    }
}

void test_no_deposit_transitions_to_idle() {
    auto g = make_state();
    const EntityHandle citizen = add_citizen(*g, 10 * TILE, 10 * TILE);
    for (uint32_t d = 0; d < g->n_deposits; ++d) {
        g->deposits[d].remaining = 0;
    }
    g->eco_assigned_deposit[citizen.index] = 0u;
    g->eco_state[citizen.index] = EcoState::SEEK;
    g->vel_x[citizen.index] = TILE;
    g->vel_y[citizen.index] = TILE;

    detail::economy_system(*g);
    CHECK(g->eco_assigned_deposit[citizen.index] == ECO_NO_DEPOSIT);
    CHECK(g->citizen_task[citizen.index] == CITIZEN_TASK_IDLE);
    CHECK(g->vel_x[citizen.index] == 0);
    CHECK(g->vel_y[citizen.index] == 0);
}

void test_save_load_replay_and_checksum_preserve_task() {
    const char* save_path = "test_citizen_task_v14.sav";
    const char* replay_path = "test_citizen_task_v3.curp";
    CHECK(SAVE_FORMAT_VERSION == 15u);
    CHECK(CHECKSUM_ALGO_VERSION == 10u);

    auto direct = make_state();
    const EntityHandle citizen = add_citizen(*direct, 10 * TILE, 10 * TILE);
    RawCommand move = move_to(direct->tick, 1u, citizen, 50 * TILE, 10 * TILE);

    ReplayWriter writer;
    writer.begin(task_cfg().seed, 1u, 1u, 1u, 0u, 20u);
    writer.tick_batch(&move, 1u, direct->tick);
    CHECK(step(*direct, &move, 1u).accepted == 1u);
    CHECK(direct->citizen_task[citizen.index] == CITIZEN_TASK_MOVE);
    const uint64_t direct_checksum = state_checksum_v1(*direct);
    CHECK(writer.finish(direct_checksum, replay_path) == 0);

    AiJobBox box{};
    ai_box_init(box, 1u);
    AiRuntimeV1 runtime{};
    CHECK(save_game(*direct, box, runtime, save_path) == 0);

    auto loaded = std::make_unique<GameState>();
    AiJobBox loaded_box{};
    AiRuntimeV1 loaded_runtime{};
    CHECK(load_game(*loaded, loaded_box, loaded_runtime, save_path) == 0);
    CHECK(loaded->citizen_task[citizen.index] == CITIZEN_TASK_MOVE);
    CHECK(state_checksum_v1(*loaded) == direct_checksum);

    ReplayData replay;
    CHECK(replay_load(replay_path, replay) == 0);
    CHECK(replay.version == 3u);
    CHECK(replay.batches.size() == 1u);
    auto replayed = make_state();
    const EntityHandle replayed_citizen =
        add_citizen(*replayed, 10 * TILE, 10 * TILE);
    CHECK(replayed_citizen.index == citizen.index);
    CHECK(step(*replayed, replay.batches[0].data(),
               static_cast<uint32_t>(replay.batches[0].size())).accepted == 1u);
    CHECK(replayed->citizen_task[citizen.index] == CITIZEN_TASK_MOVE);
    CHECK(state_checksum_v1(*replayed) == direct_checksum);

    // El campo pertenece de verdad al dominio vigente.
    loaded->citizen_task[citizen.index] = CITIZEN_TASK_GATHER;
    CHECK(state_checksum_v1(*loaded) != direct_checksum);

    std::remove(save_path);
    std::remove(replay_path);
}

}  // namespace

int main() {
    test_spawn_paths_default_to_gather();
    test_move_to_moves_then_idles_and_preserves_carry();
    test_gather_interrupts_move_and_honours_load_rule();
    test_assign_build_interrupts_move_and_finishes_idle();
    test_exactly_one_position_owner_per_task();
    test_no_deposit_transitions_to_idle();
    test_save_load_replay_and_checksum_preserve_task();

    if (g_fails == 0) {
        std::printf("citizen_task: OK\n");
        return 0;
    }
    std::printf("citizen_task: %d fallos\n", g_fails);
    return 1;
}
