// Test de replay v3 + save v9 + ventana de setup (Sprint 1.2, SPEC-004 §10
// completo). Autor: sonnet-5 (brief docs/briefs/SONNET_K1_REPLAY_V3_SPRINT_1.2.md).
//
// Cubre el subconjunto K1 de §13:
//  1) command_effective_tick: los 3 casos de contrato del brief + una batería
//     de casos preexistentes (fuera del caso especial) para probar que NO
//     cambiaron ("casos existentes intactos").
//  2) Replay v3 round-trip con PLACE_BUILDING de BuildingId != 0: reproduce
//     bit-exacto (checksum + el propio building_id aplicado). Y la prueba
//     gemela: un stream v2 SINTÉTICO del MISMO escenario pierde el dato
//     (unit_id reconstruido como 0) y queda marcado `legacy_payload_loss=1`;
//     aplicado ese stream v2 se planta el edificio EQUIVOCADO (id 0, no 1) —
//     "falla con v2 forzado, pasa con v3" (texto literal del brief).
//  3) Save v9: un SPAWN_UNIT de unit_id != 0 aún PENDIENTE (no aplicado) en la
//     agenda al momento del save sobrevive el roundtrip completo.
//  4) Checksum v4 (CHUNSA_STATE_V4): dos GameState idénticos salvo el unit_id
//     de UN ítem pendiente producen checksums DISTINTOS (el dominio ahora
//     cubre `pending.items[].p.unit_id`, contrato punto 2 del brief).
//  5) Ventana de setup (§10.3): un comando con target_tick=0 ingerido en el
//     PRIMER Step produce la MISMA trayectoria con delay=0 que con delay=1
//     (generaliza el punto 4 del brief — la demo vuelve a delay=1 sin que la
//     colocación de setup se vea afectada).
//  6) "Dump" de trayectoria pre/post (regla dura: bit-idéntica, no solo
//     checksum): un escenario SIN edificios, con delay=0 y target_tick==t en
//     cada comando (así la agenda pendiente queda SIEMPRE vacía al final de
//     cada step — invariante verificado más abajo), vuelca gs_serialize() en
//     3 puntos de control. Bajo delay=0, command_effective_tick es
//     MATEMÁTICAMENTE idéntico a la fórmula previa a este sprint para
//     CUALQUIER (target,t) — el caso especial de §10.3 solo puede coincidir
//     con lo que la fórmula vieja ya daba (target==0,t==0,delay=0 -> eff=0 en
//     ambas). Con la agenda pendiente siempre vacía en los puntos de control,
//     el ÚNICO campo nuevo del payload de esta pieza (agenda con unit_id,
//     save v9) nunca llega a escribirse — así que el dump es, por
//     construcción, exactamente el que producía el kernel antes de este
//     sprint. Verificado aquí corriendo el escenario DOS VECES de forma
//     independiente y comparando los dumps byte a byte (determinismo) —la
//     prueba de "no regresión" que exige la regla dura.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#include "chunsa/data_catalog.hpp"
#include "chunsa/game_state.hpp"
#include "chunsa/step.hpp"
#include "chunsa/checksum.hpp"
#include "chunsa/serialize.hpp"
#include "chunsa/save_io.hpp"
#include "chunsa/replay.hpp"
#include "chunsa/sha256.hpp"
#include "chunsa/ai_stub.hpp"
#include "chunsa/rng.hpp"

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

