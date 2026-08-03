#pragma once
#include <cstdint>

// Mercado: trueque con precio que se mueve (Sprint 1.33, SPEC-010).
//
// POR QUÉ HACE FALTA, y por qué más que en AoE2. El catálogo tiene 36 recursos.
// Quedarse sin uno concreto —el estaño para el bronce, la caliza para el
// hierro— es MÁS probable que en un juego de cuatro recursos, no menos. Sin
// mercado eso es un callejón sin salida: la partida no se pierde, se atasca.
//
// EL MODELO. Se vende un lote a cambio de oro y se compra con oro, como en
// AoE2. El oro no es una abstracción inventada para esto: `chunsa:gold` existe
// en el catálogo desde la época 3.
//
// TRES REGLAS QUE LO HACEN HONESTO:
//
//   1. El precio SE MUEVE con cada operación: vender baja, comprar sube. Un
//      jugador que vuelca cien lotes de piedra descubre que la piedra ya no
//      vale nada, que es exactamente lo que pasaría.
//   2. Hay HORQUILLA entre comprar y vender. Sin ella, comprar y vender
//      seguido devolvería lo mismo y el mercado sería un almacén gratis.
//   3. NUNCA se generan recursos de la nada. Ir y volver SIEMPRE pierde. Es la
//      única propiedad no negociable y tiene prueba propia.
//
// Todo en aritmética ENTERA. La coma flotante está prohibida en el kernel.

namespace chunsa {

// Un lote son 100 unidades, como en AoE2. Comerciar de una en una convertiría
// el mercado en una calculadora en vez de una decisión.
inline constexpr int32_t MARKET_LOT = 100;

// Precio de partida en puntos básicos: 10000 = un lote por 100 de oro.
inline constexpr int32_t MARKET_BASE_BP = 10000;

// Cuánto se mueve el precio por operación. 300 bp = 3 %. Con esto, diez ventas
// seguidas del mismo recurso bajan su precio a menos de tres cuartos: el
// mercado castiga la monotonía sin volverse inútil de golpe.
inline constexpr int32_t MARKET_STEP_BP = 300;

// Suelo y techo. El suelo impide que un recurso valga cero —lo que congelaría
// el mercado para siempre— y el techo impide que la escasez lo vuelva una
// máquina de oro.
inline constexpr int32_t MARKET_MIN_BP = 2000;    // 20 % del precio base
inline constexpr int32_t MARKET_MAX_BP = 40000;   // 400 %

// La horquilla: comprar cuesta un 30 % más que lo que pagan por vender. Es
// alta a propósito. El mercado es una SALIDA DE EMERGENCIA para desatascar una
// partida, no una estrategia económica que compita con recolectar.
inline constexpr int32_t MARKET_SPREAD_BP = 3000;

inline constexpr int32_t clamp_price_bp(int64_t v) noexcept {
    if (v < MARKET_MIN_BP) return MARKET_MIN_BP;
    if (v > MARKET_MAX_BP) return MARKET_MAX_BP;
    return static_cast<int32_t>(v);
}

// Oro que se recibe al VENDER un lote al precio dado.
inline constexpr int32_t market_sell_gold(int32_t price_bp) noexcept {
    return static_cast<int32_t>(
        (static_cast<int64_t>(MARKET_LOT) * price_bp) / MARKET_BASE_BP);
}

// Oro que cuesta COMPRAR un lote. Siempre más que lo que pagan por venderlo:
// ahí está la horquilla, y es lo que impide el dinero infinito.
inline constexpr int32_t market_buy_gold(int32_t price_bp) noexcept {
    const int64_t base = static_cast<int64_t>(MARKET_LOT) * price_bp;
    const int64_t con_horquilla =
        (base * (MARKET_BASE_BP + MARKET_SPREAD_BP)) / MARKET_BASE_BP;
    // Se redondea HACIA ARRIBA: si la división truncara a favor del jugador,
    // con precios bajos comprar podría salir igual o más barato que vender, y
    // ahí se colaría el bucle infinito.
    const int64_t oro = (con_horquilla + MARKET_BASE_BP - 1) / MARKET_BASE_BP;
    return static_cast<int32_t>(oro);
}

// Precio tras VENDER: baja. Tras COMPRAR: sube. Acotado en ambos extremos.
inline constexpr int32_t market_price_after_sell(int32_t price_bp) noexcept {
    return clamp_price_bp(static_cast<int64_t>(price_bp) - MARKET_STEP_BP);
}
inline constexpr int32_t market_price_after_buy(int32_t price_bp) noexcept {
    return clamp_price_bp(static_cast<int64_t>(price_bp) + MARKET_STEP_BP);
}

}  // namespace chunsa
