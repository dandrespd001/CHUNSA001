#pragma once

#include <cstdint>

namespace chunsa::presentation {

// El panel es una rejilla fija de 3x5. Esta política no sabe nada de Godot ni
// de cómo se dibuja una celda: solo decide cómo partir una lista en páginas.
inline constexpr uint32_t COMMAND_PANEL_PAGE_SIZE = 15u;

struct PageRange {
    // Rango semiabierto [begin, end): end nunca supera el número de elementos.
    uint32_t begin;
    uint32_t end;
};

// Cuántas páginas hacen falta para `item_count` elementos. Una lista vacía no
// tiene página: el adaptador la representa como página 0 sin botones.
inline constexpr uint32_t command_page_count(uint32_t item_count) noexcept {
    return item_count / COMMAND_PANEL_PAGE_SIZE +
            (item_count % COMMAND_PANEL_PAGE_SIZE == 0u ? 0u : 1u);
}

// Rango de elementos que corresponde a una página válida. Una página fuera de
// rango, incluida cualquier página de una lista vacía, devuelve un rango vacío
// en vez de permitir que el llamante dibuje elementos inexistentes.
inline constexpr PageRange command_page_range(uint32_t item_count,
                                              uint32_t page) noexcept {
    const uint32_t pages = command_page_count(item_count);
    if (page >= pages) return PageRange{0u, 0u};

    const uint32_t begin = page * COMMAND_PANEL_PAGE_SIZE;
    const uint32_t remaining = item_count - begin;
    const uint32_t length = remaining < COMMAND_PANEL_PAGE_SIZE
            ? remaining : COMMAND_PANEL_PAGE_SIZE;
    return PageRange{begin, begin + length};
}

// Las páginas se recorren circularmente. Con cero páginas no hay estado que
// avanzar y se conserva el valor canónico 0.
inline constexpr uint32_t next_command_page(uint32_t current_page,
                                             uint32_t pages) noexcept {
    if (pages == 0u || current_page >= pages) return 0u;
    return current_page + 1u < pages ? current_page + 1u : 0u;
}

inline constexpr uint32_t previous_command_page(uint32_t current_page,
                                                 uint32_t pages) noexcept {
    if (pages == 0u || current_page >= pages) return 0u;
    return current_page == 0u ? pages - 1u : current_page - 1u;
}

// Una selección nueva puede producir una lista más corta. Si la página que
// estaba abierta ya no existe, se vuelve a la primera; nunca se deja una
// rejilla vacía en una página fantasma. Si sigue existiendo, se conserva para
// no arrebatarle al jugador su posición al cambiar de frame.
inline constexpr uint32_t command_page_after_selection_change(
        uint32_t current_page, uint32_t item_count) noexcept {
    return current_page < command_page_count(item_count) ? current_page : 0u;
}

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
