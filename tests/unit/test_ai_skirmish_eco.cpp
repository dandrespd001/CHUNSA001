// Test del escenario de skirmish CON ECONOMÍA (Sprint 1.4 K3, SPEC-004 §7.1 —
// enmienda "aldeanos vulnerables"). Autor: sonnet-5 (brief docs/briefs/
// SONNET_K3_ALDEANOS_VULNERABLES_SPRINT_1.4.md).
//
// Demuestra el valor central del cierre K3: una partida CON economía real
// (aldeanos vivos, recolectando de verdad vía economy_system) que TERMINA
// por conquista dentro del presupuesto de ticks del gate — imposible antes
// de la enmienda §7.1 (un aldeano intocable jamás permitía declarar
// derrotado a su dueño, SPEC-005 §6). El escenario (chunsa/skirmish_eco.hpp)
// debe:
//   - Correr hasta game_over==1 con winner != 0xFF en < 36000 ticks.
//   - Dos corridas idénticas -> mismo winner y mismo tick de fin (determinismo).
//   - Save a mitad + continuar -> mismo resultado que la corrida continua.
//   - Replay -> reproduce bit-exacto (checksum, winner, tick de fin),
//     ai_executions==0 en feed-mode, schedule_mismatches==0.
//   - Al menos un aldeano del defensor entregó recurso (economía real, no
//     decorado) y al menos un aldeano murió en combate (la enmienda §7.1 en
//     acción, no un accidente de trayectoria).
//
// NOTA: GameState SIEMPRE en heap (make_unique) — un GameState en pila
// segfaultea bajo ctest (lección K1, ver game_state.hpp).
#include <cstdint>
#include <cstdio>
#include <memory>

#include "chunsa/skirmish_eco.hpp"

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

namespace {

MatchConfig01A skirmish_eco_cfg(uint64_t seed) {
    MatchConfig01A cfg{};
    cfg.max_entities = 512;
    cfg.player_count = 2;
    cfg.human_input_delay_ticks = 0;
    cfg.max_future_command_ticks = 20;
    cfg.checksum_every_ticks = 1;
    cfg.map_tiles_x = 256; cfg.map_tiles_y = 256;
    cfg.seed = seed;
    return cfg;
}

std::unique_ptr<GameState> make_skirmish_eco_state(const DataCatalogV1& cat, uint64_t seed) {
    auto g = std::make_unique<GameState>();
    gs_init(*g, skirmish_eco_cfg(seed));
    gs_bind_catalog(*g, cat);
    gs_init_epoch_from_catalog(*g);
    return g;
}

}  // namespace

// ============================================================================
// A) La partida CON ECONOMÍA termina en victoria real (< 36000 ticks):
//    game_over==1, winner != 0xFF — la demostración central del cierre K3.
// ============================================================================
static void test_skirmish_eco_concludes_in_victory() {
    static const DataCatalogV1 cat = skirmish_eco_make_catalog();
    auto g = make_skirmish_eco_state(cat, 20260724ull);
    AiJobBox box{}; ai_box_init(box, 1);
    AiRuntimeV1 rt{0u, 7u};  // 6 soldados + 1 centro del atacante en el setup

    SkirmishEcoOpts o{};
    o.ticks = 36000;
    SkirmishEcoOut out{};
    const int code = drive_skirmish_eco(o, *g, box, rt, out);

    CHECK(code == 0);
    CHECK(out.fatal == FatalReason::NONE);
    CHECK(out.game_over == 1u);
    CHECK(out.winner != 0xFFu);
    CHECK(out.end_tick < 36000u);
    CHECK(out.ai_executions > 0u);
    // El ganador es el atacante (owner=1): asimetría deliberada del
    // escenario (mismo patrón que skirmish.hpp), ahora con economía real en
    // juego del lado del defensor.
    CHECK(out.winner == 1u);
    std::printf("skirmish_eco A: end_tick=%u winner=%u ai_executions=%u state=%016llx cont=%016llx\n",
                out.end_tick, static_cast<unsigned>(out.winner), out.ai_executions,
                static_cast<unsigned long long>(out.final_checksum),
                static_cast<unsigned long long>(out.continuation_checksum));
}

