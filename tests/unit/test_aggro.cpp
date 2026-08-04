// Test de aggro/persecución v1 (Sprint 0.3+): dos escuadras que empiezan
// FUERA de rango de arma (1.5 tiles) pero DENTRO del radio de aggro (10
// tiles) deben perseguirse, entrar en combate y llegar a la aniquilación de
// un bando — el estancamiento observado en la demo (supervivientes inertes
// fuera de rango) es exactamente lo que este test previene. Autor: Arquitecto.
//
// ALCANCE DE CONTACTO (Sprint 2026-08-05): el contacto es propiedad del MUNDO,
// no del arma. `range_millitiles` expresa el alcance MAS ALLA del contacto, y
// el suelo es de un tile (MELEE_CONTACT_RAW). Sin el suelo, range_mt=0 hacia
// el filtro de combate exigir d2==0 — el enemigo en la MISMA coordenada raw —
// y seis de las nueve unidades del catalogo estaban asi: dos ejercitos podian
// orbitar a 4,4 tiles cien mil ticks sin tocarse. Este montaje (GameState +
// SPAWN_UNIT/SPAWN_CITIZEN + step) es el mas comodo para medir el suelo.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>

#include "chunsa/game_state.hpp"
#include "chunsa/step.hpp"
#include "chunsa/checksum.hpp"

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

static constexpr uint32_t N_PER_SIDE  = 12;
static constexpr uint32_t TOTAL_TICKS = 2000;

// Escenario: 12 caballería (owner 0) en columna alrededor de x=120 y 12
// artillería (owner 1) alrededor de x=127 — separación de ~7 tiles: fuera de
// range_mt=1500 (1.5 tiles), dentro de AGGRO_RANGE_MT=10000 (10 tiles). Nadie
// recibe MOVE_TO: si se aniquila un bando, fue la persecución del kernel.
static void run_scenario(GameState& g) {
    // Sprint 0.4: camino debug legado explícito (ver test_combat.cpp).
    MatchConfig01A cfg{128u, 2u, 1u, 20u, 20u, 256u, 256u, 11ull, 1u};
    gs_init(g, cfg);

    static RawCommand batch[2 * N_PER_SIDE];

    for (uint32_t t = 0; t < TOTAL_TICKS; ++t) {
        uint32_t n = 0;
        if (t == 0u) {
            for (uint32_t i = 0; i < N_PER_SIDE; ++i) {
                RawCommand& c = batch[n];
                std::memset(&c, 0, sizeof(RawCommand));
                c.target_tick  = 0;
                c.emitter      = 0;
                c.type         = CommandType::SPAWN_UNIT;
                c.sequence     = i + 1u;
                const uint32_t tile_x = 120u + (i % 2u);   // ∈{120,121}
                const uint32_t tile_y = 122u + (i / 2u);   // ∈[122,127]
                c.p.x_raw      = static_cast<int64_t>(tile_x) * 65536 + 32768;
                c.p.y_raw      = static_cast<int64_t>(tile_y) * 65536 + 32768;
                c.p.speed_mtpt = 150;
                c.p.hp         = 100;
                c.p.attack     = 20;
                c.p.range_mt   = 1500;
                c.p.unit_class = 1;  // cavalry (ventaja RPS vs artillery)
                c.p.unit_id    = INVALID_UNIT_ID;  // camino debug (Sprint 0.4)
                ++n;
            }
            for (uint32_t i = 0; i < N_PER_SIDE; ++i) {
                RawCommand& c = batch[n];
                std::memset(&c, 0, sizeof(RawCommand));
                c.target_tick  = 0;
                c.emitter      = 1;
                c.type         = CommandType::SPAWN_UNIT;
                c.sequence     = i + 1u;
                const uint32_t tile_x = 127u + (i % 2u);   // ∈{127,128}
                const uint32_t tile_y = 122u + (i / 2u);   // ∈[122,127]
                c.p.x_raw      = static_cast<int64_t>(tile_x) * 65536 + 32768;
                c.p.y_raw      = static_cast<int64_t>(tile_y) * 65536 + 32768;
                c.p.speed_mtpt = 80;
                c.p.hp         = 100;
                c.p.attack     = 20;
                c.p.range_mt   = 1500;
                c.p.unit_class = 2;  // artillery
                c.p.unit_id    = INVALID_UNIT_ID;  // camino debug (Sprint 0.4)
                ++n;
            }
        }
        step(g, batch, n);
    }
}

