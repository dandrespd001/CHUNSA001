#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

#include "chunsa/driver.hpp"   // step/ai_stub/save_io/replay + continuation_checksum (reutilizado, no duplicado)

// chunsa — escenario CLI de la APERTURA ECONÓMICA (Sprint 1.6B, pieza K2,
// SPEC-004 Parte III §20, el DoD del sprint). Autor: sonnet-5 (brief
// docs/briefs/SONNET_K2_GATHER_APERTURA_SPRINT_1.6B.md).
//
// Archivo NUEVO, separado de skirmish.hpp/skirmish_eco.hpp (append-only, K2
// de Sprint 1.4 y K3 de Sprint 1.4-cierre intactos como regresión). A
// diferencia de esos dos (catálogo SINTÉTICO embebido en C++), este
// escenario usa el CATÁLOGO REAL compilado del repo (data/compiled/
// chunsa_base.chdb): las DOS civilizaciones reales (egipto/rome), sus
// centros/aldeanos reales, y los 12 `resource_spawns` reales del mapa
// (SPEC-004 §15.1) vía `gs_init_economy_from_catalog` — es el PUNTO del
// sprint: nadie la había llamado desde un escenario hasta ahora (K1 la dejó
// opt-in). La resolución de ids (civ/building/unit) es TOTALMENTE dinámica
// (catalog_find_*): este header NO hardcodea ningún índice numérico del
// blob — sobrevive a cualquier reordenamiento futuro de los records.
//
// Asimetría deliberada y documentada (mismo precedente que skirmish.hpp/
// skirmish_eco.hpp): el jugador 0 (egipto) es "humano-scripted" — SOLO
// recibe su batch de setup en t==0 (centro + 3 aldeanos) y jamás vuelve a
// emitir un comando; su economía SÍ corre sola (economy_system no filtra por
// owner, SPEC-004 §3.4 base). El jugador 1 (rome) es la IA de 3 capas real de
// ai_stub.hpp — sin ninguna trampa, sin ejército/edificios militares
// inyectados: arranca con EXACTAMENTE lo mismo que egipto (centro + 3
// aldeanos) y debe recorrer sola recolectar -> construir (castra_barracks) ->
// entrenar (legionary) -> atacar -> derrotar a egipto. Elegir a Rome como
// atacante (y no egipto) es deliberado y documentado: con los datos reales
// del repo, TODO el contenido de Rome (forum_center/castra_barracks/
// legionary/ballista_crew/camp_work_crew) vive en una ÚNICA época (5), así
// que la IA nunca necesita depender de EPOCH_UP (fuera del alcance de este
// escenario) para desbloquear su cuartel — egipto, en cambio, tiene
// chariotry_stable en epoch_window [3,4] pero chariot_warrior exige epoch 4
// exacto (mientras que la época inicial de egipto es 3, el mínimo de SU
// catálogo): jugable pero exige que la IA suba de época sola primero. Ambos
// caminos son válidos por diseño de la IA v1 (§4.1 tech/época), pero elegir
// a Rome como el actor que demuestra el camino completo mantiene el
// escenario del DoD enfocado en §18/§19 (GATHER + capa económica) sin
// entrelazarlo con la mecánica de épocas de SPEC-004 §12 (fuera del alcance
// de este brief).
//
// Nota de diseño (misma que skirmish_eco.hpp): SIN ciudadanos vivos en
// NINGÚN bando la derrota (SPEC-005 §6) sería inalcanzable estructuralmente
// — aquí AMBOS bandos tienen ciudadanos reales desde el tick 0 (a diferencia
// de skirmish.hpp), así que la condición de derrota de egipto (SPEC-004
// §7.1: ciudadanos SÍ son objetivo válido) puede darse limpiamente cuando el
// ejército de Rome destruye el centro Y los 3 aldeanos egipcios.

