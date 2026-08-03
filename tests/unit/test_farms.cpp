// Granjas: comida renovable (Sprint 1.28, SPEC-007 §15).
//
// POR QUÉ EXISTE. El marco de 15 épocas del Sprint 1.25 hizo posible una
// partida larga que el mapa hace IMPOSIBLE de terminar:
//
//   · 6000 de comida en todo el mapa, para los dos jugadores.
//   · Subir de época cuesta 200. Catorce subidas son 2800 — casi la mitad del
//     mapa sólo en avanzar.
//   · Un aldeano cuesta 25. Lo que queda da para unos 60 por bando en TODA la
//     partida, sin contar ejército.
//   · La comida no se regenera: cero ocurrencias en economy.hpp y step.hpp.
//
// No es un problema de balance de números. Falta un sistema.
//
// EL DISEÑO, y por qué éste. Una granja es un DEPÓSITO QUE SE REGENERA. Se
// descartaron dos alternativas:
//   · Receta madera→comida con el sistema CRAFT: no toca el kernel, pero sólo
//     mueve el muro a la madera, que también es finita.
//   · Edificio que produce comida sin entradas: degenera en «construye granjas
//     y espera».
// La agricultura regenera de verdad —el trabajo lo hace el sol— y es el único
// mecanismo que hace posible una partida larga. Queda acotado por el coste de
// la granja, el espacio que ocupa y el tiempo de aldeano que consume.
//
// LA GRANJA ES UN EDIFICIO, NO UNA MENA (corrección del Director, 2026-08-03).
// El depósito que registra es un detalle interno del mecanismo de recolección.
// La granja tiene HP, ocupa espacio, se derriba, y cuenta como edificio para
// ADVANCE_EPOCH igual que cualquier otro. Un yacimiento agotado desaparece;
// una granja agotada sigue ahí, regenerando.

#include <cstdint>
#include <cstdio>

#include "chunsa/economy.hpp"

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

int main() {
    // 1) COMPATIBILIDAD: un depósito con regen 0 es EXACTAMENTE lo de hoy.
    //    Es la comprobación que separa «he añadido algo» de «he cambiado
    //    todo». Si esto falla, los 22 depósitos del mapa cambiaron de
    //    comportamiento sin que nadie lo pidiera.
    {
        EcoDeposit d{};
        d.remaining = 100;
        d.regen_per_tick = 0;
        d.cap = 0;
        for (uint32_t t = 0; t < 1000u; ++t) eco_regen_deposit(d);
        CHECK(d.remaining == 100);
    }

    // 2) Un depósito agotado con regen 0 SIGUE agotado. Un yacimiento vacío no
    //    resucita.
    {
        EcoDeposit d{};
        d.remaining = 0;
        d.regen_per_tick = 0;
        for (uint32_t t = 0; t < 100u; ++t) eco_regen_deposit(d);
        CHECK(d.remaining == 0);
    }

    // 3) La granja regenera hasta el techo y NO lo pasa. Sin el tope, una
    //    granja vieja acumularía comida infinita y el jugador que no la usara
    //    sería premiado por no jugar.
    {
        EcoDeposit d{};
        d.remaining = 0;
        d.regen_per_tick = 1;
        d.cap = 10;
        for (uint32_t t = 0; t < 100u; ++t) eco_regen_deposit(d);
        CHECK(d.remaining == 10);
    }

    // 4) Regeneración PARCIAL: tras 5 ticks con 1/tick hay exactamente 5.
    //    Fija el ritmo, no sólo el destino.
    {
        EcoDeposit d{};
        d.remaining = 0;
        d.regen_per_tick = 1;
        d.cap = 100;
        for (uint32_t t = 0; t < 5u; ++t) eco_regen_deposit(d);
        CHECK(d.remaining == 5);
    }

    // 5) Si ya está POR ENCIMA del techo —dato mal puesto, o un techo que baja
    //    con una tecnología— no se recorta. Quitarle recursos al jugador por un
    //    cambio de catálogo sería peor que el exceso.
    {
        EcoDeposit d{};
        d.remaining = 50;
        d.regen_per_tick = 1;
        d.cap = 10;
        eco_regen_deposit(d);
        CHECK(d.remaining == 50);
    }

    // 6) La regeneración es ENTERA y no se desborda con un ritmo absurdo.
    //    `cap` manda siempre.
    {
        EcoDeposit d{};
        d.remaining = 0;
        d.regen_per_tick = 1000000;
        d.cap = 150;
        eco_regen_deposit(d);
        CHECK(d.remaining == 150);
    }

    // 7) UN DEPÓSITO DE GRANJA SE DISTINGUE DE UN YACIMIENTO. Es la corrección
    //    del Director hecha comprobación: quien cuente yacimientos del mapa
    //    debe poder excluir las granjas, y quien dibuje el minimapa también.
    {
        EcoDeposit mina{};
        mina.owner_building = ECO_NO_OWNER;
        EcoDeposit granja{};
        granja.owner_building = 7u;
        CHECK(!eco_deposit_is_farm(mina));
        CHECK(eco_deposit_is_farm(granja));
    }

    if (g_fails == 0) {
        std::printf("farms OK\n");
        return 0;
    }
    std::printf("farms: %d fallo(s)\n", g_fails);
    return 1;
}
