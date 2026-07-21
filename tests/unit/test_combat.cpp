// Test de combate RPS v1 (Sprint 0.3): choque de dos bandos (caballería vs
// artillería) que se buscan por el spatial hash y se dañan según el
// triángulo Piedra-Papel-Tijera. Autor: sonnet-5 (contrato cerrado del
// Arquitecto).
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "chunsa/game_state.hpp"
#include "chunsa/step.hpp"
#include "chunsa/checksum.hpp"

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

static constexpr uint32_t N_PER_SIDE  = 60;
static constexpr uint32_t TOTAL_TICKS = 400;

// Escenario: 60 caballería (owner 0) en x∈[120,130] y∈[120,136), y 60
// artillería (owner 1) en x∈[126,136] y∈[120,136), solapando en la franja
// x∈[126,130] para que entren en rango (range_mt=1500 = 1.5 tiles).
static void run_scenario(GameState& g) {
    MatchConfig01A cfg{512u, 2u, 1u, 20u, 20u, 256u, 256u, 7ull};
    gs_init(g, cfg);

    static RawCommand batch[2 * N_PER_SIDE];

    for (uint32_t t = 0; t < TOTAL_TICKS; ++t) {
        uint32_t n = 0;
        if (t == 0u) {
            for (uint32_t i = 0; i < N_PER_SIDE; ++i) {
                RawCommand& c = batch[n];
                std::memset(&c, 0, sizeof(RawCommand));
                c.target_tick  = 0;
                c.emitter      = 0;
                c.type         = CommandType::SPAWN_UNIT;
                c.sequence     = i + 1u;
                c.p.handle     = EntityHandle{i, 1u};
                const uint32_t tile_x = 120u + (i % 11u);          // ∈[120,130]
                const uint32_t tile_y = 120u + (i / 11u);          // ∈[120,125]
                c.p.x_raw      = static_cast<int64_t>(tile_x) * 65536 + 32768;
                c.p.y_raw      = static_cast<int64_t>(tile_y) * 65536 + 32768;
                c.p.hp         = 100;
                c.p.attack     = 20;
                c.p.range_mt   = 1500;
                c.p.unit_class = 1;  // cavalry
                ++n;
            }
            for (uint32_t i = 0; i < N_PER_SIDE; ++i) {
                RawCommand& c = batch[n];
                std::memset(&c, 0, sizeof(RawCommand));
                c.target_tick  = 0;
                c.emitter      = 1;
                c.type         = CommandType::SPAWN_UNIT;
                c.sequence     = i + 1u;
                c.p.handle     = EntityHandle{N_PER_SIDE + i, 1u};
                const uint32_t tile_x = 126u + (i % 11u);          // ∈[126,136]
                const uint32_t tile_y = 120u + (i / 11u);          // ∈[120,125]
                c.p.x_raw      = static_cast<int64_t>(tile_x) * 65536 + 32768;
                c.p.y_raw      = static_cast<int64_t>(tile_y) * 65536 + 32768;
                c.p.hp         = 100;
                c.p.attack     = 20;
                c.p.range_mt   = 1500;
                c.p.unit_class = 2;  // artillery
                ++n;
            }
        }
        step(g, batch, n);
    }
}

int main() {
    auto* g1 = new GameState();
    run_scenario(*g1);

    CHECK(g1->fatal == FatalReason::NONE);

    uint32_t alive0 = 0, alive1 = 0;
    for (uint32_t i = 0; i < g1->entities.capacity; ++i) {
        if (!g1->entities.alive[i]) continue;
        if (g1->owner[i] == 0u) ++alive0;
        else if (g1->owner[i] == 1u) ++alive1;
    }
    const uint32_t total_alive = alive0 + alive1;

    CHECK(alive0 > alive1);                       // ventaja RPS de la caballería
    CHECK(total_alive < 2u * N_PER_SIDE);          // el combate ocurrió

    const uint64_t checksum1 = state_checksum_v1(*g1);
    delete g1;

    // Determinismo: segunda corrida fresca idéntica → mismo checksum final.
    auto* g2 = new GameState();
    run_scenario(*g2);
    const uint64_t checksum2 = state_checksum_v1(*g2);
    delete g2;

    CHECK(checksum1 == checksum2);

    std::printf("combat: owner0=%u owner1=%u checksum=%llx\n",
                alive0, alive1, (unsigned long long)checksum1);

    if (g_fails == 0) { std::printf("combat: OK\n"); return 0; }
    std::printf("combat: %d fallos\n", g_fails);
    return 1;
}
