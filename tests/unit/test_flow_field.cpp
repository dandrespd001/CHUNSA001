// Test del FlowField v1: caso 3x3 exacto, descenso monótono en mapa con muros,
// y estabilidad bit-exacta doble corrida. Autor: Arquitecto.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "chunsa/flow_field.hpp"
#include "chunsa/rng.hpp"

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)
using namespace chunsa;

static FlowField ff_a, ff_b;   // ~1MB c/u: estáticos, no en stack

int main() {
    // 1) 3x3 uniforme, goal centro: ortogonales=10, diagonales=14, dirs al centro.
    {
        uint8_t cost[9]; std::memset(cost, 1, 9);
        ff_compute(ff_a, cost, 3, 3, 1, 1);
        CHECK(ff_a.integ[4] == 0);
        CHECK(ff_a.integ[1] == 10 && ff_a.integ[3] == 10 && ff_a.integ[5] == 10 && ff_a.integ[7] == 10);
        CHECK(ff_a.integ[0] == 14 && ff_a.integ[2] == 14 && ff_a.integ[6] == 14 && ff_a.integ[8] == 14);
        for (uint32_t i = 0; i < 9; ++i) {
            if (i == 4) { CHECK(ff_a.dir_x[4] == 0 && ff_a.dir_y[4] == 0); continue; }
            const int32_t x = int32_t(i % 3) + ff_a.dir_x[i];
            const int32_t y = int32_t(i / 3) + ff_a.dir_y[i];
            CHECK(x == 1 && y == 1);
        }
    }
    // 2) Mapa 64x64 con muros pseudoaleatorios deterministas: desde toda celda
    //    alcanzable, seguir dirs llega al goal en <64*64 pasos con integ decreciente.
    {
        const uint32_t W = 64, H = 64;
        static uint8_t cost[64 * 64];
        FatalReason f = FatalReason::NONE;
        for (uint32_t i = 0; i < W * H; ++i)
            cost[i] = (rng_range(7u, 1u, 0u, i, 20u, 0u, 100u, f) < 20u) ? FF_WALL : uint8_t(1 + i % 3);
        const uint32_t gx = 5, gy = 60; cost[gy * W + gx] = 1;
        ff_compute(ff_a, cost, W, H, gx, gy);
        uint32_t reachable = 0, ok_paths = 0;
        for (uint32_t i = 0; i < W * H; ++i) {
            if (ff_a.integ[i] == FF_UNREACHABLE) continue;
            ++reachable;
            uint32_t x = i % W, y = i / W, steps = 0;
            uint16_t prev = 0xFFFF;
            bool ok = true;
            while (!(x == gx && y == gy) && steps < W * H) {
                const uint32_t c = y * W + x;
                if (ff_a.integ[c] >= prev) { ok = false; break; }   // debe DECRECER estrictamente
                prev = ff_a.integ[c];
                const int8_t dx = ff_a.dir_x[c], dy = ff_a.dir_y[c];
                if (dx == 0 && dy == 0) { ok = false; break; }
                x = uint32_t(int32_t(x) + dx); y = uint32_t(int32_t(y) + dy);
                ++steps;
            }
            if (ok && x == gx && y == gy) ++ok_paths;
        }
        CHECK(reachable > 2000);
        CHECK(ok_paths == reachable);
        // 3) doble corrida bit-exacta
        ff_compute(ff_b, cost, W, H, gx, gy);
        CHECK(ff_checksum(ff_a) == ff_checksum(ff_b));
        std::printf("flow_field: %u alcanzables, %u caminos OK, checksum=%llx\n",
                    reachable, ok_paths, (unsigned long long)ff_checksum(ff_a));
    }
    if (g_fails == 0) { std::printf("flow_field: OK\n"); return 0; }
    std::printf("flow_field: %d fallos\n", g_fails);
    return 1;
}
