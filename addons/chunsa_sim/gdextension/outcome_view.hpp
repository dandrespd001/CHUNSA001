#pragma once

#include <cstdint>

#include <chunsa/game_state.hpp>

namespace chunsa::presentation {

enum class MatchOutcome : uint8_t {
    DRAW = 0,
    VICTORY = 1,
    DEFEAT = 2,
};

// `winner` sale de step.hpp: es el ÍNDICE del único jugador no derrotado, o el
// centinela 0xFF cuando todos cayeron en el mismo tick (game_state.hpp:356). El
// 0xFF es lo ÚNICO que significa "sin ganador"; un 0 es una victoria legítima
// del jugador 0. Cualquier ganador que no sea el espectador es una derrota para
// él, hasta los 8 jugadores de SPEC-008 §4 — no un empate.
inline MatchOutcome match_outcome(uint8_t winner, uint8_t viewer) noexcept {
    if (winner == NO_WINNER) return MatchOutcome::DRAW;
    return winner == viewer ? MatchOutcome::VICTORY : MatchOutcome::DEFEAT;
}

inline const char* outcome_label(MatchOutcome outcome) noexcept {
    switch (outcome) {
        case MatchOutcome::VICTORY: return "VICTORIA";
        case MatchOutcome::DEFEAT:  return "DERROTA";
        case MatchOutcome::DRAW:    return "EMPATE";
    }
    return "EMPATE";
}

}  // namespace chunsa::presentation