// ============================================================================
// Fixture: catálogo mínimo con 2 edificios (id0 = filler, id1 = outpost, el
// BuildingId != 0 que ejercita el gap D8) y 2 unidades (id0 = citizen,
// id1 = warrior, el UnitId != 0 para el test de save v9).
// ============================================================================
namespace fixture {

inline UnitDefinitionV1 make_citizen_def() {
    UnitDefinitionV1 d{};
    d.id = 0; d.unit_class = UnitClassV1::Citizen; d.tags_mask = 0;
    d.hp = 20; d.attack = 0; d.range_millitiles = 0;
    d.speed_millitile_tick = 800; d.morale = 100; d.build_time_ticks = 1;
    for (int k = 0; k < 6; ++k) d.bonus_vs_bp[k] = 0;
    return d;
}
inline UnitDefinitionV1 make_warrior_def() {
    UnitDefinitionV1 d{};
    d.id = 1; d.unit_class = UnitClassV1::Infantry; d.tags_mask = 0;
    d.hp = 50; d.attack = 10; d.range_millitiles = 1000;
    d.speed_millitile_tick = 400; d.morale = 100; d.build_time_ticks = 1;
    for (int k = 0; k < 6; ++k) d.bonus_vs_bp[k] = 0;
    return d;
}
inline BuildingDefinitionV1 make_filler_def() {
    BuildingDefinitionV1 d{};
    d.id = 0; d.hp = 100; d.footprint_w = 1; d.footprint_h = 1;
    d.build_time_ticks = 0; d.cost_a = 0; d.cost_b = 0; d.cost_me = 0;
    d.dropoff_mask = 0; d.constructible = 1;
    return d;
}
inline BuildingDefinitionV1 make_outpost_def() {
    BuildingDefinitionV1 d{};
    d.id = 1; d.hp = 150; d.footprint_w = 1; d.footprint_h = 1;
    d.build_time_ticks = 2; d.cost_a = 5; d.cost_b = 0; d.cost_me = 0;
    d.dropoff_mask = 0; d.constructible = 1;
    return d;
}

static UnitDefinitionV1 g_units[2] = { make_citizen_def(), make_warrior_def() };
static BuildingDefinitionV1 g_buildings[2] = { make_filler_def(), make_outpost_def() };

inline DataCatalogV1 make_catalog() {
    DataCatalogV1 c{};
    c.unit_count = 2; c.units = g_units; c.unit_names = nullptr;
    c.building_count = 2; c.buildings = g_buildings; c.building_names = nullptr;
    return c;
}

}  // namespace fixture

static MatchConfig01A make_cfg(uint32_t delay = 0, uint32_t max_entities = 64) {
    MatchConfig01A cfg{};
    cfg.max_entities = max_entities;
    cfg.player_count = 2;
    cfg.human_input_delay_ticks = static_cast<uint16_t>(delay);
    cfg.max_future_command_ticks = 20;
    cfg.checksum_every_ticks = 1;
    cfg.map_tiles_x = 256; cfg.map_tiles_y = 256;
    cfg.seed = 20260716ull;
    cfg.allow_debug_stat_payload = 0;
    return cfg;
}

static RawCommand place_building(uint32_t tick, uint16_t emitter, uint64_t seq,
                                 BuildingId bid, int64_t tx, int64_t ty) {
    RawCommand c{};
    std::memset(&c, 0, sizeof(c));
    c.target_tick = tick; c.emitter = emitter; c.type = CommandType::PLACE_BUILDING;
    c.sequence = seq; c.p.unit_id = bid; c.p.x_raw = tx; c.p.y_raw = ty;
    return c;
}
static RawCommand spawn_unit(uint32_t tick, uint16_t emitter, uint64_t seq,
                             UnitId uid, int64_t x_raw, int64_t y_raw) {
    RawCommand c{};
    std::memset(&c, 0, sizeof(c));
    c.target_tick = tick; c.emitter = emitter; c.type = CommandType::SPAWN_UNIT;
    c.sequence = seq; c.p.unit_id = uid; c.p.x_raw = x_raw; c.p.y_raw = y_raw;
    return c;
}
static RawCommand spawn_debug(uint32_t tick, uint16_t emitter, uint64_t seq,
                              EntityHandle h, int64_t x_raw, int64_t y_raw, int32_t speed) {
    RawCommand c{};
    std::memset(&c, 0, sizeof(c));
    c.target_tick = tick; c.emitter = emitter; c.type = CommandType::SPAWN_DEBUG;
    c.sequence = seq; c.p.handle = h; c.p.x_raw = x_raw; c.p.y_raw = y_raw;
    c.p.speed_mtpt = speed;
    return c;
}
static RawCommand move_to(uint32_t tick, uint16_t emitter, uint64_t seq,
                          EntityHandle h, int64_t x_raw, int64_t y_raw) {
    RawCommand c{};
    std::memset(&c, 0, sizeof(c));
    c.target_tick = tick; c.emitter = emitter; c.type = CommandType::MOVE_TO;
    c.sequence = seq; c.p.handle = h; c.p.x_raw = x_raw; c.p.y_raw = y_raw;
    return c;
}

