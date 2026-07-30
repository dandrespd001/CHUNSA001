#pragma once

#include <cstdint>

namespace chunsa::presentation {

enum class ButtonState : uint8_t {
    AVAILABLE = 0,
    DIMMED_COST = 1,
    DIMMED_EPOCH = 2,
    UNAVAILABLE = 3,
};

struct ButtonGating {
    bool belongs_to_player_civ;
    bool epoch_reached;
    bool affordable;
    bool otherwise_blocked;
};

// La precedencia NO es arbitraria: reproduce el orden en que el kernel valida
// (SPEC-007 §12.4 y el TRAIN_UNIT/RESEARCH_TECH de SPEC-004), para que la
// interfaz nombre siempre la MISMA razón que daría un rechazo. Si dijera «te
// falta madera» cuando lo que falta es llegar a la edad 5, el jugador juntaría
// madera indefinidamente sin entender nada.
inline ButtonState button_state(const ButtonGating& g) noexcept {
    if (!g.belongs_to_player_civ || g.otherwise_blocked) {
        return ButtonState::UNAVAILABLE;
    }
    if (!g.epoch_reached) return ButtonState::DIMMED_EPOCH;
    if (!g.affordable) return ButtonState::DIMMED_COST;
    return ButtonState::AVAILABLE;
}

// El kernel rechaza con `tdef.epoch > player_epoch` (step.hpp:602): una
// tecnología NO caduca al pasar de edad. draw_selection_panel usaba igualdad
// exacta y, con las tecnologías egipcias de época 4, al llegar a la 5 habría
// dicho «Requiere época 4» sobre algo perfectamente investigable.
inline bool tech_epoch_reached(uint8_t tech_epoch, uint8_t player_epoch) noexcept {
    return tech_epoch <= player_epoch;
}

inline bool window_epoch_reached(uint8_t epoch_min,
                                 uint8_t epoch_max,
                                 uint8_t player_epoch) noexcept {
    return player_epoch >= epoch_min && player_epoch <= epoch_max;
}

// Se enseña la edad actual y UNA de adelanto. Mostrar las 15 edades haría el
// panel ilegible desde el primer minuto; mostrar solo la actual escondería
// hacia dónde va la partida, que es la mitad de la decisión de subir de época.
inline bool panel_item_visible(uint8_t epoch_min,
                               uint8_t epoch_max,
                               uint8_t player_epoch,
                               bool belongs_to_player_civ) noexcept {
    if (!belongs_to_player_civ) return false;
    if (player_epoch > epoch_max) return false;  // su ventana ya pasó
    return epoch_min <= static_cast<uint8_t>(player_epoch + 1u);
}

}  // namespace chunsa::presentation
