#pragma once

#include <cstdint>

namespace chunsa {

// Sprint 1.8A/1.8B (SPEC-007 §9.3/§18): capacidad estructural completa del
// stock y nombres estables de los tres recursos existentes. Los autores de
// YAML usan record_id; estos slots los asigna el compilador de datos.
inline constexpr uint32_t RESOURCE_COUNT = 32u;
inline constexpr uint32_t RESOURCE_INDEX_FOOD = 0u;
inline constexpr uint32_t RESOURCE_INDEX_WOOD = 1u;
inline constexpr uint32_t RESOURCE_INDEX_STONE = 2u;

}  // namespace chunsa