// ============================================================================
// A2) La economía es REAL: al menos un aldeano del defensor entregó recurso
//    ANTES del fin de la partida (player_stock creció), y al menos un
//    aldeano del defensor murió en combate (unit_class==3 vulnerable —
//    SPEC-004 §7.1). Sin esto, el test A podría "pasar" con un escenario
//    puramente decorativo (aldeanos que nunca llegan a nada relevante).
// ============================================================================
static void test_skirmish_eco_real_economy_and_citizen_death() {
    static const DataCatalogV1 cat = skirmish_eco_make_catalog();
    auto g = make_skirmish_eco_state(cat, 20260724ull);
    AiJobBox box{}; ai_box_init(box, 1);
    AiRuntimeV1 rt{0u, 7u};

    SkirmishEcoOpts o{};
    o.ticks = 36000;
    SkirmishEcoOut out{};
    const int code = drive_skirmish_eco(o, *g, box, rt, out);
    CHECK(code == 0);
    CHECK(out.game_over == 1u);

    // Economía real: el defensor (owner=0) acumuló stock de algún recurso.
    const bool defender_gathered =
        g->player_stock[0][0] > 0 || g->player_stock[0][1] > 0 || g->player_stock[0][2] > 0;
    CHECK(defender_gathered);

    // El defensor perdió TODOS sus aldeanos (derrota exige 0 ciudadanos Y 0
    // edificios, SPEC-005 §6) — evidencia directa de que la enmienda §7.1
    // (aldeano vulnerable) es lo que permitió que la partida concluyera.
    uint32_t defender_citizens_alive = 0, defender_buildings_alive = 0;
    for (uint32_t i = 0; i < g->entities.capacity; ++i) {
        if (!g->entities.alive[i] || g->owner[i] != 0) continue;
        if (g->unit_class[i] == 3u) ++defender_citizens_alive;
        if (g->entity_kind[i] == 1u) ++defender_buildings_alive;
    }
    CHECK(defender_citizens_alive == 0u);
    CHECK(defender_buildings_alive == 0u);
    std::printf("skirmish_eco A2: stock_defensor A=%lld B=%lld Me=%lld\n",
                static_cast<long long>(g->player_stock[0][0]),
                static_cast<long long>(g->player_stock[0][1]),
                static_cast<long long>(g->player_stock[0][2]));
}

// ============================================================================
// B) Determinismo: dos corridas INDEPENDIENTES (GameState/caja/runtime
//    frescos cada una) con los mismos parámetros dan el MISMO winner, el
//    MISMO tick de fin y los MISMOS checksums.
// ============================================================================
static void test_skirmish_eco_deterministic_two_runs() {
    static const DataCatalogV1 cat = skirmish_eco_make_catalog();

    auto run_once = [&](SkirmishEcoOut& out) {
        auto g = make_skirmish_eco_state(cat, 20260724ull);
        AiJobBox box{}; ai_box_init(box, 1);
        AiRuntimeV1 rt{0u, 7u};
        SkirmishEcoOpts o{};
        o.ticks = 36000;
        const int code = drive_skirmish_eco(o, *g, box, rt, out);
        CHECK(code == 0);
    };

    SkirmishEcoOut out_a{}, out_b{};
    run_once(out_a);
    run_once(out_b);

    CHECK(out_a.game_over == 1u && out_b.game_over == 1u);
    CHECK(out_a.winner == out_b.winner);
    CHECK(out_a.winner != 0xFFu);
    CHECK(out_a.end_tick == out_b.end_tick);
    CHECK(out_a.final_checksum == out_b.final_checksum);
    CHECK(out_a.continuation_checksum == out_b.continuation_checksum);
    CHECK(out_a.ai_executions == out_b.ai_executions);
}

// ============================================================================
// C) Save a mitad + continuar == corrida continua (mismo winner, mismo tick
//    de fin, mismo checksum final).
// ============================================================================
static void test_skirmish_eco_save_and_continue() {
    static const DataCatalogV1 cat = skirmish_eco_make_catalog();
    const char* save_path = "test_skirmish_eco_mid.sav";

    SkirmishEcoOut out_ref{};
    {
        auto g = make_skirmish_eco_state(cat, 20260724ull);
        AiJobBox box{}; ai_box_init(box, 1);
        AiRuntimeV1 rt{0u, 7u};
        SkirmishEcoOpts o{};
        o.ticks = 36000;
        const int code = drive_skirmish_eco(o, *g, box, rt, out_ref);
        CHECK(code == 0);
        CHECK(out_ref.game_over == 1u);
    }

    SkirmishEcoOut out_a{};
    {
        auto g = make_skirmish_eco_state(cat, 20260724ull);
        AiJobBox box{}; ai_box_init(box, 1);
        AiRuntimeV1 rt{0u, 7u};
        SkirmishEcoOpts o{};
        o.ticks = 36000;
        o.save_at = out_ref.end_tick / 2u;
        o.save_path = save_path;
        const int code = drive_skirmish_eco(o, *g, box, rt, out_a);
        CHECK(code == 0);
        CHECK(out_a.save_result == 0);
    }
    CHECK(out_a.game_over == out_ref.game_over);
    CHECK(out_a.winner == out_ref.winner);
    CHECK(out_a.end_tick == out_ref.end_tick);
    CHECK(out_a.final_checksum == out_ref.final_checksum);

    SkirmishEcoOut out_b{};
    {
        auto g = std::make_unique<GameState>();
        AiJobBox box{}; AiRuntimeV1 rt{};
        CHECK(load_game(*g, box, rt, save_path) == 0);
        // Catálogo: binding runtime puro, jamás serializado — re-enlazar tras load.
        gs_bind_catalog(*g, cat);
        CHECK(g->tick == out_ref.end_tick / 2u);
        SkirmishEcoOpts o{};
        o.ticks = 36000;
        const int code = drive_skirmish_eco(o, *g, box, rt, out_b);
        CHECK(code == 0);
    }
    CHECK(out_b.game_over == out_ref.game_over);
    CHECK(out_b.winner == out_ref.winner);
    CHECK(out_b.end_tick == out_ref.end_tick);
    CHECK(out_b.final_checksum == out_ref.final_checksum);
    CHECK(out_b.continuation_checksum == out_ref.continuation_checksum);

    std::remove(save_path);
}