// ---------------------------------------------------------------------------
// ALCANCE DE CONTACTO (MELEE_CONTACT_RAW = 1 tile). El contacto es un SUELO,
// no una barra libre: una unidad con range_mt=0 golpea hasta a un tile, y el
// suelo no recorta a quien ya tenia alcance real ni convierte aldeanos en
// soldados.
//
// Las cuatro pruebas comparten un montaje de DOS unidades enemigas separadas
// `dist_raw` en el eje X (misma Y), con el owner 0 (indice bajo) en A. Ya han
// dado UN step al volver (el del spawn), asi que el combate del primer tick ya
// ocurrio; la 3 necesita ticks extra por el vuelo del proyectil.
//
// GameState vive en el heap (unique_ptr): son ~15 MB de arrays fijos y no
// caben en el stack, como ya hace el run_scenario de este mismo archivo.
struct ContactFixture {
    std::unique_ptr<GameState> g;
    uint32_t low = 0;   // owner 0, indice bajo
    uint32_t high = 0;  // owner 1, indice alto
};

static ContactFixture make_contact_pair(int32_t atk_a, int32_t atk_b,
                                        int32_t range_mt, int64_t dist_raw,
                                        bool b_is_citizen = false) {
    ContactFixture f;
    f.g = std::make_unique<GameState>();
    GameState& g = *f.g;
    MatchConfig01A cfg{128u, 2u, 1u, 20u, 20u, 256u, 256u, 11ull, 1u};
    gs_init(g, cfg);

    const int64_t y = static_cast<int64_t>(122) * 65536 + 32768;
    const int64_t xa = static_cast<int64_t>(100) * 65536 + 32768;
    const int64_t xb = xa + dist_raw;

    RawCommand batch[2];

    std::memset(&batch[0], 0, sizeof(RawCommand));
    batch[0].target_tick = 0;
    batch[0].emitter = 0;
    batch[0].type = CommandType::SPAWN_UNIT;
    batch[0].sequence = 1u;
    batch[0].p.x_raw = xa;
    batch[0].p.y_raw = y;
    batch[0].p.speed_mtpt = 50;
    batch[0].p.hp = 100;
    batch[0].p.attack = atk_a;
    batch[0].p.range_mt = range_mt;
    batch[0].p.unit_class = 1;
    batch[0].p.unit_id = INVALID_UNIT_ID;

    std::memset(&batch[1], 0, sizeof(RawCommand));
    batch[1].target_tick = 0;
    batch[1].emitter = 1;
    batch[1].type = b_is_citizen ? CommandType::SPAWN_CITIZEN
                                 : CommandType::SPAWN_UNIT;
    batch[1].sequence = 1u;
    batch[1].p.x_raw = xb;
    batch[1].p.y_raw = y;
    batch[1].p.speed_mtpt = 50;
    if (b_is_citizen) {
        // Camino debug de SPAWN_CITIZEN: hp=20, attack=0, unit_class=3
        // cableados; el payload no aporta stats.
        batch[1].p.hp = 0;
        batch[1].p.attack = 0;
        batch[1].p.range_mt = 0;
        batch[1].p.unit_class = 0;
    } else {
        batch[1].p.hp = 100;
        batch[1].p.attack = atk_b;
        batch[1].p.range_mt = range_mt;
        batch[1].p.unit_class = 1;
    }
    batch[1].p.unit_id = INVALID_UNIT_ID;

    step(g, batch, 2);

    for (uint32_t i = 0; i < g.entities.capacity; ++i) {
        if (!g.entities.alive[i]) continue;
        if (g.owner[i] == 0u) f.low = i;
        else f.high = i;
    }
    return f;
}

// 1) EL FALLO LITERAL. Dos enemigos con range_mt=0 a MEDIA casilla. Tras un
// step, la de indice bajo le ha hecho dano a la alta. Antes de este arreglo
// nunca se tocaban: range_sq==0 exigia d2==0 (la MISMA coordenada raw).
static void test_contact_half_tile_hits() {
    ContactFixture f = make_contact_pair(20, 20, 0, FX_ONE_RAW / 2);
    CHECK(f.low < f.high);
    CHECK(f.g->entities.alive[f.low]);
    CHECK(f.g->entities.alive[f.high]);
    CHECK(f.g->hp[f.high] == 80);  // la baja le hizo 20: ANTES quedaba en 100
    CHECK(f.g->hp[f.low] == 80);   // y la alta le devolvio 20 en el mismo tick

    // Determinismo: corrida fresca identica -> mismo checksum.
    ContactFixture f2 = make_contact_pair(20, 20, 0, FX_ONE_RAW / 2);
    const uint64_t c1 = state_checksum_v1(*f.g);
    const uint64_t c2 = state_checksum_v1(*f2.g);
    CHECK(c1 == c2);
}

