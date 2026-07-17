#pragma once

// chunsa_sim_core - Vec2Fx, cota de mundo y normalize_v1 (SPEC-001 §4.3)
// generado: minimax-m3  ·  revisado: Arquitecto 2026-07-16

#include <bit>
#include <cstdint>

#include "chunsa/fatal.hpp"
#include "chunsa/wide128.hpp"
#include "chunsa/fixed64.hpp"

namespace chunsa {

// ---------------------------------------------------------------
// Geometria de mundo y normalizacion determinista (punto fijo).
// Reglas duras: sin float/double, sin excepciones, sin UB de
// overflow signed (magnitudes SIEMPRE via uint64_t; shifts sobre
// tipos unsigned).
// ---------------------------------------------------------------

// Cota del mundo: WORLD_RAW_MAX = 2^29 (8192 tiles * 65536 raw/tile).
inline constexpr int64_t WORLD_TILES_MAX = 8192;
inline constexpr int64_t WORLD_RAW_MAX   = WORLD_TILES_MAX * FX_ONE_RAW; // 2^29

// Vector 2D en punto fijo (componentes Fx).
struct Vec2Fx {
    Fx x;
    Fx y;
};

// (1) Punto dentro de la cota de mundo (semi-abierto en el limite
//     superior).  p en [0, WORLD_RAW_MAX) x [0, WORLD_RAW_MAX).
[[nodiscard]]
inline bool world_contains(Vec2Fx p) noexcept {
    return (p.x.raw >= 0) && (p.y.raw >= 0)
        && (p.x.raw < WORLD_RAW_MAX) && (p.y.raw < WORLD_RAW_MAX);
}

// (2) Magnitud sin UB de signed overflow:
//        v < 0 ? (0u - (uint64_t)v) : (uint64_t)v
//     Helper reutilizado por dist_sq_raw y normalize_v1.
inline uint64_t mag_u64(int64_t v) noexcept {
    return v < 0 ? (0u - (uint64_t)v) : (uint64_t)v;
}

// (3) Distancia al cuadrado entre dos puntos, en raw (sin sqrt).
//     - Resta unsigned + reinterpretacion a int64_t => wrapping
//       bien definido (sin UB de overflow signed).
//     - Si |dx| o |dy| > 2^31 => fuera de cota => marca
//       WORLD_BOUNDS (solo si f == NONE) y devuelve UINT64_MAX.
//     - Caso normal: devuelve mag(dx)^2 + mag(dy)^2, que gracias a
//       la cota cabe holgadamente en uint64_t.
[[nodiscard]]
inline uint64_t dist_sq_raw(Vec2Fx a, Vec2Fx b, FatalReason& f) noexcept {
    // Diferencias via resta unsigned y re-cast a signed (wrap definido).
    const int64_t dx = (int64_t)((uint64_t)b.x.raw - (uint64_t)a.x.raw);
    const int64_t dy = (int64_t)((uint64_t)b.y.raw - (uint64_t)a.y.raw);

    const uint64_t ax = mag_u64(dx);
    const uint64_t ay = mag_u64(dy);

    constexpr uint64_t kBoundsThreshold = 1ull << 31;
    if (ax > kBoundsThreshold || ay > kBoundsThreshold) {
        if (f == FatalReason::NONE) {
            f = FatalReason::WORLD_BOUNDS;
        }
        return UINT64_MAX;
    }

    // ax, ay <= 2^31  =>  ax^2+ay^2 <= 2 * 2^62 = 2^63  -> cabe en u64.
    return ax * ax + ay * ay;
}

// (4) normalize_v1 - ALGORITMO CONGELADO (SPEC-001 §4.3 §12).
//     Normaliza un delta en fixed-point a una norma ~ FX_ONE_RAW.
//     Sin floats, sin sqrt no acotado: usa isqrt_u64 sobre enteros.
//     Sin caso de error: m == 0 -> {0,0}; `f` nunca se modifica.
//     Postcondicion testeable: para |delta| >= 65 raw,
//         | isqrt(outx^2 + outy^2) - 65536 | <= 2.
[[nodiscard]]
inline Vec2Fx normalize_v1(Vec2Fx delta, FatalReason& /*f*/) noexcept {
    // (a) Magnitudes (caminos unsigned -> sin UB de overflow signed).
    const uint64_t ux = mag_u64(delta.x.raw);
    const uint64_t uy = mag_u64(delta.y.raw);
    const uint64_t m  = (ux > uy) ? ux : uy;

    // (b) Vector cero -> resultado cero (caso definido, sin marcar f).
    if (m == 0) {
        return Vec2Fx{ Fx{0}, Fx{0} };
    }

    // (c) Indice del bit mas alto de m; como m > 0 cae en [0,63].
    const int bit = 63 - std::countl_zero(m);

    // (d) Factor de escala s = 28 - bit; s in [-35, 28].
    //     Escala AMBAS magnitudes con shift sobre uint64_t:
    //       sx,sy quedan en [0, 2^29)  -> m_scaled in [2^28, 2^29).
    const int s = 28 - bit;
    const uint64_t sx = (s > 0) ? (ux << s) : (ux >> (-s));
    const uint64_t sy = (s > 0) ? (uy << s) : (uy >> (-s));

    // (e) Longitud entera. Como sx,sy in [0,2^29):
    //       sx^2 + sy^2 <= 2 * (2^29)^2 = 2^59  -> cabe en u64.
    //       len = isqrt(...) >= m_scaled >= 2^28 -> divisor > 0.
    const uint64_t len = isqrt_u64(sx * sx + sy * sy);

    // (f) Proyeccion a escala FX_ONE_RAW (= 65536) sobre magnitudes.
    //       sx <= 2^29  =>  sx*65536 <= 2^45
    //       out <= 2^45 / 2^28 = 2^17 = 131072  -> cabe holgado.
    const uint64_t outx_mag = (sx * 65536ull) / len;
    const uint64_t outy_mag = (sy * 65536ull) / len;

    // (g) Reaplicar los signos originales (positivos o negativos) sobre
    //     las magnitudes escaladas: -(int64_t)mag si delta.<axis>.raw<0.
    const int64_t outx_raw = (delta.x.raw < 0)
                             ? -static_cast<int64_t>(outx_mag)
                             :  static_cast<int64_t>(outx_mag);
    const int64_t outy_raw = (delta.y.raw < 0)
                             ? -static_cast<int64_t>(outy_mag)
                             :  static_cast<int64_t>(outy_mag);

    // (h) `f` nunca se modifica: este algoritmo no tiene caso de error.
    return Vec2Fx{ Fx{outx_raw}, Fx{outy_raw} };
}

// (5) Multiplicacion de un Fx por un entero pequeno, SIN saturacion.
//     Uso: helpers internos de movimiento / integracion.
//     PRECONDICION: |a.raw * k| < 2^62
//                   (responsabilidad del llamador, no hay saturacion).
inline Fx fx_mul_small(Fx a, int64_t k) noexcept {
    return Fx{ a.raw * k };
}

} // namespace chunsa
