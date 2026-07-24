// Test del gate de fase (Sprint 1.4 K2, SPEC-005 §8.3 — EL CORAZÓN DEL
// GATE). Autor: sonnet-5 (brief docs/briefs/SONNET_K2_IA_EXECUTE_SPRINT_1.4.md).
//
// El escenario de skirmish (chunsa/skirmish.hpp: humano-scripted owner=0 vs
// IA real owner=1) debe:
//   - Correr hasta game_over==1 con winner != 0xFF en < 36000 ticks.
//   - Dos corridas idénticas -> mismo winner y mismo tick de fin (determinismo).
//   - Save a mitad + continuar -> mismo resultado que la corrida continua.
//   - Replay -> reproduce bit-exacto (checksum, winner, tick de fin),
//     ai_executions==0 en feed-mode, schedule_mismatches==0.
//
// NOTA: GameState SIEMPRE en heap (make_unique) — un GameState en pila
// segfaultea bajo ctest (lección K1, ver game_state.hpp).
#include <cstdint>
#include <cstdio>
#include <memory>

#include "chunsa/skirmish.hpp"

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

namespace {

MatchConfig01A skirmish_cfg(uint64_t seed) {
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

// Construye un GameState heap-allocado, ya enlazado al catálogo del
// skirmish y con la época inicial derivada — listo para drive_skirmish().
std::unique_ptr<GameState> make_skirmish_state(const DataCatalogV1& cat, uint64_t seed) {
    auto g = std::make_unique<GameState>();
    gs_init(*g, skirmish_cfg(seed));
    gs_bind_catalog(*g, cat);
    gs_init_epoch_from_catalog(*g);
    return g;
}

}  // namespace

// ============================================================================
// A) La partida TERMINA en victoria real (< 36000 ticks): game_over==1,
//    winner != 0xFF.
// ============================================================================
static void test_skirmish_concludes_in_victory() {
    static const DataCatalogV1 cat = skirmish_make_catalog();
    auto g = make_skirmish_state(cat, 20260724ull);
    AiJobBox box{}; ai_box_init(box, 1);
    AiRuntimeV1 rt{0u, 7u};  // 6 soldados + 1 centro del atacante en el setup

    SkirmishOpts o{};
    o.ticks = 36000;
    SkirmishOut out{};
    const int code = drive_skirmish(o, *g, box, rt, out);

    CHECK(code == 0);
    CHECK(out.fatal == FatalReason::NONE);
    CHECK(out.game_over == 1u);
    CHECK(out.winner != 0xFFu);
    CHECK(out.end_tick < 36000u);
    CHECK(out.ai_executions > 0u);
    std::printf("skirmish A: end_tick=%u winner=%u ai_executions=%u state=%016llx cont=%016llx\n",
                out.end_tick, static_cast<unsigned>(out.winner), out.ai_executions,
                static_cast<unsigned long long>(out.final_checksum),
                static_cast<unsigned long long>(out.continuation_checksum));
}

// ============================================================================
// B) Determinismo: dos corridas INDEPENDIENTES (GameState/caja/runtime
//    frescos cada una) con los mismos parámetros dan el MISMO winner, el
//    MISMO tick de fin y los MISMOS checksums.
// ============================================================================
static void test_skirmish_deterministic_two_runs() {
    static const DataCatalogV1 cat = skirmish_make_catalog();

    auto run_once = [&](SkirmishOut& out) {
        auto g = make_skirmish_state(cat, 20260724ull);
        AiJobBox box{}; ai_box_init(box, 1);
        AiRuntimeV1 rt{0u, 7u};
        SkirmishOpts o{};
        o.ticks = 36000;
        const int code = drive_skirmish(o, *g, box, rt, out);
        CHECK(code == 0);
    };

    SkirmishOut out_a{}, out_b{};
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
static void test_skirmish_save_and_continue() {
    static const DataCatalogV1 cat = skirmish_make_catalog();
    const char* save_path = "test_skirmish_mid.sav";

    // Corrida continua de referencia.
    SkirmishOut out_ref{};
    {
        auto g = make_skirmish_state(cat, 20260724ull);
        AiJobBox box{}; ai_box_init(box, 1);
        AiRuntimeV1 rt{0u, 7u};
        SkirmishOpts o{};
        o.ticks = 36000;
        const int code = drive_skirmish(o, *g, box, rt, out_ref);
        CHECK(code == 0);
        CHECK(out_ref.game_over == 1u);
    }

    // Corrida A: guarda a mitad de camino (bien antes del fin natural) y
    // sigue hasta el fin en la MISMA invocación (sin recargar) — produce el
    // save_result y confirma que save-at no perturba la trayectoria.
    SkirmishOut out_a{};
    {
        auto g = make_skirmish_state(cat, 20260724ull);
        AiJobBox box{}; ai_box_init(box, 1);
        AiRuntimeV1 rt{0u, 7u};
        SkirmishOpts o{};
        o.ticks = 36000;
        o.save_at = out_ref.end_tick / 2u;
        o.save_path = save_path;
        const int code = drive_skirmish(o, *g, box, rt, out_a);
        CHECK(code == 0);
        CHECK(out_a.save_result == 0);
    }
    CHECK(out_a.game_over == out_ref.game_over);
    CHECK(out_a.winner == out_ref.winner);
    CHECK(out_a.end_tick == out_ref.end_tick);
    CHECK(out_a.final_checksum == out_ref.final_checksum);

    // Corrida B: CARGA el save de mitad de camino y CONTINÚA — debe llegar
    // al MISMO resultado final que la corrida continua.
    SkirmishOut out_b{};
    {
        auto g = std::make_unique<GameState>();
        AiJobBox box{}; AiRuntimeV1 rt{};
        CHECK(load_game(*g, box, rt, save_path) == 0);
        // El catálogo es BINDING RUNTIME puro (game_state.hpp): jamás se
        // serializa/deserializa. Tras un load hay que re-enlazarlo
        // EXPLÍCITAMENTE — si no, g->catalog queda nullptr y la capa
        // estratégica de la IA se apaga en silencio a partir de aquí
        // (diverge de la corrida continua, que nunca pierde el binding).
        gs_bind_catalog(*g, cat);
        CHECK(g->tick == out_ref.end_tick / 2u);
        SkirmishOpts o{};
        o.ticks = 36000;  // sin save_path: solo continuar
        const int code = drive_skirmish(o, *g, box, rt, out_b);
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
static void test_skirmish_replay_bit_exact() {
    static const DataCatalogV1 cat = skirmish_make_catalog();
    const char* replay_path = "test_skirmish.curp";

    // Paso 0 (solo para conocer `ticks` de antemano): el recorder necesita
    // el nº EXACTO de tick_batch() que habrá — la partida termina mucho
    // antes de o.ticks (game_over congela el bucle temprano); `ReplayWriter::
    // begin` fija ese nº en la cabecera y `replay_load` exige encontrar
    // exactamente esa cantidad de registros en el archivo (SPEC-001 §11.3).
    // Grabar con `ticks=36000` (el límite del gate) escribiría solo ~1200
    // registros bajo una cabecera que promete 36000 -> replay_load fallaría
    // por EOF prematuro. Correr una vez sin grabar para fijar `ticks` al
    // tick de fin real no reintroduce no-determinismo: la corrida grabada
    // reproduce el MISMO resultado (ver test B, dos corridas independientes
    // ya dan resultados idénticos).
    uint32_t known_end_tick = 0;
    {
        auto g = make_skirmish_state(cat, 20260724ull);
        AiJobBox box{}; ai_box_init(box, 1);
        AiRuntimeV1 rt{0u, 7u};
        SkirmishOpts o{};
        o.ticks = 36000;
        SkirmishOut out{};
        CHECK(drive_skirmish(o, *g, box, rt, out) == 0);
        known_end_tick = out.end_tick;
    }

    SkirmishOut out_rec{};
    {
        auto g = make_skirmish_state(cat, 20260724ull);
        AiJobBox box{}; ai_box_init(box, 1);
        AiRuntimeV1 rt{0u, 7u};
        SkirmishOpts o{};
        o.ticks = known_end_tick;
        ReplayWriter rec;
        rec.begin(o.seed, o.defender_soldiers + o.attacker_soldiers, o.ticks, 1u,
                  0u /*human_input_delay_ticks*/, 20u /*max_future_command_ticks*/);
        o.rec = &rec;
        const int code = drive_skirmish(o, *g, box, rt, out_rec);
        CHECK(code == 0);
        CHECK(out_rec.game_over == 1u);
        CHECK(out_rec.end_tick == known_end_tick);
        CHECK(rec.finish(out_rec.final_checksum, replay_path) == 0);
    }

    ReplayData data;
    CHECK(replay_load(replay_path, data) == 0);
    CHECK(data.version == 3u);
    CHECK(data.final_checksum == out_rec.final_checksum);

    SkirmishOut out_verify{};
    {
        auto g = make_skirmish_state(cat, 20260724ull);
        AiJobBox box{}; ai_box_init(box, 1);
        AiRuntimeV1 rt{0u, 7u};
        SkirmishOpts o{};
        o.ticks = data.ticks;
        o.feed = &data;
        const int code = drive_skirmish(o, *g, box, rt, out_verify);
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
    test_skirmish_concludes_in_victory();
    test_skirmish_deterministic_two_runs();
    test_skirmish_save_and_continue();
    test_skirmish_replay_bit_exact();

    if (g_fails == 0) { std::printf("ai_skirmish: OK\n"); return 0; }
    std::printf("ai_skirmish: %d fallos\n", g_fails);
    return 1;
}
