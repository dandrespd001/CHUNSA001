// Sprint 1.18 — fórmula de daño por tipo (SPEC-004 Parte VI §27.1).
//
// La fórmula es una función pura de tres enteros, así que se prueba sola, sin
// montar una partida. Esa es exactamente la parte que no puede equivocarse: el
// resto del combate ya tiene sus pruebas.
//
// Modelo adoptado: resta PLANA de armadura (AoE2), NO resistencia porcentual
// (0 A.D.). El motivo es del kernel: somos enteros y deterministas sin float.
// Una resta es exacta; un porcentaje obliga a una división y a fijar para
// siempre una regla de redondeo que hay que respetar en cada plataforma.
//
// DESVIACIÓN sobre AoE2, deliberada: allí el bono contra clase es daño PLANO
// ("+8 vs caballería"). El nuestro se queda PROPORCIONAL, en puntos básicos,
// porque el esquema y los datos ya lo expresan así y porque a lo largo de
// quince edades un bono plano se vuelve irrelevante mientras que uno
// proporcional sigue significando lo mismo.

#include <cstdint>
#include <cstdio>

#include "chunsa/combat_damage.hpp"

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

int main() {
    // Sin armadura ni bono: el ataque pelado.
    CHECK(compute_damage(20, 0, 0) == 20);

    // La armadura RESTA, y solo del ataque base.
    CHECK(compute_damage(20, 5, 0) == 15);
    CHECK(compute_damage(20, 20, 0) == 1);

    // Nunca por debajo de 1: una unidad jamás es invulnerable a otra, solo
    // muy resistente. Vale en AoE2 y en 0 A.D. por igual.
    CHECK(compute_damage(10, 999, 0) == 1);
    CHECK(compute_damage(1, 1, 0) == 1);

    // El bono es proporcional al ataque BASE, en puntos básicos.
    CHECK(compute_damage(20, 0, 5000) == 30);    // +50 %
    CHECK(compute_damage(20, 0, 10000) == 40);   // +100 %
    CHECK(compute_damage(20, 0, -5000) == 10);   // penalización

    // LO QUE IMPORTA: la armadura NO frena el bono. Es lo que hace que un
    // contador sea un contador y no una mejora marginal. Con ataque 20,
    // armadura 15 y +100 %: base 20-15=5, bono 20, total 25.
    CHECK(compute_damage(20, 15, 10000) == 25);

    // Y con la armadura por encima del ataque, el bono sigue entrando: la
    // base cae al suelo de 1 y el bono se suma encima.
    CHECK(compute_damage(20, 100, 10000) == 21);

    // División entera hacia cero, fijada aquí para siempre: 20 * 3333 / 10000
    // = 6 (6,666 truncado). Sin esta regla escrita, dos plataformas podrían
    // discrepar y el determinismo se rompe sin que nadie lo vea.
    CHECK(compute_damage(20, 0, 3333) == 26);
    CHECK(compute_damage(20, 0, -3333) == 14);

    // LOS CONTADORES PORTADOS de la tabla rps_mult_bp que este sprint retira.
    // Eran multiplicadores cableados en el kernel; ahora son bonus_vs_bp en los
    // datos de cada unidad, y deben seguir significando LO MISMO:
    //   caballería vs artillería  x1.3  ->  +3000 bp
    //   artillería vs caballería  x0.8  ->  -2000 bp
    // Con ataque 10 y sin armadura: la caballería pega 13 y la artillería 8.
    // Esa asimetría es la que hacía ganar a la caballería, y sigue en pie.
    CHECK(compute_damage(10, 0, 3000) == 13);
    CHECK(compute_damage(10, 0, -2000) == 8);
    CHECK(compute_damage(10, 0, 3000) > compute_damage(10, 0, -2000));

    // Ataque cero: sigue haciendo el mínimo. Una unidad sin ataque no debería
    // atacar, pero si el sistema la deja, no se cuelga en 0.
    CHECK(compute_damage(0, 0, 0) == 1);

    if (g_fails == 0) {
        std::printf("combat_damage OK\n");
        return 0;
    }
    std::printf("combat_damage: %d fallo(s)\n", g_fails);
    return 1;
}