namespace chunsa {

// ---------------------------------------------------------------------------
// Resolución dinámica de ids reales del catálogo (SPEC-002: record_id ->
// CivId/BuildingId/UnitId por búsqueda binaria en las tablas ya ordenadas).
// `ok=false` si CUALQUIERA de los 6 records esperados no existe en el
// catálogo enlazado — el caller debe abortar el escenario en ese caso (dato
// real ausente/renombrado, no un bug de este header).
// ---------------------------------------------------------------------------
struct SkirmishAperturaSetup {
    CivId      civ_egipto          = INVALID_CIV_ID;
    CivId      civ_rome            = INVALID_CIV_ID;
    BuildingId bld_egipto_center   = INVALID_BUILDING_ID;
    BuildingId bld_rome_center     = INVALID_BUILDING_ID;
    UnitId     unit_egipto_citizen = INVALID_UNIT_ID;
    UnitId     unit_rome_citizen   = INVALID_UNIT_ID;
    bool       ok = false;
};

inline SkirmishAperturaSetup skirmish_apertura_resolve(const DataCatalogV1& cat) noexcept {
    SkirmishAperturaSetup s{};
    static constexpr char kEgipto[]        = "egipto:dynastic_nile";
    static constexpr char kRome[]          = "rome:republic_imperial";
    static constexpr char kEgiptoCenter[]  = "egipto:settlement_center";
    static constexpr char kRomeCenter[]    = "rome:forum_center";
    static constexpr char kEgiptoCitizen[] = "egipto:work_crew";
    static constexpr char kRomeCitizen[]   = "rome:camp_work_crew";
    s.civ_egipto          = catalog_find_civ(cat, kEgipto, sizeof(kEgipto) - 1);
    s.civ_rome            = catalog_find_civ(cat, kRome, sizeof(kRome) - 1);
    s.bld_egipto_center   = catalog_find_building(cat, kEgiptoCenter, sizeof(kEgiptoCenter) - 1);
    s.bld_rome_center     = catalog_find_building(cat, kRomeCenter, sizeof(kRomeCenter) - 1);
    s.unit_egipto_citizen = catalog_find_unit(cat, kEgiptoCitizen, sizeof(kEgiptoCitizen) - 1);
    s.unit_rome_citizen   = catalog_find_unit(cat, kRomeCitizen, sizeof(kRomeCitizen) - 1);
    s.ok = (s.civ_egipto != INVALID_CIV_ID) && (s.civ_rome != INVALID_CIV_ID)
        && (s.bld_egipto_center != INVALID_BUILDING_ID) && (s.bld_rome_center != INVALID_BUILDING_ID)
        && (s.unit_egipto_citizen != INVALID_UNIT_ID) && (s.unit_rome_citizen != INVALID_UNIT_ID);
    return s;
}

// Anclas reales del mapa base:demo_desert_basin (SPEC-004 §15.1):
// starting_positions slot0=20500/128500 mt, slot1=235500/128500 mt.
// Footprint del centro = 3x3 en AMBAS civs (egipto:settlement_center /
// rome:forum_center) — anchor_tile = (start_mt - mitad_footprint_mt)/1000,
// mitad_footprint = 1500mt (mitad de 3 tiles): coloca el CENTRO GEOMÉTRICO
// del edificio (SPEC-004 §3: anchor*T + (footprint*T)/2) EXACTAMENTE sobre
// el starting_position del mapa, no una aproximación.
inline constexpr int64_t APERTURA_EGIPTO_ANCHOR_TX = 19;
inline constexpr int64_t APERTURA_EGIPTO_ANCHOR_TY = 127;
inline constexpr int64_t APERTURA_ROME_ANCHOR_TX   = 234;
inline constexpr int64_t APERTURA_ROME_ANCHOR_TY   = 127;

// Batch de escenario, SOLO en t==0 (exención de setup, SPEC-004 §10.3):
// centro + 3 aldeanos por jugador, NADA MÁS (cero ejército, cero edificios
// militares — el DoD exige que la IA los alcance sola). Función PURA de
// (t, setup) — reconstruible tras un load.
inline uint32_t build_apertura_batch(std::vector<RawCommand>& batch, uint32_t t,
                                     const SkirmishAperturaSetup& setup) noexcept {
    if (t != 0u) return 0u;
    uint32_t n = 0;
    uint64_t seq0 = 0, seq1 = 0;
    const int64_t T = FX_ONE_RAW;

    RawCommand& center0 = batch[n++];
    std::memset(&center0, 0, sizeof(RawCommand));
    center0.target_tick = 0; center0.emitter = 0; center0.type = CommandType::PLACE_BUILDING;
    center0.sequence = ++seq0; center0.p.unit_id = setup.bld_egipto_center;
    center0.p.x_raw = APERTURA_EGIPTO_ANCHOR_TX; center0.p.y_raw = APERTURA_EGIPTO_ANCHOR_TY;

    for (uint32_t i = 0; i < 3; ++i) {
        RawCommand& c = batch[n++];
        std::memset(&c, 0, sizeof(RawCommand));
        c.target_tick = 0; c.emitter = 0; c.type = CommandType::SPAWN_UNIT;
        c.sequence = ++seq0; c.p.unit_id = setup.unit_egipto_citizen;
        c.p.x_raw = (APERTURA_EGIPTO_ANCHOR_TX + 3 + static_cast<int64_t>(i)) * T;
        c.p.y_raw = (APERTURA_EGIPTO_ANCHOR_TY + 3) * T;
    }

    RawCommand& center1 = batch[n++];
    std::memset(&center1, 0, sizeof(RawCommand));
    center1.target_tick = 0; center1.emitter = 1; center1.type = CommandType::PLACE_BUILDING;
    center1.sequence = ++seq1; center1.p.unit_id = setup.bld_rome_center;
    center1.p.x_raw = APERTURA_ROME_ANCHOR_TX; center1.p.y_raw = APERTURA_ROME_ANCHOR_TY;

    for (uint32_t i = 0; i < 3; ++i) {
        RawCommand& c = batch[n++];
        std::memset(&c, 0, sizeof(RawCommand));
        c.target_tick = 0; c.emitter = 1; c.type = CommandType::SPAWN_UNIT;
        c.sequence = ++seq1; c.p.unit_id = setup.unit_rome_citizen;
        c.p.x_raw = (APERTURA_ROME_ANCHOR_TX + 3 + static_cast<int64_t>(i)) * T;
        c.p.y_raw = (APERTURA_ROME_ANCHOR_TY + 3) * T;
    }
    return n;
}

struct SkirmishAperturaOpts {
    uint32_t ticks = 36000;              // límite duro del DoD (30 min @ 20 Hz)
    // Save-at (mismo patrón que SkirmishOpts): si save_path != nullptr, guarda
    // cuando gs.tick == save_at.
    uint32_t save_at = 0;
    const char* save_path = nullptr;
    // Modo replay-feed (mismo contrato que SkirmishOpts::feed): IA JAMÁS se
    // ejecuta; alimenta los batches grabados.
    const ReplayData* feed = nullptr;
    ReplayWriter* rec = nullptr;
};

struct SkirmishAperturaOut {
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
    // Fases del DoD (SPEC-004 §20) — bookkeeping EXTERNO al kernel, solo para
    // reporte/verificación del gate: lee gs de solo lectura al final de cada
    // tick, NUNCA alimenta ninguna decisión de ai_execute/step (no rompe la
    // regla de oro, SPEC-005 §0 — no es parte del kernel determinista, es el
    // mismo tipo de instrumentación externa que ya hacen los printf de los
    // demás escenarios CLI).
    bool p0_resources_gathered = false;  // egipto: player_stock[0][*] > 0 en algún tick
    bool p1_resources_gathered = false;  // rome: player_stock[1][*] > 0 en algún tick
    bool p1_built_military = false;      // rome: un edificio != su centro, COMPLETO
    bool p1_trained_military = false;    // rome: alguna unidad de combate (unit_class<=2) viva
};

// Bucle principal (mismo esqueleto que skirmish.hpp::drive_skirmish/
// skirmish_eco.hpp::drive_skirmish_eco, especializado a la apertura real).
// Termina en cuanto gs.game_over==1 (o al alcanzar o.ticks, o.fatal!=NONE).
inline int drive_skirmish_apertura(const SkirmishAperturaOpts& o, GameState& gs,
                                   const SkirmishAperturaSetup& setup,
                                   AiJobBox& box, AiRuntimeV1& rt,
                                   SkirmishAperturaOut& out) {
    std::vector<RawCommand> batch(8 + AI_MAX_COMMANDS);

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
                // Auditoría multimodelo 2026-07-27, F-00: el loader admite hasta
                // MAX_PER_TICK (4096) comandos por tick (replay.hpp), muy por encima de la
                // cota de la IA con la que se dimensiona el buffer. Sin esta línea un replay
                // legal escribe fuera del heap. Amortizado O(1): los batches reales del
                // escenario son < 72 y jamás redimensionan, así que ninguna trayectoria
                // existente cambia.
                if (b.size() > batch.size()) batch.resize(b.size());
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
            n = build_apertura_batch(batch, t, setup);
            // Pump de la IA única (owner=1, rome) — mismo lifecycle 0.1B, sin cambios.
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

        // ---- Bookkeeping de fases del DoD (ver comentario de SkirmishAperturaOut) ----
        if (gs.player_stock[0][0] > 0 || gs.player_stock[0][1] > 0 || gs.player_stock[0][2] > 0) {
            out.p0_resources_gathered = true;
        }
        if (gs.player_stock[1][0] > 0 || gs.player_stock[1][1] > 0 || gs.player_stock[1][2] > 0) {
            out.p1_resources_gathered = true;
        }
        if (!out.p1_built_military && gs.catalog != nullptr) {
            for (uint32_t i = 0; i < gs.entities.capacity; ++i) {
                if (!gs.entities.alive[i] || gs.owner[i] != 1u || gs.entity_kind[i] != 1u) continue;
                if (gs.building_id[i] == setup.bld_rome_center) continue;  // el centro no es "militar"
                if (gs.building_id[i] >= gs.catalog->building_count) continue;
                const BuildingDefinitionV1& bdef = gs.catalog->buildings[gs.building_id[i]];
                if (gs.build_progress[i] >= bdef.build_time_ticks) { out.p1_built_military = true; break; }
            }
        }
        if (!out.p1_trained_military) {
            for (uint32_t i = 0; i < gs.entities.capacity; ++i) {
                if (gs.entities.alive[i] && gs.owner[i] == 1u && gs.unit_class[i] <= 2u) {
                    out.p1_trained_military = true;
                    break;
                }
            }
        }
    }

    out.fatal = gs.fatal;
    out.end_tick = gs.tick;
    out.game_over = gs.game_over;
    out.winner = gs.winner;
    out.continuation_checksum = continuation_checksum(gs, box, rt);
    return gs.fatal == FatalReason::NONE ? 0 : 3;
}

}  // namespace chunsa
