#pragma once

#include <cstdint>

#include <chunsa/vision.hpp>

namespace chunsa::presentation {

enum class FogLevel : uint8_t {
    UNEXPLORED = 0,
    EXPLORED = 1,
    VISIBLE = 2,
};

inline FogLevel fog_level_at(const uint64_t* visible,
                             const uint64_t* explored,
                             uint32_t map_w,
                             uint32_t map_h,
                             uint32_t tx,
                             uint32_t ty) noexcept {
    if (visible == nullptr || explored == nullptr ||
        tx >= map_w || ty >= map_h ||
        tx >= VIS_AXIS || ty >= VIS_AXIS) {
        return FogLevel::UNEXPLORED;
    }

    const uint32_t bit = ty * VIS_AXIS + tx;
    const uint32_t word = bit >> 6;
    const uint64_t mask = uint64_t{1} << (bit & 63u);
    if ((visible[word] & mask) != 0u) {
        return FogLevel::VISIBLE;
    }
    if ((explored[word] & mask) != 0u) {
        return FogLevel::EXPLORED;
    }
    return FogLevel::UNEXPLORED;
}

inline bool entity_visible_to_player(uint8_t entity_owner,
                                     uint8_t viewer,
                                     const uint64_t* visible,
                                     uint32_t map_w,
                                     uint32_t map_h,
                                     int64_t x_raw,
                                     int64_t y_raw) noexcept {
    if (entity_owner == viewer) {
        return true;
    }
    if (visible == nullptr || x_raw < 0 || y_raw < 0) {
        return false;
    }

    const int64_t tx_raw = x_raw >> 16;
    const int64_t ty_raw = y_raw >> 16;
    if (tx_raw >= static_cast<int64_t>(map_w) ||
        ty_raw >= static_cast<int64_t>(map_h) ||
        tx_raw >= static_cast<int64_t>(VIS_AXIS) ||
        ty_raw >= static_cast<int64_t>(VIS_AXIS)) {
        return false;
    }

    const uint32_t tx = static_cast<uint32_t>(tx_raw);
    const uint32_t ty = static_cast<uint32_t>(ty_raw);
    const uint32_t bit = ty * VIS_AXIS + tx;
    const uint32_t word = bit >> 6;
    const uint64_t mask = uint64_t{1} << (bit & 63u);
    return (visible[word] & mask) != 0u;
}

inline FogLevel fog_block_level(const uint64_t* visible,
                                const uint64_t* explored,
                                uint32_t map_w,
                                uint32_t map_h,
                                uint32_t block_tx,
                                uint32_t block_ty,
                                uint32_t block_size) noexcept {
    if (visible == nullptr || explored == nullptr || block_size == 0u ||
        block_tx >= map_w || block_ty >= map_h ||
        block_tx >= VIS_AXIS || block_ty >= VIS_AXIS) {
        return FogLevel::UNEXPLORED;
    }

    const uint32_t useful_w = map_w < VIS_AXIS ? map_w : VIS_AXIS;
    const uint32_t useful_h = map_h < VIS_AXIS ? map_h : VIS_AXIS;
    const uint32_t width =
        block_size < useful_w - block_tx ? block_size : useful_w - block_tx;
    const uint32_t height =
        block_size < useful_h - block_ty ? block_size : useful_h - block_ty;
    bool any_explored = false;

    for (uint32_t dy = 0; dy < height; ++dy) {
        for (uint32_t dx = 0; dx < width; ++dx) {
            const uint32_t tx = block_tx + dx;
            const uint32_t ty = block_ty + dy;
            const uint32_t bit = ty * VIS_AXIS + tx;
            const uint32_t word = bit >> 6;
            const uint64_t mask = uint64_t{1} << (bit & 63u);
            if ((visible[word] & mask) != 0u) {
                return FogLevel::VISIBLE;
            }
            any_explored = any_explored || ((explored[word] & mask) != 0u);
        }
    }

    return any_explored ? FogLevel::EXPLORED : FogLevel::UNEXPLORED;
}

}  // namespace chunsa::presentation
