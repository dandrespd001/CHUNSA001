// Bosques como zonas (Sprint 1.45).
//
// POR QUÉ EXISTE. El banco de partida larga del 1.40 dejó la comida resuelta y
// movió el muro a la MADERA: el bosque plantado es de época [7,15] y no se
// llega a la 7 sin madera. La vía estructural es que el mapa vuelva a tener
// madera como ÁREAS: un bosque es un depósito con centro, radio y madera, y el
// radio ENCOGE al talarlo — se tala por el borde, y el borde retrocede.
//
// La regla que manda en TODO el fichero es la compatibilidad: con
// `radius_raw == 0` (el valor por defecto) cada pieza debe comportarse
// EXACTAMENTE como antes. Los 22 yacimientos del mapa y las granjas son
// depósitos puntuales y no pueden cambiar. La prueba 7 es la que lo vigila.

#include <cstdint>
#include <cstdio>

#include "chunsa/economy.hpp"

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

// Replica literal de la logica ANTERIOR a Sprint 1.45: orden por (d_sq, indice).
// Sirve de testigo en la prueba de no-regresion (7).
static uint32_t legacy_nearest(const EcoDeposit* deposits, uint32_t n_deposits,
                               int64_t x_raw, int64_t y_raw,
                               uint8_t preferred_resource_idx,
                               uint64_t eligible_mask) noexcept {
    FatalReason f = FatalReason::NONE;
    const Vec2Fx here{Fx{x_raw}, Fx{y_raw}};
    if (preferred_resource_idx != ECO_ANY_RESOURCE) {
        uint32_t best = ECO_NO_DEPOSIT;
        uint64_t best_d_sq = UINT64_MAX;
        for (uint32_t i = 0; i < n_deposits; ++i) {
            if (i >= ECO_MAX_DEPOSITS
                || (eligible_mask & (uint64_t{1} << i)) == 0u) continue;
            if (deposits[i].remaining <= 0) continue;
            if (deposits[i].resource_idx != preferred_resource_idx) continue;
            const Vec2Fx there{Fx{deposits[i].x_raw}, Fx{deposits[i].y_raw}};
            const uint64_t d_sq = dist_sq_raw(here, there, f);
            if (d_sq < best_d_sq) { best_d_sq = d_sq; best = i; }
        }
        if (best != ECO_NO_DEPOSIT) return best;
    }
    uint32_t best = ECO_NO_DEPOSIT;
    uint64_t best_d_sq = UINT64_MAX;
    for (uint32_t i = 0; i < n_deposits; ++i) {
        if (i >= ECO_MAX_DEPOSITS
            || (eligible_mask & (uint64_t{1} << i)) == 0u) continue;
        if (deposits[i].remaining <= 0) continue;
        const Vec2Fx there{Fx{deposits[i].x_raw}, Fx{deposits[i].y_raw}};
        const uint64_t d_sq = dist_sq_raw(here, there, f);
        if (d_sq < best_d_sq) { best_d_sq = d_sq; best = i; }
    }
    return best;
}

