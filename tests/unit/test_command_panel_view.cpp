// Sprint 1.8H — política pura del panel de comandos (SPEC-006 Parte V §28).
//
// Dos decisiones se prueban aquí, y las dos tienen consecuencias visibles:
//
// 1. QUÉ SE VE. Un panel que mostrara todo el catálogo de 15 edades sería
//    ilegible en la edad 1; uno que mostrara solo la edad actual escondería
//    hacia dónde va la partida. Se enseña la edad actual y UNA de adelanto.
//
// 2. POR QUÉ NO SE PUEDE. Cuando varias razones aplican a la vez, la interfaz
//    debe nombrar LA MISMA que nombraría el kernel al rechazar, y el kernel
//    valida en un orden contractual (SPEC-007 §12.4): civilización y estado
//    antes que época, y época antes que stock. Si la UI dijera «te falta
//    madera» cuando en realidad falta llegar a la edad 5, el jugador juntaría
//    madera para siempre.

#include <cstdint>
#include <iostream>

#include "command_panel_view.hpp"

namespace {

using chunsa::presentation::ButtonGating;
using chunsa::presentation::ButtonState;
using chunsa::presentation::button_state;
using chunsa::presentation::panel_item_visible;

void expect(bool condition, const char* name, int& failures) {
    if (!condition) {
        std::cerr << "FALLO: " << name << '\n';
        ++failures;
    }
}

ButtonGating gating(bool civ, bool epoch, bool afford, bool blocked) {
    ButtonGating g;
    g.belongs_to_player_civ = civ;
    g.epoch_reached = epoch;
    g.affordable = afford;
    g.otherwise_blocked = blocked;
    return g;
}

}  // namespace

int main() {
    int failures = 0;

    // --- Qué se ve -----------------------------------------------------
    // Edad actual: visible.
    expect(panel_item_visible(3u, 4u, 3u, true),
           "un elemento de la edad actual se ve", failures);

    // Una edad de adelanto: visible (atenuado lo decide button_state).
    expect(panel_item_visible(5u, 5u, 4u, true),
           "un elemento de la edad siguiente se ve, para enseñar lo que viene",
           failures);

    // Dos edades por delante: NO, o el panel se inunda.
    expect(!panel_item_visible(5u, 5u, 3u, true),
           "un elemento a dos edades de distancia no se ve todavía", failures);

    // Obsoleto: su ventana ya pasó.
    expect(!panel_item_visible(1u, 2u, 3u, true),
           "un elemento cuya ventana de edad ya pasó no se ve", failures);

    // Otra civilización: nunca.
    expect(!panel_item_visible(3u, 4u, 3u, false),
           "un elemento de otra civilización no se ve nunca", failures);

    // --- Por qué no se puede -------------------------------------------
    expect(button_state(gating(true, true, true, false)) == ButtonState::AVAILABLE,
           "todo en orden es accionable", failures);

    expect(button_state(gating(true, true, false, false)) == ButtonState::DIMMED_COST,
           "solo falta stock", failures);

    expect(button_state(gating(true, false, true, false)) == ButtonState::DIMMED_EPOCH,
           "solo falta edad", failures);

    expect(button_state(gating(false, true, true, false)) == ButtonState::UNAVAILABLE,
           "otra civilización es no disponible", failures);

    expect(button_state(gating(true, true, true, true)) == ButtonState::UNAVAILABLE,
           "un bloqueo de estado es no disponible", failures);

    // La precedencia, que es el punto de todo esto: cuando falta la edad Y el
    // stock, se nombra la EDAD, igual que haría el kernel.
    expect(button_state(gating(true, false, false, false)) == ButtonState::DIMMED_EPOCH,
           "si faltan edad y stock, manda la edad", failures);

    // Y el bloqueo de estado manda sobre todo lo demás.
    expect(button_state(gating(true, false, false, true)) == ButtonState::UNAVAILABLE,
           "el bloqueo de estado manda sobre edad y stock", failures);

    expect(button_state(gating(false, false, false, false)) == ButtonState::UNAVAILABLE,
           "la civilización manda sobre edad y stock", failures);

    if (failures == 0) {
        std::cout << "command_panel_view OK\n";
        return 0;
    }
    std::cerr << "command_panel_view: " << failures << " fallo(s)\n";
    return 1;
}
