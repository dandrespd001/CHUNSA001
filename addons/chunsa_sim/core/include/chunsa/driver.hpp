#pragma once
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#include "chunsa/step.hpp"
#include "chunsa/ai_stub.hpp"
#include "chunsa/save_io.hpp"
#include "chunsa/replay.hpp"

// chunsa — driver del escenario synthetic_movement_v1@1 con IA opcional,
// recorder, save-at y modo replay-feed. Autor: Arquitecto (orquestación de
// gates G3/G4/G5, SPEC-001 §14). Capa CLI: STL permitida.
//
// Determinismo del driver: el batch humano es f(tick) pura (sequences
// reconstruibles tras un load); los comandos de IA salen del AiJobBox
// (state machine §7.3) y también son f(source_tick, runtime) puros.
// Nota 0.1B: el stub lee g "en vivo" al ejecutar; bajo el fixture (sin
// spawns/destroys dentro de la ventana de decisión) equivale al snapshot
// congelado. El AiWorldView real llega en 0.2 (ver reporte).

namespace chunsa {

inline constexpr uint32_t DRIVER_COORD_LO = 65536u;
inline constexpr uint32_t DRIVER_COORD_HI = 255u * 65536u;

struct DriveOpts {
    uint32_t units = 600;
    uint32_t ticks = 2000;              // tick final M (exclusive): corre hasta tick==M
    uint16_t checksum_every = 1;
    uint64_t seed = 20260716ull;
    bool with_ai = false;               // emisor 1 = IA stub
    // Config de normalización §6.2 (defaults históricos del fixture). En verify
    // se sobreescriben con los del replay v2 → reproducción auto-contenida.
    uint32_t human_input_delay_ticks = 1;
    uint32_t max_future_command_ticks = 20;
    // Save-at (G3/G4): si save_path != nullptr, guarda cuando gs.tick == save_at.
    uint32_t save_at = 0;
    const char* save_path = nullptr;
    // Política de job al guardar (G4): "natural" | "dispatched" (difiere el
    // execute hasta después del save, dejando el job DISPATCHED en el archivo).
    bool hold_dispatched_until_save = false;
    // Modo replay-feed (G5): alimenta los batches grabados; PROHIBIDO ejecutar IA.
    const ReplayData* feed = nullptr;
    ReplayWriter* rec = nullptr;        // recorder (graba el batch + agenda por tick)
};

// Nº de comandos cuya agenda grabada (v2) NO coincide con la recomputada bajo
// la config §6.2 del propio replay. > 0 ⇒ archivo corrupto o regla de
// normalización cambiada tras grabar. Ver DriveOut::schedule_mismatches.

struct DriveOut {
    uint64_t final_checksum = 0;        // último checksum de estado computado
    uint64_t continuation_checksum = 0; // estado + caja de IA (dominio continuation)
    FatalReason fatal = FatalReason::NONE;
    uint64_t accepted = 0, rejected = 0;
    uint32_t ai_executions = 0;         // gate G5: en feed-mode DEBE ser 0
    uint32_t schedule_mismatches = 0;   // verify v2: DEBE ser 0 (agenda exacta)
    int save_result = -1;               // 0 ok si hubo save
};

// Checksum de continuación (SPEC-001 §10): state_checksum + serialización de IA.
// Dominio bump a V2 en Sprint 0.4 junto con state_checksum_v1 (que ya hashea
// CHECKSUM_ALGO_VERSION=2 internamente; ver checksum.hpp).
inline uint64_t continuation_checksum(const GameState& g, const AiJobBox& box,
                                      const AiRuntimeV1& rt) noexcept {
    uint8_t ai_buf[8192];
    ByteWriter w{ai_buf, sizeof(ai_buf), 0, false};
    ai_serialize(box, rt, w);
    detail::Hasher h;
    h.init();
    h.bytes("CHUNSA_CONT_V2", 14);
    h.u64(state_checksum_v1(g));
    h.bytes(ai_buf, w.len);
    return h.digest();
}

// Nº de retargets humanos ocurridos en ticks < t (t' % 200 == 100).
inline uint32_t retargets_before(uint32_t t) noexcept {
    return (t > 100u) ? ((t - 101u) / 200u + 1u) : 0u;
}

// Batch humano del escenario, función PURA de (t, units, seed) — reconstruible
// tras load. `spawned_at_zero`: los spawns solo existen si la corrida nace en 0.
inline uint32_t build_human_batch(std::vector<RawCommand>& batch, uint32_t t,
                                  uint32_t units, uint64_t seed, bool with_ai) noexcept {
    // Con IA: los índices impares nacen del emisor 1 (la IA los gobierna);
    // los retargets humanos solo tocan índices pares. units debe ser PAR.
    constexpr uint32_t BENCH = static_cast<uint32_t>(RngStream::BENCH);
    FatalReason dummy = FatalReason::NONE;
    uint32_t n = 0;
    if (t == 0u) {
        for (uint32_t i = 0; i < units; ++i) {
            RawCommand& c = batch[n];
            std::memset(&c, 0, sizeof(RawCommand));
            const bool ai_owned = with_ai && (i % 2u == 1u);
            c.target_tick = 0; c.emitter = ai_owned ? uint16_t{1} : uint16_t{0};
            c.type = CommandType::SPAWN_DEBUG;
            c.sequence = (i / 2u) + 1u;             // contador por emisor
            if (!with_ai) c.sequence = i + 1u;
            c.p.handle = EntityHandle{i, 1u};
            c.p.x_raw = static_cast<int64_t>(rng_range(seed, BENCH, 0u, i, 1u, DRIVER_COORD_LO, DRIVER_COORD_HI, dummy));
            c.p.y_raw = static_cast<int64_t>(rng_range(seed, BENCH, 0u, i, 2u, DRIVER_COORD_LO, DRIVER_COORD_HI, dummy));
            c.p.speed_mtpt = static_cast<int32_t>(rng_range(seed, BENCH, 0u, i, 3u, 50u, 501u, dummy));
            ++n;
        }
    } else if (t % 200u == 100u) {
        const uint32_t h_units = with_ai ? units / 2u : units;
        const uint64_t h_spawns = with_ai ? units - units / 2u : units;
        const uint64_t seq_base = h_spawns + static_cast<uint64_t>(h_units) * retargets_before(t);
        uint32_t idx = 0;
        for (uint32_t i = 0; i < units; ++i) {
            if (with_ai && (i % 2u == 1u)) continue;   // la IA gobierna los impares
            RawCommand& c = batch[n];
            std::memset(&c, 0, sizeof(RawCommand));
            c.target_tick = t; c.emitter = 0; c.type = CommandType::MOVE_TO;
            c.sequence = seq_base + (++idx);
            c.p.handle = EntityHandle{i, 1u};
            c.p.x_raw = static_cast<int64_t>(rng_range(seed, BENCH, t, i, 4u, DRIVER_COORD_LO, DRIVER_COORD_HI, dummy));
            c.p.y_raw = static_cast<int64_t>(rng_range(seed, BENCH, t, i, 5u, DRIVER_COORD_LO, DRIVER_COORD_HI, dummy));
            ++n;
        }
    }
    return n;
}

// Bucle principal. `gs/box/rt` los aporta el caller: recién inicializados
// (partida nueva desde tick 0) o recién cargados de un save (continuación).
//
// CONTRATO DEL HOST (SPEC-004 §10.3, comentario — sin cambio de lógica): el
// batch que este driver mete en la PRIMERA llamada a step() (t==0, dentro de
// build_human_batch) es, por contrato, EXCLUSIVAMENTE de setup de escenario
// (spawns iniciales), nunca input real de un jugador — command_effective_tick
// explota esto para dar eff=0 a los comandos con target_tick==0 de ese primer
// batch, sin importar human_input_delay_ticks. Este driver ya lo respeta
// (build_human_batch solo emite comandos de jugador — MOVE_TO — a partir de
// t>0); ver command_effective_tick en step.hpp para la regla completa.
inline int drive(const DriveOpts& o, GameState& gs, AiJobBox& box, AiRuntimeV1& rt,
                 DriveOut& out) {
    std::vector<RawCommand> batch(static_cast<size_t>(o.units) + AI_MAX_COMMANDS);

    while (gs.tick < o.ticks) {
        const uint32_t t = gs.tick;

        // --- SAVE-AT: frontera exacta de inicio de tick (SPEC-001 §2) ---
        if (o.save_path != nullptr && t == o.save_at) {
            out.save_result = save_game(gs, box, rt, o.save_path);
            if (out.save_result != 0) return 2;
        }

        uint32_t n = 0;
        if (o.feed != nullptr) {
            // --- G5: replay-feed. IA JAMÁS se ejecuta aquí. ---
            if (t < o.feed->batches.size()) {
                const auto& b = o.feed->batches[t];
                // v2: la agenda grabada debe reproducir bit a bit la recomputada
                // bajo la config §6.2 del propio replay (auto-verificación).
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
            n = build_human_batch(batch, t, o.units, o.seed, o.with_ai);
            if (o.with_ai) {
                // Pump de la state machine (§7.3):
                if (ai_should_dispatch(box, t)) ai_dispatch(box, t, rt);
                const bool defer = o.hold_dispatched_until_save
                                && o.save_path != nullptr && t < o.save_at;
                if (box.state == AiJobState::DISPATCHED && !defer) {
                    ai_execute(box, gs);
                    ++out.ai_executions;
                }
                if (ai_stalled(box, t)) {  // stall: resolver ahora (política CLI)
                    ai_execute(box, gs);
                    ++out.ai_executions;
                }
                if (ai_due(box, t)) {
                    for (uint32_t k = 0; k < box.result_count && n < batch.size(); ++k)
                        batch[n++] = box.result[k];
                    ai_commit(box, rt);
                }
            }
        }

        if (o.rec != nullptr) o.rec->tick_batch(batch.data(), n, t);

        const StepResult res = step(gs, batch.data(), n);
        if (res.checksum_computed) out.final_checksum = res.checksum;
        out.accepted += res.accepted;
        out.rejected += res.rejected;
    }

    out.fatal = gs.fatal;
    out.continuation_checksum = continuation_checksum(gs, box, rt);
    return gs.fatal == FatalReason::NONE ? 0 : 3;
}

// Conveniencia: partida nueva (heap) + drive + limpieza. Devuelve el código de drive.
inline int drive_fresh(const DriveOpts& o, DriveOut& out) {
    const uint32_t u16_max =
        static_cast<uint32_t>(std::numeric_limits<uint16_t>::max());
    if (o.human_input_delay_ticks > u16_max ||
        o.max_future_command_ticks > u16_max) {
        return 2;
    }

    MatchConfig01A cfg{};
    cfg.max_entities = o.units + 4;
    cfg.player_count = o.with_ai ? uint8_t{2} : uint8_t{1};
    cfg.human_input_delay_ticks =
        static_cast<uint16_t>(o.human_input_delay_ticks);
    cfg.max_future_command_ticks =
        static_cast<uint16_t>(o.max_future_command_ticks);
    cfg.checksum_every_ticks = o.checksum_every;
    cfg.map_tiles_x = 256; cfg.map_tiles_y = 256;
    cfg.seed = o.seed;
    if (!config_validate(cfg)) return 2;
    GameState* gs = new GameState();
    gs_init(*gs, cfg);
    if (o.with_ai && (o.units % 2u) != 0u) { delete gs; return 2; }  // fixture exige par
    AiJobBox box; ai_box_init(box, 1);
    AiRuntimeV1 rt{0, o.with_ai ? static_cast<uint64_t>(o.units / 2u) : 0u};  // seq tras spawns IA
    const int code = drive(o, *gs, box, rt, out);
    delete gs;
    return code;
}

}  // namespace chunsa
