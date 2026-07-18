#pragma once

// chunsa_sim_core — Vision v1: visible+explored por jugador, bitsets deterministas (SPEC-001 §8)
// generado: minimax-m3 · revisado: Arquitecto 2026-07-18 (campos owner/pos corregidos: viven en GameState)

#include <cstdint>
// (sin include de game_state: la actualización por fase vive en step.hpp — evita ciclo)

namespace chunsa {

inline constexpr uint32_t VIS_AXIS      = 256;                       // 1 celda = 1 tile
inline constexpr uint32_t VIS_WORDS     = VIS_AXIS * VIS_AXIS / 64;  // 1024 u64 por jugador
inline constexpr uint32_t VIS_MAX_PLAYERS = 8;

struct VisionGrid {
    uint32_t w, h;                                                  // tamaño útil del mapa (<= VIS_AXIS)
    uint64_t visible [VIS_MAX_PLAYERS][VIS_WORDS];                  // se borra cada tick de visión
    uint64_t explored[VIS_MAX_PLAYERS][VIS_WORDS];                  // acumulativo (niebla)
};

// 1. Inicializa: fija dimensiones y limpia ambos bitsets.
inline void vis_init(VisionGrid& v, uint32_t map_tiles_x, uint32_t map_tiles_y) noexcept {
    v.w = map_tiles_x;
    v.h = map_tiles_y;
    for (uint32_t p = 0; p < VIS_MAX_PLAYERS; ++p) {
        for (uint32_t i = 0; i < VIS_WORDS; ++i) {
            v.visible [p][i] = 0;
            v.explored[p][i] = 0;
        }
    }
}

// 2. Índice de bit global. Stride fijo = VIS_AXIS (layout independiente del mapa).
inline uint32_t vis_cell(const VisionGrid&, uint32_t tx, uint32_t ty) noexcept {
    return ty * VIS_AXIS + tx;
}

// 3. Lecturas seguras con bounds-check.
inline bool vis_visible(const VisionGrid& v, uint8_t player, uint32_t tx, uint32_t ty) noexcept {
    if (player >= VIS_MAX_PLAYERS) return false;
    if (tx >= v.w || ty >= v.h)    return false;
    const uint32_t bit  = ty * VIS_AXIS + tx;
    const uint32_t word = bit >> 6;
    const uint64_t mask = uint64_t{1} << (bit & 63);
    return (v.visible[player][word] & mask) != 0;
}

inline bool vis_explored(const VisionGrid& v, uint8_t player, uint32_t tx, uint32_t ty) noexcept {
    if (player >= VIS_MAX_PLAYERS) return false;
    if (tx >= v.w || ty >= v.h)    return false;
    const uint32_t bit  = ty * VIS_AXIS + tx;
    const uint32_t word = bit >> 6;
    const uint64_t mask = uint64_t{1} << (bit & 63);
    return (v.explored[player][word] & mask) != 0;
}

// 4. Marca disco entero de radio r (en tiles) en visible y explored.
//    Centro desde Q47.16 -> tile (shift aritmético /65536). Recorrido dy,dx ASC.
inline void vis_mark_circle(VisionGrid& v, uint8_t player,
                            int64_t x_raw, int64_t y_raw, uint32_t radius_tiles) noexcept {
    if (player >= VIS_MAX_PLAYERS) return;
    if (v.w == 0 || v.h == 0)      return;

    // Centro en tiles (x_raw >= 0 por cota de mundo; clamp defensivo).
    int64_t cx = x_raw >> 16;
    int64_t cy = y_raw >> 16;
    if (cx < 0) cx = 0;
    if (cy < 0) cy = 0;
    if (cx >= static_cast<int64_t>(v.w)) cx = static_cast<int64_t>(v.w) - 1;
    if (cy >= static_cast<int64_t>(v.h)) cy = static_cast<int64_t>(v.h) - 1;

    const uint32_t cx_u = static_cast<uint32_t>(cx);
    const uint32_t cy_u = static_cast<uint32_t>(cy);
    const int64_t  r    = static_cast<int64_t>(radius_tiles);
    const int64_t  r2   = r * r;

    for (int64_t dy = -r; dy <= r; ++dy) {
        const int64_t dy2 = dy * dy;
        const int64_t ty64 = static_cast<int64_t>(cy_u) + dy;
        if (ty64 < 0 || ty64 >= static_cast<int64_t>(v.h)) continue;
        for (int64_t dx = -r; dx <= r; ++dx) {
            if (dx * dx + dy2 > r2) continue;
            const int64_t tx64 = static_cast<int64_t>(cx_u) + dx;
            if (tx64 < 0 || tx64 >= static_cast<int64_t>(v.w)) continue;
            const uint32_t tx   = static_cast<uint32_t>(tx64);
            const uint32_t ty   = static_cast<uint32_t>(ty64);
            const uint32_t bit  = ty * VIS_AXIS + tx;
            const uint32_t word = bit >> 6;
            const uint64_t mask = uint64_t{1} << (bit & 63);
            v.visible [player][word] |= mask;
            v.explored[player][word] |= mask;
        }
    }
}

// 6. FNV-1a 64 sobre w,h (LE u32 c/u) + por player visible[], luego explored[] (LE u64 c/u).
inline uint64_t vis_checksum(const VisionGrid& v) noexcept {
    constexpr uint64_t OFFSET = 0xcbf29ce484222325ULL;
    constexpr uint64_t PRIME  = 0x100000001b3ULL;
    uint64_t h = OFFSET;

    auto mix_u32 = [&](uint32_t x) noexcept {
        for (uint32_t k = 0; k < 4; ++k) {
            h ^= static_cast<uint64_t>((x >> (k * 8)) & 0xFFu);
            h *= PRIME;
        }
    };
    auto mix_u64 = [&](uint64_t x) noexcept {
        for (uint32_t k = 0; k < 8; ++k) {
            h ^= static_cast<uint64_t>((x >> (k * 8)) & 0xFFu);
            h *= PRIME;
        }
    };

    mix_u32(v.w);
    mix_u32(v.h);

    for (uint32_t p = 0; p < VIS_MAX_PLAYERS; ++p) {
        for (uint32_t i = 0; i < VIS_WORDS; ++i) {
            mix_u64(v.visible[p][i]);
        }
    }
    for (uint32_t p = 0; p < VIS_MAX_PLAYERS; ++p) {
        for (uint32_t i = 0; i < VIS_WORDS; ++i) {
            mix_u64(v.explored[p][i]);
        }
    }
    return h;
}

} // namespace chunsa