// ============================================================================
// D) Replay: graba la corrida completa y reproduce en feed-mode — checksum,
//    winner y tick de fin bit-exactos; ai_executions==0 en feed-mode;
//    schedule_mismatches==0 (agenda auto-verificada, SPEC-004 §10.1/v2).
// ============================================================================
static void test_skirmish_eco_replay_bit_exact() {
    static const DataCatalogV1 cat = skirmish_eco_make_catalog();
    const char* replay_path = "test_skirmish_eco.curp";

    uint32_t known_end_tick = 0;
    {
        auto g = make_skirmish_eco_state(cat, 20260724ull);
        AiJobBox box{}; ai_box_init(box, 1);
        AiRuntimeV1 rt{0u, 7u};
        SkirmishEcoOpts o{};
        o.ticks = 36000;
        SkirmishEcoOut out{};
        CHECK(drive_skirmish_eco(o, *g, box, rt, out) == 0);
        known_end_tick = out.end_tick;
    }

    SkirmishEcoOut out_rec{};
    {
        auto g = make_skirmish_eco_state(cat, 20260724ull);
        AiJobBox box{}; ai_box_init(box, 1);
        AiRuntimeV1 rt{0u, 7u};
        SkirmishEcoOpts o{};
        o.ticks = known_end_tick;
        ReplayWriter rec;
        rec.begin(o.seed, o.defender_soldiers + o.defender_citizens + o.attacker_soldiers,
                  o.ticks, 1u, 0u /*human_input_delay_ticks*/, 20u /*max_future_command_ticks*/);
        o.rec = &rec;
        const int code = drive_skirmish_eco(o, *g, box, rt, out_rec);
        CHECK(code == 0);
        CHECK(out_rec.game_over == 1u);
        CHECK(out_rec.end_tick == known_end_tick);
        CHECK(rec.finish(out_rec.final_checksum, replay_path) == 0);
    }

    ReplayData data;
    CHECK(replay_load(replay_path, data) == 0);
    CHECK(data.version == 3u);
    CHECK(data.final_checksum == out_rec.final_checksum);

    SkirmishEcoOut out_verify{};
    {
        auto g = make_skirmish_eco_state(cat, 20260724ull);
        AiJobBox box{}; ai_box_init(box, 1);
        AiRuntimeV1 rt{0u, 7u};
        SkirmishEcoOpts o{};
        o.ticks = data.ticks;
        o.feed = &data;
        const int code = drive_skirmish_eco(o, *g, box, rt, out_verify);
        CHECK(code == 0);
    }
    CHECK(out_verify.ai_executions == 0u);          // IA JAMÁS se ejecuta en feed-mode
    CHECK(out_verify.schedule_mismatches == 0u);     // agenda recomputada == grabada
    CHECK(out_verify.game_over == out_rec.game_over);
    CHECK(out_verify.winner == out_rec.winner);
    CHECK(out_verify.end_tick == out_rec.end_tick);
    CHECK(out_verify.final_checksum == out_rec.final_checksum);

    std::remove(replay_path);
}

int main() {
    test_skirmish_eco_concludes_in_victory();
    test_skirmish_eco_real_economy_and_citizen_death();
    test_skirmish_eco_deterministic_two_runs();
    test_skirmish_eco_save_and_continue();
    test_skirmish_eco_replay_bit_exact();

    if (g_fails == 0) { std::printf("ai_skirmish_eco: OK\n"); return 0; }
    std::printf("ai_skirmish_eco: %d fallos\n", g_fails);
    return 1;
}
