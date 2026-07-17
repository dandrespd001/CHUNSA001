#pragma once
#include <cstdint>

// chunsa_sim_core — DeterministicFatalState: razones de fallo determinista.
// SPEC-001 §2.3 / §4.1. Autor: Arquitecto.
// Regla global: las operaciones numéricas SIEMPRE devuelven un valor definido;
// `fatal` solo se escribe si estaba en NONE (el primer error gana).

namespace chunsa {

enum class FatalReason : uint32_t {
    NONE = 0,
    FX_OVERFLOW = 1,
    DIV_BY_ZERO = 2,
    WORLD_BOUNDS = 3,
    AI_JOB_FAILED = 4,
    POOL_EXHAUSTED_INTERNAL = 5,
    RNG_INVALID_RANGE = 6,
};

inline const char* fatal_reason_name(FatalReason r) noexcept {
    switch (r) {
        case FatalReason::NONE: return "NONE";
        case FatalReason::FX_OVERFLOW: return "FX_OVERFLOW";
        case FatalReason::DIV_BY_ZERO: return "DIV_BY_ZERO";
        case FatalReason::WORLD_BOUNDS: return "WORLD_BOUNDS";
        case FatalReason::AI_JOB_FAILED: return "AI_JOB_FAILED";
        case FatalReason::POOL_EXHAUSTED_INTERNAL: return "POOL_EXHAUSTED_INTERNAL";
        case FatalReason::RNG_INVALID_RANGE: return "RNG_INVALID_RANGE";
    }
    return "UNKNOWN";
}

// Escribe `reason` en `fatal` solo si no había error previo.
inline void set_fatal(FatalReason& fatal, FatalReason reason) noexcept {
    if (fatal == FatalReason::NONE) {
        fatal = reason;
    }
}

}  // namespace chunsa
