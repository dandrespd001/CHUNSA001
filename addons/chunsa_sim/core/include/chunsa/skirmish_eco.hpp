#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

#include "chunsa/driver.hpp"   // step/ai_stub/save_io/replay + continuation_checksum (reutilizado, no duplicado)

// chunsa — escenario CLI de skirmish CON ECONOMÍA (Sprint 1.4 K3, SPEC-004
// §7.1: la enmienda "aldeanos vulnerables"). Autor: sonnet-5 (brief
// docs/briefs/SONNET_K3_ALDEANOS_VULNERABLES_SPRINT_1.4.md).
//
// Archivo NUEVO, separado de skirmish.hpp (append-only, K2 intacto como
// regresión — ver test_ai_skirmish.cpp, que sigue pasando sin tocar una
// línea). Este escenario existe para demostrar exactamente lo que la
// enmienda §7.1 habilita: una partida CON economía real (aldeanos vivos,
// recolectando) que SÍ termina por conquista, porque ahora el aldeano
// también es un objetivo válido de combate — antes de K3 esto era
// estructuralmente imposible (ver la nota de diseño de skirmish.hpp: un
// jugador con un aldeano vivo, intocable, jamás podía ser declarado
// derrotado).
//
// Mismo esqueleto que skirmish.hpp (humano-scripted owner=0 "defensor" vs IA
// real owner=1 "atacante", asimetría deliberada para que el desenlace nunca
// sea empate), con dos diferencias:
//   1. El catálogo añade un tipo "citizen" (unit_class=Citizen) además del
//      soldado — el defensor recibe, en su batch de setup de t==0, tanto
//      soldados de guarnición como CIUDADANOS REALES que desde ese instante
//      corren en piloto automático via economy_system (SEEK/HARVEST/RETURN,
//      chunsa/economy.hpp) sin necesitar un solo comando más: es economía de
//      verdad, no un decorado.
//   2. La base del defensor se ancla EXACTAMENTE en el tile de su dropoff
//      fijo (gs_init_economy: dropoff_x/y[owner] = (20+28·owner, 128)) —
//      ver la nota geométrica más abajo (SKIRMISH_ECO_DEFENDER_TX/TY) sobre
//      por qué esto es lo que GARANTIZA que la partida concluya dentro del
//      presupuesto de ticks, no una casualidad de balance.
//
// El atacante (owner=1) es puramente militar, igual que en skirmish.hpp — su
// catálogo de edificios NUNCA lista un "trainer" de ciudadanos, así que la
// capa económica de ai_execute se salta con gracia (utilidad 0) exactamente
// como documenta skirmish.hpp; no se reintroduce riesgo en la IA ya probada
// en K2.