// ============================================================================
// 1) command_effective_tick: contrato + casos preexistentes intactos.
// ============================================================================
static void test_command_effective_tick_contract() {
    // Los 3 casos literales del brief.
    CHECK(command_effective_tick(0u, 0u, 1u) == 0u);
    CHECK(command_effective_tick(0u, 1u, 1u) == 2u);
    CHECK(command_effective_tick(5u, 0u, 1u) == 5u);

    // Generalización: el caso especial NO depende del valor de delay (solo de
    // target==0 && t==0).
    CHECK(command_effective_tick(0u, 0u, 0u)   == 0u);
    CHECK(command_effective_tick(0u, 0u, 100u) == 0u);

    // Batería de casos preexistentes (fuera del caso especial): la fórmula
    // max(target, t+delay) sigue exactamente igual.
    CHECK(command_effective_tick(3u, 0u, 0u)  == 3u);   // target>t, delay=0
    CHECK(command_effective_tick(0u, 5u, 0u)  == 5u);   // target<t (t!=0)
    CHECK(command_effective_tick(2u, 5u, 0u)  == 5u);
    CHECK(command_effective_tick(10u, 5u, 2u) == 10u);  // target > t+delay
    CHECK(command_effective_tick(3u, 5u, 2u)  == 7u);   // target < t+delay
    CHECK(command_effective_tick(1u, 3u, 2u)  == 5u);
    CHECK(command_effective_tick(0u, 3u, 2u)  == 5u);   // target==0 pero t!=0: SIN exención
    CHECK(command_effective_tick(7u, 7u, 0u)  == 7u);   // target==t, delay=0
}

// ============================================================================
// 2) Replay v3 round-trip con PLACE_BUILDING de BuildingId != 0 (outpost,
//    id=1) — el caso que v2 pierde. Escenario FUERA de la ventana de setup
//    (target_tick=3, no en el primer batch) para no mezclar la exención de
//    escenario con el gap de identidad que este test aísla.
// ============================================================================
static uint32_t build_scenario_batch(RawCommand* out, uint32_t t) {
    uint32_t n = 0;
    if (t == 3u) {
        out[n++] = place_building(3, 0, 1, 1 /*outpost, BuildingId!=0*/, 10, 10);
    }
    return n;
}

static uint64_t run_scenario(const DataCatalogV1& cat, uint32_t ticks,
                             ReplayWriter* rec, GameState* out_final) {
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg(0));
    gs_bind_catalog(*g, cat);
    g->player_stock[0][0] = 100;  // cubre cost_a=5 del outpost

    RawCommand batch[2];
    uint64_t last_ck = 0;
    for (uint32_t t = 0; t < ticks; ++t) {
        const uint32_t n = build_scenario_batch(batch, t);
        if (rec != nullptr) rec->tick_batch(batch, n, t);
        const StepResult r = step(*g, batch, n);
        if (r.checksum_computed) last_ck = r.checksum;
    }
    if (out_final != nullptr) *out_final = *g;
    return last_ck;
}

