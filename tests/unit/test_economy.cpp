// Test de economía mínima v1 (Sprint 0.3, base §3.4): ciudadanos recolectan de
// un depósito y entregan en el dropoff de su jugador. Autor: Arquitecto
// (economy.hpp: minimax-m3; wiring: Arquitecto).
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>

#include "chunsa/game_state.hpp"
#include "chunsa/step.hpp"
#include "chunsa/checksum.hpp"

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

static constexpr uint32_t N_CITIZENS = 8;
static constexpr uint32_t TOTAL_TICKS = 500;

static BuildingDefinitionV1 make_economy_zone_center() {
    BuildingDefinitionV1 d{};
    d.id = 0u;
    d.hp = 500;
    d.footprint_w = 2u;
    d.footprint_h = 2u;
    d.build_time_ticks = 0u;
    d.epoch_min = 1u;
    d.epoch_max = 15u;
    return d;
}

static BuildingDefinitionV1 g_economy_buildings[1] = {
    make_economy_zone_center()
};

static DataCatalogV1 make_economy_catalog() {
    DataCatalogV1 cat{};
    cat.building_count = 1u;
    cat.buildings = g_economy_buildings;
    return cat;
}

// Escenario: N_CITIZENS ciudadanos del owner 0 spawean cerca del depósito de
// Alimentos en (40,40) (ver gs_init_economy) y deben recolectar+entregar en el
// dropoff del owner 0 (tile ~20). Con velocidad alta para converger rápido.
static void run_scenario(GameState& g) {
    // Sprint 0.4: SPAWN_CITIZEN es data-driven por defecto; este test ejercita
    // el camino debug legado (hp=20 hardcodeado), por lo que activa
    // allow_debug_stat_payload y marca unit_id=INVALID en el comando.
    MatchConfig01A cfg{256u, 2u, 1u, 20u, 20u, 256u, 256u, 5ull, 1u};
    gs_init(g, cfg);
    static const DataCatalogV1 cat = make_economy_catalog();
    gs_bind_catalog(g, cat);

    // SPEC-004 §23: el fixture económico necesita una zona aliada real. El
    // centro completo se sitúa sobre el depósito legacy 0; no altera el
    // dropoff (mask=0), pero habilita la auto-asignación dentro de 32 tiles.
    const EntityHandle center = et_spawn(g.entities);
    CHECK(center.index == 0u);
    if (handle_eq(center, NULL_HANDLE)) return;
    g.entity_kind[center.index] = 1u;
    g.building_id[center.index] = 0u;
    g.build_progress[center.index] = 0u;
    g.owner[center.index] = 0u;
    g.unit_class[center.index] = 255u;
    g.hp[center.index] = g.max_hp[center.index] = 500;
    g.pos_x[center.index] = g.deposits[0].x_raw;
    g.pos_y[center.index] = g.deposits[0].y_raw;

    static RawCommand batch[N_CITIZENS];

    for (uint32_t t = 0; t < TOTAL_TICKS; ++t) {
        uint32_t n = 0;
        if (t == 0u) {
            for (uint32_t i = 0; i < N_CITIZENS; ++i) {
                RawCommand& c = batch[n];
                std::memset(&c, 0, sizeof(RawCommand));
                c.target_tick  = 0;
                c.emitter      = 0;
                c.type         = CommandType::SPAWN_CITIZEN;
                c.sequence     = i + 1u;
                c.p.handle     = EntityHandle{i, 1u};
                // Cerca del depósito de A en tile (40,40): dispersos en un cuadro pequeño.
                const uint32_t tile_x = 36u + (i % 4u);
                const uint32_t tile_y = 36u + (i / 4u);
                c.p.x_raw      = static_cast<int64_t>(tile_x) * 65536 + 32768;
                c.p.y_raw      = static_cast<int64_t>(tile_y) * 65536 + 32768;
                c.p.speed_mtpt = 800;  // rápido: converge en pocos ticks
                c.p.unit_id    = INVALID_UNIT_ID;  // camino debug (Sprint 0.4)
                ++n;
            }
        }
        step(g, batch, n);
    }
}

