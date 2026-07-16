#pragma once

// aeon_sim_core — Fixed64<16> (Q47.16), truncamiento hacia cero (SPEC-001 §4.1)
// generado: minimax-m3 · revisado: Arquitecto 2026-07-16

#include "aeon/fatal.hpp"
#include "aeon/wide128.hpp"

#include <cstdint>

namespace aeon {

// ============================================================================
// Tipo público y constantes del formato Q47.16
// ============================================================================

struct Fx {
    int64_t raw;
};

inline constexpr int     FX_FRAC_BITS = 16;
inline constexpr int64_t FX_ONE_RAW   = 65536;             // 2^16
inline constexpr int64_t FX_RAW_MAX   = INT64_MAX;         //  +2^63 - 1
inline constexpr int64_t FX_RAW_MIN   = -INT64_MAX;        //  -2^63 + 1
                                                            //  Saturación simétrica: INT64_MIN jamás
                                                            //  se entrega como resultado de una operación.

// ============================================================================
// Utilidades internas — detección portable de overflow signed sin UB
// (compatible con GCC/Clang/MSVC/ICc; no depende de builtins específicos)
// ============================================================================

namespace detail {

// Suma con detección de overflow; evita UB de signed overflow.
inline bool fx_add_overflow(int64_t a, int64_t b, int64_t& out) noexcept {
    uint64_t ua = static_cast<uint64_t>(a);
    uint64_t ub = static_cast<uint64_t>(b);
    uint64_t ur = ua + ub;
    out = static_cast<int64_t>(ur);
    int sa = static_cast<int>(ua >> 63);
    int sb = static_cast<int>(ub >> 63);
    int sr = static_cast<int>(ur >> 63);
    // Overflow si ambos operandos comparten signo y el resultado difiere.
    return (sa == sb) && (sr != sa);
}

// Resta con detección de overflow; evita UB de signed overflow.
inline bool fx_sub_overflow(int64_t a, int64_t b, int64_t& out) noexcept {
    uint64_t ua = static_cast<uint64_t>(a);
    uint64_t ub = static_cast<uint64_t>(b);
    uint64_t ur = ua - ub;
    out = static_cast<int64_t>(ur);
    int sa = static_cast<int>(ua >> 63);
    int sb = static_cast<int>(ub >> 63);
    int sr = static_cast<int>(ur >> 63);
    // Overflow si los signos son distintos y el resultado no toma el signo del minuendo.
    return (sa != sb) && (sr != sa);
}

// Marca el primer error fatal (el primer error gana sobre NONE).
inline void fx_set_fatal(FatalReason& f, FatalReason r) noexcept {
    if (f == FatalReason::NONE) f = r;
}

} // namespace detail

// ============================================================================
// Constructores
// ============================================================================

// Envuelve un valor crudo sin validar (uso kernel interno).
constexpr Fx fx_from_raw(int64_t raw) noexcept {
    return Fx{raw};
}

// Construye desde unidades enteras SIN comprobar overflow (solo constantes de kernel).
// Se opera con magnitud unsigned para no invocar UB de signed overflow.
constexpr Fx fx_from_int(int64_t units) noexcept {
    return Fx{static_cast<int64_t>(static_cast<uint64_t>(units) *
                                   static_cast<uint64_t>(FX_ONE_RAW))};
}

// Construye desde unidades enteras comprobando overflow; satura si excede el rango.
inline Fx fx_from_int_checked(int64_t units, FatalReason& f) noexcept {
    // Rango seguro: units ∈ [FX_RAW_MIN/65536, INT64_MAX/65536].
    constexpr int64_t MAX_UNITS = INT64_MAX / FX_ONE_RAW;    //  +140737488355327
    constexpr int64_t MIN_UNITS = FX_RAW_MIN / FX_ONE_RAW;   //  -140737488355327
    if (units > MAX_UNITS || units < MIN_UNITS) {
        detail::fx_set_fatal(f, FatalReason::FX_OVERFLOW);
        return Fx{units < 0 ? FX_RAW_MIN : FX_RAW_MAX};
    }
    return Fx{units * FX_ONE_RAW};
}

// ============================================================================
// Aritmética básica — saturación simétrica, primer error gana
// ============================================================================

inline Fx fx_add(Fx a, Fx b, FatalReason& f) noexcept {
    int64_t r;
    if (detail::fx_add_overflow(a.raw, b.raw, r)) {
        detail::fx_set_fatal(f, FatalReason::FX_OVERFLOW);
        // Ambos positivos → MAX; ambos negativos → MIN.
        bool neg = (a.raw < 0) && (b.raw < 0);
        return Fx{neg ? FX_RAW_MIN : FX_RAW_MAX};
    }
    // Mantener invariante: INT64_MIN jamás se entrega como resultado.
    if (r == INT64_MIN) {
        detail::fx_set_fatal(f, FatalReason::FX_OVERFLOW);
        return Fx{FX_RAW_MIN};
    }
    return Fx{r};
}

inline Fx fx_sub(Fx a, Fx b, FatalReason& f) noexcept {
    int64_t r;
    if (detail::fx_sub_overflow(a.raw, b.raw, r)) {
        detail::fx_set_fatal(f, FatalReason::FX_OVERFLOW);
        // a<0 y b>0 → resultado más negativo → MIN; resto → MAX.
        bool neg = (a.raw < 0) && (b.raw > 0);
        return Fx{neg ? FX_RAW_MIN : FX_RAW_MAX};
    }
    if (r == INT64_MIN) {
        detail::fx_set_fatal(f, FatalReason::FX_OVERFLOW);
        return Fx{FX_RAW_MIN};
    }
    return Fx{r};
}

inline Fx fx_neg(Fx a, FatalReason& f) noexcept {
    if (a.raw == INT64_MIN) {
        // -INT64_MIN sería UB; saturamos a MAX y marcamos overflow.
        detail::fx_set_fatal(f, FatalReason::FX_OVERFLOW);
        return Fx{FX_RAW_MAX};
    }
    return Fx{-a.raw};
}

// ============================================================================
// Multiplicación y división — truncamiento hacia cero vía Wide128
// ============================================================================

inline Fx fx_mul(Fx a, Fx b, FatalReason& f) noexcept {
    // Producto exacto en 128 bits; cociente por 65536 con truncamiento a cero.
    Wide128  prod = w128_mul_i64(a.raw, b.raw);
    W128DivI64 qr = w128_div_i64(prod, FX_ONE_RAW);

    if (qr.quot_overflow) {
        detail::fx_set_fatal(f, FatalReason::FX_OVERFLOW);
        // Signo matemático del cociente = XOR de los signos de los operandos.
        bool neg = (a.raw < 0) != (b.raw < 0);
        return Fx{neg ? FX_RAW_MIN : FX_RAW_MAX};
    }
    // El cociente puede caer por debajo de FX_RAW_MIN (== INT64_MIN).
    if (qr.quot < FX_RAW_MIN || qr.quot > FX_RAW_MAX) {
        detail::fx_set_fatal(f, FatalReason::FX_OVERFLOW);
        return Fx{FX_RAW_MIN};
    }
    return Fx{qr.quot};
}

inline Fx fx_div(Fx a, Fx b, FatalReason& f) noexcept {
    if (b.raw == 0) {
        detail::fx_set_fatal(f, FatalReason::DIV_BY_ZERO);
        return Fx{0};
    }
    // (a << 16) / b  — preserva precisión y aplica truncamiento hacia cero
    //                  gracias a la semántica de w128_div_i64.
    Wide128   shifted = w128_shl(w128_from_i64(a.raw), 16);
    W128DivI64 qr     = w128_div_i64(shifted, b.raw);

    if (qr.quot_overflow) {
        detail::fx_set_fatal(f, FatalReason::FX_OVERFLOW);
        bool neg = (a.raw < 0) != (b.raw < 0);
        return Fx{neg ? FX_RAW_MIN : FX_RAW_MAX};
    }
    if (qr.quot < FX_RAW_MIN || qr.quot > FX_RAW_MAX) {
        detail::fx_set_fatal(f, FatalReason::FX_OVERFLOW);
        return Fx{FX_RAW_MIN};
    }
    return Fx{qr.quot};
}

// ============================================================================
// Conversión y comparación
// ============================================================================

// Parte entera con truncamiento HACIA CERO (no hacia -infinito).
// Para negativos se opera sobre la magnitud unsigned para evitar la extensión
// de signo que produciría un desplazamiento aritmético.
constexpr int64_t fx_trunc(Fx a) noexcept {
    if (a.raw >= 0) {
        return a.raw >> FX_FRAC_BITS;
    }
    uint64_t mag = static_cast<uint64_t>(0) - static_cast<uint64_t>(a.raw);
    return -static_cast<int64_t>(mag >> FX_FRAC_BITS);
}

constexpr bool fx_eq(Fx a, Fx b) noexcept { return a.raw == b.raw; }
constexpr bool fx_lt(Fx a, Fx b) noexcept { return a.raw <  b.raw; }

} // namespace aeon
