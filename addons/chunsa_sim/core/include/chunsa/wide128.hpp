#pragma once
// chunsa_sim_core — Wide128: aritmética 128-bit para Fixed64 (SPEC-001 §4.2)
// generado: minimax-m3 · revisado: Arquitecto 2026-07-16

#include <cstdint>

#if defined(_MSC_VER) && defined(_M_X64) && !defined(CHUNSA_WIDE128_FORCE_PORTABLE) && !defined(__SIZEOF_INT128__)
#  include <intrin.h>
#  pragma intrinsic(_mul128, _umul128, _udiv128)
#endif

namespace chunsa {

// Representación 128-bit en two's complement: hi es la parte alta.
struct Wide128 {
    uint64_t lo;
    uint64_t hi;
};

struct W128DivI64 {
    int64_t quot;
    int64_t rem;
    bool quot_overflow;
};

// Selección de backend y nombre.
#if defined(CHUNSA_WIDE128_FORCE_PORTABLE)
#  define CHUNSA_WIDE128_BACKEND_NAME "portable"
#  define CHUNSA_WIDE128_USE_PORTABLE
#elif defined(__SIZEOF_INT128__)
#  define CHUNSA_WIDE128_BACKEND_NAME "int128"
#  define CHUNSA_WIDE128_USE_INT128
#elif defined(_MSC_VER) && defined(_M_X64)
#  define CHUNSA_WIDE128_BACKEND_NAME "msvc"
#  define CHUNSA_WIDE128_USE_MSVC
#else
#  define CHUNSA_WIDE128_BACKEND_NAME "portable"
#  define CHUNSA_WIDE128_USE_PORTABLE
#endif

// ================== Funciones comunes (independientes del backend) ==================

// Extensión de signo de int64_t a 128 bits.
inline Wide128 w128_from_i64(int64_t v) noexcept {
    Wide128 r;
    r.lo = static_cast<uint64_t>(v);
    r.hi = (v < 0) ? UINT64_MAX : 0;
    return r;
}

// Devuelve true si el número es negativo (bit 63 de hi).
inline bool w128_is_neg(Wide128 a) noexcept {
    return (a.hi >> 63) & 1ULL;
}

// Negación en complemento a dos: ~a + 1.
inline Wide128 w128_neg(Wide128 a) noexcept {
    uint64_t lo = ~a.lo;
    uint64_t hi = ~a.hi;
    uint64_t lo2 = lo + 1;
    uint64_t hi2 = hi + (lo2 == 0 ? 1 : 0);
    return {lo2, hi2};
}

// Suma wrapping con acarreo.
inline Wide128 w128_add(Wide128 a, Wide128 b) noexcept {
    uint64_t lo = a.lo + b.lo;
    uint64_t carry = (lo < a.lo) ? 1 : 0;
    uint64_t hi = a.hi + b.hi + carry;
    return {lo, hi};
}

// Resta wrapping con borrow.
inline Wide128 w128_sub(Wide128 a, Wide128 b) noexcept {
    uint64_t lo = a.lo - b.lo;
    uint64_t borrow = (a.lo < b.lo) ? 1 : 0;
    uint64_t hi = a.hi - b.hi - borrow;
    return {lo, hi};
}

// Comparación signed (-1,0,+1).
inline int w128_cmp(Wide128 a, Wide128 b) noexcept {
    int64_t a_hi = static_cast<int64_t>(a.hi);
    int64_t b_hi = static_cast<int64_t>(b.hi);
    if (a_hi < b_hi) return -1;
    if (a_hi > b_hi) return 1;
    if (a.lo < b.lo) return -1;
    if (a.lo > b.lo) return 1;
    return 0;
}

// Desplazamiento lógico a la izquierda (0..127). Evita UB en shifts >=64.
inline Wide128 w128_shl(Wide128 a, unsigned n) noexcept {
    if (n == 0) return a;
    if (n < 64) {
        uint64_t lo = a.lo << n;
        uint64_t hi = (a.hi << n) | (a.lo >> (64 - n));
        return {lo, hi};
    } else {
        // n >= 64 y n < 128
        unsigned s = n - 64;
        uint64_t lo = 0;
        uint64_t hi = a.lo << s;
        return {lo, hi};
    }
}

// Raíz cuadrada entera (floor) por método bit a bit (resta con dígitos de 2 bits).
// Común a todos los backends.
inline uint64_t isqrt_u64(uint64_t x) noexcept {
    // Forma canónica digito-a-digito (base 4): res acumula (raíz parcial << k).
    // CORREGIDO por el Arquitecto: la variante generada `(res<<2)|bit` desalinea
    // las escalas de res y bit y falla en entradas grandes (detectado por golden).
    uint64_t res = 0;
    uint64_t bit = 1ULL << 62;  // mayor potencia de 4 ≤ 2^64
    while (bit > x) bit >>= 2;
    while (bit != 0) {
        if (x >= res + bit) {
            x -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return res;
}

// =====================================================================
// Implementaciones específicas de backend para w128_mul_i64 y w128_div_i64
// =====================================================================

#ifdef CHUNSA_WIDE128_USE_PORTABLE

namespace detail {

// Multiplicación unsigned 64x64 -> 128 usando mitades de 32 bits.
static inline void umul_u64(uint64_t a, uint64_t b, uint64_t* lo, uint64_t* hi) noexcept {
    uint32_t a0 = static_cast<uint32_t>(a);
    uint32_t a1 = static_cast<uint32_t>(a >> 32);
    uint32_t b0 = static_cast<uint32_t>(b);
    uint32_t b1 = static_cast<uint32_t>(b >> 32);
    uint64_t t0 = static_cast<uint64_t>(a0) * b0;
    uint64_t t1 = static_cast<uint64_t>(a0) * b1;
    uint64_t t2 = static_cast<uint64_t>(a1) * b0;
    uint64_t t3 = static_cast<uint64_t>(a1) * b1;
    uint64_t lo_acc = t0;
    uint64_t hi_acc = t3;
    // Sumar t1 (low32 << 32) y su parte alta.
    uint64_t t1_low = t1 & 0xFFFFFFFFu;
    uint64_t t1_high = t1 >> 32;
    uint64_t sum = lo_acc + (t1_low << 32);
    uint64_t carry = (sum < lo_acc) ? 1 : 0;
    lo_acc = sum;
    hi_acc += t1_high + carry;
    // Sumar t2.
    uint64_t t2_low = t2 & 0xFFFFFFFFu;
    uint64_t t2_high = t2 >> 32;
    sum = lo_acc + (t2_low << 32);
    carry = (sum < lo_acc) ? 1 : 0;
    lo_acc = sum;
    hi_acc += t2_high + carry;
    *lo = lo_acc;
    *hi = hi_acc;
}

// División unsigned 128/64 -> cociente 128, resto 64 (restauradora bit a bit).
static inline void udivmod_u128(uint64_t n_hi, uint64_t n_lo, uint64_t d,
                                uint64_t* q_hi, uint64_t* q_lo, uint64_t* r) noexcept {
    uint64_t rem_hi = 0, rem_lo = 0;
    uint64_t qh = 0, ql = 0;
    for (int i = 127; i >= 0; --i) {
        uint64_t bit;
        if (i >= 64) {
            bit = (n_hi >> (i - 64)) & 1ULL;
        } else {
            bit = (n_lo >> i) & 1ULL;
        }
        // Shift left 1 bit del resto y trae el bit.
        uint64_t new_rem_hi = (rem_hi << 1) | (rem_lo >> 63);
        uint64_t new_rem_lo = (rem_lo << 1) | bit;
        rem_hi = new_rem_hi;
        rem_lo = new_rem_lo;
        // Comparar resto >= divisor.
        bool ge;
        if (rem_hi) {
            ge = true;
        } else {
            ge = (rem_lo >= d);
        }
        if (ge) {
            // Restar divisor.
            if (rem_hi) {
                rem_lo = rem_lo - d; // 2^64 + rem_lo - d < 2^64
                rem_hi = 0;
            } else {
                rem_lo = rem_lo - d;
            }
            // Set bit i del cociente.
            if (i >= 64) {
                qh |= (1ULL << (i - 64));
            } else {
                ql |= (1ULL << i);
            }
        }
    }
    *q_hi = qh;
    *q_lo = ql;
    *r = rem_lo;
}

} // namespace detail

// Multiplicación signed 64x64 -> 128 exacta.
inline Wide128 w128_mul_i64(int64_t a, int64_t b) noexcept {
    bool neg = (a < 0) ^ (b < 0);
    uint64_t ua = a < 0 ? (uint64_t)0 - (uint64_t)a : (uint64_t)a;
    uint64_t ub = b < 0 ? (uint64_t)0 - (uint64_t)b : (uint64_t)b;
    uint64_t lo, hi;
    detail::umul_u64(ua, ub, &lo, &hi);
    Wide128 r{lo, hi};
    if (neg) r = w128_neg(r);
    return r;
}

// División signed 128/64 -> cociente y resto (truncamiento hacia cero).
inline W128DivI64 w128_div_i64(Wide128 num, int64_t den) noexcept {
    bool num_neg = w128_is_neg(num);
    bool den_neg = (den < 0);
    // Magnitud del numerador.
    Wide128 num_mag = num_neg ? w128_neg(num) : num;
    // Magnitud del denominador.
    uint64_t uden = den_neg ? (uint64_t)0 - (uint64_t)den : (uint64_t)den;
    // División unsigned.
    uint64_t q_hi, q_lo, r;
    detail::udivmod_u128(num_mag.hi, num_mag.lo, uden, &q_hi, &q_lo, &r);
    Wide128 q_mag{q_lo, q_hi};
    // Determinar overflow del cociente.
    bool quot_neg = num_neg ^ den_neg;
    uint64_t max_mag = quot_neg ? 0x8000000000000000ULL : 0x7FFFFFFFFFFFFFFFULL;
    bool overflow = (q_mag.hi > 0) || (q_mag.hi == 0 && q_mag.lo > max_mag);
    W128DivI64 result;
    if (overflow) {
        result.quot = 0;
        result.rem = 0;
        result.quot_overflow = true;
        return result;
    }
    // Aplicar signos.
    // Negación vía unsigned: magnitud 2^63 → INT64_MIN sin UB.
    int64_t q = static_cast<int64_t>(quot_neg ? (0 - q_mag.lo) : q_mag.lo);
    int64_t rem = num_neg ? -(int64_t)r : (int64_t)r;
    result.quot = q;
    result.rem = rem;
    result.quot_overflow = false;
    return result;
}

#endif // CHUNSA_WIDE128_USE_PORTABLE

// ---------------------------------------------------------------------
// Backend __int128
// ---------------------------------------------------------------------
#ifdef CHUNSA_WIDE128_USE_INT128

inline Wide128 w128_mul_i64(int64_t a, int64_t b) noexcept {
    bool neg = (a < 0) ^ (b < 0);
    uint64_t ua = a < 0 ? (uint64_t)0 - (uint64_t)a : (uint64_t)a;
    uint64_t ub = b < 0 ? (uint64_t)0 - (uint64_t)b : (uint64_t)b;
    unsigned __int128 prod = static_cast<unsigned __int128>(ua) * static_cast<unsigned __int128>(ub);
    Wide128 r;
    r.lo = static_cast<uint64_t>(prod);
    r.hi = static_cast<uint64_t>(prod >> 64);
    if (neg) r = w128_neg(r);
    return r;
}

inline W128DivI64 w128_div_i64(Wide128 num, int64_t den) noexcept {
    bool num_neg = w128_is_neg(num);
    bool den_neg = (den < 0);
    // Magnitud unsigned 128.
    unsigned __int128 num_mag;
    if (num_neg) {
        unsigned __int128 n = (static_cast<unsigned __int128>(num.hi) << 64) | num.lo;
        num_mag = ~n + 1;
    } else {
        num_mag = (static_cast<unsigned __int128>(num.hi) << 64) | num.lo;
    }
    uint64_t uden = den_neg ? (uint64_t)0 - (uint64_t)den : (uint64_t)den;
    unsigned __int128 q_mag = num_mag / uden;
    uint64_t r = static_cast<uint64_t>(num_mag % uden);
    // Comprobar overflow.
    bool quot_neg = num_neg ^ den_neg;
    uint64_t max_mag = quot_neg ? 0x8000000000000000ULL : 0x7FFFFFFFFFFFFFFFULL;
    uint64_t q_hi = static_cast<uint64_t>(q_mag >> 64);
    uint64_t q_lo = static_cast<uint64_t>(q_mag);
    bool overflow = (q_hi > 0) || (q_hi == 0 && q_lo > max_mag);
    W128DivI64 result;
    if (overflow) {
        result.quot = 0;
        result.rem = 0;
        result.quot_overflow = true;
        return result;
    }
    // Negación vía unsigned: magnitud 2^63 → INT64_MIN sin UB.
    int64_t q = static_cast<int64_t>(quot_neg ? (0 - q_lo) : q_lo);
    int64_t rem = num_neg ? -(int64_t)r : (int64_t)r;
    result.quot = q;
    result.rem = rem;
    result.quot_overflow = false;
    return result;
}

#endif // CHUNSA_WIDE128_USE_INT128

// ---------------------------------------------------------------------
// Backend MSVC (intrinsics) — validar en CI Windows
// ---------------------------------------------------------------------
#ifdef CHUNSA_WIDE128_USE_MSVC

// intrinsics incluidos arriba, fuera del namespace

inline Wide128 w128_mul_i64(int64_t a, int64_t b) noexcept {
    int64_t high;
    int64_t low = _mul128(a, b, &high);
    return { static_cast<uint64_t>(low), static_cast<uint64_t>(high) };
}

inline W128DivI64 w128_div_i64(Wide128 num, int64_t den) noexcept {
    bool num_neg = w128_is_neg(num);
    bool den_neg = (den < 0);
    Wide128 num_mag = num_neg ? w128_neg(num) : num;
    uint64_t uden = den_neg ? (uint64_t)0 - (uint64_t)den : (uint64_t)den;
    W128DivI64 result;
    if (num_mag.hi < uden) {
        uint64_t r;
        uint64_t q = _udiv128(num_mag.hi, num_mag.lo, uden, &r);
        bool quot_neg = num_neg ^ den_neg;
        uint64_t max_mag = quot_neg ? 0x8000000000000000ULL : 0x7FFFFFFFFFFFFFFFULL;
        if (q > max_mag) {
            result.quot = 0;
            result.rem = 0;
            result.quot_overflow = true;
            return result;
        }
        // Negación vía unsigned: magnitud 2^63 → INT64_MIN sin UB.
        int64_t q_signed = static_cast<int64_t>(quot_neg ? (0 - q) : q);
        int64_t r_signed = num_neg ? -(int64_t)r : (int64_t)r;
        result.quot = q_signed;
        result.rem = r_signed;
        result.quot_overflow = false;
        return result;
    } else {
        result.quot = 0;
        result.rem = 0;
        result.quot_overflow = true;
        return result;
    }
}

#endif // CHUNSA_WIDE128_USE_MSVC

}  // namespace chunsa
