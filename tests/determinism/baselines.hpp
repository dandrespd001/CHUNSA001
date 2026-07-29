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
// Cambió solo por el bump V8→V9; alloc_delta=0 y doble corrida idéntica.
inline constexpr uint64_t G1_SYNTHETIC_STATE = 0x6d66e42f0109605aull;

// G3: savetest canónico sin IA, save@200 y continuación hasta tick 400.
// Cambió solo por el bump V8→V9; save/load conserva la continuación.
inline constexpr uint64_t G3_SAVETEST_STATE = 0x5a20ac5093ec9708ull;
inline constexpr uint64_t G3_SAVETEST_CONTINUATION = 0xf19faf596c019cb4ull;

// G4: savetest canónico con IA, save@200 y continuación hasta tick 400.
// Cambió solo por el bump V8→V9; save/load con IA conserva la continuación.
inline constexpr uint64_t G4_SAVETEST_AI_STATE = 0xd3dbc590712d0b7bull;
inline constexpr uint64_t G4_SAVETEST_AI_CONTINUATION = 0x958ca2dcbbd8e4e3ull;

// SPEC-005 §8.3: skirmish militar sin ciudadanos.
// Cambió solo por el bump V8→V9; winner=1 y end_tick=1226 intactos.
inline constexpr uint64_t AI_SKIRMISH_STATE = 0x1bb2d03ff34709d3ull;
inline constexpr uint64_t AI_SKIRMISH_CONTINUATION = 0xd7410b71f1c526b1ull;

// SPEC-004 §7.1: skirmish con economía y ciudadanos vulnerables.
// Sprint 1.7 §23: trayectoria nueva por zona aliada y depósito base del
// fixture sintético; conserva economía real, winner=1 y fin <36000.
// Sprint 1.8A: hashes cambiados solo por V8→V9; end_tick=1107 intacto.
inline constexpr uint64_t AI_SKIRMISH_ECO_STATE = 0xe268dbc0346607edull;
inline constexpr uint64_t AI_SKIRMISH_ECO_CONTINUATION = 0xd07629db4b491e7aull;
inline constexpr uint32_t AI_SKIRMISH_ECO_END_TICK = 1107u;

// SPEC-004 §20/§22: apertura económica completa con control de ciudadano.
// Sprint 1.7 §23: la auto-recolección acotada evita marchas a neutrales
// remotos; winner=1 y las cuatro fases se conservan, fin 12292→9317.
// Sprint 1.8A: hashes cambiados solo por V8→V9; end_tick=9317 intacto.
inline constexpr uint64_t AI_SKIRMISH_APERTURA_STATE = 0xfb9f9d45c3430ba4ull;
inline constexpr uint64_t AI_SKIRMISH_APERTURA_CONTINUATION = 0x738854e75ae38caeull;
inline constexpr uint32_t AI_SKIRMISH_APERTURA_END_TICK = 9317u;

}  // namespace chunsa::determinism_baselines