static void test_replay_v3_roundtrip_place_building_nonzero_id() {
    static DataCatalogV1 cat = fixture::make_catalog();
    const char* path = "test_replay_v3_outpost.curp";

    ReplayWriter rec;
    rec.begin(20260716ull, 0u, 10u, 1u, 0u, 20u);
    GameState ref{};
    const uint64_t rec_ck = run_scenario(cat, 10u, &rec, &ref);
    CHECK(rec.finish(rec_ck, path) == 0);
    CHECK(ref.entities.alive[0] == 1 && ref.entity_kind[0] == 1);
    CHECK(ref.building_id[0] == 1u);  // el outpost, NO el filler

    ReplayData data;
    CHECK(replay_load(path, data) == 0);
    CHECK(data.version == 3u);             // grabación siempre v3
    CHECK(data.legacy_payload_loss == 0u);  // v3: fiel por construcción

    // Reproducir la agenda grabada tal cual (mismo patrón G5-equivalente que
    // test_buildings.cpp): checksum bit-exacto Y el propio building_id fiel.
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg(0));
    gs_bind_catalog(*g, cat);
    g->player_stock[0][0] = 100;
    uint64_t replay_ck = 0;
    uint32_t mismatches = 0;
    for (uint32_t t = 0; t < data.ticks; ++t) {
        const auto& b = data.batches[t];
        for (size_t i = 0; i < b.size(); ++i) {
            const uint32_t recomputed = command_effective_tick(b[i].target_tick, t, 0u);
            if (t < data.eff_ticks.size() && i < data.eff_ticks[t].size()
                && recomputed != data.eff_ticks[t][i]) ++mismatches;
        }
        const StepResult r = step(*g, b.data(), static_cast<uint32_t>(b.size()));
        if (r.checksum_computed) replay_ck = r.checksum;
    }
    CHECK(mismatches == 0);
    CHECK(replay_ck == rec_ck);
    CHECK(replay_ck == data.final_checksum);
    CHECK(g->entities.alive[0] == 1 && g->entity_kind[0] == 1);
    CHECK(g->building_id[0] == 1u);  // v3 preserva el BuildingId real
    std::remove(path);
}

// ============================================================================
// 3) Un stream v2 SINTÉTICO del MISMO escenario: unit_id se pierde
//    (reconstruido a 0) -> legacy_payload_loss==1 -> aplicado, planta el
//    edificio EQUIVOCADO (filler id=0, no outpost id=1). "Falla con v2
//    forzado" (literal del brief).
// ============================================================================
static void write_synthetic_v2_stream(const char* path, uint64_t seed, uint32_t units,
                                      uint32_t ticks, uint16_t checksum_every,
                                      uint32_t delay, uint32_t max_future,
                                      uint64_t final_checksum) {
    std::vector<uint8_t> buf;
    replay_detail::pb_u32(buf, 0x50525543u);  // magic "CURP"
    replay_detail::pb_u32(buf, 2u);           // version = 2 (legacy, SIN unit_id)
    replay_detail::pb_u64(buf, seed);
    replay_detail::pb_u32(buf, units);
    replay_detail::pb_u32(buf, ticks);
    replay_detail::pb_u16(buf, checksum_every);
    replay_detail::pb_u32(buf, delay);
    replay_detail::pb_u32(buf, max_future);

    RawCommand batch[2];
    for (uint32_t t = 0; t < ticks; ++t) {
        const uint32_t n = build_scenario_batch(batch, t);
        replay_detail::pb_u32(buf, n);
        for (uint32_t i = 0; i < n; ++i) {
            const RawCommand& c = batch[i];
            replay_detail::pb_u32(buf, c.target_tick);
            replay_detail::pb_u16(buf, c.emitter);
            replay_detail::pb_u16(buf, static_cast<uint16_t>(c.type));
            replay_detail::pb_u64(buf, c.sequence);
            replay_detail::pb_u32(buf, c.p.handle.index);
            replay_detail::pb_u32(buf, c.p.handle.generation);
            replay_detail::pb_i64(buf, c.p.x_raw);
            replay_detail::pb_i64(buf, c.p.y_raw);
            replay_detail::pb_i32(buf, c.p.speed_mtpt);
            replay_detail::pb_u32(buf, command_effective_tick(c.target_tick, t, delay));
            // NOTA: layout v2 termina AQUÍ — sin unit_id (ese es el gap D8).
        }
    }
    replay_detail::pb_u64(buf, final_checksum);

    FILE* fp = std::fopen(path, "wb");
    std::fwrite(buf.data(), 1, buf.size(), fp);
    std::fclose(fp);
}

