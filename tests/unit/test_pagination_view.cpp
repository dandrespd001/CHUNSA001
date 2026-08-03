// Sprint 1.16 — política pura de paginación del panel de comandos.
//
// Se prueba sin Godot: la rejilla solo tiene 15 huecos, pero el catálogo puede
// tener muchos más elementos. Los límites 15/16 y el retorno desde una página
// que desaparece son precisamente los casos que no deben quedar implícitos en
// el código de dibujo.

#include <cstdint>
#include <cstdio>

#include "command_panel_view.hpp"

static int g_fails = 0;
#define CHECK(cond) do { \
    if (!(cond)) { \
        ++g_fails; \
        std::printf("CHECK L%d: %s\n", __LINE__, #cond); \
    } \
} while (false)

namespace {

using chunsa::presentation::COMMAND_PANEL_PAGE_SIZE;
using chunsa::presentation::PageRange;
using chunsa::presentation::command_page_after_selection_change;
using chunsa::presentation::command_page_count;
using chunsa::presentation::command_page_range;
using chunsa::presentation::next_command_page;
using chunsa::presentation::previous_command_page;

void check_range(PageRange range, uint32_t begin, uint32_t end) {
    CHECK(range.begin == begin);
    CHECK(range.end == end);
}

}  // namespace

int main() {
    // La capacidad contractual es exactamente la rejilla 3x5.
    CHECK(COMMAND_PANEL_PAGE_SIZE == 15u);

    // 0 elementos: no hay páginas ni rango dibujable.
    CHECK(command_page_count(0u) == 0u);
    check_range(command_page_range(0u, 0u), 0u, 0u);

    // Límite inferior: 15 elementos llenan una sola página, no crean una
    // segunda página vacía.
    CHECK(command_page_count(15u) == 1u);
    check_range(command_page_range(15u, 0u), 0u, 15u);
    check_range(command_page_range(15u, 1u), 0u, 0u);

    // Límite superior: el elemento 16 abre la página 1 y ocupa solo su primer
    // hueco; la primera página sigue siendo [0, 15).
    CHECK(command_page_count(16u) == 2u);
    check_range(command_page_range(16u, 0u), 0u, 15u);
    check_range(command_page_range(16u, 1u), 15u, 16u);

    // Dos páginas exactas y una tercera parcial.
    CHECK(command_page_count(30u) == 2u);
    check_range(command_page_range(30u, 1u), 15u, 30u);
    CHECK(command_page_count(31u) == 3u);
    check_range(command_page_range(31u, 2u), 30u, 31u);

    // Avance circular: última -> primera.
    CHECK(next_command_page(0u, 3u) == 1u);
    CHECK(next_command_page(1u, 3u) == 2u);
    CHECK(next_command_page(2u, 3u) == 0u);

    // Retroceso circular: primera -> última.
    CHECK(previous_command_page(2u, 3u) == 1u);
    CHECK(previous_command_page(1u, 3u) == 0u);
    CHECK(previous_command_page(0u, 3u) == 2u);

    // Reducción estando en la última página: al desaparecer la página 2, la
    // respuesta es página 0, nunca una página vacía. Si sigue existiendo, se
    // conserva la página actual.
    CHECK(command_page_after_selection_change(2u, 31u) == 2u);
    CHECK(command_page_after_selection_change(2u, 30u) == 0u);
    CHECK(command_page_after_selection_change(1u, 16u) == 1u);
    CHECK(command_page_after_selection_change(1u, 15u) == 0u);
    CHECK(command_page_after_selection_change(0u, 0u) == 0u);

    // Estados defensivos: una página inválida nunca se propaga a una lista
    // circular ni produce un rango fuera del catálogo.
    CHECK(next_command_page(99u, 3u) == 0u);
    CHECK(previous_command_page(99u, 3u) == 0u);
    check_range(command_page_range(31u, 3u), 0u, 0u);

    if (g_fails == 0) {
        std::printf("pagination_view OK\n");
        return 0;
    }
    std::printf("pagination_view: %d fallo(s)\n", g_fails);
    return 1;
}

