// Sprint 1.8E — política pura de asequibilidad del HUD.

#include <cstdint>
#include <iostream>

#include "affordability_view.hpp"

namespace {

using chunsa::presentation::AffordabilityResult;
using chunsa::presentation::assess_affordability;

void expect(bool condition, const char* name, int& failures) {
    if (!condition) {
        std::cerr << "FALLO: " << name << '\n';
        ++failures;
    }
}

}  // namespace

int main() {
    int failures = 0;

    // Alcanza justo: coste y stock iguales no producen faltantes.
    {
        const int32_t cost[] = {100, 50, 0};
        const int64_t stock[] = {100, 50, 0};
        const AffordabilityResult result = assess_affordability(cost, stock, 3u);
        expect(result.affordable, "coste justo es asequible", failures);
        expect(result.missing_count == 0u, "coste justo no tiene faltantes", failures);
    }

    // Falta un único recurso y se informa la cantidad exacta.
    {
        const int32_t cost[] = {100, 50, 0};
        const int64_t stock[] = {80, 50, 0};
        const AffordabilityResult result = assess_affordability(cost, stock, 3u);
        expect(!result.affordable, "un faltante hace no asequible", failures);
        expect(result.missing_count == 1u, "un solo recurso faltante", failures);
        expect(result.missing[0].resource_index == 0u &&
                       result.missing[0].amount == 20,
               "faltante único exacto", failures);
    }

    // Faltan varios recursos y se conservan ambos índices/cantidades.
    {
        const int32_t cost[] = {100, 50, 25};
        const int64_t stock[] = {80, 10, 25};
        const AffordabilityResult result = assess_affordability(cost, stock, 3u);
        expect(!result.affordable, "varios faltantes hacen no asequible", failures);
        expect(result.missing_count == 2u, "se enumeran varios faltantes", failures);
        expect(result.missing[0].resource_index == 0u &&
                       result.missing[0].amount == 20 &&
                       result.missing[1].resource_index == 1u &&
                       result.missing[1].amount == 40,
               "varios faltantes exactos", failures);
    }

    // Coste vacío: siempre asequible, incluso con stock cero.
    {
        const int32_t cost[] = {0, 0, 0};
        const int64_t stock[] = {0, 0, 0};
        const AffordabilityResult result = assess_affordability(cost, stock, 3u);
        expect(result.affordable, "coste vacío es asequible", failures);
        expect(result.missing_count == 0u, "coste vacío no tiene faltantes", failures);
    }

    // Stock cero con coste positivo: falta todo el coste.
    {
        const int32_t cost[] = {60, 30};
        const int64_t stock[] = {0, 0};
        const AffordabilityResult result = assess_affordability(cost, stock, 2u);
        expect(!result.affordable, "stock cero no alcanza un coste positivo", failures);
        expect(result.missing_count == 2u, "stock cero enumera todos los faltantes", failures);
        expect(result.missing[0].amount == 60 && result.missing[1].amount == 30,
               "stock cero informa cantidades completas", failures);
    }

    if (failures == 0) {
        std::cout << "affordability_view OK\n";
        return 0;
    }
    std::cerr << "affordability_view: " << failures << " fallo(s)\n";
    return 1;
}