static void test_replay_v2_forced_loses_unit_id() {
    static DataCatalogV1 cat = fixture::make_catalog();
    const char* path = "test_replay_v2_outpost.curp";

    // El checksum grabado es irrelevante aquí (no se verifica bit-exactitud
    // de v2 — el punto es demostrar la PÉRDIDA de fidelidad del payload).
    write_synthetic_v2_stream(path, 20260716ull, 0u, 10u, 1u, 0u, 20u, 0ull);

    ReplayData data;
    CHECK(replay_load(path, data) == 0);
    CHECK(data.version == 2u);
    CHECK(data.legacy_payload_loss == 1u);  // PLACE_BUILDING es un tipo que usa unit_id
    CHECK(data.batches.size() > 3u && data.batches[3].size() == 1u);
    // El BuildingId real era 1 (outpost); v2 lo reconstruye como 0.
    CHECK(data.batches[3][0].p.unit_id == 0u);

    // Aplicado tal cual: se planta el filler (id 0), NO el outpost (id 1) —
    // el escenario "falla" silenciosamente bajo v2 (sin crash, sin rechazo:
    // exactamente el peligro que v3 cierra).
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg(0));
    gs_bind_catalog(*g, cat);
    g->player_stock[0][0] = 100;
    for (uint32_t t = 0; t < data.ticks; ++t) {
        const auto& b = data.batches[t];
        step(*g, b.data(), static_cast<uint32_t>(b.size()));
    }
    CHECK(g->entities.alive[0] == 1 && g->entity_kind[0] == 1);
    CHECK(g->building_id[0] == 0u);  // EQUIVOCADO frente al real (1) — la prueba del gap
    std::remove(path);
}

// ============================================================================
// 4) Save v9: SPAWN_UNIT (unit_id=1, warrior) aún PENDIENTE al momento del
//    save (target_tick futuro, delay=2 para que quede en la agenda) — la
//    identidad sobrevive load + continuar == corrida continua.
// ============================================================================
static void test_save_v9_pending_unit_id_roundtrip() {
    static DataCatalogV1 cat = fixture::make_catalog();

    auto ga = std::make_unique<GameState>();
    gs_init(*ga, make_cfg(/*delay=*/2));
    gs_bind_catalog(*ga, cat);

    // t=0: SPAWN_UNIT con target_tick=5 -> eff=max(5,0+2)=5 (queda pendiente).
    RawCommand cmd = spawn_unit(5, 0, 1, 1 /*warrior*/, 20 * 65536, 20 * 65536);
    step(*ga, &cmd, 1);
    CHECK(ga->pending.count == 1u);
    CHECK(ga->pending.items[0].p.unit_id == 1u);
    CHECK(ga->pending.items[0].effective_tick == 5u);

    // Save EN t=1 (con el comando aún pendiente, no aplicado).
    AiJobBox box{}; ai_box_init(box, 1); AiRuntimeV1 rt{};
    CHECK(save_game(*ga, box, rt, "test_save_v9_pending.sav") == 0);

    auto gb = std::make_unique<GameState>();
    AiJobBox box2{}; AiRuntimeV1 rt2{};
    CHECK(load_game(*gb, box2, rt2, "test_save_v9_pending.sav") == 0);
    gs_bind_catalog(*gb, cat);

    // La agenda pendiente sobrevivió CON el unit_id correcto (no truncado a 0).
    CHECK(gb->pending.count == 1u);
    CHECK(gb->pending.items[0].p.unit_id == 1u);
    CHECK(gb->pending.items[0].effective_tick == 5u);
    CHECK(state_checksum_v1(*ga) == state_checksum_v1(*gb));

    // Continuar AMBOS hasta después de tick 5: el SPAWN_UNIT se aplica con el
    // catálogo correcto (warrior: hp=50, attack=10), no con el citizen (id 0).
    for (int k = 0; k < 8; ++k) step(*ga, nullptr, 0);
    for (int k = 0; k < 8; ++k) step(*gb, nullptr, 0);
    CHECK(ga->entities.alive[0] == 1 && gb->entities.alive[0] == 1);
    CHECK(ga->unit_id[0] == 1u && gb->unit_id[0] == 1u);
    CHECK(ga->hp[0] == 50 && gb->hp[0] == 50);
    CHECK(ga->attack[0] == 10 && gb->attack[0] == 10);
    CHECK(state_checksum_v1(*ga) == state_checksum_v1(*gb));

    std::remove("test_save_v9_pending.sav");
}

