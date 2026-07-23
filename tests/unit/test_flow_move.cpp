// Test de integración FlowField→MovementSystem (Sprint 0.2): unidades marchan
// rodeando un muro siguiendo el campo de flujo hasta colarse por un hueco.
// Autor: sonnet-5 (contrato cerrado del Arquitecto).
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "chunsa/game_state.hpp"
#include "chunsa/step.hpp"
#include "chunsa/checksum.hpp"

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

static constexpr uint32_t N_UNITS     = 200;
static constexpr uint32_t TOTAL_TICKS = 600;

// Escenario: N_UNITS nacen a la izquierda (tiles x∈[8,40], y∈[112,144]) en el
// tick 0; en el tick 1 el emisor 0 emite FLOW_MOVE hacia el lado derecho
// (tile 220,128), detrás de un muro vertical con un hueco (ver gs_init_cost_grid).
static void run_scenario(GameState& g) {
    MatchConfig01A cfg{N_UNITS + 16u, 1u, 1u, 20u, 20u, 256u, 256u, 99ull, 0u};
    gs_init(g, cfg);

    static RawCommand batch[N_UNITS];

    for (uint32_t t = 0; t < TOTAL_TICKS; ++t) {
        uint32_t n = 0;
        if (t == 0u) {
            for (uint32_t i = 0; i < N_UNITS; ++i) {
                RawCommand& c = batch[n];
                std::memset(&c, 0, sizeof(RawCommand));
                c.target_tick  = 0;
                c.emitter      = 0;
                c.type         = CommandType::SPAWN_DEBUG;
                c.sequence     = i + 1u;
                c.p.handle     = EntityHandle{i, 1u};
                const uint32_t tile_x = 8u + (i % 33u);          // ∈[8,40]
                const uint32_t tile_y = 112u + ((i * 17u) % 33u); // ∈[112,144]
                c.p.x_raw      = static_cast<int64_t>(tile_x) * 65536 + 32768;
                c.p.y_raw      = static_cast<int64_t>(tile_y) * 65536 + 32768;
                c.p.speed_mtpt = 200;
                ++n;
            }
        } else if (t == 1u) {
            RawCommand& c = batch[n];
            std::memset(&c, 0, sizeof(RawCommand));
            c.target_tick  = 1;
            c.emitter      = 0;
            c.type         = CommandType::FLOW_MOVE;
            c.sequence     = N_UNITS + 1u;
            c.p.x_raw      = static_cast<int64_t>(220) * 65536 + 32768;
            c.p.y_raw      = static_cast<int64_t>(128) * 65536 + 32768;
            ++n;
        }
        step(g, batch, n);
    }
}

int main() {
    auto* g1 = new GameState();
    run_scenario(*g1);

    CHECK(g1->fatal == FatalReason::NONE);

    // Rodeo: ninguna unidad viva termina sobre un tile-muro.
    // Progreso: al menos 70% cruzaron al lado del goal (solo posible por el hueco).
    uint32_t alive_count = 0, crossed = 0;
    for (uint32_t i = 0; i < g1->entities.capacity; ++i) {
        if (!g1->entities.alive[i]) continue;
        ++alive_count;
        const uint32_t tx = static_cast<uint32_t>(g1->pos_x[i] >> 16);
        const uint32_t ty = static_cast<uint32_t>(g1->pos_y[i] >> 16);
        const uint32_t cell = ty * FF_AXIS + tx;
        CHECK(g1->cost_grid[cell] != FF_WALL);
        if (g1->pos_x[i] > static_cast<int64_t>(128) * 65536) ++crossed;
    }
    CHECK(alive_count == N_UNITS);
    CHECK(static_cast<uint64_t>(crossed) * 100ull >= static_cast<uint64_t>(alive_count) * 70ull);

    const uint64_t checksum1 = state_checksum_v1(*g1);
    delete g1;

    // Determinismo: segunda corrida idéntica desde cero → mismo checksum final.
    auto* g2 = new GameState();
    run_scenario(*g2);
    const uint64_t checksum2 = state_checksum_v1(*g2);
    delete g2;

    CHECK(checksum1 == checksum2);

    std::printf("flow_move: alive=%u crossed=%u checksum=%llx\n",
                alive_count, crossed, (unsigned long long)checksum1);

    if (g_fails == 0) { std::printf("flow_move: OK\n"); return 0; }
    std::printf("flow_move: %d fallos\n", g_fails);
    return 1;
}
