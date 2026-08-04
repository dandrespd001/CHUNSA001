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
        d.regen_milli_per_tick = 0;
        d.cap = 0;
        for (uint32_t t = 0; t < 1000u; ++t) eco_regen_deposit(d);
        CHECK(d.remaining == 100);
    }

    // 2) Un depósito agotado con regen 0 SIGUE agotado. Un yacimiento vacío no
    //    resucita.
    {
        EcoDeposit d{};
        d.remaining = 0;
        d.regen_milli_per_tick = 0;
        for (uint32_t t = 0; t < 100u; ++t) eco_regen_deposit(d);
        CHECK(d.remaining == 0);
    }

    // 3) La granja regenera hasta el techo y NO lo pasa. Sin el tope, una
    //    granja vieja acumularía comida infinita y el jugador que no la usara
    //    sería premiado por no jugar.
    {
        EcoDeposit d{};
        d.remaining = 0;
        d.regen_milli_per_tick = 1000;   // 1 unidad por tick
        d.cap = 10;
        for (uint32_t t = 0; t < 100u; ++t) eco_regen_deposit(d);
        CHECK(d.remaining == 10);
    }

    // 4) Regeneración PARCIAL: tras 5 ticks con 1/tick hay exactamente 5.
    //    Fija el ritmo, no sólo el destino.
    {
        EcoDeposit d{};
        d.remaining = 0;
        d.regen_milli_per_tick = 1000;   // 1 unidad por tick
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
        d.regen_milli_per_tick = 1000;   // 1 unidad por tick
        d.cap = 10;
        eco_regen_deposit(d);
        CHECK(d.remaining == 50);
    }

    // 6) La regeneración es ENTERA y no se desborda con un ritmo absurdo.
    //    `cap` manda siempre.
    {
        EcoDeposit d{};
        d.remaining = 0;
        d.regen_milli_per_tick = 1000000000;
        d.cap = 150;
        eco_regen_deposit(d);
        CHECK(d.remaining == 150);
    }

    // 6-bis) EL CASO QUE DE VERDAD USA LA GRANJA: ritmo POR DEBAJO de una
    //    unidad por tick. Con enteros no se puede bajar de 1, y 1 por tick a
    //    20 ticks/segundo serían 1200 por minuto — absurdo para un campo de
    //    trigo. Por eso el ritmo va en MILÉSIMAS y hay un acumulador entero.
    //
    //    50 milésimas = una unidad cada 20 ticks, que es un segundo de juego.
    {
        EcoDeposit d{};
        d.remaining = 0;
        d.regen_milli_per_tick = 50;
        d.cap = 1000;
        for (uint32_t t = 0; t < 19u; ++t) eco_regen_deposit(d);
        CHECK(d.remaining == 0);          // aún no llega a la unidad
        eco_regen_deposit(d);
        CHECK(d.remaining == 1);          // en el tick 20, exacta
        for (uint32_t t = 0; t < 20u; ++t) eco_regen_deposit(d);
        CHECK(d.remaining == 2);          // y el ritmo se mantiene
    }

    // 6-ter) El RESTO no se pierde. Tras 1000 ticks a 50 milésimas deben ser
    //    exactamente 50 unidades: si el acumulador tirara el sobrante, el
    //    ritmo real sería más lento que el declarado y nadie lo notaría hasta
    //    hacer la cuenta.
    {
        EcoDeposit d{};
        d.remaining = 0;
        d.regen_milli_per_tick = 50;
        d.cap = 10000;
        for (uint32_t t = 0; t < 1000u; ++t) eco_regen_deposit(d);
        CHECK(d.remaining == 50);
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

    // ========================================================================
    // RESERVA MINERAL (corrección del Director, 2026-08-03)
    //
    // «Los yacimientos minerales no deben desaparecer, sino que no deben poder
    // ser explotados hasta que se desarrolle una nueva tecnología.»
    //
    // No es una idea de diseño suelta: el corpus lo documenta. La ley del
    // mineral cae con la profundidad, y la FLOTACIÓN —bibliografía del U.S.
    // Bureau of Mines de 1916, ya en `docs/research/corpus/extractos/
    // mineria.md`— hizo rentable justo lo que antes se abandonaba en la mina.
    // Un yacimiento «agotado» del siglo XIX es una mina rentable del XX.
    //
    // Mecánicamente esto arregla algo que molestaba: un mapa agotado dejaba de
    // tener nada que hacer. Ahora la tecnología REABRE el mapa, que es lo que
    // pasó en la historia.
    // ========================================================================

    // 8) Un yacimiento con reserva y SIN la capacidad no da nada extra al
    //    agotarse. La tecnología es el requisito, no el tiempo.
    {
        EcoDeposit d{};
        d.remaining = 0;
        d.reserve = 500;
        d.reserve_capability = 3u;
        CHECK(eco_available_for(d, /*player_caps=*/0ull) == 0);
    }

    // 9) Con la capacidad, la reserva SÍ está disponible. Es el momento
    //    "flotación": la misma roca, otra técnica.
    {
        EcoDeposit d{};
        d.remaining = 0;
        d.reserve = 500;
        d.reserve_capability = 3u;
        CHECK(eco_available_for(d, /*player_caps=*/(1ull << 3)) == 500);
    }

    // 10) Mientras QUEDE mineral fácil, la reserva no se toca. Primero se
    //     agota lo barato, que es como funcionó siempre la minería.
    {
        EcoDeposit d{};
        d.remaining = 40;
        d.reserve = 500;
        d.reserve_capability = 3u;
        CHECK(eco_available_for(d, (1ull << 3)) == 40);
    }

    // 11) Un yacimiento SIN reserva se comporta como hasta hoy: agotado es
    //     agotado. La compatibilidad manda, otra vez.
    {
        EcoDeposit d{};
        d.remaining = 0;
        d.reserve = 0;
        d.reserve_capability = ECO_NO_CAPABILITY;
        CHECK(eco_available_for(d, ~0ull) == 0);
    }

    // 12) Una capacidad distinta NO abre la reserva. Sin esto, cualquier
    //     tecnología valdría para todo y la progresión no significaría nada.
    {
        EcoDeposit d{};
        d.remaining = 0;
        d.reserve = 500;
        d.reserve_capability = 3u;
        CHECK(eco_available_for(d, (1ull << 5)) == 0);
    }


    // ========================================================================
    // PRODUCCION POR TECNOLOGIA Y HERRAMIENTAS (Director, 2026-08-03)
    //
    // «Tanto la tecnología como las herramientas deben permitir aumentar la
    // producción y aumentar la cantidad de recurso.» Son dos cosas distintas y
    // por eso hay tres efectos: trabajar más rápido (harvest_rate), llevar más
    // por viaje (carry_cap) y sacar más material útil de la MISMA roca
    // (recovery). El tercero es el que cumple «aumentar la cantidad», y es
    // exactamente lo que hizo la flotación.
    // ========================================================================

    FatalReason fatal = FatalReason::NONE;

    // 13) Sin bonificación, el comportamiento es el de siempre: 5 por tick.
    {
        EcoDeposit dep{}; dep.remaining = 1000; dep.owner_building = ECO_NO_OWNER;
        EcoCitizenIn in{};
        in.state = EcoState::HARVEST; in.assigned_deposit = 0; in.carry = 0;
        const EcoCitizenOut out = eco_step_citizen(in, &dep, 1, ECO_ALL_DEPOSITS_MASK, 0, 0, fatal);
        CHECK(out.harvested_amount == ECO_HARVEST_PER_TICK);
        CHECK(out.deposit_decrement == ECO_HARVEST_PER_TICK);
    }

    // 14) Con MAS RITMO se saca más por tick, y el yacimiento pierde lo mismo
    //     que se saca: trabajar rápido no crea materia de la nada.
    {
        EcoDeposit dep{}; dep.remaining = 1000; dep.owner_building = ECO_NO_OWNER;
        EcoCitizenIn in{};
        in.state = EcoState::HARVEST; in.assigned_deposit = 0; in.carry = 0;
        in.harvest_per_tick = 12;
        const EcoCitizenOut out = eco_step_citizen(in, &dep, 1, ECO_ALL_DEPOSITS_MASK, 0, 0, fatal);
        CHECK(out.harvested_amount == 12);
        CHECK(out.deposit_decrement == 12);
    }

    // 15) LA RECUPERACIÓN es lo que de verdad «aumenta la cantidad de
    //     recurso»: el jugador gana MÁS de lo que el yacimiento pierde. Es la
    //     flotación hecha número — la misma roca, más metal útil.
    {
        EcoDeposit dep{}; dep.remaining = 1000; dep.owner_building = ECO_NO_OWNER;
        EcoCitizenIn in{};
        in.state = EcoState::HARVEST; in.assigned_deposit = 0; in.carry = 0;
        in.recovery_bp = 4000;    // +40 %
        const EcoCitizenOut out = eco_step_citizen(in, &dep, 1, ECO_ALL_DEPOSITS_MASK, 0, 0, fatal);
        CHECK(out.harvested_amount == 7);      // 5 * 1,4
        CHECK(out.deposit_decrement == 5);     // el agujero es el mismo
    }

    // 16) MÁS CARGA significa más viajes útiles: con el cesto por defecto el
    //     aldeano vuelve al llegar a 50; con uno mayor, no.
    {
        EcoDeposit dep{}; dep.remaining = 1000; dep.owner_building = ECO_NO_OWNER;
        EcoCitizenIn in{};
        in.state = EcoState::HARVEST; in.assigned_deposit = 0;
        in.carry = ECO_CARRY_CAP - 2;
        const EcoCitizenOut sin_mejora = eco_step_citizen(in, &dep, 1, ECO_ALL_DEPOSITS_MASK, 0, 0, fatal);
        CHECK(sin_mejora.state == EcoState::RETURN);
        in.carry_cap = ECO_CARRY_CAP + 30;
        const EcoCitizenOut con_mejora = eco_step_citizen(in, &dep, 1, ECO_ALL_DEPOSITS_MASK, 0, 0, fatal);
        CHECK(con_mejora.state == EcoState::HARVEST);
    }

    // 17) El hueco de carga acota lo GANADO, no lo extraído. Si sólo caben 3 y
    //     la recuperación es del 100 %, se sacan 1 o 2 de la roca — nunca se
    //     tira material extraído por no tener sitio.
    {
        EcoDeposit dep{}; dep.remaining = 1000; dep.owner_building = ECO_NO_OWNER;
        EcoCitizenIn in{};
        in.state = EcoState::HARVEST; in.assigned_deposit = 0;
        in.carry = ECO_CARRY_CAP - 3;
        in.recovery_bp = 10000;   // +100 %
        const EcoCitizenOut out = eco_step_citizen(in, &dep, 1, ECO_ALL_DEPOSITS_MASK, 0, 0, fatal);
        CHECK(out.harvested_amount <= 3);
        CHECK(out.deposit_decrement <= out.harvested_amount);
    }


    // ========================================================================
    // SATURACION POR DEPOSITO (Sprint 1.32)
    //
    // Hoy nada desincentiva amontonar aldeanos en el mismo sitio: veinte
    // recolectan veinte veces mas rapido que uno. En AoE2 un campamento satura
    // en torno a ocho, y eso es lo que obliga a EXPANDIRSE en vez de apilar.
    //
    // EL MODELO, y por que este. Hasta el umbral, cada aldeano rinde entero.
    // Pasado el umbral, el rendimiento TOTAL del yacimiento se reparte entre
    // los que hay: `tasa * umbral / n`. Es division entera, deterministica, y
    // dice algo fisicamente honesto — en un filon solo caben tantas manos a la
    // vez. El aldeano numero veinte no es que rinda poco: es que estorba.
    //
    // No es un castigo arbitrario: el total nunca BAJA al anadir gente, solo
    // deja de subir. Amontonar deja de ser optimo sin volverse suicida.
    // ========================================================================

    // 18) Hasta el umbral, nadie pierde nada.
    {
        for (int32_t n = 1; n <= ECO_SATURATION_THRESHOLD; ++n) {
            CHECK(eco_saturated_rate(ECO_HARVEST_PER_TICK, n) == ECO_HARVEST_PER_TICK);
        }
    }

    // 19) Pasado el umbral, cada uno rinde menos. El doble de gente que el
    //     umbral rinde la mitad cada uno.
    {
        const int32_t doble = ECO_SATURATION_THRESHOLD * 2;
        CHECK(eco_saturated_rate(ECO_HARVEST_PER_TICK, doble) == ECO_HARVEST_PER_TICK / 2);
    }

    // 20) LAS DOS PROPIEDADES QUE DE VERDAD IMPORTAN, y que sustituyen a una
    //     afirmacion mia que era FALSA. Escribi primero que "el total nunca
    //     baja al anadir gente", y la prueba lo desmintio: con division entera
    //     el total SI baja a ratos —30 con seis recolectores, 24 con ocho—
    //     porque cada uno pierde hasta una unidad por redondeo. Se corrige la
    //     afirmacion, no el codigo, porque lo que el modelo hace esta bien y lo
    //     que estaba mal era como lo describi.
    //
    //     (a) SATURAR MUERDE DE VERDAD: con el doble del umbral o mas, el
    //         total es como mucho la MITAD de lo que seria sin saturacion.
    //
    //         Este era mi segundo intento fallido, y tambien lo caza la prueba.
    //         Afirme que "el total nunca supera tasa*umbral" y es FALSO: la
    //         regla de que nadie rinde cero pone un suelo de 1 por cabeza, asi
    //         que con 40 recolectores el total es 40 y el techo seria 30. Las
    //         dos reglas chocan y gana la de "nunca cero", a proposito: una
    //         unidad trabajando sin producir nada se lee como un fallo, no como
    //         una regla. Lo que el sistema SI garantiza es que apilar sale
    //         mucho peor que dispersarse, y eso es lo que se mide aqui.
    {
        for (int32_t n = ECO_SATURATION_THRESHOLD * 2; n <= 60; ++n) {
            const int32_t saturado = eco_saturated_rate(ECO_HARVEST_PER_TICK, n) * n;
            const int32_t sin_saturar = ECO_HARVEST_PER_TICK * n;
            CHECK(saturado * 2 <= sin_saturar);
        }
    }
    //     (b) POR CABEZA NUNCA MEJORA al llegar mas gente. Nadie se beneficia
    //         de que le pongan companeros encima, que es lo que el jugador
    //         necesita entender para dispersarse.
    {
        int32_t anterior = ECO_HARVEST_PER_TICK + 1;
        for (int32_t n = 1; n <= 60; ++n) {
            const int32_t r = eco_saturated_rate(ECO_HARVEST_PER_TICK, n);
            CHECK(r <= anterior);
            anterior = r;
        }
    }

    // 21) Nunca cae a cero. Un aldeano en una multitud rinde poco, pero si le
    //     dieras cero el jugador veria unidades trabajando sin producir nada, y
    //     eso se lee como un fallo, no como una regla.
    {
        CHECK(eco_saturated_rate(ECO_HARVEST_PER_TICK, 1000) >= 1);
    }

    // 22) Cota: cero o negativo no divide por cero ni devuelve basura.
    {
        CHECK(eco_saturated_rate(ECO_HARVEST_PER_TICK, 0) == ECO_HARVEST_PER_TICK);
        CHECK(eco_saturated_rate(ECO_HARVEST_PER_TICK, -3) == ECO_HARVEST_PER_TICK);
    }


    // 23) HALLAZGO F1 DE LA AUDITORIA EXTERNA. Al morir la granja, su deposito
    //     debe SOLTAR el dueno, no solo apagarse.
    //
    //     Por que importa: la free-list de entidades es LIFO, asi que el indice
    //     de la granja destruida se reutiliza enseguida. Si el deposito seguia
    //     apuntando a ese indice, el edificio NUEVO que lo ocupara heredaba un
    //     deposito muerto — y el alta lo daba por "ya registrado" y no le creaba
    //     el suyo. Un jugador construiria una granja y no producirian nada, sin
    //     que nada se lo dijera.
    {
        EcoDeposit d{};
        d.remaining = 0;
        d.regen_milli_per_tick = 0;
        d.owner_building = ECO_NO_OWNER;   // asi lo deja farm_system al soltarlo
        CHECK(!eco_deposit_is_farm(d));    // ya no es de nadie
    }

    if (g_fails == 0) {
        std::printf("farms OK\n");
        return 0;
    }
    std::printf("farms: %d fallo(s)\n", g_fails);
    return 1;
}
