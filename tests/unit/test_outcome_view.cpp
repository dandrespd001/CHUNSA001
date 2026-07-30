// Sprint 1.8F — política pura de presentación del resultado de partida.
//
// Motivación (test de juego del Director, 2026-07-30): la partida terminó con
// `winner=0`, que es una VICTORIA legítima del jugador 0, y el HUD la mostró
// bien. Pero la consola imprimía el entero crudo y `0` se lee como "nadie":
// el centinela de "sin ganador" es 0xFF, no 0 (game_state.hpp:356).
//
// Y al revisarlo apareció un defecto real: el adaptador cableaba dos jugadores
// (`winner==1` ⇒ DERROTA, cualquier otro ⇒ EMPATE), así que con los 8 jugadores
// que admite SPEC-008 §4 un `winner` de 2..7 se habría mostrado como EMPATE
// estando el espectador derrotado.

#include <cstdint>
#include <iostream>

#include "outcome_view.hpp"

namespace {

using chunsa::presentation::MatchOutcome;
using chunsa::presentation::match_outcome;
using chunsa::presentation::outcome_label;

void expect(bool condition, const char* name, int& failures) {
    if (!condition) {
        std::cerr << "FALLO: " << name << '\n';
        ++failures;
    }
}

}  // namespace

int main() {
    int failures = 0;

    // El caso del test de juego: gana el jugador 0 y lo mira el jugador 0.
    expect(match_outcome(0u, 0u) == MatchOutcome::VICTORY,
           "winner=0 visto por el jugador 0 es VICTORIA", failures);

    // El centinela, que es lo único que significa "sin ganador".
    expect(match_outcome(0xFFu, 0u) == MatchOutcome::DRAW,
           "winner=0xFF es EMPATE", failures);

    // Derrota clásica de dos jugadores.
    expect(match_outcome(1u, 0u) == MatchOutcome::DEFEAT,
           "winner=1 visto por el jugador 0 es DERROTA", failures);

    // El defecto que cazó esta prueba: con 8 jugadores (SPEC-008 §4) cualquier
    // ganador distinto del espectador es una DERROTA, nunca un empate.
    for (uint8_t w = 2u; w < 8u; ++w) {
        expect(match_outcome(w, 0u) == MatchOutcome::DEFEAT,
               "winner de 2..7 visto por el jugador 0 es DERROTA", failures);
    }

    // Simétrico: el espectador no siempre es el jugador 0.
    expect(match_outcome(3u, 3u) == MatchOutcome::VICTORY,
           "winner=3 visto por el jugador 3 es VICTORIA", failures);
    expect(match_outcome(0u, 3u) == MatchOutcome::DEFEAT,
           "winner=0 visto por el jugador 3 es DERROTA", failures);
    expect(match_outcome(0xFFu, 7u) == MatchOutcome::DRAW,
           "el empate es empate para cualquier espectador", failures);

    // La etiqueta existe y NO es vacía para los tres resultados: un hueco
    // invisible en la consola es justo lo que costó esta sesión de pruebas.
    const char* v = outcome_label(MatchOutcome::VICTORY);
    const char* d = outcome_label(MatchOutcome::DEFEAT);
    const char* e = outcome_label(MatchOutcome::DRAW);
    expect(v != nullptr && v[0] != '\0', "VICTORIA tiene etiqueta no vacía", failures);
    expect(d != nullptr && d[0] != '\0', "DERROTA tiene etiqueta no vacía", failures);
    expect(e != nullptr && e[0] != '\0', "EMPATE tiene etiqueta no vacía", failures);

    // Las tres etiquetas son distintas entre sí: si dos coincidieran, la
    // consola volvería a ser ambigua, que es el bug original.
    expect(v != d && d != e && v != e, "las tres etiquetas son distintas", failures);

    if (failures == 0) {
        std::cout << "outcome_view OK\n";
        return 0;
    }
    std::cerr << "outcome_view: " << failures << " fallo(s)\n";
    return 1;
}
