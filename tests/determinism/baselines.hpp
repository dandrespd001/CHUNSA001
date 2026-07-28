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
// Cambió solo por el bump V7→V8; trayectoria V7 bit-exacta probada en 4be7110.
inline constexpr uint64_t G1_SYNTHETIC_STATE = 0x770b83a7cf97bd12ull;

// G3: savetest canónico sin IA, save@200 y continuación hasta tick 400.
// Cambió solo por el bump V7→V8; trayectoria V7 bit-exacta probada en 4be7110.
inline constexpr uint64_t G3_SAVETEST_STATE = 0x4083889b6a9f9a14ull;
// Cambió solo por el bump V7→V8; trayectoria V7 bit-exacta probada en 4be7110.
inline constexpr uint64_t G3_SAVETEST_CONTINUATION = 0xead0dc41779bdc9eull;

// G4: savetest canónico con IA, save@200 y continuación hasta tick 400.
// Cambió solo por el bump V7→V8; trayectoria V7 bit-exacta probada en 4be7110.
inline constexpr uint64_t G4_SAVETEST_AI_STATE = 0x6d2552c57b2b4f7eull;
// Cambió solo por el bump V7→V8; trayectoria V7 bit-exacta probada en 4be7110.
inline constexpr uint64_t G4_SAVETEST_AI_CONTINUATION = 0x8f39cd2b72df2871ull;

// SPEC-005 §8.3: skirmish militar sin ciudadanos.
// Cambió solo por el bump V7→V8; trayectoria V7 bit-exacta probada en 4be7110.
inline constexpr uint64_t AI_SKIRMISH_STATE = 0x5d7603757c533e97ull;
// Cambió solo por el bump V7→V8; trayectoria V7 bit-exacta probada en 4be7110.
inline constexpr uint64_t AI_SKIRMISH_CONTINUATION = 0x4cdfd0b15dc12daaull;

// SPEC-004 §7.1: skirmish con economía y ciudadanos vulnerables.
// Cambió solo por el bump V7→V8; trayectoria V7 bit-exacta probada en 4be7110.
inline constexpr uint64_t AI_SKIRMISH_ECO_STATE = 0x68ae70ca41c3834bull;
// Cambió solo por el bump V7→V8; trayectoria V7 bit-exacta probada en 4be7110.
inline constexpr uint64_t AI_SKIRMISH_ECO_CONTINUATION = 0xbbcd6fba69413eeeull;

// SPEC-004 §20/§22: apertura económica completa con control de ciudadano.
// Ya era V8; la corrección universal reprodujo el valor medido en 4be7110.
inline constexpr uint64_t AI_SKIRMISH_APERTURA_STATE = 0xc7b04caea8c32e64ull;
// Ya era V8; la corrección universal reprodujo el valor medido en 4be7110.
inline constexpr uint64_t AI_SKIRMISH_APERTURA_CONTINUATION = 0x4ae3ddd1ad5ab4f9ull;
inline constexpr uint32_t AI_SKIRMISH_APERTURA_END_TICK = 12292u;

}  // namespace chunsa::determinism_baselines
