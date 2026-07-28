#pragma once

// Baselines canónicos de determinismo.
//
// Procedimiento de actualización: ejecuta el gate canónico afectado, confirma
// primero que el resultado funcional y las invariantes del escenario siguen
// siendo correctos, y sustituye aquí únicamente el valor hexadecimal medido.
// Todo cambio de baseline debe explicar en el commit que lo introduce qué
// cambio de dominio o trayectoria lo hace intencional; nunca se actualiza solo
// para volver verde una prueba.

#include <cstdint>

namespace chunsa::determinism_baselines {

// Vectores dorados Fixed64 + normalize_v1 de tests/determinism/golden.
inline constexpr long GOLDEN_VECTOR_CASES = 1074;

// G1: synthetic_movement_v1@1, 600 unidades, 2000 ticks, seed 20260716.
inline constexpr uint64_t G1_SYNTHETIC_STATE = 0xfefa48125dd35736ull;

// G3: savetest canónico sin IA, save@200 y continuación hasta tick 400.
inline constexpr uint64_t G3_SAVETEST_STATE = 0x969199722657b853ull;
inline constexpr uint64_t G3_SAVETEST_CONTINUATION = 0x6145075498b2fb7dull;

// G4: savetest canónico con IA, save@200 y continuación hasta tick 400.
inline constexpr uint64_t G4_SAVETEST_AI_STATE = 0x774316057e5667fbull;
inline constexpr uint64_t G4_SAVETEST_AI_CONTINUATION = 0xd52ac0019700684full;

// SPEC-005 §8.3: skirmish militar sin ciudadanos.
inline constexpr uint64_t AI_SKIRMISH_STATE = 0x3f64d3223b74d477ull;
inline constexpr uint64_t AI_SKIRMISH_CONTINUATION = 0x92ec9aa95374a429ull;

// SPEC-004 §7.1: skirmish con economía y ciudadanos vulnerables.
inline constexpr uint64_t AI_SKIRMISH_ECO_STATE = 0xd610feef89ed9c65ull;
inline constexpr uint64_t AI_SKIRMISH_ECO_CONTINUATION = 0x5e1527e0921edf27ull;

// SPEC-004 §20/§22: apertura económica completa con control de ciudadano.
inline constexpr uint64_t AI_SKIRMISH_APERTURA_STATE = 0xc7b04caea8c32e64ull;
inline constexpr uint64_t AI_SKIRMISH_APERTURA_CONTINUATION = 0x4ae3ddd1ad5ab4f9ull;
inline constexpr uint32_t AI_SKIRMISH_APERTURA_END_TICK = 12292u;

}  // namespace chunsa::determinism_baselines
