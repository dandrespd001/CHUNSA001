// Test del "pánico permanente" (Arquitecto 2026-08-04,
// docs/DISENO_DESEMPATE_COMBATE_2026-08-04.md): la ZONA MUERTA de la moral y
// el ACORRALADO que se planta y pelea. Cubre los cinco casos mínimos del plan:
//   1. La zona muerta (el fallo literal): con enemigos cerca y números parejos,
//      moral 26 SUBE al llamar a step. Antes se quedaba en 26 para siempre.
//   2. En desventaja clara (enemigos > aliados + 1) la moral SIGUE BAJANDO:
//      el arreglo no convierte el pánico en imposible.
//   3. Un pánico a 20 con números parejos se rehace en un número acotado de
//      ticks y fleeing vuelve a 0.
//   4. Acorralada: unidad huyendo pegada a la esquina del mundo (posición
//      0,0) con un enemigo al lado. Tras un step, fleeing==0 y morale >=
//      MORALE_RALLY. Sin el arreglo B esa unidad huye contra el clamp para
//      siempre.
//   5. No acorralada: la misma unidad con sitio de sobra sigue huyendo
//      (fleeing==1). Es el testigo: si esta prueba también pasara con B roto,
//      la 4 no probaría nada.
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "chunsa/game_state.hpp"
#include "chunsa/step.hpp"
#include "chunsa/checksum.hpp"

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

// Estado limpio con el camino debug legado explícito (mismo patrón que
// test_aggro.cpp / test_morale.cpp, Sprint 0.4).
static GameState* make_state() {
    MatchConfig01A cfg{128u, 2u, 1u, 20u, 20u, 256u, 256u, 11ull, 1u};
    auto* g = new GameState();
    gs_init(*g, cfg);
    return g;
}

static void fill_spawn(RawCommand& c, uint32_t emitter, uint32_t seq,
                       int64_t x_raw, int64_t y_raw,
                       uint32_t hp, uint32_t attack, uint32_t range_mt,
                       uint32_t unit_class, uint32_t speed_mtpt) {
    std::memset(&c, 0, sizeof(RawCommand));
    c.target_tick  = 0;
    // static_cast explicito: `emitter` es uint16_t en RawCommand y el parametro
    // llega como uint32_t. MSVC avisa (C4244) donde gcc y clang callan, y con
    // -Werror eso no rompe una prueba: rompe la CI entera de Windows. Ya nos
    // costo una tarde en el sprint 1.43.
    c.emitter      = static_cast<uint16_t>(emitter);
    c.type         = CommandType::SPAWN_UNIT;
    c.sequence     = seq;
    c.p.x_raw      = x_raw;
    c.p.y_raw      = y_raw;
    c.p.hp         = static_cast<int32_t>(hp);
    c.p.attack     = static_cast<int32_t>(attack);
    c.p.range_mt   = static_cast<int32_t>(range_mt);
    c.p.unit_class = static_cast<uint8_t>(unit_class);
    c.p.speed_mtpt = static_cast<int32_t>(speed_mtpt);
    c.p.unit_id    = INVALID_UNIT_ID;  // camino debug (Sprint 0.4)
}

// Centro de un tile en raw Q47.16.
static int64_t tile_cx(uint32_t tx) {
    return static_cast<int64_t>(tx) * 65536 + 32768;
}

// ---------------------------------------------------------------
// 1) La zona muerta: 2 aliados + 2 enemigos pegados (números parejos),
//    moral 26 → debe SUBIR con un step. Antes se quedaba en 26 para siempre.
// ---------------------------------------------------------------
static void test_zone_dead_recovers() {
    auto* g = make_state();
    RawCommand batch[4];
    uint32_t n = 0;
    fill_spawn(batch[n++], 0u, 1u, tile_cx(20), tile_cx(20), 100, 0, 0, 0, 200);
    fill_spawn(batch[n++], 0u, 2u, tile_cx(20), tile_cx(21), 100, 0, 0, 0, 200);
    fill_spawn(batch[n++], 1u, 1u, tile_cx(21), tile_cx(20), 100, 0, 0, 0, 200);
    fill_spawn(batch[n++], 1u, 2u, tile_cx(21), tile_cx(21), 100, 0, 0, 0, 200);
    step(*g, batch, n);  // tick 0: spawn; el spatial hash se reconstruye al final
    g->morale[0] = 26;   // la firma del fallo: congelada en la zona muerta
    g->fleeing[0] = 0;
    const int32_t before = g->morale[0];
    step(*g, batch, 0);  // tick 1: números parejos con enemigos cerca → moral sube
    CHECK(g->morale[0] == before + MORALE_REGEN);
    CHECK(g->morale[0] > 26);
    CHECK(g->fleeing[0] == 0u);
    CHECK(g->fatal == FatalReason::NONE);
    delete g;
}

// ---------------------------------------------------------------
// 2) Desventaja clara: 1 aliado vs 4 enemigos pegados → moral SIGUE bajando.
//    El arreglo no convierte el pánico en imposible.
// ---------------------------------------------------------------
static void test_disadvantage_still_drops() {
    auto* g = make_state();
    RawCommand batch[5];
    uint32_t n = 0;
    fill_spawn(batch[n++], 0u, 1u, tile_cx(20), tile_cx(20), 100, 0, 0, 0, 200);
    fill_spawn(batch[n++], 1u, 1u, tile_cx(19), tile_cx(19), 100, 0, 0, 0, 200);
    fill_spawn(batch[n++], 1u, 2u, tile_cx(19), tile_cx(20), 100, 0, 0, 0, 200);
    fill_spawn(batch[n++], 1u, 3u, tile_cx(20), tile_cx(19), 100, 0, 0, 0, 200);
    fill_spawn(batch[n++], 1u, 4u, tile_cx(21), tile_cx(21), 100, 0, 0, 0, 200);
    step(*g, batch, n);
    g->morale[0] = 50;
    g->fleeing[0] = 0;
    const int32_t before = g->morale[0];
    step(*g, batch, 0);
    CHECK(g->morale[0] == before - MORALE_DROP);
    CHECK(g->fatal == FatalReason::NONE);
    delete g;
}

