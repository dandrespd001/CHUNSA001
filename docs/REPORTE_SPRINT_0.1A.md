# REPORTE DE CIERRE — Sprint 0.1A (2026-07-17)

**Estado: COMPLETO — todos los ítems del DoD en verde.** Repo: `github.com/dandrespd001/CHUNSA001` (renombrado de AEON001 el 17-07 por decisión del Director; renombrado integral de código aeon→chunsa incluido).

## Definición de HECHO — resultados

| Ítem | Resultado |
|---|---|
| Estructura base §2.1 + CMake C++20 `-Werror` limpio | ✅ 0 warnings (GCC 16.1.1 / Clang 22.1.8) |
| Golden vectors byte-idénticos GCC/Clang/portable | ✅ 1074/1074 en las 3 lanes |
| **G1** (2 corridas bit-exactas) | ✅ selftest (600u×2000t) `b0b839e526cc7498` idéntico en las 3 lanes; **100 000 ticks** `653f7bb3181b1415`; 200u `069cb7d3…`; 1000u `3e414a0e…` |
| bench JSON campos §13 | ✅ `chunsa_perf0_v1`, nearest-rank, headless/synthetic |
| Contador de allocations = 0 en bucle estable | ✅ `alloc_delta=0` (gate en selftest) |
| Workflows CI (4 lanes Linux + windows-msvc) | ✅ commiteados; MSVC valida `_mul128/_udiv128` en el primer run |
| Reporte final | ✅ este documento |

**MEASURED (sintético, máquina de desarrollo — NO es la referencia UHD 620 de PERF-0):** tick p95 = 40 µs @600u · 98 µs @1000u (movimiento+spatial+checksum). El envelope es 40 ms: margen ×400 en este subconjunto.

## Delegación Claude ↔ MiniMax (scorecard honesto)

**Generado por MiniMax-M3** (7 tareas, specs cerradas): `wide128.hpp` (9459f95b) · `fixed64.hpp` (97d9c487) · `vec2fx.hpp` (8e1f128d) · `rng.hpp` (cb4612ac) · `entity_table.hpp` (b9b487c6) · `spatial_hash.hpp` (4c51541e) · `cli_run.hpp` (eb3ac85f). Calidad general alta; la división 128/64 portable y el escenario del CLI salieron perfectos a la primera.

**Correcciones del Arquitecto en revisión** (la cascada funcionó): `isqrt_u64` reescrito (regla de fallback — la variante generada fallaba en entradas grandes; **cazado por los golden vectors**) · UB de negación del cociente 2⁶³ (negación unsigned) · namespace + `<intrin.h>` (omisión del prompt) · comentario alucinado ("0.1 Å") · zero-init de `XXH3_state_t` (GCC16) · pragma `mismatched-new-delete` (falso positivo) · `RNG_INVALID_RANGE` faltante en `fatal.hpp` (**error del Arquitecto**: prometido en prompt, no añadido al header).

**Autoría directa del Arquitecto**: `fatal/commands/game_state/checksum/step` (asiento de invariantes), generador de golden (Python exacto), tests de propiedades, CMake/CI/gates, parche del contador de allocations.

## Desviaciones de SPEC-001

Ninguna de contrato. Notas: el checksum usa dominio `CHUNSA_STATE_V1` y seed `CHUN_ST1` (renombrado); xxHash vendored v0.8.3 sha256 `17973c0d…` (pin en TOOLCHAIN.md); G2 Windows↔Linux completo pendiente del primer run de CI con runner Windows (job commiteado).

## Plan 0.1B (según SPEC-001 §15)

Serialización canónica + envelope de save con endurecimiento y fuzzing · recorder mínimo · **G3/G4/G5** · ring de snapshots + demo visual mínima en adaptador · `chunsa_data_compiler` + `unit.schema.json` · PERF-0 físico en UHD 620 · ficha de coste de arte por civ.
