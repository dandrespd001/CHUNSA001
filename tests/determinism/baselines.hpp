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
inline constexpr uint64_t G1_SYNTHETIC_STATE = 0xa4066425c201a6edull;

// G3: savetest canónico sin IA, save@200 y continuación hasta tick 400.
// Cambió solo por el bump V8→V9; save/load conserva la continuación.
inline constexpr uint64_t G3_SAVETEST_STATE = 0xa270724f3cf82b43ull;
inline constexpr uint64_t G3_SAVETEST_CONTINUATION = 0x664015c31beb0485ull;

// G4: savetest canónico con IA, save@200 y continuación hasta tick 400.
// Cambió solo por el bump V8→V9; save/load con IA conserva la continuación.
inline constexpr uint64_t G4_SAVETEST_AI_STATE = 0x8299d1017cfda67bull;
inline constexpr uint64_t G4_SAVETEST_AI_CONTINUATION = 0x59d87ef2adbe14e2ull;

// SPEC-005 §8.3: skirmish militar sin ciudadanos.
// Cambió solo por el bump V8→V9; winner=1 y end_tick=1226 intactos.
inline constexpr uint64_t AI_SKIRMISH_STATE = 0xbe2d1b16c3e9e114ull;
inline constexpr uint64_t AI_SKIRMISH_CONTINUATION = 0xf6eb46ea0ebcb3f3ull;

// SPEC-004 §7.1: skirmish con economía y ciudadanos vulnerables.
// Sprint 1.7 §23: trayectoria nueva por zona aliada y depósito base del
// fixture sintético; conserva economía real, winner=1 y fin <36000.
// Sprint 1.8A: hashes cambiados solo por V8→V9; end_tick=1107 intacto.
inline constexpr uint64_t AI_SKIRMISH_ECO_STATE = 0xc03881286b166992ull;
inline constexpr uint64_t AI_SKIRMISH_ECO_CONTINUATION = 0x0c38afa6e9814a2eull;
inline constexpr uint32_t AI_SKIRMISH_ECO_END_TICK = 1107u;

// SPEC-004 §20/§22: apertura económica completa con control de ciudadano.
// Sprint 1.7 §23: la auto-recolección acotada evita marchas a neutrales
// remotos; winner=1 y las cuatro fases se conservan, fin 12292→9317.
// Sprint 1.8A: hashes cambiados solo por V8→V9; end_tick=9317 intacto.
// Sprint 1.9 (recetas y CRAFT): el checksum sube a V10 (craft_recipe/
// craft_progress entran al dominio) y el mapa gana un par espejado de ESTAÑO
// sin el cual el bronce es infabricable, asi que la trayectoria se mueve:
// 9438 -> 9411. winner=1 y las CUATRO fases se conservan.
//
// Sprint 1.8D (contenido, depósitos y costes reales): trayectorias se mueven
// INTENCIONALMENTE — los nuevos depósitos cubren las épocas 1-4 (cobre/oro/
// arcilla/sal en el centro, food/wood/stone en zona propia con cantidades
// 3x) y los costes de unidades/edificios se redimensionan (≤3 recursos cada
// uno, sin nuevos recursos en coste para no romper regresiones en el catálogo
// golden). winner=1, las cuatro fases observadas, fin 9438<36000. Los
// hashes y el end_tick son los medidos por el gate canónico contra el CHDB
// recompilado — re-registrados con justificación en docs/RESULT_MINIMAX_1.8D.md.
inline constexpr uint64_t AI_SKIRMISH_APERTURA_STATE = 0xeb55df84d4262787ull;
inline constexpr uint64_t AI_SKIRMISH_APERTURA_CONTINUATION = 0xfc48d08239643d49ull;
inline constexpr uint32_t AI_SKIRMISH_APERTURA_END_TICK = 9411u;

}  // namespace chunsa::determinism_baselines