static EcoCitizenIn citizen_at(int64_t x_raw, int64_t y_raw,
                               EcoState state, uint32_t assigned,
                               int32_t carry = 0, uint8_t resource = 0) {
    EcoCitizenIn in{};
    in.pos_x = x_raw;
    in.pos_y = y_raw;
    in.state = state;
    in.assigned_deposit = assigned;
    in.carry = carry;
    in.carry_resource_idx = resource;
    in.speed_mtpt = 100;
    return in;
}

static void test_nearest_tie_and_invalid_retarget() {
    constexpr int64_t T = FX_ONE_RAW;
    const EcoDeposit deposits[2] = {
        {-2 * T, 0, 0, 50, 0, 0, 0, 0, 0},
        { 2 * T, 0, 1, 50, 0, 0, 0, 0, 0},
    };
    FatalReason fatal = FatalReason::NONE;

    CHECK(eco_find_nearest_deposit(
        deposits, 2, 0, 0, ECO_ANY_RESOURCE, ECO_ALL_DEPOSITS_MASK, fatal) == 0u);

    const EcoCitizenIn invalid = citizen_at(0, 0, EcoState::SEEK, 99u);
    const EcoCitizenOut out =
        eco_step_citizen(
            invalid, deposits, 2, ECO_ALL_DEPOSITS_MASK, 20 * T, 20 * T, fatal);
    CHECK(fatal == FatalReason::NONE);
    CHECK(out.assigned_deposit == 0u);
    CHECK(out.state == EcoState::SEEK);
}

static void test_exhaustion_retargets_deterministically() {
    constexpr int64_t T = FX_ONE_RAW;
    const EcoDeposit deposits[2] = {
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        {2 * T, 0, 1, 50, 0, 0, 0, 0, 0},
    };
    FatalReason fatal = FatalReason::NONE;

    const EcoCitizenIn harvesting =
        citizen_at(0, 0, EcoState::HARVEST, 0u);
    const EcoCitizenOut exhausted =
        eco_step_citizen(
            harvesting, deposits, 2, ECO_ALL_DEPOSITS_MASK, 20 * T, 20 * T, fatal);
    CHECK(exhausted.state == EcoState::SEEK);
    CHECK(exhausted.assigned_deposit == 0u);
    CHECK(!exhausted.did_harvest);

    EcoCitizenIn seeking = citizen_at(exhausted.pos_x, exhausted.pos_y,
                                      exhausted.state,
                                      exhausted.assigned_deposit,
                                      exhausted.carry,
                                      exhausted.carry_resource_idx);
    const EcoCitizenOut retargeted =
        eco_step_citizen(
            seeking, deposits, 2, ECO_ALL_DEPOSITS_MASK, 20 * T, 20 * T, fatal);
    CHECK(fatal == FatalReason::NONE);
    CHECK(retargeted.assigned_deposit == 1u);
    CHECK(retargeted.state == EcoState::SEEK);
}

