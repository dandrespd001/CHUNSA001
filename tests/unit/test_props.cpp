// Tests de propiedades del contrato numérico (SPEC-001 §4).
// Autor: Arquitecto. Complementa los golden vectors con propiedades algebraicas:
// simetría de signo (el motivo del truncamiento-hacia-cero), antisimetría de
// normalize_v1, módulo dentro de ±2 LSB y primer-error-gana.
#include <cstdint>
#include <cstdio>

#include "aeon/fatal.hpp"
#include "aeon/wide128.hpp"
#include "aeon/fixed64.hpp"
#include "aeon/vec2fx.hpp"

static int g_fails = 0;
#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            ++g_fails;                                                     \
            std::printf("CHECK falló L%d: %s\n", __LINE__, #cond);         \
        }                                                                  \
    } while (0)

using namespace aeon;

int main() {
    const int64_t pool[] = {0, 1, -1, 65, -65, 65535, 65536, -65536, 98304,
                            -98304, 131072, 1'000'000'000, -1'000'000'000,
                            (int64_t{1} << 45), -(int64_t{1} << 45),
                            FX_RAW_MAX, FX_RAW_MIN};

    // 1) Simetría de signo: fx_mul(-a,b) == fx_neg(fx_mul(a,b)), incluso saturando
    //    (la saturación es simétrica: FX_RAW_MIN == -FX_RAW_MAX).
    for (int64_t a : pool) {
        for (int64_t b : pool) {
            FatalReason f1 = FatalReason::NONE, f2 = FatalReason::NONE,
                        f3 = FatalReason::NONE;
            Fx lhs = fx_mul(fx_neg(Fx{a}, f1), Fx{b}, f1);
            Fx rhs = fx_neg(fx_mul(Fx{a}, Fx{b}, f2), f3);
            CHECK(lhs.raw == rhs.raw);
        }
    }

    // 2) Truncamiento hacia cero (no hacia -inf).
    {
        FatalReason f = FatalReason::NONE;
        CHECK(fx_div(Fx{-3}, Fx{131072}, f).raw == -1);
        CHECK(fx_trunc(Fx{-98304}) == -1);
        CHECK(fx_trunc(Fx{98304}) == 1);
    }

    // 3) División por cero: valor definido {0} + DIV_BY_ZERO.
    {
        FatalReason f = FatalReason::NONE;
        CHECK(fx_div(Fx{100}, Fx{0}, f).raw == 0);
        CHECK(f == FatalReason::DIV_BY_ZERO);
    }

    // 4) Primer-error-gana: un OVF posterior no pisa un DIV_BY_ZERO previo.
    {
        FatalReason f = FatalReason::NONE;
        (void)fx_div(Fx{100}, Fx{0}, f);
        (void)fx_mul(Fx{FX_RAW_MAX}, Fx{131072}, f);
        CHECK(f == FatalReason::DIV_BY_ZERO);
    }

    // 5) normalize_v1: antisimetría exacta y módulo en 65536±2 para |v|>=65 raw.
    const int64_t vpool[] = {0, 1, -1, 3, 4, 65, -65, 300, -400, 65536,
                             -131072, (int64_t{1} << 28), -((int64_t{1} << 29) - 1)};
    for (int64_t dx : vpool) {
        for (int64_t dy : vpool) {
            FatalReason f1 = FatalReason::NONE, f2 = FatalReason::NONE;
            Vec2Fx v{{dx}, {dy}};
            Vec2Fx nv{{-dx}, {-dy}};
            Vec2Fx a = normalize_v1(v, f1);
            Vec2Fx b = normalize_v1(nv, f2);
            CHECK(a.x.raw == -b.x.raw && a.y.raw == -b.y.raw);
            uint64_t ax = mag_u64(a.x.raw), ay = mag_u64(a.y.raw);
            uint64_t mag2 = ax * ax + ay * ay;
            uint64_t mod = isqrt_u64(mag2);
            uint64_t vx = mag_u64(dx), vy = mag_u64(dy);
            if (vx * vx + vy * vy >= 65ull * 65ull && (dx != 0 || dy != 0)) {
                CHECK(mod >= 65534 && mod <= 65538);
            }
            if (dx == 0 && dy == 0) {
                CHECK(a.x.raw == 0 && a.y.raw == 0);
            }
        }
    }

    // 6) Wide128 spot-checks del contrato.
    {
        Wide128 p = w128_mul_i64(INT64_MIN, -1);
        CHECK(p.hi == 0 && p.lo == 0x8000000000000000ull);
        W128DivI64 d = w128_div_i64(w128_from_i64(-196608), 131072);
        CHECK(d.quot == -1 && d.rem == -65536 && !d.quot_overflow);
        CHECK(isqrt_u64(2) == 1 && isqrt_u64(4) == 2);
        CHECK(isqrt_u64(UINT64_MAX) == 4294967295ull);
    }

    if (g_fails == 0) {
        std::printf("props: OK (backend=%s)\n", AEON_WIDE128_BACKEND_NAME);
        return 0;
    }
    std::printf("props: %d fallos\n", g_fails);
    return 1;
}
