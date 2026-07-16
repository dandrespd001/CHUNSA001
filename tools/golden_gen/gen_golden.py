#!/usr/bin/env python3
"""Generador de golden vectors — implementación de REFERENCIA de Fixed64<16>.

Enteros de Python (precisión arbitraria) = oráculo exacto. Semántica congelada
por SPEC-001 §4: truncamiento hacia cero, saturación simétrica, primer-error-gana.
Los CSV generados son la fuente de verdad que TODOS los backends C++ deben
reproducir byte a byte. Autor: Arquitecto. Versión de vectores: v1 (2026-07-16).
"""
import math
import random

FX_ONE = 65536
I64_MIN = -(2**63)
FX_MAX = 2**63 - 1
FX_MIN = -(2**63 - 1)          # saturación SIMÉTRICA (raw INT64_MIN nunca se produce)
NONE, OVF, DIV0 = "NONE", "FX_OVERFLOW", "DIV_BY_ZERO"


def trunc_div(a: int, b: int) -> int:
    """División con truncamiento hacia cero (como C++)."""
    q = abs(a) // abs(b)
    return -q if (a < 0) != (b < 0) else q


def sat(q: int):
    if q > FX_MAX:
        return FX_MAX, OVF
    if q < FX_MIN:
        return FX_MIN, OVF
    return q, NONE


def ref_add(a, b):  # entradas restringidas a >= FX_MIN (ver pools)
    return sat(a + b)

def ref_sub(a, b):
    return sat(a - b)

def ref_neg(a, _b=0):
    if a == I64_MIN:
        return FX_MAX, OVF
    return sat(-a)

def ref_mul(a, b):
    return sat(trunc_div(a * b, FX_ONE))

def ref_div(a, b):
    if b == 0:
        return 0, DIV0
    return sat(trunc_div(a * FX_ONE, b))

def ref_trunc(a, _b=0):
    return trunc_div(a, FX_ONE), NONE

def ref_isqrt(a, _b=0):
    return math.isqrt(a), NONE


def ref_normalize(dx: int, dy: int):
    """normalize_v1 CONGELADO (SPEC-001 §4.3): pre-escalado CLZ + isqrt + trunc."""
    ux, uy = abs(dx), abs(dy)
    m = max(ux, uy)
    if m == 0:
        return 0, 0
    bit = m.bit_length() - 1
    s = 28 - bit
    if s >= 0:
        sx, sy = ux << s, uy << s
    else:
        sx, sy = ux >> (-s), uy >> (-s)
    ln = math.isqrt(sx * sx + sy * sy)
    ox = (sx * FX_ONE) // ln
    oy = (sy * FX_ONE) // ln
    return (-ox if dx < 0 else ox), (-oy if dy < 0 else oy)


OPS = {"add": ref_add, "sub": ref_sub, "neg": ref_neg, "mul": ref_mul,
       "div": ref_div, "trunc": ref_trunc, "isqrt": ref_isqrt}

# ---- pools de valores -----------------------------------------------------------
EDGE = [0, 1, 2, 3, 65, 65535, FX_ONE, 65537, 98304, 131072, 2**31, 2**45,
        FX_MAX, FX_MAX - 1, FX_MIN, FX_MIN + 1,
        -1, -2, -65, -65535, -FX_ONE, -98304, -131072, -(2**31), -(2**45)]
EDGE_WITH_I64MIN = EDGE + [I64_MIN]          # solo neg/mul/div/trunc (magnitud unsigned)
ISQRT_EDGE = [0, 1, 2, 3, 4, 15, 16, 2**32 - 1, 2**32, 2**32 + 1,
              (2**32 - 1)**2, (2**32 - 1)**2 - 1, (2**32 - 1)**2 + 1, 2**64 - 1]
NORM_EDGE = [(FX_ONE, 0), (0, -FX_ONE), (1, 1), (-1, -1), (65, 0), (0, 65),
             (-65, 65), (1, 0), (0, -1), (3, 4), (-300, 400),
             (2**29 - 1, 12345), (-(2**29 - 1), 2**29 - 1), (98304, -131072),
             (7, -24), (65536 * 100, 65536 * 100)]

rng = random.Random(20260716)

def rnd_i64():
    return rng.randint(-(2**62), 2**62)

rows = []
# casos de borde deterministas: producto cartesiano acotado por op
for op in ("add", "sub"):
    for a in EDGE:
        for b in (0, 1, -1, FX_ONE, FX_MAX, FX_MIN):
            rows.append((op, a, b))
for op in ("mul", "div"):
    for a in EDGE_WITH_I64MIN:
        for b in (0, 1, -1, FX_ONE, -FX_ONE, 131072, -98304, FX_MAX, FX_MIN):
            rows.append((op, a, b))
for a in EDGE_WITH_I64MIN:
    rows.append(("neg", a, 0))
    rows.append(("trunc", a, 0))
for a in ISQRT_EDGE:
    rows.append(("isqrt", a, 0))
# barrido pseudoaleatorio determinista
for _ in range(40):
    rows.append(("add", rnd_i64(), rnd_i64()))
    rows.append(("sub", rnd_i64(), rnd_i64()))
    rows.append(("mul", rng.randint(-(2**40), 2**40), rng.randint(-(2**40), 2**40)))
    rows.append(("div", rnd_i64(), rng.choice([v for v in EDGE if v != 0])))
    rows.append(("isqrt", rng.randint(0, 2**64 - 1), 0))

with open("fixed64_v1.csv", "w", encoding="utf-8") as f:
    f.write("# golden fixed64_v1 — generado por tools/golden_gen/gen_golden.py (2026-07-16)\n")
    f.write("# op,a,b,expect,fatal   (a/b: raw decimal; isqrt usa a como u64)\n")
    for op, a, b in rows:
        expect, fatal = OPS[op](a, b)
        f.write(f"{op},{a},{b},{expect},{fatal}\n")
print(f"fixed64_v1.csv: {len(rows)} casos")

norm_rows = list(NORM_EDGE)
for _ in range(24):
    norm_rows.append((rng.randint(-(2**29) + 1, 2**29 - 1), rng.randint(-(2**29) + 1, 2**29 - 1)))
with open("normalize_v1.csv", "w", encoding="utf-8") as f:
    f.write("# golden normalize_v1 — dx,dy,ex,ey (algoritmo congelado SPEC-001 §4.3)\n")
    for dx, dy in norm_rows:
        ex, ey = ref_normalize(dx, dy)
        f.write(f"{dx},{dy},{ex},{ey}\n")
print(f"normalize_v1.csv: {len(norm_rows)} casos")
