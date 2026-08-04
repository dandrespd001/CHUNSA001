// Test del escenario del DoD (Sprint 1.6B, pieza K2, SPEC-004 §20 — EL
// CORAZÓN DEL SPRINT). Autor: sonnet-5 (brief
// docs/briefs/SONNET_K2_GATHER_APERTURA_SPRINT_1.6B.md).
//
// El escenario de apertura (chunsa/skirmish_apertura.hpp: egipto
// humano-scripted vs rome IA real, AMBOS con centro + 3 aldeanos y CERO
// ejército/edificios militares inyectados, catálogo REAL + 14 depósitos
// reales del mapa vía gs_init_economy_from_catalog) debe:
//   - Correr hasta game_over==1 con winner == 1 en < 36000 ticks, con la
//     IA recorriendo sola recolectar -> construir -> entrenar -> atacar.
//   - Dos corridas idénticas -> mismo winner y mismo tick de fin (determinismo).
//   - Save a MITAD DE RECOLECCIÓN + continuar -> mismo resultado que la
//     corrida continua.
//   - Replay -> reproduce bit-exacto.
//
// NOTA: GameState SIEMPRE en heap (make_unique) — un GameState en pila
// segfaultea bajo ctest (lección K1, ver game_state.hpp).
#include <cstdint>
#include <cstdio>
#include <memory>

#include "chunsa/skirmish_apertura.hpp"
#include "baselines.hpp"

#ifndef CHUNSA_GOLDEN_CHDB_PATH
#error "CHUNSA_GOLDEN_CHDB_PATH debe definirse via CMake (ver CMakeLists.txt: chunsa_test_ai_skirmish_apertura)"
#endif

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