static void test_harvest_clamps_and_exact_dropoff() {
    constexpr int64_t T = FX_ONE_RAW;
    FatalReason fatal = FatalReason::NONE;

    {
        const EcoDeposit deposits[1] = {{0, 0, 2, 3, 0, 0, 0, 0, 0}};
        const EcoCitizenIn harvesting =
            citizen_at(0, 0, EcoState::HARVEST, 0u);
        const EcoCitizenOut out =
            eco_step_citizen(
                harvesting, deposits, 1, ECO_ALL_DEPOSITS_MASK,
                20 * T, 20 * T, fatal);
        CHECK(out.did_harvest);
        CHECK(out.harvested_amount == 3);
        CHECK(out.carry == 3);
        CHECK(out.carry_resource_idx == 2u);
    }

    {
        const EcoDeposit deposits[1] = {{0, 0, 1, 100, 0, 0, 0, 0, 0}};
        const EcoCitizenIn almost_full =
            citizen_at(0, 0, EcoState::HARVEST, 0u, ECO_CARRY_CAP - 1, 1u);
        const EcoCitizenOut out =
            eco_step_citizen(
                almost_full, deposits, 1, ECO_ALL_DEPOSITS_MASK,
                20 * T, 20 * T, fatal);
        CHECK(out.did_harvest);
        CHECK(out.harvested_amount == 1);
        CHECK(out.carry == ECO_CARRY_CAP);
        CHECK(out.state == EcoState::RETURN);
    }

    {
        const EcoDeposit deposits[1] = {{0, 0, 0, 50, 0, 0, 0, 0, 0}};
        const EcoCitizenIn returning =
            citizen_at(10 * T, 10 * T, EcoState::RETURN, 0u, 37, 2u);
        const EcoCitizenOut out =
            eco_step_citizen(
                returning, deposits, 1, ECO_ALL_DEPOSITS_MASK,
                10 * T, 10 * T, fatal);
        CHECK(out.did_dropoff);
        CHECK(out.dropoff_amount == 37);
        CHECK(out.dropoff_resource_idx == 2u);
        CHECK(out.carry == 0);
        CHECK(out.state == EcoState::SEEK);
    }

    CHECK(fatal == FatalReason::NONE);
}

int main() {
    test_nearest_tie_and_invalid_retarget();
    test_exhaustion_retargets_deterministically();
    test_harvest_clamps_and_exact_dropoff();

    auto g1 = std::make_unique<GameState>();
    run_scenario(*g1);

    CHECK(g1->fatal == FatalReason::NONE);

    // (2) La economía funcionó: el jugador 0 acumuló Alimentos (índice 0).
    CHECK(g1->player_stock[0][0] > 0);

    // (3) El depósito de Alimentos más cercano (índice 0, en (40,40)) se agotó
    // al menos parcialmente: remaining < 500 (se extrajo algo de él).
    CHECK(g1->deposits[0].remaining < 500);

    // (4) Ningún ciudadano se perdió: los 8 siguen vivos. Este escenario NUNCA
    // spawnea al owner 1 (sin comandos de emitter=1 en ningún tick), así que
    // no hay ningún enemigo vivo que pueda alcanzarlos — el guard
    // `owner[j]==owner[i]` de combat_system/aggro_system los excluye de
    // cualquier interacción entre sí. Desde SPEC-004 §7.1 (Sprint 1.4-cierre)
    // SÍ existe mecanismo de muerte para citizens en presencia de un enemigo
    // real (ver tests/unit/test_combat.cpp: test_citizen_is_vulnerable_target)
    // — aquí sigue valiendo el check porque el escenario, por construcción,
    // no tiene ningún atacante enemigo.
    uint32_t alive_citizens = 0;
    for (uint32_t i = 0; i < g1->entities.capacity; ++i) {
        if (g1->entities.alive[i] && g1->unit_class[i] == 3) ++alive_citizens;
    }
    CHECK(alive_citizens == N_CITIZENS);

    const uint64_t checksum1 = state_checksum_v1(*g1);
    const int64_t stock0 = g1->player_stock[0][0];
    g1.reset();

    // (5) Determinismo: segunda corrida fresca idéntica → mismo checksum final.
    auto g2 = std::make_unique<GameState>();
    run_scenario(*g2);
    const uint64_t checksum2 = state_checksum_v1(*g2);
    g2.reset();

    CHECK(checksum1 == checksum2);

    std::printf("economy: stock_A=%lld deposit0_remaining_ok checksum=%llx\n",
                static_cast<long long>(stock0), (unsigned long long)checksum1);

    if (g_fails == 0) { std::printf("economy: OK\n"); return 0; }
    std::printf("economy: %d fallos\n", g_fails);
    return 1;
}