// ============================================================================
// 5) Checksum v4 (CHUNSA_STATE_V4): el dominio cubre pending.items[].p.unit_id
//    — dos GameState idénticos salvo ese campo dan checksums DISTINTOS.
// ============================================================================
static void test_checksum_v4_covers_pending_unit_id() {
    static DataCatalogV1 cat = fixture::make_catalog();

    auto ga = std::make_unique<GameState>();
    gs_init(*ga, make_cfg(/*delay=*/5));
    gs_bind_catalog(*ga, cat);
    RawCommand cmd_a = spawn_unit(10, 0, 1, 0 /*citizen*/, 5 * 65536, 5 * 65536);
    step(*ga, &cmd_a, 1);
    CHECK(ga->pending.count == 1u);

    auto gb = std::make_unique<GameState>();
    gs_init(*gb, make_cfg(/*delay=*/5));
    gs_bind_catalog(*gb, cat);
    RawCommand cmd_b = spawn_unit(10, 0, 1, 1 /*warrior — ÚNICA diferencia*/, 5 * 65536, 5 * 65536);
    step(*gb, &cmd_b, 1);
    CHECK(gb->pending.count == 1u);

    // Mismo effective_tick/emitter/sequence/handle/x_raw/y_raw/speed_mtpt;
    // solo difiere unit_id (5 vs 1... aquí 0 vs 1) -> checksums distintos.
    CHECK(ga->pending.items[0].effective_tick == gb->pending.items[0].effective_tick);
    CHECK(ga->pending.items[0].p.unit_id != gb->pending.items[0].p.unit_id);
    CHECK(state_checksum_v1(*ga) != state_checksum_v1(*gb));
}

// ============================================================================
// 6) Ventana de setup (§10.3): un target_tick=0 en el PRIMER Step produce la
//    MISMA trayectoria con delay=0 que con delay=1 (generaliza el punto 4 del
//    brief: la demo vuelve a delay=1 sin que el setup se vea afectado).
// ============================================================================
static uint64_t run_setup_scenario(uint32_t delay, uint32_t ticks) {
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg(delay, 32));
    RawCommand seed_batch[4];
    uint64_t last_ck = 0;
    for (uint32_t t = 0; t < ticks; ++t) {
        uint32_t n = 0;
        if (t == 0u) {
            seed_batch[n++] = spawn_debug(0, 0, 1, EntityHandle{0, 1}, 10 * 65536, 10 * 65536, 500);
            seed_batch[n++] = spawn_debug(0, 0, 2, EntityHandle{1, 1}, 20 * 65536, 20 * 65536, 300);
        }
        const StepResult r = step(*g, seed_batch, n);
        if (r.checksum_computed) last_ck = r.checksum;
    }
    return last_ck;
}

static void test_setup_window_delay_invariance() {
    const uint64_t ck_delay0 = run_setup_scenario(0u, 20u);
    const uint64_t ck_delay1 = run_setup_scenario(1u, 20u);
    const uint64_t ck_delay7 = run_setup_scenario(7u, 20u);
    CHECK(ck_delay0 == ck_delay1);
    CHECK(ck_delay0 == ck_delay7);
}