namespace {

void check_baseline(const char* name, uint64_t expected, uint64_t obtained) {
    if (expected == obtained) return;
    ++g_fails;
    std::printf("BASELINE %s: esperado=%016llx obtenido=%016llx\n",
                name,
                static_cast<unsigned long long>(expected),
                static_cast<unsigned long long>(obtained));
}

MatchConfig01A apertura_cfg(uint64_t seed) {
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

// GameState heap-allocado, enlazado al catálogo REAL, civ/época por jugador
// Y los 14 depósitos reales del mapa (gs_init_economy_from_catalog — EL
// PUNTO del sprint, SPEC-004 §16/§20: nadie la llamaba desde un escenario
// hasta este). Listo para drive_skirmish_apertura().
std::unique_ptr<GameState> make_apertura_state(const DataCatalogV1& cat,
                                               const SkirmishAperturaSetup& setup,
                                               uint64_t seed) {
    auto g = std::make_unique<GameState>();
    gs_init(*g, apertura_cfg(seed));
    gs_bind_catalog(*g, cat);
    gs_set_player_civ(*g, 0, setup.civ_egipto);
    gs_set_player_civ(*g, 1, setup.civ_rome);
    gs_init_epoch_from_catalog_per_player(*g);
    // Sprint 1.22 — LA APERTURA SE FIJA EN LA ÉPOCA 5, A PROPÓSITO.
    //
    // Desde el 1.22 ambas civilizaciones arrancan en la época 1 (Paleolítica),
    // porque las épocas 1-4 por fin tienen contenido. Al dejar que este
    // escenario heredara ese arranque, la apertura DEJÓ DE TERMINAR: 36000
    // ticks, winner=255, p1_built=0, p1_trained=0.
    //
    // La causa no es un fallo de datos: es que `ai_execute` NO SABE SUBIR DE
    // ÉPOCA. Nunca emite ADVANCE_EPOCH, así que en la época 1 se queda mirando
    // un catálogo militar que empieza en la 5 y no construye nada. Es un
    // límite REAL del kernel, no un artefacto de la prueba, y está anotado
    // como el trabajo siguiente (SPEC-005: la IA necesita política de época).
    //
    // Este escenario existe para vigilar la APERTURA ECONÓMICA Y MILITAR de
    // la época 5 —recolección, dropoff, cuartel, tropa— y lleva baselines de
    // determinismo colgando de eso. Dejar que un cambio de época inicial le
    // cambie el significado en silencio sería perder la prueba y el aviso a la
    // vez. Se fija donde estaba; que la IA aprenda a jugar las épocas 1-4 es
    // otro sprint, y cuando lo haga tendrá su propio escenario.
    for (uint8_t p = 0; p < 2u; ++p) {
        g->player_epoch[p] = 5u;
        g->epoch_initial[p] = 5u;
    }
    gs_init_economy_from_catalog(*g);
    // Pre-flight duro del escenario: un estado con otro número de depósitos
    // no es una apertura válida y no debe producir ruido derivado en el resto
    // de asertos.
    // Sprint 1.46: +6 bosques (4 propios espejados + 2 neutrales) → 28.
    if (g->n_deposits != 28u) return nullptr;
    return g;
}

}  // namespace

static bool test_apertura_preflight() {
    DataCatalogStorageV1 store;
    const auto load_code =
        catalog_load_file_v1(CHUNSA_GOLDEN_CHDB_PATH, CatalogLoadProfile::Verified, store);
    CHECK(load_code == CatalogLoadCode::Ok);
    if (load_code != CatalogLoadCode::Ok || !store.valid()) return false;
    const SkirmishAperturaSetup setup = skirmish_apertura_resolve(store.catalog());
    CHECK(setup.ok);
    if (!setup.ok) return false;
    auto g = make_apertura_state(store.catalog(), setup, 20260724ull);
    CHECK(g != nullptr);
    if (g == nullptr) {
        std::printf("apertura pre-flight: se esperaban 28 depositos reales (1.22: +lino x2, +lana x2; 1.46: +6 bosques)\n");
        return false;
    }
    return true;
}

// ============================================================================
// A) La partida TERMINA en victoria real (< 36000 ticks): game_over==1,
//    winner == 1, y las 3 fases del DoD se alcanzaron (recursos > 0 en
//    ambos bandos, edificio militar construido Y unidad entrenada por rome).
// ============================================================================
static void test_apertura_concludes_in_victory() {
    DataCatalogStorageV1 store;
    const auto load_code = catalog_load_file_v1(CHUNSA_GOLDEN_CHDB_PATH, CatalogLoadProfile::Verified, store);
    CHECK(load_code == CatalogLoadCode::Ok);
    if (!store.valid()) { std::printf("apertura: catálogo inválido, abortando subtest\n"); return; }
    const DataCatalogV1& cat = store.catalog();
    const SkirmishAperturaSetup setup = skirmish_apertura_resolve(cat);
    CHECK(setup.ok);
    if (!setup.ok) { std::printf("apertura: no resolvió ids reales, abortando subtest\n"); return; }

    auto g = make_apertura_state(cat, setup, 20260724ull);
    if (g == nullptr) return;
    AiJobBox box{}; ai_box_init(box, 1);
    AiRuntimeV1 rt{0u, 4u};  // 1 PLACE_BUILDING + 3 SPAWN_UNIT del setup de rome (emitter=1)

    SkirmishAperturaOpts o{};
    o.ticks = 36000;
    SkirmishAperturaOut out{};
    const int code = drive_skirmish_apertura(o, *g, setup, box, rt, out);

    CHECK(code == 0);
    CHECK(out.fatal == FatalReason::NONE);
    CHECK(out.game_over == 1u);
    CHECK(out.winner == 1u);
    CHECK(out.end_tick < 36000u);
    CHECK(out.ai_executions > 0u);
    CHECK(out.p0_resources_gathered);
    CHECK(out.p1_resources_gathered);
    CHECK(out.p1_built_military);
    CHECK(out.p1_trained_military);
    CHECK(out.end_tick == determinism_baselines::AI_SKIRMISH_APERTURA_END_TICK);
    check_baseline("ai_skirmish_apertura.state",
                   determinism_baselines::AI_SKIRMISH_APERTURA_STATE,
                   out.final_checksum);
    check_baseline("ai_skirmish_apertura.continuation",
                   determinism_baselines::AI_SKIRMISH_APERTURA_CONTINUATION,
                   out.continuation_checksum);

    std::printf("apertura A: end_tick=%u winner=%u ai_executions=%u "
                "p0_gather=%d p1_gather=%d p1_built=%d p1_trained=%d "
                "state=%016llx cont=%016llx\n",
                out.end_tick, static_cast<unsigned>(out.winner), out.ai_executions,
                out.p0_resources_gathered, out.p1_resources_gathered,
                out.p1_built_military, out.p1_trained_military,
                static_cast<unsigned long long>(out.final_checksum),
                static_cast<unsigned long long>(out.continuation_checksum));
}

// ============================================================================
// B) Determinismo: dos corridas INDEPENDIENTES (GameState/caja/runtime
//    frescos, mismo catálogo cargado dos veces) dan el MISMO winner, el
//    MISMO tick de fin y los MISMOS checksums.
// ============================================================================
static void test_apertura_deterministic_two_runs() {
    auto run_once = [&](SkirmishAperturaOut& out) -> bool {
        DataCatalogStorageV1 store;
        const auto load_code = catalog_load_file_v1(CHUNSA_GOLDEN_CHDB_PATH, CatalogLoadProfile::Verified, store);
        if (load_code != CatalogLoadCode::Ok || !store.valid()) return false;
        const DataCatalogV1& cat = store.catalog();
        const SkirmishAperturaSetup setup = skirmish_apertura_resolve(cat);
        if (!setup.ok) return false;

        auto g = make_apertura_state(cat, setup, 20260724ull);
        if (g == nullptr) return false;
        AiJobBox box{}; ai_box_init(box, 1);
        AiRuntimeV1 rt{0u, 4u};
        SkirmishAperturaOpts o{};
        o.ticks = 36000;
        const int code = drive_skirmish_apertura(o, *g, setup, box, rt, out);
        return code == 0;
    };

    SkirmishAperturaOut out_a{}, out_b{};
    CHECK(run_once(out_a));
    CHECK(run_once(out_b));

    CHECK(out_a.game_over == 1u && out_b.game_over == 1u);
    CHECK(out_a.winner == out_b.winner);
    CHECK(out_a.winner == 1u);
    CHECK(out_a.end_tick == out_b.end_tick);
    CHECK(out_a.final_checksum == out_b.final_checksum);
    CHECK(out_a.continuation_checksum == out_b.continuation_checksum);
    CHECK(out_a.ai_executions == out_b.ai_executions);
}

// ============================================================================
// C) Save a MITAD DE RECOLECCIÓN + continuar == corrida continua. El punto
//    de guardado se fija por estado transitorio observable: un ciudadano de
//    Rome lleva carga o está en HARVEST/RETURN, con depósito vivo y la partida
//    aún activa.
// ============================================================================
static void test_apertura_save_mid_gather_and_continue() {
    DataCatalogStorageV1 store;
    const auto load_code = catalog_load_file_v1(CHUNSA_GOLDEN_CHDB_PATH, CatalogLoadProfile::Verified, store);
    CHECK(load_code == CatalogLoadCode::Ok);
    if (!store.valid()) { std::printf("apertura C: catálogo inválido, abortando subtest\n"); return; }
    const DataCatalogV1& cat = store.catalog();
    const SkirmishAperturaSetup setup = skirmish_apertura_resolve(cat);
    CHECK(setup.ok);
    if (!setup.ok) return;

    const char* save_path = "test_apertura_mid_gather.sav";

    // Corrida continua de referencia.
    SkirmishAperturaOut out_ref{};
    uint32_t mid_gather_tick = 0;
    uint32_t observed_ci = UINT32_MAX;
    EcoState observed_state = EcoState::SEEK;
    uint32_t observed_deposit = ECO_NO_DEPOSIT;
    int32_t observed_carry = 0;
    uint8_t observed_carry_resource = 0u;
    uint64_t observed_continuation = 0;
    {
        auto g = make_apertura_state(cat, setup, 20260724ull);
        if (g == nullptr) return;
        AiJobBox box{}; ai_box_init(box, 1);
        AiRuntimeV1 rt{0u, 4u};
        SkirmishAperturaOpts o{};
        o.ticks = 36000;
        const int code = drive_skirmish_apertura(o, *g, setup, box, rt, out_ref);
        CHECK(code == 0);
        CHECK(out_ref.game_over == 1u);
    }
    // Corrida de sondeo: guarda la primera frontera con estado económico
    // transitorio observable (mismo catálogo/setup/semilla).
    {
        auto g = make_apertura_state(cat, setup, 20260724ull);
        if (g == nullptr) return;
        AiJobBox box{}; ai_box_init(box, 1);
        AiRuntimeV1 rt{0u, 4u};
        std::vector<RawCommand> batch(8 + AI_MAX_COMMANDS);
        while (g->tick < out_ref.end_tick) {
            const uint32_t t = g->tick;
            uint32_t n = build_apertura_batch(batch, t, setup);
            if (ai_should_dispatch(box, t)) ai_dispatch(box, t, rt);
            if (box.state == AiJobState::DISPATCHED) ai_execute(box, *g);
            if (ai_stalled(box, t)) ai_execute(box, *g);
            if (ai_due(box, t)) {
                // Auditoría F-00: esta sonda reproduce solo la ruta de IA,
                // cuya cota contractual sigue siendo AI_MAX_COMMANDS. El
                // guard de copia se conserva y estas comprobaciones hacen
                // explícita la capacidad 8 + AI_MAX_COMMANDS del fixture.
                CHECK(n <= 8u);
                CHECK(box.result_count <= AI_MAX_COMMANDS);
                for (uint32_t k = 0; k < box.result_count && n < batch.size(); ++k) batch[n++] = box.result[k];
                ai_commit(box, rt);
            }
            step(*g, batch.data(), n);
            if (g->game_over != 0u) continue;
            for (uint32_t i = 0; i < g->entities.capacity; ++i) {
                if (!g->entities.alive[i] || g->owner[i] != 1u
                    || g->unit_class[i] != 3u) {
                    continue;
                }
                const uint32_t dep = g->eco_assigned_deposit[i];
                const bool gathering_active =
                    dep != ECO_NO_DEPOSIT && dep < g->n_deposits
                    && g->deposits[dep].remaining > 0;
                // Endurecimiento del Arquitecto (revisión K3): la disyunción
                // original (`carry>0 || HARVEST || RETURN`) la satisfacía
                // SIEMPRE primero un aldeano ENTRANDO en HARVEST con carry==0,
                // porque `carry` solo crece una vez ya dentro de HARVEST. Con
                // eso los asertos post-load sobre `eco_carry` y
                // `eco_carry_resource` comparaban 0 con 0: tautologías justo
                // sobre los dos campos que F-02 existe para blindar. Exigir
                // carry>0 implica ya HARVEST o RETURN y fuerza que el save
                // cruce una carga parcial real.
                const bool transient = g->eco_carry[i] > 0;
                if (!gathering_active || !transient) continue;
                mid_gather_tick = g->tick;
                observed_ci = i;
                observed_state = g->eco_state[i];
                observed_deposit = dep;
                observed_carry = g->eco_carry[i];
                observed_carry_resource = g->eco_carry_resource[i];
                observed_continuation = continuation_checksum(*g, box, rt);
                break;
            }
            if (observed_ci != UINT32_MAX) break;
        }
    }
    CHECK(mid_gather_tick > 0u);
    CHECK(mid_gather_tick < out_ref.end_tick);
    CHECK(observed_ci != UINT32_MAX);
    if (observed_ci == UINT32_MAX) return;
    std::printf("apertura C save-boundary: tick=%u citizen=%u state=%u "
                "deposit=%u carry=%d resource=%u cont=%016llx\n",
                mid_gather_tick, observed_ci, static_cast<unsigned>(observed_state),
                observed_deposit, observed_carry,
                static_cast<unsigned>(observed_carry_resource),
                static_cast<unsigned long long>(observed_continuation));

    // Corrida A: guarda EXACTAMENTE en mid_gather_tick (mitad de
    // recolección) y sigue hasta el fin en la MISMA invocación.
    SkirmishAperturaOut out_a{};
    {
        auto g = make_apertura_state(cat, setup, 20260724ull);
        if (g == nullptr) return;
        AiJobBox box{}; ai_box_init(box, 1);
        AiRuntimeV1 rt{0u, 4u};
        SkirmishAperturaOpts o{};
        o.ticks = 36000;
        o.save_at = mid_gather_tick;
        o.save_path = save_path;
        const int code = drive_skirmish_apertura(o, *g, setup, box, rt, out_a);
        CHECK(code == 0);
        CHECK(out_a.save_result == 0);
    }
    CHECK(out_a.game_over == out_ref.game_over);
    CHECK(out_a.winner == out_ref.winner);
    CHECK(out_a.end_tick == out_ref.end_tick);
    CHECK(out_a.final_checksum == out_ref.final_checksum);

    // Corrida B: CARGA el save de mitad de recolección y CONTINÚA — debe
    // llegar al MISMO resultado final que la corrida continua.
    SkirmishAperturaOut out_b{};
    {
        auto g = std::make_unique<GameState>();
        AiJobBox box{}; AiRuntimeV1 rt{};
        CHECK(load_game(*g, box, rt, save_path) == 0);
        // El catálogo es BINDING RUNTIME puro: jamás se serializa. Re-enlazar
        // EXPLÍCITAMENTE tras el load (mismo precedente que test_ai_skirmish.cpp).
        gs_bind_catalog(*g, cat);
        CHECK(g->tick == mid_gather_tick);
        CHECK(g->entities.alive[observed_ci] == 1u);
        CHECK(g->eco_state[observed_ci] == observed_state);
        CHECK(g->eco_assigned_deposit[observed_ci] == observed_deposit);
        CHECK(g->eco_carry[observed_ci] == observed_carry);
        CHECK(g->eco_carry_resource[observed_ci] == observed_carry_resource);
        CHECK(continuation_checksum(*g, box, rt) == observed_continuation);
        SkirmishAperturaOpts o{};
        o.ticks = 36000;  // sin save_path: solo continuar
        const int code = drive_skirmish_apertura(o, *g, setup, box, rt, out_b);
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
//    schedule_mismatches==0.
// ============================================================================
static void test_apertura_replay_bit_exact() {
    DataCatalogStorageV1 store;
    const auto load_code = catalog_load_file_v1(CHUNSA_GOLDEN_CHDB_PATH, CatalogLoadProfile::Verified, store);
    CHECK(load_code == CatalogLoadCode::Ok);
    if (!store.valid()) { std::printf("apertura D: catálogo inválido, abortando subtest\n"); return; }
    const DataCatalogV1& cat = store.catalog();
    const SkirmishAperturaSetup setup = skirmish_apertura_resolve(cat);
    CHECK(setup.ok);
    if (!setup.ok) return;

    const char* replay_path = "test_apertura.curp";

    uint32_t known_end_tick = 0;
    {
        auto g = make_apertura_state(cat, setup, 20260724ull);
        if (g == nullptr) return;
        AiJobBox box{}; ai_box_init(box, 1);
        AiRuntimeV1 rt{0u, 4u};
        SkirmishAperturaOpts o{};
        o.ticks = 36000;
        SkirmishAperturaOut out{};
        CHECK(drive_skirmish_apertura(o, *g, setup, box, rt, out) == 0);
        known_end_tick = out.end_tick;
    }

    SkirmishAperturaOut out_rec{};
    {
        auto g = make_apertura_state(cat, setup, 20260724ull);
        if (g == nullptr) return;
        AiJobBox box{}; ai_box_init(box, 1);
        AiRuntimeV1 rt{0u, 4u};
        SkirmishAperturaOpts o{};
        o.ticks = known_end_tick;
        ReplayWriter rec;
        rec.begin(/*seed=*/20260724ull, /*units=*/8u, o.ticks, 1u,
                  0u /*human_input_delay_ticks*/, 20u /*max_future_command_ticks*/);
        o.rec = &rec;
        const int code = drive_skirmish_apertura(o, *g, setup, box, rt, out_rec);
        CHECK(code == 0);
        CHECK(out_rec.game_over == 1u);
        CHECK(out_rec.end_tick == known_end_tick);
        CHECK(rec.finish(out_rec.final_checksum, replay_path) == 0);
    }

    ReplayData data;
    CHECK(replay_load(replay_path, data) == 0);
    CHECK(data.version == 3u);
    CHECK(data.final_checksum == out_rec.final_checksum);

    SkirmishAperturaOut out_verify{};
    {
        auto g = make_apertura_state(cat, setup, 20260724ull);
        if (g == nullptr) return;
        AiJobBox box{}; ai_box_init(box, 1);
        AiRuntimeV1 rt{0u, 4u};
        SkirmishAperturaOpts o{};
        o.ticks = data.ticks;
        o.feed = &data;
        const int code = drive_skirmish_apertura(o, *g, setup, box, rt, out_verify);
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
    if (!test_apertura_preflight()) {
        std::printf("ai_skirmish_apertura: pre-flight falló; subtests abortados\n");
        return 1;
    }
    test_apertura_concludes_in_victory();
    test_apertura_deterministic_two_runs();
    test_apertura_save_mid_gather_and_continue();
    test_apertura_replay_bit_exact();

    if (g_fails == 0) { std::printf("ai_skirmish_apertura: OK\n"); return 0; }
    std::printf("ai_skirmish_apertura: %d fallos\n", g_fails);
    return 1;
}
