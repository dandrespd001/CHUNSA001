#pragma once

#include <cstdint>

namespace chunsa {

// Sprint 1.8A (SPEC-007 §9.3): capacidad estructural completa del stock y de
// los costes. Los índices 0=A, 1=B y 2=Me conservan su significado actual;
// 3..31 permanecen reservados y en cero hasta los sprints de contenido.
inline constexpr uint32_t RESOURCE_COUNT = 32u;
inline constexpr uint32_t RESOURCE_INDEX_A = 0u;
inline constexpr uint32_t RESOURCE_INDEX_B = 1u;
inline constexpr uint32_t RESOURCE_INDEX_ME = 2u;

}  // namespace chunsa
