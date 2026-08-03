// Mercado: trueque con precio móvil (Sprint 1.33).
//
// La prueba que importa es la 5. Todas las demás describen el mercado; ésa lo
// impide degenerar. Un mercado del que se puede sacar recurso de la nada no es
// un mercado mal ajustado: es un juego roto, y el jugador que lo descubra deja
// de jugar a lo demás.

#include <cstdint>
#include <cstdio>

#include "chunsa/market.hpp"

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

int main() {
    // 1) Al precio base, un lote de 100 se vende por 100 de oro.
    {
        CHECK(market_sell_gold(MARKET_BASE_BP) == MARKET_LOT);
    }

    // 2) Comprar SIEMPRE cuesta más que lo que dan por vender, a cualquier
    //    precio. Es la horquilla, y sin ella el mercado sería un almacén
    //    gratuito donde guardar excedentes sin coste.
    {
        for (int32_t p = MARKET_MIN_BP; p <= MARKET_MAX_BP; p += 137) {
            CHECK(market_buy_gold(p) > market_sell_gold(p));
        }
    }

    // 3) Vender BAJA el precio y comprar lo SUBE. Volcar cien lotes de piedra
    //    debe enseñarle al jugador que la piedra ya no vale nada.
    {
        const int32_t p = MARKET_BASE_BP;
        CHECK(market_price_after_sell(p) < p);
        CHECK(market_price_after_buy(p) > p);
    }

    // 4) El precio no se escapa por ningún extremo. El suelo evita que un
    //    recurso valga cero —lo que congelaría su mercado para siempre— y el
    //    techo evita que la escasez lo convierta en una máquina de oro.
    {
        int32_t p = MARKET_BASE_BP;
        for (int k = 0; k < 500; ++k) p = market_price_after_sell(p);
        CHECK(p == MARKET_MIN_BP);
        for (int k = 0; k < 500; ++k) p = market_price_after_buy(p);
        CHECK(p == MARKET_MAX_BP);
    }

    // 5) LA PROPIEDAD NO NEGOCIABLE: ir y volver SIEMPRE pierde.
    //
    //    Se vende un lote y se recompra inmediatamente, a TODOS los precios
    //    posibles. El oro recibido nunca alcanza para recuperar el lote. Si
    //    esta prueba falla algún día, hay una forma de fabricar recursos de la
    //    nada y el resto del juego deja de importar.
    //
    //    Ojo al detalle: la recompra se hace al precio YA MOVIDO por la venta,
    //    que es lo que pasaría de verdad. Aun así pierde, porque la horquilla
    //    es mayor que el movimiento del precio.
    {
        for (int32_t p = MARKET_MIN_BP; p <= MARKET_MAX_BP; p += 17) {
            const int32_t recibido = market_sell_gold(p);
            const int32_t precio_tras_vender = market_price_after_sell(p);
            const int32_t coste_recompra = market_buy_gold(precio_tras_vender);
            CHECK(recibido < coste_recompra);
        }
    }

    // 6) Y tampoco se gana dando la vuelta al ciclo: comprar primero y vender
    //    después. Un jugador listo probaría el bucle en los dos sentidos.
    {
        for (int32_t p = MARKET_MIN_BP; p <= MARKET_MAX_BP; p += 17) {
            const int32_t pagado = market_buy_gold(p);
            const int32_t precio_tras_comprar = market_price_after_buy(p);
            const int32_t recuperado = market_sell_gold(precio_tras_comprar);
            CHECK(recuperado < pagado);
        }
    }

    // 7) EN EL SUELO, que es donde más apretaría el redondeo. Con el precio
    //    mínimo la venta da poco y la división entera podría truncar a favor
    //    del jugador; por eso la compra redondea HACIA ARRIBA.
    {
        const int32_t p = MARKET_MIN_BP;
        CHECK(market_sell_gold(p) < market_buy_gold(p));
        CHECK(market_price_after_sell(p) == MARKET_MIN_BP);  // ya no baja más
        CHECK(market_sell_gold(p) < market_buy_gold(market_price_after_sell(p)));
    }

    // 8) Determinismo: la misma entrada da siempre la misma salida. Es trivial
    //    porque todo es `constexpr` e entero, y se comprueba precisamente por
    //    eso — para que nadie meta un float aquí sin que salte algo.
    {
        for (int32_t p = MARKET_MIN_BP; p <= MARKET_MAX_BP; p += 999) {
            CHECK(market_sell_gold(p) == market_sell_gold(p));
            CHECK(market_buy_gold(p) == market_buy_gold(p));
        }
    }

    if (g_fails == 0) {
        std::printf("market OK\n");
        return 0;
    }
    std::printf("market: %d fallo(s)\n", g_fails);
    return 1;
}