// ---------------------------------------------------------------
// 3) Pánico a 20 con números parejos: se rehace en un número acotado de
//    ticks y fleeing vuelve a 0.
// ---------------------------------------------------------------
static void test_panic_rebuilds_in_bounded_ticks() {
    auto* g = make_state();
    RawCommand batch[4];
    uint32_t n = 0;
    fill_spawn(batch[n++], 0u, 1u, tile_cx(20), tile_cx(20), 100, 0, 0, 0, 200);
    fill_spawn(batch[n++], 0u, 2u, tile_cx(20), tile_cx(21), 100, 0, 0, 0, 200);
    fill_spawn(batch[n++], 1u, 1u, tile_cx(21), tile_cx(20), 100, 0, 0, 0, 200);
    fill_spawn(batch[n++], 1u, 2u, tile_cx(21), tile_cx(21), 100, 0, 0, 0, 200);
    step(*g, batch, n);
    g->morale[0] = MORALE_PANIC;  // 20
    g->fleeing[0] = 1u;
    uint32_t tick_stopped = 0u;
    for (uint32_t t = 0u; t < 60u; ++t) {
        step(*g, batch, 0);
        if (g->fleeing[0] == 0u) { tick_stopped = t + 1u; break; }
    }
    CHECK(tick_stopped > 0u);           // se rehace
    CHECK(tick_stopped <= 40u);         // en un número acotado de ticks
    CHECK(g->morale[0] >= MORALE_RALLY);
    CHECK(g->fatal == FatalReason::NONE);
    delete g;
}

// ---------------------------------------------------------------
// 4) Acorralada: pegada a la esquina del mundo (0,0), enemigo al lado.
//    Tras un step: fleeing==0 y morale >= MORALE_RALLY.
// ---------------------------------------------------------------
static void test_cornered_rallies() {
    auto* g = make_state();
    RawCommand batch[2];
    uint32_t n = 0;
    fill_spawn(batch[n++], 0u, 1u, 0, 0, 100, 0, 0, 0, 200);          // acorralada
    fill_spawn(batch[n++], 1u, 1u, tile_cx(1), tile_cx(0), 100, 0, 0, 0, 200);
    step(*g, batch, n);
    g->morale[0] = 26;
    g->fleeing[0] = 1u;
    step(*g, batch, 0);
    CHECK(g->fleeing[0] == 0u);
    CHECK(g->morale[0] >= MORALE_RALLY);
    CHECK(g->fatal == FatalReason::NONE);
    delete g;
}

// ---------------------------------------------------------------
// 5) No acorralada: la misma unidad con sitio de sobra sigue huyendo.
//    El testigo: con B roto, la 4 no probaría nada.
// ---------------------------------------------------------------
static void test_not_cornered_keeps_fleeing() {
    auto* g = make_state();
    RawCommand batch[2];
    uint32_t n = 0;
    fill_spawn(batch[n++], 0u, 1u, tile_cx(30), tile_cx(30), 100, 0, 0, 0, 200);
    fill_spawn(batch[n++], 1u, 1u, tile_cx(31), tile_cx(30), 100, 0, 0, 0, 200);
    step(*g, batch, n);
    g->morale[0] = 26;
    g->fleeing[0] = 1u;
    step(*g, batch, 0);
    CHECK(g->fleeing[0] == 1u);
    CHECK(g->fatal == FatalReason::NONE);
    delete g;
}

// ---------------------------------------------------------------
// Determinismo: la zona muerta, dos corridas frescas → mismo resultado.
// ---------------------------------------------------------------
static void test_zone_dead_deterministic() {
    auto run_once = []() -> int32_t {
        auto* g = make_state();
        RawCommand batch[4];
        uint32_t n = 0;
        fill_spawn(batch[n++], 0u, 1u, tile_cx(20), tile_cx(20), 100, 0, 0, 0, 200);
        fill_spawn(batch[n++], 0u, 2u, tile_cx(20), tile_cx(21), 100, 0, 0, 0, 200);
        fill_spawn(batch[n++], 1u, 1u, tile_cx(21), tile_cx(20), 100, 0, 0, 0, 200);
        fill_spawn(batch[n++], 1u, 2u, tile_cx(21), tile_cx(21), 100, 0, 0, 0, 200);
        step(*g, batch, n);
        g->morale[0] = 26;
        g->fleeing[0] = 0;
        step(*g, batch, 0);
        const int32_t out = g->morale[0];
        delete g;
        return out;
    };
    CHECK(run_once() == run_once());
}

int main() {
    test_zone_dead_recovers();
    test_disadvantage_still_drops();
    test_panic_rebuilds_in_bounded_ticks();
    test_cornered_rallies();
    test_not_cornered_keeps_fleeing();
    test_zone_dead_deterministic();

    if (g_fails == 0) { std::printf("moral: OK\n"); return 0; }
    std::printf("moral: %d fallos\n", g_fails);
    return 1;
}