// 2) EL TESTIGO. Las mismas a DOS casillas: NO se hacen dano. El contacto es
// un SUELO, no una barra libre.
static void test_contact_two_tiles_no_hit() {
    ContactFixture f = make_contact_pair(20, 20, 0, 2 * FX_ONE_RAW);
    CHECK(f.g->hp[f.low] == 100);
    CHECK(f.g->hp[f.high] == 100);
}

// 3) LA BALLISTA NO SE TOCA. Con range_mt=4000 (4 tiles) sigue golpeando a 3
// tiles. El suelo no puede recortar a quien ya tenia alcance.
//
// CORRECCION del Arquitecto sobre el test del delegado: esperaba que el dano
// tardara unos ticks "porque es de proyectil". No lo es. combat_system aplica
// el dano INMEDIATAMENTE en el mismo tick, en orden ascendente de indice; los
// proyectiles son otro sistema. El test fallaba por esa suposicion, no por el
// arreglo. Se comprueba lo que de verdad importa: a 3 tiles, con alcance 4,
// pega en el primer tick.
static void test_contact_does_not_trim_real_range() {
    ContactFixture f = make_contact_pair(20, 20, 4000, 3 * FX_ONE_RAW);
    CHECK(f.g->hp[f.high] < 100);   // alcanza a 3 tiles, y en el acto
    // Y el testigo del suelo: la MISMA distancia con alcance 0 no toca a nadie,
    // porque 3 tiles pasa de largo del contacto de 1 tile.
    ContactFixture sin_alcance = make_contact_pair(20, 20, 0, 3 * FX_ONE_RAW);
    CHECK(sin_alcance.g->hp[sin_alcance.high] == 100);
}

// 4) EL ALDEANO. Un ciudadano (unit_class==3, SPAWN_CITIZEN) sigue sin atacar
// a media casilla: el suelo de contacto no convierte a los aldeanos en
// soldados. Como BLANCO sigue siendo vulnerable (SPEC-004 §7.1): el soldado
// le pega, el ciudadano no le devuelve ni un punto.
static void test_contact_citizen_still_does_not_attack() {
    ContactFixture f = make_contact_pair(5, 0, 0, FX_ONE_RAW / 2,
                                         /*b_is_citizen=*/true);
    CHECK(f.g->unit_class[f.high] == 3u);       // el alto ES el ciudadano
    CHECK(f.g->hp[f.high] == 15);               // el soldado le hizo 5
    CHECK(f.g->hp[f.low] == 100);               // el ciudadano no ataco
}

// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Sprint 1.50 — EL ATAQUE-MOVIMIENTO ENGANCHA AUNQUE SE ESTE MOVIENDO.
//
// aggro_system exigia que la unidad estuviera QUIETA (pos == tgt) para adquirir
// objetivo. Las 40 unidades de p0 en el banco marchaban en ATTACK_MOVE hacia
// waypoints de su propia base, asi que nunca estaban quietas y el aggro NO
// disparaba jamas — con el enemigo a 4,4 tiles y rango de aggro 10.
//
// Un ataque-movimiento que no ataca mientras se mueve es un MOVE, y entonces la
// orden no significa nada.
//
// El observable de "ha enganchado" es que aggro_system reescribe `tgt` hacia el
// enemigo. Por eso el waypoint se pone al OESTE y el enemigo al ESTE: si engancha,
// tgt cambia de sentido; si no, se queda donde estaba.
// ---------------------------------------------------------------------------
static ContactFixture make_marching_pair(uint8_t order_mode) {
    // 3 tiles de separacion: fuera del contacto (1 tile) y dentro del aggro (10).
    ContactFixture f = make_contact_pair(20, 20, 0, 3 * FX_ONE_RAW);
    GameState& g = *f.g;
    g.order_mode[f.low] = order_mode;
    // Waypoint MUY al oeste: la unidad esta en marcha (pos != tgt) y en sentido
    // contrario al enemigo.
    g.tgt_x[f.low] = static_cast<int64_t>(10) * FX_ONE_RAW;
    g.tgt_y[f.low] = g.pos_y[f.low];
    step(g, nullptr, 0);
    return f;
}

