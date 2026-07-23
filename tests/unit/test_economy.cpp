// Test de economía mínima v1 (Sprint 0.3, base §3.4): ciudadanos recolectan de
// un depósito y entregan en el dropoff de su jugador. Autor: Arquitecto
// (economy.hpp: minimax-m3; wiring: Arquitecto).
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "chunsa/game_state.hpp"
#include "chunsa/step.hpp"
#include "chunsa/checksum.hpp"

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

static constexpr uint32_t N_CITIZENS = 8;
static constexpr uint32_t TOTAL_TICKS = 500;

// Escenario: N_CITIZENS ciudadanos del owner 0 spawean cerca del depósito de
// Alimentos en (40,40) (ver gs_init_economy) y deben recolectar+entregar en el
// dropoff del owner 0 (tile ~20). Con velocidad alta para converger rápido.
static void run_scenario(GameState& g) {
    // Sprint 0.4: SPAWN_CITIZEN es data-driven por defecto; este test ejercita
    // el camino debug legado (hp=20 hardcodeado), por lo que activa
    // allow_debug_stat_payload y marca unit_id=INVALID en el comando.
    MatchConfig01A cfg{256u, 2u, 1u, 20u, 20u, 256u, 256u, 5ull, 1u};
    gs_init(g, cfg);

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

int main() {
    auto* g1 = new GameState();
    run_scenario(*g1);

    CHECK(g1->fatal == FatalReason::NONE);

    // (2) La economía funcionó: el jugador 0 acumuló Alimentos (índice 0).
    CHECK(g1->player_stock[0][0] > 0);

    // (3) El depósito de Alimentos más cercano (índice 0, en (40,40)) se agotó
    // al menos parcialmente: remaining < 500 (se extrajo algo de él).
    CHECK(g1->deposits[0].remaining < 500);

    // (4) Ningún ciudadano se perdió: los 8 siguen vivos (no hay mecanismo de
    // muerte para citizens en v1; si esto falla, algo mató a un ciudadano).
    uint32_t alive_citizens = 0;
    for (uint32_t i = 0; i < g1->entities.capacity; ++i) {
        if (g1->entities.alive[i] && g1->unit_class[i] == 3) ++alive_citizens;
    }
    CHECK(alive_citizens == N_CITIZENS);

    const uint64_t checksum1 = state_checksum_v1(*g1);
    const int64_t stock0 = g1->player_stock[0][0];
    delete g1;

    // (5) Determinismo: segunda corrida fresca idéntica → mismo checksum final.
    auto* g2 = new GameState();
    run_scenario(*g2);
    const uint64_t checksum2 = state_checksum_v1(*g2);
    delete g2;

    CHECK(checksum1 == checksum2);

    std::printf("economy: stock_A=%lld deposit0_remaining_ok checksum=%llx\n",
                static_cast<long long>(stock0), (unsigned long long)checksum1);

    if (g_fails == 0) { std::printf("economy: OK\n"); return 0; }
    std::printf("economy: %d fallos\n", g_fails);
    return 1;
}
