#pragma once

#include <cstdint>

// chunsa_sim_core — FlowField v1: Dial determinista + direcciones 8-dir (base §10.4)
// generado: minimax-m3 · revisado: Arquitecto 2026-07-18

namespace chunsa {

inline constexpr uint32_t FF_AXIS = 256;
inline constexpr uint32_t FF_CELLS = FF_AXIS * FF_AXIS;
inline constexpr uint16_t FF_UNREACHABLE = 0xFFFF;
inline constexpr uint8_t  FF_WALL = 255;        // cost==255 ⇒ muro

struct FlowField {
    uint32_t w, h;                   // <= FF_AXIS cada uno
    uint16_t integ[FF_CELLS];        // coste integrado; FF_UNREACHABLE si no alcanzable
    int8_t   dir_x[FF_CELLS];        // -1/0/+1 hacia el goal
    int8_t   dir_y[FF_CELLS];
    // scratch DIAL (excluidos del checksum): push-front LIFO
    uint32_t bucket_head[65536];     // 0xFFFFFFFF = bucket vacío
    uint32_t node_next [FF_CELLS];
};

// 8 direcciones en orden fijo N,NE,E,SE,S,SW,W,NW (N ≡ y-1)
inline constexpr int8_t   FF_DX [8] = { 0, +1, +1, +1,  0, -1, -1, -1};
inline constexpr int8_t   FF_DY [8] = {-1, -1,  0, +1, +1, +1,  0, -1};
inline constexpr uint16_t FF_EC[8]  = {10, 14, 10, 14, 10, 14, 10, 14}; // orto=10, diag=14

inline void ff_compute(FlowField& f, const uint8_t* cost,
                       uint32_t w, uint32_t h,
                       uint32_t goal_x, uint32_t goal_y) noexcept {
    f.w = w;
    f.h = h;
    const uint32_t total = w * h;

    // 1. Reset: integ inalcanzable, dirs a 0, buckets vacíos
    for (uint32_t i = 0; i < total; ++i) {
        f.integ[i] = FF_UNREACHABLE;
        f.dir_x[i] = 0;
        f.dir_y[i] = 0;
    }
    for (uint32_t b = 0; b < 65536u; ++b) {
        f.bucket_head[b] = 0xFFFFFFFFu;
    }

    // Goal fuera de rango o muro ⇒ todo inalcanzable (ya inicializado)
    if (goal_x >= w || goal_y >= h) return;
    const uint32_t goal_idx = goal_y * w + goal_x;
    if (cost[goal_idx] == FF_WALL) return;

    // 2. DIAL: semilla = goal en bucket 0
    f.integ[goal_idx]      = 0;
    f.bucket_head[0]       = goal_idx;
    f.node_next[goal_idx]  = 0xFFFFFFFFu;

    // 3. Procesa buckets 0..65534 en orden ascendente; dentro de cada bucket, LIFO
    //    Saturación: newcost > 65534 ⇒ no relajar (se trata como inalcanzable)
    for (uint32_t d = 0; d < 65535u; ++d) {
        uint32_t curr = f.bucket_head[d];
        while (curr != 0xFFFFFFFFu) {
            const uint32_t next = f.node_next[curr];
            f.bucket_head[d] = next;

            // Entrada obsoleta (stale) — defensivo, mantiene determinismo
            if (f.integ[curr] != static_cast<uint16_t>(d)) {
                curr = next;
                continue;
            }

            const uint32_t cx = curr % w;
            const uint32_t cy = curr / w;

            for (int k = 0; k < 8; ++k) {
                const int32_t nx = static_cast<int32_t>(cx) + FF_DX[k];
                const int32_t ny = static_cast<int32_t>(cy) + FF_DY[k];
                if (nx < 0 || ny < 0 ||
                    nx >= static_cast<int32_t>(w) ||
                    ny >= static_cast<int32_t>(h)) continue;

                const uint32_t nidx =
                    static_cast<uint32_t>(ny) * w + static_cast<uint32_t>(nx);
                if (cost[nidx] == FF_WALL) continue;

                const uint32_t newcost =
                    d + static_cast<uint32_t>(FF_EC[k]) *
                        static_cast<uint32_t>(cost[nidx]);
                if (newcost > 65534u) continue;   // saturado ⇒ inalcanzable

                if (newcost < static_cast<uint32_t>(f.integ[nidx])) {
                    f.integ[nidx] = static_cast<uint16_t>(newcost);
                    // push-front (LIFO determinista) al bucket newcost
                    f.node_next[nidx]      = f.bucket_head[newcost];
                    f.bucket_head[newcost] = nidx;
                }
            }

            curr = f.bucket_head[d];   // siguiente en el mismo bucket
        }
    }

    // 4. Direcciones: gradiente descendente en orden fijo.
    //    Reglas: alcanzable y ≠goal ⇒ vecino con integ estrictamente menor
    //    (empate → gana el PRIMERO del orden N,NE,E,SE,S,SW,W,NW).
    //    Goal ⇒ (0,0). Inalcanzable ⇒ (0,0).
    for (uint32_t i = 0; i < total; ++i) {
        if (f.integ[i] == FF_UNREACHABLE) continue;
        if (i == goal_idx) continue;             // goal ya tiene (0,0)

        const uint32_t cx = i % w;
        const uint32_t cy = i / w;
        uint16_t best = f.integ[i];
        int best_k = -1;

        for (int k = 0; k < 8; ++k) {
            const int32_t nx = static_cast<int32_t>(cx) + FF_DX[k];
            const int32_t ny = static_cast<int32_t>(cy) + FF_DY[k];
            if (nx < 0 || ny < 0 ||
                nx >= static_cast<int32_t>(w) ||
                ny >= static_cast<int32_t>(h)) continue;

            const uint32_t nidx =
                static_cast<uint32_t>(ny) * w + static_cast<uint32_t>(nx);
            if (f.integ[nidx] == FF_UNREACHABLE) continue;

            // estrictamente menor; empates no actualizan → gana el primero
            if (f.integ[nidx] < best) {
                best   = f.integ[nidx];
                best_k = k;
            }
        }

        if (best_k >= 0) {
            f.dir_x[i] = FF_DX[best_k];
            f.dir_y[i] = FF_DY[best_k];
        }
        // Si best_k == -1 (caso degenerado aislado), se queda en (0,0).
    }
}

inline uint64_t ff_checksum(const FlowField& f) noexcept {
    constexpr uint64_t OFFSET = 0xcbf29ce484222325ULL;
    constexpr uint64_t PRIME  = 0x100000001b3ULL;     // FNV-1a 64

    uint64_t hash = OFFSET;
    const uint32_t total = f.w * f.h;

    // Por índice 0..total-1: integ (2 B LE) + dir_x (1 B) + dir_y (1 B).
    // NO se incluyen los campos scratch (bucket_head, node_next).
    for (uint32_t i = 0; i < total; ++i) {
        const uint16_t iv = f.integ[i];
        const uint8_t  b0 = static_cast<uint8_t>( iv        & 0xFFu);
        const uint8_t  b1 = static_cast<uint8_t>((iv >> 8)  & 0xFFu);
        hash = (hash ^ b0) * PRIME;
        hash = (hash ^ b1) * PRIME;
        const uint8_t dx = static_cast<uint8_t>(f.dir_x[i]);
        const uint8_t dy = static_cast<uint8_t>(f.dir_y[i]);
        hash = (hash ^ dx) * PRIME;
        hash = (hash ^ dy) * PRIME;
    }
    return hash;
}

} // namespace chunsa