// 1) EL FALLO LITERAL: en ATTACK_MOVE y en marcha, engancha.
static void test_attack_move_acquires_while_marching() {
    ContactFixture f = make_marching_pair(ORDER_MODE_ATTACK_MOVE);
    // El aggro reescribio el destino hacia el enemigo (al este), abandonando el
    // waypoint del oeste. Antes de este arreglo tgt seguia en 10 tiles.
    CHECK(f.g->tgt_x[f.low] > static_cast<int64_t>(10) * FX_ONE_RAW);
}

// 2) EL TESTIGO, y es lo que da valor a la prueba 1. En MOVE y en marcha, NO
//    engancha: quien va a un sitio con MOVE no se para a pelear, y esa es justo
//    la diferencia entre las dos ordenes. Si esta pasara con el arreglo roto,
//    la 1 no probaria nada.
static void test_move_does_not_acquire_while_marching() {
    ContactFixture f = make_marching_pair(ORDER_MODE_MOVE);
    CHECK(f.g->tgt_x[f.low] == static_cast<int64_t>(10) * FX_ONE_RAW);
}

// 3) ATTACK sigue excluido (SPEC-004 §24.4: con ATTACK activo manda el jugador
//    y el aggro NO reasigna aunque pase otro enemigo mas cerca). El arreglo no
//    puede romper esa regla.
static void test_attack_order_still_not_reassigned() {
    ContactFixture f = make_marching_pair(ORDER_MODE_ATTACK);
    CHECK(f.g->tgt_x[f.low] == static_cast<int64_t>(10) * FX_ONE_RAW);
}

// 4) Y la unidad QUIETA sigue adquiriendo como siempre: el arreglo anade un
//    caso, no sustituye el que habia.
static void test_idle_still_acquires() {
    ContactFixture f = make_contact_pair(20, 20, 0, 3 * FX_ONE_RAW);
    GameState& g = *f.g;
    g.order_mode[f.low] = ORDER_MODE_NONE;
    g.tgt_x[f.low] = g.pos_x[f.low];   // quieta
    g.tgt_y[f.low] = g.pos_y[f.low];
    const int64_t antes = g.tgt_x[f.low];
    step(g, nullptr, 0);
    CHECK(g.tgt_x[f.low] != antes);    // se movio hacia el enemigo
}

int main() {
    test_attack_move_acquires_while_marching();
    test_move_does_not_acquire_while_marching();
    test_attack_order_still_not_reassigned();
    test_idle_still_acquires();

    auto* g1 = new GameState();
    run_scenario(*g1);

    CHECK(g1->fatal == FatalReason::NONE);

    uint32_t cav = 0, art = 0;
    for (uint32_t i = 0; i < g1->entities.capacity; ++i) {
        if (!g1->entities.alive[i]) continue;
        if (g1->unit_class[i] == 1u) ++cav;
        else if (g1->unit_class[i] == 2u) ++art;
    }

    // (2) La persecución arrancó el combate y lo llevó a resolución: la
    // artillería (en desventaja RPS y de velocidad) fue ANIQUILADA. Sin
    // aggro_system este escenario termina 12 vs 12 intactos (fuera de rango).
    CHECK(art == 0);

    // (3) La caballería ganadora conserva supervivientes.
    CHECK(cav > 0);

    const uint64_t checksum1 = state_checksum_v1(*g1);
    delete g1;

    // (4) Determinismo: segunda corrida fresca idéntica → mismo checksum final.
    auto* g2 = new GameState();
    run_scenario(*g2);
    const uint64_t checksum2 = state_checksum_v1(*g2);
    delete g2;

    CHECK(checksum1 == checksum2);

    std::printf("aggro: cav=%u art=%u checksum=%llx\n", cav, art,
                (unsigned long long)checksum1);

    // ALCANCE DE CONTACTO: las cuatro pruebas del suelo.
    test_contact_half_tile_hits();
    test_contact_two_tiles_no_hit();
    test_contact_does_not_trim_real_range();
    test_contact_citizen_still_does_not_attack();

    if (g_fails == 0) { std::printf("aggro: OK\n"); return 0; }
    std::printf("aggro: %d fallos\n", g_fails);
    return 1;
}
