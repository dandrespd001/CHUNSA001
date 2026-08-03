// Sprint 1.8A (SPEC-007 §9.3/§11): ampliación puramente estructural del
// dominio de recursos. Esta prueba se escribió antes de la implementación y
// debe nacer roja por aserción, nunca por compilación.
//
// GameState siempre vive en heap: su tamaño excede una pila segura bajo ctest.
#include <cstdint>
#include <cstdio>
#include <memory>
#include <type_traits>

#include "chunsa/save_io.hpp"

static int g_fails = 0;
#define CHECK_EQ(obtained_expr, expected_expr) do { \
    const auto obtained_value = (obtained_expr); \
    const auto expected_value = (expected_expr); \
    if (!(obtained_value == expected_value)) { \
        ++g_fails; \
        std::printf("CHECK_EQ L%d: esperado=%llu obtenido=%llu (%s)\n", \
                    __LINE__, \
                    static_cast<unsigned long long>(expected_value), \
                    static_cast<unsigned long long>(obtained_value), \
                    #obtained_expr); \
    } \
} while (0)

using namespace chunsa;

namespace {

using PlayerStockArray = decltype(GameState::player_stock);
inline constexpr size_t PLAYER_STOCK_RESOURCE_COUNT =
    std::extent_v<PlayerStockArray, 1>;

template <typename Definition>
concept HasResourceCostVector = requires(Definition& definition) {
    definition.cost[0];
};

MatchConfig01A resource_cfg() {
    MatchConfig01A cfg{};
    cfg.max_entities = 16u;
    cfg.player_count = 2u;
    cfg.human_input_delay_ticks = 0u;
    cfg.max_future_command_ticks = 20u;
    cfg.checksum_every_ticks = 1u;
    cfg.map_tiles_x = 256u;
    cfg.map_tiles_y = 256u;
    cfg.seed = 20260729ull;
    cfg.allow_debug_stat_payload = 0u;
    return cfg;
}

std::unique_ptr<GameState> make_state() {
    auto state = std::make_unique<GameState>();
    gs_init(*state, resource_cfg());
    return state;
}

void test_named_capacity_and_versions() {
    CHECK_EQ(RESOURCE_COUNT, 64u);
    CHECK_EQ(SAVE_FORMAT_VERSION, 18u);
    CHECK_EQ(CHECKSUM_ALGO_VERSION, 13u);
}

void test_player_stock_index_31_round_trip_in_memory() {
    CHECK_EQ(PLAYER_STOCK_RESOURCE_COUNT, 64u);
    if constexpr (PLAYER_STOCK_RESOURCE_COUNT > 31u) {
        auto state = make_state();
        state->player_stock[0][31] = 0x123456789ll;
        CHECK_EQ(state->player_stock[0][31], 0x123456789ll);
        state->player_stock[MAX_EMITTERS - 1u][31] = 77;
        CHECK_EQ(state->player_stock[MAX_EMITTERS - 1u][31], 77);
    }
}

template <typename Mask>
void check_dropoff_bit_31() {
    // Sprint 1.9C: la mascara pasa a 64 bits porque el tope de recursos lo
    // hace. Son la MISMA restriccion: un bit por recurso.
    CHECK_EQ(sizeof(Mask), sizeof(uint64_t));
    if constexpr (sizeof(Mask) >= sizeof(uint64_t)) {
        constexpr uint32_t LOW_BIT = uint32_t{1} << 0u;
        constexpr uint32_t HIGH_BIT = uint32_t{1} << 31u;
        Mask mask = static_cast<Mask>(LOW_BIT | HIGH_BIT);
        CHECK_EQ(static_cast<uint32_t>(mask) & HIGH_BIT, HIGH_BIT);
        CHECK_EQ(static_cast<uint32_t>(mask) & LOW_BIT, LOW_BIT);
        mask = static_cast<Mask>(static_cast<uint32_t>(mask) & ~LOW_BIT);
        CHECK_EQ(static_cast<uint32_t>(mask) & HIGH_BIT, HIGH_BIT);
        CHECK_EQ(static_cast<uint32_t>(mask) & LOW_BIT, 0u);
    }
}

void test_dropoff_mask_accepts_and_distinguishes_bit_31() {
    check_dropoff_bit_31<decltype(BuildingDefinitionV1::dropoff_mask)>();
}

template <typename Definition>
void check_cost_vector_shape() {
    CHECK_EQ(HasResourceCostVector<Definition>, true);
    if constexpr (HasResourceCostVector<Definition>) {
        using CostArray = decltype(Definition::cost);
        CHECK_EQ(std::extent_v<CostArray>, RESOURCE_COUNT);
    }
}

void test_all_definition_cost_vectors_have_resource_count_entries() {
    check_cost_vector_shape<BuildingDefinitionV1>();
    check_cost_vector_shape<UnitDefinitionV1>();
    check_cost_vector_shape<TechDefinitionV1>();
}

template <typename Definition>
void check_index_31_building_cost_is_deducted() {
    CHECK_EQ(HasResourceCostVector<Definition>, true);
    CHECK_EQ(PLAYER_STOCK_RESOURCE_COUNT, 64u);
    if constexpr (HasResourceCostVector<Definition>
                  && PLAYER_STOCK_RESOURCE_COUNT > 31u) {
        Definition definition{};
        definition.id = 0u;
        definition.hp = 100;
        definition.footprint_w = 1u;
        definition.footprint_h = 1u;
        definition.build_time_ticks = 1u;
        definition.constructible = 1u;
        definition.epoch_min = 0u;
        definition.epoch_max = 15u;
        definition.civ_id = INVALID_CIV_ID;
        definition.cost[31] = 25;

        DataCatalogV1 catalog{};
        catalog.building_count = 1u;
        catalog.buildings = &definition;

        auto state = make_state();
        gs_bind_catalog(*state, catalog);
        state->player_stock[0][31] = 100;

        RawCommand command{};
        command.target_tick = 1u;
        command.emitter = 0u;
        command.type = CommandType::PLACE_BUILDING;
        command.sequence = 1u;
        command.p.unit_id = 0u;
        command.p.x_raw = 10;
        command.p.y_raw = 10;

        step(*state, &command, 1u);
        const StepResult applied = step(*state, nullptr, 0u);
        CHECK_EQ(applied.accepted, 1u);
        CHECK_EQ(state->player_stock[0][31], 75);
    }
}

void test_index_31_cost_is_deducted_from_stock() {
    check_index_31_building_cost_is_deducted<BuildingDefinitionV1>();
}

void test_save_load_preserves_all_32_stock_entries_including_zeroes() {
    CHECK_EQ(PLAYER_STOCK_RESOURCE_COUNT, 64u);
    if constexpr (PLAYER_STOCK_RESOURCE_COUNT > 31u) {
        const char* save_path = "test_resource_count_v14.sav";
        auto source = make_state();
        for (uint32_t resource = 0; resource < RESOURCE_COUNT; ++resource) {
            source->player_stock[0][resource] =
                (resource % 3u == 0u) ? 0 : static_cast<int64_t>(1000u + resource);
        }

        AiJobBox box{};
        ai_box_init(box, 1u);
        AiRuntimeV1 runtime{};
        CHECK_EQ(save_game(*source, box, runtime, save_path), 0);

        auto loaded = std::make_unique<GameState>();
        AiJobBox loaded_box{};
        AiRuntimeV1 loaded_runtime{};
        CHECK_EQ(load_game(*loaded, loaded_box, loaded_runtime, save_path), 0);
        for (uint32_t resource = 0; resource < RESOURCE_COUNT; ++resource) {
            CHECK_EQ(loaded->player_stock[0][resource],
                     source->player_stock[0][resource]);
        }
        CHECK_EQ(loaded->player_stock[0][0], 0);
        CHECK_EQ(loaded->player_stock[0][30], 0);
        CHECK_EQ(loaded->player_stock[0][31], 1031);
        std::remove(save_path);
    }
}

void test_index_31_belongs_to_checksum_domain() {
    CHECK_EQ(PLAYER_STOCK_RESOURCE_COUNT, 64u);
    if constexpr (PLAYER_STOCK_RESOURCE_COUNT > 31u) {
        auto state = make_state();
        const uint64_t before = state_checksum_v1(*state);
        state->player_stock[0][31] = 1;
        const uint64_t after = state_checksum_v1(*state);
        CHECK_EQ(before == after, false);
    }
}

}  // namespace

int main() {
    test_named_capacity_and_versions();
    test_player_stock_index_31_round_trip_in_memory();
    test_dropoff_mask_accepts_and_distinguishes_bit_31();
    test_all_definition_cost_vectors_have_resource_count_entries();
    test_index_31_cost_is_deducted_from_stock();
    test_save_load_preserves_all_32_stock_entries_including_zeroes();
    test_index_31_belongs_to_checksum_domain();

    if (g_fails == 0) {
        std::printf("resource_count: OK\n");
        return 0;
    }
    std::printf("resource_count: %d fallos\n", g_fails);
    return 1;
}
