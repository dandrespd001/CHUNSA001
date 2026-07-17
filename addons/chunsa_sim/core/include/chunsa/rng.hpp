#pragma once

// chunsa_sim_core — RNG por contador con slots estables (SPEC-001 §5)
// generado: minimax-m3 · revisado: Arquitecto 2026-07-17

#include <cstdint>
#include "chunsa/fatal.hpp"

// TODO CONGELADO — la implementación ES la spec.

namespace chunsa {

inline constexpr uint32_t RNG_ALGO_VERSION = 1;

// Núcleo de mezcla SplitMix64. Determinista, sin estado global y puramente
// funcional sobre su argumento: cada llamada es independiente de las anteriores.
// Cualquier modificación aquí invalida la suite de regresión completa.
inline constexpr uint64_t splitmix64_mix(uint64_t z) noexcept {
    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

// Empaqueta el par (stable_slot, sub) en el índice de extracción que consume
// rng_draw. El byte bajo reserva el contador de reintentos del propio slot,
// permitiendo rechazo trazable sin colisionar con slots vecinos.
//
// Precondición (no comprobada, responsabilidad del caller):
//   sub < 256 — garantiza que el OR no pisa el byte alto del slot.
inline constexpr uint32_t make_draw_index(uint32_t stable_slot, uint32_t sub) noexcept {
    return (stable_slot << 8) | sub;
}

// Extracción base: mezcla seed, stream, tick y (entity_index, draw_index) en
// cuatro rondas de splitmix64_mix. Completamente determinista respecto al
// 5-tuplo (seed, stream_id, tick, entity_index, draw_index).
inline constexpr uint64_t rng_draw(uint64_t seed,
                                   uint32_t stream_id,
                                   uint32_t tick,
                                   uint32_t entity_index,
                                   uint32_t draw_index) noexcept {
    uint64_t h = seed;
    h = splitmix64_mix(h ^ (uint64_t)stream_id);
    h = splitmix64_mix(h ^ (uint64_t)tick);
    h = splitmix64_mix(h ^ (((uint64_t)entity_index << 32) | (uint64_t)draw_index));
    return splitmix64_mix(h);
}

// Catálogo de streams lógicos del simulador. append-only: nunca reasignar
// ni reordenar valores; los nuevos streams se añaden al final.
enum class RngStream : uint32_t {
    BENCH           = 1,
    COMBAT_VARIANCE = 2,
    LOOT            = 3,
    AI_TIEBREAK     = 4,
    MAP_GEN         = 5,
};

// Muestreo uniforme en [lo, hi) mediante rechazo sin sesgo (variante Lemire).
//   - Si lo >= hi: marca f con RNG_INVALID_RANGE y devuelve lo. Degradación
//     segura y trazable; el caller puede inspeccionar `f` para abortar.
//   - Cada rechazo consume un sub-índice del MISMO stable_slot: el rechazo es
//     reentrante, no roba draws a otros slots y queda registrado en el log.
//   - Precondición documentada (no comprobada): el contador de reintentos jamás
//     alcanza 256 en la práctica (width siempre acotado por el diseño del
//     subsistema que invoca; e.g. dado de daño, ancho de pool de loot, etc.).
inline uint32_t rng_range(uint64_t seed,
                          uint32_t stream_id,
                          uint32_t tick,
                          uint32_t entity_index,
                          uint32_t stable_slot,
                          uint32_t lo,
                          uint32_t hi,
                          FatalReason& f) noexcept {
    if (lo >= hi) {
        set_fatal(f, FatalReason::RNG_INVALID_RANGE);
        return lo;
    }
    uint64_t width     = (uint64_t)hi - (uint64_t)lo;
    uint64_t threshold = (0 - width) % width;   // == 2^64 mod width (aritmética u64)
    uint32_t retry = 0;
    uint64_t x;
    do {
        x = rng_draw(seed, stream_id, tick, entity_index,
                     make_draw_index(stable_slot, retry++));
    } while (x < threshold);
    return lo + (uint32_t)(x % width);
}

// Fracción cruda en formato Q47.16 con valor real en [0, 1).
// Construida extrayendo los 16 bits altos del draw del sub=0 del slot, de
// forma que el resultado es siempre no-negativo y cabe sin pérdida en int64_t.
// Diseñada para alimentar fx_mul / fx_div sin redondeos intermedios.
inline constexpr int64_t rng_fx01_raw(uint64_t seed,
                                      uint32_t stream_id,
                                      uint32_t tick,
                                      uint32_t entity_index,
                                      uint32_t stable_slot) noexcept {
    return (int64_t)(rng_draw(seed, stream_id, tick, entity_index,
                              make_draw_index(stable_slot, 0)) >> 48);
}

// Nota: no se incluyen valores de autotest hardcodeados aquí; el Arquitecto
// fija los vectores canónicos en su test contra esta implementación congelada.

}  // namespace chunsa