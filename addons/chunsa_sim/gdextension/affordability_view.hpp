#pragma once

#include <cstdint>

namespace chunsa::presentation {

struct ResourceShortage {
    uint32_t resource_index;
    int64_t amount;
};

struct AffordabilityResult {
    bool affordable;
    uint32_t missing_count;
    ResourceShortage missing[32];
};

inline AffordabilityResult assess_affordability(const int32_t* cost,
                                                const int64_t* stock,
                                                uint32_t resource_count) noexcept {
    AffordabilityResult result{true, 0u, {}};
    if (cost == nullptr || stock == nullptr) {
        result.affordable = false;
        return result;
    }

    constexpr uint32_t MAX_RESOURCES = 32u;
    const uint32_t count = resource_count < MAX_RESOURCES
            ? resource_count
            : MAX_RESOURCES;
    for (uint32_t resource = 0; resource < count; ++resource) {
        if (cost[resource] <= 0) continue;
        const int64_t required = static_cast<int64_t>(cost[resource]);
        if (stock[resource] >= required) continue;
        result.affordable = false;
        result.missing[result.missing_count++] = ResourceShortage{
                resource, required - stock[resource]};
    }
    return result;
}

}  // namespace chunsa::presentation