int main() {
    // 1) COMPATIBILIDAD: radius_raw == 0 es un depósito PUNTUAL y su zona es 0.
    //    Da igual cuanta madera tenga: sin radio no hay zona que encoger.
    {
        EcoDeposit d{};
        d.radius_raw = 0;
        d.initial_amount = 1000;
        d.remaining = 500;
        CHECK(eco_zone_radius(d) == 0);
    }

    // 2) A PLENA CARGA (o por encima) el radio es el nominal EXACTO. Una
    //    granja regenerada no debe crecer más allá de su radio, aunque su
    //    remaining pase del inicial.
    {
        EcoDeposit d{};
        d.radius_raw = 4 * FX_ONE_RAW;
        d.initial_amount = 2000;
        d.remaining = 2000;
        CHECK(eco_zone_radius(d) == d.radius_raw);
        d.remaining = 3000;                      // por encima del inicial
        CHECK(eco_zone_radius(d) == d.radius_raw);
    }

    // 3) AGOTADO: radio 0. Un bosque talado del todo no deja zona, igual que
    //    un yacimiento vacío no deja mena.
    {
        EcoDeposit d{};
        d.radius_raw = 4 * FX_ONE_RAW;
        d.initial_amount = 2000;
        d.remaining = 0;
        CHECK(eco_zone_radius(d) == 0);
        d.remaining = -5;
        CHECK(eco_zone_radius(d) == 0);
    }

    // 4) LA LEY ES CUADRÁTICA, no lineal: talado a la mitad, el radio está
    //    entre el 69 % y el 71 % del inicial. Ese es sqrt(0.5) = 0.707, y es
    //    la prueba que separa «el bosque aguanta grande y se desploma al
    //    final» de «un radio lineal que parece derretirse desde el primer
    //    hachazo». Con R0 = 8 tiles y mitad de madera: r = isqrt(R0^2 / 2).
    {
        EcoDeposit d{};
        d.radius_raw = 8 * FX_ONE_RAW;
        d.initial_amount = 10000;
        d.remaining = 5000;
        const int64_t r = eco_zone_radius(d);
        CHECK(100 * r >= 69 * d.radius_raw);
        CHECK(100 * r <= 71 * d.radius_raw);
    }

    // 5) SIN DESBORDE CON VALORES EXTREMOS. radius_raw = 16 tiles
    //    (= 1 048 576 raw) e initial_amount = 2 000 000 000. El producto
    //    intermedio R0^2 * remaining alcanza ~2.2e21 (71 bits) — desbordaría
    //    uint64_t de largo. La implementación usa 128 bits; la comprobación es
    //    que el resultado sea MONÓTONO decreciente y jamás absurdo (> R0).
    {
        EcoDeposit d{};
        d.radius_raw = 16 * FX_ONE_RAW;
        d.initial_amount = 2'000'000'000;
        int64_t prev = d.radius_raw;
        for (int64_t rem = d.initial_amount; rem > 0; rem -= 10'000'000) {
            d.remaining = static_cast<int32_t>(rem);
            const int64_t r = eco_zone_radius(d);
            CHECK(r >= 0);
            CHECK(r <= d.radius_raw);   // nunca crece por encima del nominal
            CHECK(r <= prev);           // monótono decreciente al talar
            prev = r;
        }
        // Punto medio con el extremo: ~sqrt(0.5) * R0, un número sano.
        d.remaining = 1'000'000'000;
        const int64_t r_half = eco_zone_radius(d);
        CHECK(100 * r_half >= 69 * d.radius_raw);
        CHECK(100 * r_half <= 71 * d.radius_raw);
    }

    // 5-bis) UN RADIO ABSURDO NO PUEDE DESBORDAR. Anadida por el Arquitecto:
    //    la prueba 5 usa 16 tiles, que es sensato, pero `radius_raw` es int64 y
    //    nadie lo valida todavia (el cargador de mapas aun no existe). Con un
    //    radio de un cuarto del maximo de int64, `r0 * rem` desbordaria en
    //    int64 con signo — comportamiento INDEFINIDO, no un numero raro. El
    //    clamp a ECO_MAX_ZONE_RADIUS_RAW lo impide, y esto lo demuestra: bajo
    //    UBSan esta prueba es la que grita si el clamp desaparece.
    {
        EcoDeposit d{};
        d.radius_raw = INT64_MAX / 4;
        d.initial_amount = 2'000'000'000;
        d.remaining = 1'000'000'000;
        const int64_t r = eco_zone_radius(d);
        CHECK(r >= 0);
        CHECK(r <= ECO_MAX_ZONE_RADIUS_RAW);
        // Sigue cumpliendo la ley de la raiz sobre el radio YA acotado.
        CHECK(100 * r >= 69 * ECO_MAX_ZONE_RADIUS_RAW);
        CHECK(100 * r <= 71 * ECO_MAX_ZONE_RADIUS_RAW);
    }

    FatalReason fatal = FatalReason::NONE;

    // 6) SE LLEGA AL BORDE, no al centro. Un bosque de radio 4 tiles se puede
    //    talar desde el borde: el aldeano que está a 3 tiles del CENTRO (es
    //    decir, dentro del radio de la zona) pasa a HARVEST de inmediato. Con
    //    el mismo centro y un depósito PUNTUAL, a 3 tiles del centro NO llega
    //    (llega a 1 tile) y camina.
    {
        EcoDeposit bosque{};
        bosque.x_raw = 0; bosque.y_raw = 0;
        bosque.resource_idx = 0;
        bosque.remaining = 10000;
        bosque.radius_raw = 4 * FX_ONE_RAW;    // zona de 4 tiles
        bosque.initial_amount = 10000;
        EcoCitizenIn in{};
        in.pos_x = 3 * FX_ONE_RAW; in.pos_y = 0;
        in.state = EcoState::SEEK; in.assigned_deposit = 0;
        in.speed_mtpt = 1000;
        const EcoCitizenOut out_bosque =
            eco_step_citizen(in, &bosque, 1, ECO_ALL_DEPOSITS_MASK, 0, 0, fatal);
        CHECK(out_bosque.state == EcoState::HARVEST);

        EcoDeposit puntual{};
        puntual.x_raw = 0; puntual.y_raw = 0;
        puntual.resource_idx = 0;
        puntual.remaining = 10000;
        puntual.radius_raw = 0;                // punto de toda la vida
        puntual.initial_amount = 0;
        const EcoCitizenOut out_puntual =
            eco_step_citizen(in, &puntual, 1, ECO_ALL_DEPOSITS_MASK, 0, 0, fatal);
        CHECK(out_puntual.state == EcoState::SEEK);   // aún no llega
        CHECK(out_puntual.pos_x != in.pos_x || out_puntual.pos_y != in.pos_y);
    }

    // 7) NO-REGRESIÓN, la prueba más importante del encargo: con TODOS los
    //    radios a 0, eco_find_nearest_deposit debe devolver el MISMO índice
    //    que la lógica anterior (d_sq, índice) en un conjunto construido a
    //    mano que incluye empates de verdad:
    //      · d0 y d1 en la MISMA posición  -> empate exacto de d_sq
    //      · d0/d1 y d3 con isqrt(d_sq) IGUAL (92681) pero d_sq distinta
    //        -> el empate que introduce la truncación de isqrt lo rompe la
    //           d_sq exacta, como antes
    //      · d4 agotado y una máscara que excluye d0, para cubrir las dos
    //        pasadas (recurso preferido y cualquiera).
    {
        EcoDeposit dep[8]{};
        auto init_dep = [&](uint32_t i, int64_t x, int64_t y, uint8_t res, int32_t rem) {
            dep[i].x_raw = x; dep[i].y_raw = y;
            dep[i].resource_idx = res;
            dep[i].remaining = rem;
            dep[i].radius_raw = 0;
            dep[i].initial_amount = 0;
        };
        init_dep(0, 1 * FX_ONE_RAW, 1 * FX_ONE_RAW, 0, 100);        // d_sq = 2 tile^2, isqrt 92681
        init_dep(1, 1 * FX_ONE_RAW, 1 * FX_ONE_RAW, 0, 100);        // empate exacto con d0
        init_dep(2, 2 * FX_ONE_RAW, 0,               1, 100);
        init_dep(3, 92681,          0,               0, 100);        // d_sq = 92681^2, isqrt 92681 (¡el MISMO! pero d_sq distinta)
        init_dep(4, 3 * FX_ONE_RAW, 2 * FX_ONE_RAW,  0, 0);          // agotado: nunca candidato
        init_dep(5, 4 * FX_ONE_RAW, 0,               2, 40);
        init_dep(6, 1 * FX_ONE_RAW, 0,               1, 100);
        init_dep(7, 8 * FX_ONE_RAW, 8 * FX_ONE_RAW,  0, 200);

        struct Query { int64_t x, y; uint8_t pref; uint64_t mask; };
        const Query queries[] = {
            { 0, 0, ECO_ANY_RESOURCE, ECO_ALL_DEPOSITS_MASK },
            { 0, 0, 0, ECO_ALL_DEPOSITS_MASK },                     // empate isqrt 92681, lo rompe d_sq
            { 0, 0, 1, ECO_ALL_DEPOSITS_MASK },                     // pasada de recurso 1
            { 1 * FX_ONE_RAW, 1 * FX_ONE_RAW, ECO_ANY_RESOURCE, ECO_ALL_DEPOSITS_MASK },
            { 1 * FX_ONE_RAW, 1 * FX_ONE_RAW, 0, ECO_ALL_DEPOSITS_MASK }, // empate exacto -> índice bajo
            { 2 * FX_ONE_RAW, 0, 0, ECO_ALL_DEPOSITS_MASK },        // sobre d2
            { 5 * FX_ONE_RAW, 3 * FX_ONE_RAW, ECO_ANY_RESOURCE, ECO_ALL_DEPOSITS_MASK },
            { 0, 0, 0, ~(uint64_t{1} << 0) },                       // d0 fuera de la máscara -> gana d1/d3, nunca d0
            { 10 * FX_ONE_RAW, 10 * FX_ONE_RAW, ECO_ANY_RESOURCE, ECO_ALL_DEPOSITS_MASK },
            { 10 * FX_ONE_RAW, 10 * FX_ONE_RAW, 2, ECO_ALL_DEPOSITS_MASK }, // solo d5 es de recurso 2
        };
        for (const Query& q : queries) {
            const uint32_t legacy = legacy_nearest(dep, 8, q.x, q.y, q.pref, q.mask);
            const uint32_t actual = eco_find_nearest_deposit(dep, 8, q.x, q.y,
                                                             q.pref, q.mask, fatal);
            CHECK(actual == legacy);
        }
    }

    if (g_fails == 0) {
        std::printf("bosques OK\n");
        return 0;
    }
    std::printf("bosques: %d fallo(s)\n", g_fails);
    return 1;
}
