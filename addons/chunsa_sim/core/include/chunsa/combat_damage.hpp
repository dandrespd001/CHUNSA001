#pragma once

#include <cstdint>

namespace chunsa {

// Sprint 1.18 (SPEC-004 Parte VI). Tres tipos de daño, tomando la distinción de
// 0 A.D. porque separa el asedio de lo demás y nosotros vamos a tener quince
// edades de artillería.
enum class DamageTypeV1 : uint8_t {
    Cut = 0,      // cuerpo a cuerpo
    Pierce = 1,   // proyectiles
    Impact = 2,   // asedio
};
inline constexpr uint32_t DAMAGE_TYPE_COUNT = 3;

// SPEC-004 §27.1:
//
//     base  = attack - armor[tipo del arma]          (suelo en 1)
//     bono  = attack * bonus_bp / 10000              (proporcional, en bp)
//     daño  = max(1, base + bono)
//
// Tres decisiones, y las tres importan:
//
// 1. RESTA PLANA, como AoE2, no resistencia porcentual como 0 A.D. Somos
//    enteros y deterministas sin float: una resta es exacta, un porcentaje
//    obliga a una division y a fijar para siempre una regla de redondeo que
//    hay que respetar en cada plataforma.
//
// 2. LA ARMADURA NO FRENA EL BONO. Es lo que hace que un contador sea un
//    contador y no una mejora marginal; en AoE2 funciona igual.
//
// 3. NUNCA POR DEBAJO DE 1. Una unidad jamas es invulnerable a otra, solo muy
//    resistente. Vale en AoE2 y en 0 A.D. por igual.
//
// La division entera trunca hacia cero (C++ desde C++11 para operandos con
// signo): queda FIJADA aqui y probada, porque dos plataformas que redondearan
// distinto romperian el determinismo sin que nadie lo viera.
inline int32_t compute_damage(int32_t attack,
                              int32_t armor_of_type,
                              int32_t bonus_bp) noexcept {
    int32_t base = attack - armor_of_type;
    if (base < 1) base = 1;
    const int64_t bonus =
            (static_cast<int64_t>(attack) * static_cast<int64_t>(bonus_bp)) / 10000;
    int64_t total = static_cast<int64_t>(base) + bonus;
    if (total < 1) total = 1;
    return static_cast<int32_t>(total);
}

// Sprint 1.18 fase 4 (SPEC-004 §28): efectos de tecnologia sobre estadisticas,
// al estilo de la herreria de AoE2.
enum class StatEffectV1 : uint8_t {
    Attack = 0,
    ArmorCut = 1,
    ArmorPierce = 2,
    ArmorImpact = 3,
};
inline constexpr uint32_t TECH_EFFECTS_MAX = 4;

struct TechEffectV1 {
    StatEffectV1 stat;
    int32_t      amount;      // suma PLANA al valor efectivo
    uint8_t      class_mask;  // un bit por unit_class (0..5); 0 = a nadie
};

// Suma los efectos que coinciden EN ESTADISTICA Y EN CLASE. Se acumulan: es lo
// que permite encadenar mejoras de herreria sin que una pise a la otra.
//
// Se aplica al VALOR EFECTIVO en el momento del calculo; la definicion del
// catalogo NO se toca. Mutarla haria que la partida dependiera del orden de
// carga y del orden en que cada jugador investiga, y el catalogo es compartido
// entre jugadores: la mejora de uno se le aplicaria al otro.
inline int32_t tech_stat_bonus(const TechEffectV1* effects,
                               uint32_t count,
                               StatEffectV1 which,
                               uint8_t unit_class) noexcept {
    if (effects == nullptr || unit_class >= 8u) return 0;
    const uint8_t bit = static_cast<uint8_t>(1u << unit_class);
    int64_t total = 0;
    for (uint32_t k = 0; k < count; ++k) {
        if (effects[k].stat != which) continue;
        if ((effects[k].class_mask & bit) == 0u) continue;
        total += effects[k].amount;
    }
    return static_cast<int32_t>(total);
}

}  // namespace chunsa
