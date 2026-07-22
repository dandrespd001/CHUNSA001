// Test de aggro/persecución v1 (Sprint 0.3+): dos escuadras que empiezan
// FUERA de rango de arma (1.5 tiles) pero DENTRO del radio de aggro (10
// tiles) deben perseguirse, entrar en combate y llegar a la aniquilación de
// un bando — el estancamiento observado en la demo (supervivientes inertes
// fuera de rango) es exactamente lo que este test previene. Autor: Arquitecto.
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "chunsa/game_state.hpp"
#include "chunsa/step.hpp"
#include "chunsa/checksum.hpp"

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

static constexpr uint32_t N_PER_SIDE  = 12;
static constexpr uint32_t TOTAL_TICKS = 2000;

// Escenario: 12 caballería (owner 0) en columna alrededor de x=120 y 12
// artillería (owner 1) alrededor de x=127 — separación de ~7 tiles: fuera de
// range_mt=1500 (1.5 tiles), dentro de AGGRO_RANGE_MT=10000 (10 tiles). Nadie
// recibe MOVE_TO: si se aniquila un bando, fue la persecución del kernel.
static void run_scenario(GameState& g) {
    MatchConfig01A cfg{128u, 2u, 1u, 20u, 20u, 256u, 256u, 11ull};
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
                const uint32_t tile_x = 120u + (i % 2u);   // ∈{120,121}
                const uint32_t tile_y = 122u + (i / 2u);   // ∈[122,127]
                c.p.x_raw      = static_cast<int64_t>(tile_x) * 65536 + 32768;
                c.p.y_raw      = static_cast<int64_t>(tile_y) * 65536 + 32768;
                c.p.speed_mtpt = 150;
                c.p.hp         = 100;
                c.p.attack     = 20;
                c.p.range_mt   = 1500;
                c.p.unit_class = 1;  // cavalry (ventaja RPS vs artillery)
                ++n;
            }
            for (uint32_t i = 0; i < N_PER_SIDE; ++i) {
                RawCommand& c = batch[n];
                std::memset(&c, 0, sizeof(RawCommand));
                c.target_tick  = 0;
                c.emitter      = 1;
                c.type         = CommandType::SPAWN_UNIT;
                c.sequence     = i + 1u;
                const uint32_t tile_x = 127u + (i % 2u);   // ∈{127,128}
                const uint32_t tile_y = 122u + (i / 2u);   // ∈[122,127]
                c.p.x_raw      = static_cast<int64_t>(tile_x) * 65536 + 32768;
                c.p.y_raw      = static_cast<int64_t>(tile_y) * 65536 + 32768;
                c.p.speed_mtpt = 80;
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

    uint32_t cav = 0, art = 0;
    for (uint32_t i = 0; i < g1->entities.capacity; ++i) {
        if (!g1->entities.alive[i]) continue;
        if (g1->unit_class[i] == 1u) ++cav;
        else if (g1->unit_class[i] == 2u) ++art;
    }

    // (2) La persecución arrancó el combate y lo llevó a resolución: la
    // artillería (en desventaja RPS y de velocidad) fue ANIQUILADA. Sin
    // aggro_system este escenario termina 12 vs 12 intactos (fuera de rango).
    CHECK(art == 0);

    // (3) La caballería ganadora conserva supervivientes.
    CHECK(cav > 0);

    const uint64_t checksum1 = state_checksum_v1(*g1);
    delete g1;

    // (4) Determinismo: segunda corrida fresca idéntica → mismo checksum final.
    auto* g2 = new GameState();
    run_scenario(*g2);
    const uint64_t checksum2 = state_checksum_v1(*g2);
    delete g2;

    CHECK(checksum1 == checksum2);

    std::printf("aggro: cav=%u art=%u checksum=%llx\n", cav, art,
                (unsigned long long)checksum1);

    if (g_fails == 0) { std::printf("aggro: OK\n"); return 0; }
    std::printf("aggro: %d fallos\n", g_fails);
    return 1;
}