namespace chunsa {

namespace skirmish_eco_detail {

inline UnitDefinitionV1 make_soldier() {
    UnitDefinitionV1 d{};
    d.id = 0; d.unit_class = UnitClassV1::Infantry; d.tags_mask = 0;
    d.hp = 60; d.attack = 8; d.range_millitiles = 1000;  // 1 tile
    d.speed_millitile_tick = 150; d.morale = 100; d.build_time_ticks = 100;
    for (int k = 0; k < 6; ++k) d.bonus_vs_bp[k] = 0;
    d.cost_a = 0; d.cost_b = 0; d.cost_me = 0; d.pop_cost = 1;
    d.epoch_min = 1; d.epoch_max = 15;
    return d;
}

// Aldeano económico real: hp bajo (vulnerable — SPEC-004 §7.1), attack=0 (no
// combate, guard de atacante intacto), velocidad razonable para que el ciclo
// SEEK/HARVEST/RETURN completo quepa muchas veces en el presupuesto de
// ticks del gate. No se entrena vía producción en este escenario (SPAWN_
// CITIZEN directo en el batch de t==0, mismo patrón que test_ai_layers.cpp)
// — cost_a/pop_cost son valores de catálogo válidos pero irrelevantes aquí.
inline UnitDefinitionV1 make_citizen() {
    UnitDefinitionV1 d{};
    d.id = 1; d.unit_class = UnitClassV1::Citizen; d.tags_mask = 0;
    d.hp = 20; d.attack = 0; d.range_millitiles = 0;
    d.speed_millitile_tick = 200; d.morale = 100; d.build_time_ticks = 1;
    for (int k = 0; k < 6; ++k) d.bonus_vs_bp[k] = 0;
    d.cost_a = 5; d.cost_b = 0; d.cost_me = 0; d.pop_cost = 1;
    d.epoch_min = 1; d.epoch_max = 15;
    return d;
}

inline BuildingDefinitionV1 make_center() {
    BuildingDefinitionV1 d{};
    d.id = 0; d.hp = 400; d.footprint_w = 2; d.footprint_h = 2;
    d.build_time_ticks = 0;  // nace completo (pre-colocado por escenario)
    d.cost_a = 0; d.cost_b = 0; d.cost_me = 0;
    // dropoff_mask=0 a propósito (igual que skirmish.hpp): el fallback
    // legacy dropoff_x/y[owner] de gs_init_economy es EXACTAMENTE el punto
    // geométrico que este escenario usa para anclar la base del defensor
    // (ver nota más abajo) — un building-dropoff real sería redundante.
    d.dropoff_mask = 0; d.constructible = 0;
    d.epoch_min = 1; d.epoch_max = 15;
    d.trains[0] = 0 /*soldier*/; d.train_count = 1;
    for (uint32_t k = 1; k < PROD_TRAINS_MAX; ++k) d.trains[k] = INVALID_UNIT_ID;
    for (uint32_t k = 0; k < PROD_TECHS_MAX; ++k) d.researches[k] = INVALID_TECH_ID;
    d.research_count = 0;
    for (uint32_t k = 0; k < BUILDING_REQCAP_MAX; ++k) d.required_capabilities[k] = INVALID_CAPABILITY_ID;
    d.required_capabilities_count = 0;
    return d;
}

// Mismo perfil literal que skirmish.hpp (base:demo_normal, SPEC-005 §3).
inline AiProfileV1 make_profile() {
    return AiProfileV1{
        0u,
        5000, 5000, 5000, 5000, 5000,
        20u, 4u,
        2500, 3000,
    };
}

inline constexpr char kProfileName[] = "base:demo_normal";

inline UnitDefinitionV1 g_units[2] = { make_soldier(), make_citizen() };
inline BuildingDefinitionV1 g_buildings[1] = { make_center() };
inline AiProfileV1 g_profiles[1] = { make_profile() };
inline AiProfileNameIndexV1 g_profile_names[1] = {
    AiProfileNameIndexV1{kProfileName, static_cast<uint16_t>(sizeof(kProfileName) - 1), 0u},
};

}  // namespace skirmish_eco_detail

inline DataCatalogV1 skirmish_eco_make_catalog() noexcept {
    DataCatalogV1 c{};
    c.unit_count = 2; c.units = skirmish_eco_detail::g_units; c.unit_names = nullptr;
    c.building_count = 1; c.buildings = skirmish_eco_detail::g_buildings; c.building_names = nullptr;
    c.tech_count = 0; c.techs = nullptr; c.tech_names = nullptr;
    c.capability_count = 0; c.capability_names = nullptr;
    c.ai_profile_count = 1; c.ai_profiles = skirmish_eco_detail::g_profiles;
    c.ai_profile_names = skirmish_eco_detail::g_profile_names;
    return c;
}

// Posiciones fijas (tiles). Separación de 160 tiles (misma que skirmish.hpp)
// para dar a la IA atacante tiempo real de acumular ejército antes del
// choque — proporción ya validada en el gate de K2.
//
// GEOMETRÍA DELIBERADA (no un detalle de balance, es lo que GARANTIZA que la
// partida concluya): la base del defensor se ancla en el MISMO tile que su
// dropoff fijo, dropoff_x/y[owner=0] de gs_init_economy = (20 + 28·0, 128) =
// (20, 128) — ver game_state.hpp::gs_init_economy. Con footprint 2×2 la
// posición real de la entidad-edificio queda en (21,129) (centro del
// footprint, SPEC-004 §3), a ~0.7 tile del dropoff: dentro del radio de
// llegada de economía (ECO_ARRIVE_RADIUS_RAW = 1 tile). Consecuencia: TODA
// vez que un ciudadano completa su tramo RETURN (economy.hpp), termina, por
// construcción, a un paso de la propia base — sin importar cuánto se haya
// alejado en su tramo SEEK/HARVEST (los depósitos fijos están a ~90 tiles,
// vértices del mapa). Cuando el ejército atacante destruye el centro y se
// queda plantado en las ruinas (aggro_system reintenta objetivo cada tick
// mientras esté ocioso), el regreso del último ciudadano lo trae de vuelta
// exactamente a esa posición — dentro de rango de combate/aggro de
// cualquier soldado atacante allí apostado — así que la derrota (0
// edificios Y 0 ciudadanos, SPEC-005 §6) se alcanza en, como mucho, un ciclo
// económico completo (~2000-3000 ticks a esta velocidad) después de la
// caída del centro, con margen amplio bajo el límite de 36000 del gate.
inline constexpr int64_t SKIRMISH_ECO_DEFENDER_TX = 20;
inline constexpr int64_t SKIRMISH_ECO_DEFENDER_TY = 128;
inline constexpr int64_t SKIRMISH_ECO_ATTACKER_TX = 180;
inline constexpr int64_t SKIRMISH_ECO_ATTACKER_TY = 128;

// Batch de escenario, SOLO en t==0 (exención de setup, SPEC-004 §10.3):
// mismo contrato que build_skirmish_batch de skirmish.hpp, con el añadido de
// `defender_citizens` (SPAWN_CITIZEN, unit_id=1) junto a los soldados de
// guarnición del defensor. Función PURA de (t, ...) — reconstruible tras un
// load.
inline uint32_t build_skirmish_eco_batch(std::vector<RawCommand>& batch, uint32_t t,
                                         uint32_t defender_soldiers, uint32_t defender_citizens,
                                         uint32_t attacker_soldiers) noexcept {
    if (t != 0u) return 0u;
    uint32_t n = 0;
    uint64_t seq0 = 0, seq1 = 0;

    RawCommand& center0 = batch[n++];
    std::memset(&center0, 0, sizeof(RawCommand));
    center0.target_tick = 0; center0.emitter = 0; center0.type = CommandType::PLACE_BUILDING;
    center0.sequence = ++seq0; center0.p.unit_id = 0 /*center*/;
    center0.p.x_raw = SKIRMISH_ECO_DEFENDER_TX; center0.p.y_raw = SKIRMISH_ECO_DEFENDER_TY;

    for (uint32_t i = 0; i < defender_soldiers; ++i) {
        RawCommand& c = batch[n++];
        std::memset(&c, 0, sizeof(RawCommand));
        c.target_tick = 0; c.emitter = 0; c.type = CommandType::SPAWN_UNIT;
        c.sequence = ++seq0; c.p.unit_id = 0 /*soldier*/;
        c.p.x_raw = (SKIRMISH_ECO_DEFENDER_TX + 4 + static_cast<int64_t>(i)) * FX_ONE_RAW;
        c.p.y_raw = SKIRMISH_ECO_DEFENDER_TY * FX_ONE_RAW;
    }

    // Ciudadanos del defensor: fila separada (TY+3) para no co-ubicarse con
    // los soldados de guarnición; puramente cosmético (economy_system los
    // moverá de inmediato).
    for (uint32_t i = 0; i < defender_citizens; ++i) {
        RawCommand& c = batch[n++];
        std::memset(&c, 0, sizeof(RawCommand));
        c.target_tick = 0; c.emitter = 0; c.type = CommandType::SPAWN_CITIZEN;
        c.sequence = ++seq0; c.p.unit_id = 1 /*citizen*/;
        c.p.x_raw = (SKIRMISH_ECO_DEFENDER_TX + 2 + static_cast<int64_t>(i)) * FX_ONE_RAW;
        c.p.y_raw = (SKIRMISH_ECO_DEFENDER_TY + 3) * FX_ONE_RAW;
    }

    RawCommand& center1 = batch[n++];
    std::memset(&center1, 0, sizeof(RawCommand));
    center1.target_tick = 0; center1.emitter = 1; center1.type = CommandType::PLACE_BUILDING;
    center1.sequence = ++seq1; center1.p.unit_id = 0 /*center*/;
    center1.p.x_raw = SKIRMISH_ECO_ATTACKER_TX; center1.p.y_raw = SKIRMISH_ECO_ATTACKER_TY;

    for (uint32_t i = 0; i < attacker_soldiers; ++i) {
        RawCommand& c = batch[n++];
        std::memset(&c, 0, sizeof(RawCommand));
        c.target_tick = 0; c.emitter = 1; c.type = CommandType::SPAWN_UNIT;
        c.sequence = ++seq1; c.p.unit_id = 0 /*soldier*/;
        c.p.x_raw = (SKIRMISH_ECO_ATTACKER_TX - 4 - static_cast<int64_t>(i)) * FX_ONE_RAW;
        c.p.y_raw = SKIRMISH_ECO_ATTACKER_TY * FX_ONE_RAW;
    }
    return n;
}

struct SkirmishEcoOpts {
    uint32_t ticks = 36000;              // límite duro del gate §8.3/K3 (30 min @ 20 Hz)
    uint64_t seed = 20260724ull;
    uint32_t defender_soldiers = 4;
    uint32_t defender_citizens = 3;
    uint32_t attacker_soldiers = 6;
    // Save-at (mismo patrón que SkirmishOpts): si save_path != nullptr, guarda
    // cuando gs.tick == save_at.
    uint32_t save_at = 0;
    const char* save_path = nullptr;
    // Modo replay-feed (mismo contrato que SkirmishOpts::feed): IA JAMÁS se
    // ejecuta; alimenta los batches grabados.
    const ReplayData* feed = nullptr;
    ReplayWriter* rec = nullptr;
};

struct SkirmishEcoOut {
    uint64_t    final_checksum = 0;
    uint64_t    continuation_checksum = 0;
    FatalReason fatal = FatalReason::NONE;
    uint64_t    accepted = 0, rejected = 0;
    uint32_t    ai_executions = 0;
    uint32_t    schedule_mismatches = 0;
    uint32_t    end_tick = 0;      // gs.tick cuando termina el bucle (game_over==1 o límite)
    uint8_t     game_over = 0;
    uint8_t     winner = 0xFFu;
    int         save_result = -1;
};

// Bucle principal — mismo esqueleto que skirmish.hpp::drive_skirmish,
// especializado a este escenario (NO reemplaza ni modifica drive_skirmish
// original). Termina en cuanto gs.game_over==1 (o al alcanzar o.ticks,
// o.fatal!=NONE).
inline int drive_skirmish_eco(const SkirmishEcoOpts& o, GameState& gs, AiJobBox& box, AiRuntimeV1& rt,
                              SkirmishEcoOut& out) {
    std::vector<RawCommand> batch(
        static_cast<size_t>(o.defender_soldiers) + o.defender_citizens
            + o.attacker_soldiers + 2 + AI_MAX_COMMANDS);

    while (gs.tick < o.ticks && gs.game_over == 0u && gs.fatal == FatalReason::NONE) {
        const uint32_t t = gs.tick;

        if (o.save_path != nullptr && t == o.save_at) {
            out.save_result = save_game(gs, box, rt, o.save_path);
            if (out.save_result != 0) return 2;
        }

        uint32_t n = 0;
        if (o.feed != nullptr) {
            // Replay-feed: IA JAMÁS se ejecuta (mismo contrato que drive()/G5).
            if (t < o.feed->batches.size()) {
                const auto& b = o.feed->batches[t];
                const bool has_agenda = (o.feed->version >= 2u)
                                     && (t < o.feed->eff_ticks.size())
                                     && (o.feed->eff_ticks[t].size() == b.size());
                for (uint32_t i = 0; i < b.size(); ++i) {
                    const RawCommand& c = b[i];
                    if (has_agenda) {
                        const uint32_t recomputed = command_effective_tick(
                                c.target_tick, t, gs.cfg.human_input_delay_ticks);
                        if (recomputed != o.feed->eff_ticks[t][i]) ++out.schedule_mismatches;
                    }
                    batch[n++] = c;
                }
            }
        } else {
            n = build_skirmish_eco_batch(batch, t, o.defender_soldiers, o.defender_citizens,
                                         o.attacker_soldiers);
            // Pump de la IA única (owner=1) — mismo lifecycle 0.1B, sin cambios.
            if (ai_should_dispatch(box, t)) ai_dispatch(box, t, rt);
            if (box.state == AiJobState::DISPATCHED) {
                ai_execute(box, gs);
                ++out.ai_executions;
            }
            if (ai_stalled(box, t)) {
                ai_execute(box, gs);
                ++out.ai_executions;
            }
            if (ai_due(box, t)) {
                for (uint32_t k = 0; k < box.result_count && n < batch.size(); ++k)
                    batch[n++] = box.result[k];
                ai_commit(box, rt);
            }
        }

        if (o.rec != nullptr) o.rec->tick_batch(batch.data(), n, t);

        const StepResult res = step(gs, batch.data(), n);
        if (res.checksum_computed) out.final_checksum = res.checksum;
        out.accepted += res.accepted;
        out.rejected += res.rejected;
    }

    out.fatal = gs.fatal;
    out.end_tick = gs.tick;
    out.game_over = gs.game_over;
    out.winner = gs.winner;
    out.continuation_checksum = continuation_checksum(gs, box, rt);
    return gs.fatal == FatalReason::NONE ? 0 : 3;
}

// Conveniencia: partida nueva (heap) + drive_skirmish_eco + limpieza. El
// catálogo es un `static` de función (duración de programa) — sobrevive
// holgadamente al GameState heap-allocado, que se libera al final.
inline int drive_skirmish_eco_fresh(const SkirmishEcoOpts& o, SkirmishEcoOut& out) {
    MatchConfig01A cfg{};
    cfg.max_entities = 512;
    cfg.player_count = 2;
    cfg.human_input_delay_ticks = 0;
    cfg.max_future_command_ticks = 20;
    cfg.checksum_every_ticks = 1;
    cfg.map_tiles_x = 256; cfg.map_tiles_y = 256;
    cfg.seed = o.seed;
    if (!config_validate(cfg)) return 2;

    static const DataCatalogV1 cat = skirmish_eco_make_catalog();

    GameState* gs = new GameState();
    gs_init(*gs, cfg);
    gs_bind_catalog(*gs, cat);
    gs_init_epoch_from_catalog(*gs);

    AiJobBox box; ai_box_init(box, 1);
    AiRuntimeV1 rt{0u, static_cast<uint64_t>(o.attacker_soldiers + 1u)};  // seq tras el setup del atacante
    const int code = drive_skirmish_eco(o, *gs, box, rt, out);
    delete gs;
    return code;
}

}  // namespace chunsa