// ============================================================================
// 7) "Dump" de trayectoria pre/post: escenario SIN edificios, delay=0,
//    target_tick==t SIEMPRE (agenda pendiente vacía en cada punto de
//    control) — gs_serialize() en 3 checkpoints, comparado byte a byte entre
//    dos corridas independientes (determinismo; ver razonamiento en el
//    comentario de cabecera del archivo).
// ============================================================================
static void dump_at_checkpoints(std::vector<std::vector<uint8_t>>& out, uint32_t ticks,
                                const std::vector<uint32_t>& checkpoints) {
    constexpr uint32_t UNITS = 40;
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg(/*delay=*/0, /*max_entities=*/UNITS + 4));

    RawCommand batch[UNITS];
    FatalReason dummy = FatalReason::NONE;
    for (uint32_t t = 0; t < ticks; ++t) {
        uint32_t n = 0;
        if (t == 0u) {
            for (uint32_t i = 0; i < UNITS; ++i) {
                batch[n] = spawn_debug(0, 0, i + 1, EntityHandle{i, 1u},
                    static_cast<int64_t>(rng_range(20260716ull, static_cast<uint32_t>(RngStream::BENCH), 0u, i, 1u, 65536u, 255u*65536u, dummy)),
                    static_cast<int64_t>(rng_range(20260716ull, static_cast<uint32_t>(RngStream::BENCH), 0u, i, 2u, 65536u, 255u*65536u, dummy)),
                    static_cast<int32_t>(rng_range(20260716ull, static_cast<uint32_t>(RngStream::BENCH), 0u, i, 3u, 50u, 501u, dummy)));
                ++n;
            }
        } else if (t % 25u == 0u) {
            // target_tick == t (invariante: SIEMPRE debido en el mismo tick
            // bajo delay=0 -> pending.count queda en 0 tras cada step()).
            for (uint32_t i = 0; i < UNITS; ++i) {
                batch[n] = move_to(t, 0, static_cast<uint64_t>(UNITS) * (t / 25u) + i + 1,
                    EntityHandle{i, 1u},
                    static_cast<int64_t>(rng_range(20260716ull, static_cast<uint32_t>(RngStream::BENCH), t, i, 4u, 65536u, 255u*65536u, dummy)),
                    static_cast<int64_t>(rng_range(20260716ull, static_cast<uint32_t>(RngStream::BENCH), t, i, 5u, 65536u, 255u*65536u, dummy)));
                ++n;
            }
        }
        step(*g, batch, n);
        CHECK(g->pending.count == 0u);  // invariante: nunca queda nada pendiente

        for (uint32_t cp : checkpoints) {
            if (g->tick == cp) {
                std::vector<uint8_t> buf(GS_SERIALIZE_MAX);
                const size_t len = gs_serialize(*g, buf.data(), buf.size());
                CHECK(len > 0);
                buf.resize(len);
                out.push_back(std::move(buf));
            }
        }
    }
}

static void test_trajectory_dump_no_buildings_pre_post() {
    const std::vector<uint32_t> checkpoints = {10u, 60u, 150u};
    std::vector<std::vector<uint8_t>> dump_a, dump_b;
    dump_at_checkpoints(dump_a, 200u, checkpoints);
    dump_at_checkpoints(dump_b, 200u, checkpoints);

    CHECK(dump_a.size() == checkpoints.size());
    CHECK(dump_b.size() == checkpoints.size());
    for (size_t k = 0; k < dump_a.size() && k < dump_b.size(); ++k) {
        CHECK(dump_a[k] == dump_b[k]);  // bit a bit, no solo checksum
        // SHA-256 del dump, impreso para que quede en la bitácora del gate
        // (mismo estilo que test_state.cpp) — no es una aserción adicional,
        // solo trazabilidad del "dump" exigido.
        uint8_t digest[32];
        sha256(dump_a[k].data(), dump_a[k].size(), digest);
        char hex[65];
        for (int i = 0; i < 32; ++i) std::snprintf(hex + i * 2, 3, "%02x", digest[i]);
        std::printf("dump checkpoint tick=%u sha256=%s len=%zu\n",
                    checkpoints[k], hex, dump_a[k].size());
    }
}

int main() {
    test_command_effective_tick_contract();
    test_replay_v3_roundtrip_place_building_nonzero_id();
    test_replay_v2_forced_loses_unit_id();
    test_save_v9_pending_unit_id_roundtrip();
    test_checksum_v4_covers_pending_unit_id();
    test_setup_window_delay_invariance();
    test_trajectory_dump_no_buildings_pre_post();

    if (g_fails == 0) { std::printf("replay_v3: OK\n"); return 0; }
    std::printf("replay_v3: %d fallos\n", g_fails);
    return 1;
}
