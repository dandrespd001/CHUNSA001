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
inline constexpr uint64_t G1_SYNTHETIC_STATE = 0xcf02938f8970154cull;

// G3: savetest canónico sin IA, save@200 y continuación hasta tick 400.
// Cambió solo por el bump V8→V9; save/load conserva la continuación.
inline constexpr uint64_t G3_SAVETEST_STATE = 0x3aa51e9a81a1aa76ull;
inline constexpr uint64_t G3_SAVETEST_CONTINUATION = 0xe0200bd08ffeef25ull;

// G4: savetest canónico con IA, save@200 y continuación hasta tick 400.
// Cambió solo por el bump V8→V9; save/load con IA conserva la continuación.
inline constexpr uint64_t G4_SAVETEST_AI_STATE = 0x64c8802509243773ull;
inline constexpr uint64_t G4_SAVETEST_AI_CONTINUATION = 0xac2d254942afd6d9ull;

// SPEC-005 §8.3: skirmish militar sin ciudadanos.
// Cambió solo por el bump V8→V9; winner=1 y end_tick=1226 intactos.
inline constexpr uint64_t AI_SKIRMISH_STATE = 0x20c03ff5bc0e0bb3ull;
inline constexpr uint64_t AI_SKIRMISH_CONTINUATION = 0xb588c1e22aab70dbull;

// SPEC-004 §7.1: skirmish con economía y ciudadanos vulnerables.
// Sprint 1.7 §23: trayectoria nueva por zona aliada y depósito base del
// fixture sintético; conserva economía real, winner=1 y fin <36000.
// Sprint 1.8A: hashes cambiados solo por V8→V9; end_tick=1107 intacto.
inline constexpr uint64_t AI_SKIRMISH_ECO_STATE = 0x5da7c43062205b85ull;
inline constexpr uint64_t AI_SKIRMISH_ECO_CONTINUATION = 0xd97d34bb86f4901dull;
inline constexpr uint32_t AI_SKIRMISH_ECO_END_TICK = 1107u;

// SPEC-004 §20/§22: apertura económica completa con control de ciudadano.
// Sprint 1.7 §23: la auto-recolección acotada evita marchas a neutrales
// remotos; winner=1 y las cuatro fases se conservan, fin 12292→9317.
// Sprint 1.8A: hashes cambiados solo por V8→V9; end_tick=9317 intacto.
// Sprint 1.13 (ordenes de combate): SOLO cambian los hashes, por el bump del
// checksum a V11 (attack_target/order_mode entran al dominio). TODOS los
// end_tick quedan intactos —1226, 1107, 9542— porque el sistema de ordenes no
// hace nada cuando nadie da ordenes, que es el caso de los escenarios de gate.
//
// Sprint 1.18 (armadura por tipo de dano): el combate deja de usar el
// multiplicador opaco rps_mult y pasa a `attack - armadura + bono`. Solo se
// movio la APERTURA (9411 -> 9542), que es el unico escenario con catalogo
// real; G1/G3/G4, skirmish y eco quedaron BIT-IDENTICOS porque sus fixtures
// sinteticos no llevan armadura ni bonos. winner=1 y las cuatro fases, intactos.
//
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
inline constexpr uint64_t AI_SKIRMISH_APERTURA_STATE = 0x974b972f70ae54ecull;
inline constexpr uint64_t AI_SKIRMISH_APERTURA_CONTINUATION = 0x4c43be2a4614a935ull;
inline constexpr uint32_t AI_SKIRMISH_APERTURA_END_TICK = 9542u;

}  // namespace chunsa::determinism_baselines
