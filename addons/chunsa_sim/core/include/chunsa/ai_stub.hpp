#pragma once

// chunsa_sim_core — AI stub v1: state machine de jobs serializable (SPEC-001 §7.3)
// generado: minimax-m3 · revisado: Arquitecto 2026-07-17

#include <cstdint>

#include "chunsa/game_state.hpp"
#include "chunsa/rng.hpp"
#include "chunsa/commands.hpp"
#include "chunsa/serialize.hpp"

namespace chunsa {

// ---------------------------------------------------------------------------
// Constantes de contrato (SPEC-001 §7.2 / §7.3). Versión del algoritmo; si
// cambia el procedimiento de decisión, bump AI_ALGO_VERSION y mantener
// compat en ai_deserialize.
// ---------------------------------------------------------------------------
inline constexpr uint32_t AI_ALGO_VERSION     = 1;
inline constexpr uint16_t AI_INPUT_DELAY_TICKS = 4;
inline constexpr uint32_t AI_DECISION_PHASE   = 7;     // dispatch cuando tick % 20 == 7
inline constexpr uint32_t AI_MAX_COMMANDS     = 64;

// Estados del ciclo de vida de UN job de IA. EMPTY = slot libre;
// DISPATCHED/RUNNING = en vuelo; COMPLETED = resultado disponible;
// COMMITTED = consumido (transitorio, inmediatamente EMPTY tras ai_commit);
// FAILED = terminal de error.
enum class AiJobState : uint8_t {
    EMPTY      = 0,
    DISPATCHED = 1,
    RUNNING    = 2,
    COMPLETED  = 3,
    COMMITTED  = 4,
    FAILED     = 5
};

// Estado de continuación de la IA (dominio continuation, SPEC-001 §10).
// Vive fuera del AiJobBox; ai_sequence es estrictamente monótono por emisor.
struct AiRuntimeV1 {
    uint32_t decision_epoch;  // nº de decisiones tomadas por esta IA
    uint64_t ai_sequence;     // sequence del emisor IA (estrictamente creciente)
};

// UN job pendiente por IA (SPEC-001 §7.2). Toda la decisión se congela en
// (source_tick, runtime_before); nunca se re-deriva desde g.tick, garantizando
// replay determinista (§7.1).
struct AiJobBox {
    AiJobState   state;                // estado actual de la máquina
    uint8_t      ai_player;            // emisor de la IA (0.1B: 1)
    uint32_t     source_tick;          // snapshot lógico del que decide
    uint32_t     decision_epoch;       // epoch capturado al despachar
    AiRuntimeV1  runtime_before;       // input congelado del job
    uint32_t     result_count;         // nº de comandos calculados en result[]
    RawCommand   result[AI_MAX_COMMANDS]; // COMPLETED: comandos calculados no aplicados
};

// ---------------------------------------------------------------------------
// 1. Inicialización del slot: EMPTY, contadores a cero vía runtime_before.
//    result[] no se toca: solo se lee hasta result_count.
// ---------------------------------------------------------------------------
inline void ai_box_init(AiJobBox& b, uint8_t ai_player) noexcept {
    b.state          = AiJobState::EMPTY;
    b.ai_player      = ai_player;
    b.source_tick    = 0u;
    b.decision_epoch = 0u;
    b.runtime_before = AiRuntimeV1{0u, 0u};
    b.result_count   = 0u;
}

// ---------------------------------------------------------------------------
// 2. ¿Toca despachar? Solo en fase exacta (tick % 20 == 7) y con slot vacío.
// ---------------------------------------------------------------------------
inline bool ai_should_dispatch(const AiJobBox& b, uint32_t tick) noexcept {
    return b.state == AiJobState::EMPTY && (tick % 20u) == AI_DECISION_PHASE;
}

// ---------------------------------------------------------------------------
// 3. Despacho: congelar input. source_tick = tick fija el snapshot lógico.
//    Tras este punto, ai_execute es función pura de (g, runtime_before).
// ---------------------------------------------------------------------------
inline void ai_dispatch(AiJobBox& b, uint32_t tick,
                        const AiRuntimeV1& runtime) noexcept {
    b.state          = AiJobState::DISPATCHED;
    b.source_tick    = tick;
    b.decision_epoch = runtime.decision_epoch;
    b.runtime_before = runtime;
    b.result_count   = 0u;
}

// ---------------------------------------------------------------------------
// 4. Ejecución determinista. Precondición: state == DISPATCHED.
//
//    REGLA DE ORO (§7.1): jamás usar g.tick ni reloj real; solo
//    b.source_tick y b.runtime_before. Si se viola, la re-ejecución diverge
//    y se rompe la replay bit-a-bit.
//
//    Estrategia: recorrido ascendente i = 0..capacity-1; para cada entidad
//    viva del jugador IA, emite un MOVE_TO con target en [1.0, 255.0] raw
//    (FX_ONE_RAW = 65536), derivado vía rng_range con el stream AI_TIEBREAK.
// ---------------------------------------------------------------------------
inline void ai_execute(AiJobBox& b, const GameState& g) noexcept {
    b.state        = AiJobState::RUNNING;
    b.result_count = 0u;

    const uint32_t cap   = g.entities.capacity;
    const uint64_t seed  = g.cfg.seed;
    uint32_t       count = 0u;

    // FatalReason local: rng_range no debe fallar aquí (lo < hi, magnitudes
    // acotadas). Sin excepciones, así que se silencia por contrato del stub.
    FatalReason dummy{};

    // Recorrido estable por índice: orden determinista entre clientes/replays.
    for (uint32_t i = 0; i < cap && count < AI_MAX_COMMANDS; ++i) {
        if (!g.entities.alive[i])                  continue;
        if (g.owner[i] != b.ai_player)    continue;

        RawCommand cmd{};
        cmd.target_tick = b.source_tick + AI_INPUT_DELAY_TICKS;
        cmd.emitter     = static_cast<uint16_t>(b.ai_player);
        cmd.type        = CommandType::MOVE_TO;
        // sequence = último sequence congelado + nº de comando (1-based).
        cmd.sequence    = b.runtime_before.ai_sequence
                        + static_cast<uint64_t>(count + 1u);
        cmd.p.handle.index      = i;
        cmd.p.handle.generation = g.entities.generation[i];

        // Targets en fixed-point crudo: lo = 1.0 raw, hi = 255.0 raw,
        // cubre tablero sin usar coma flotante. Slot 10 → X, slot 11 → Y.
        cmd.p.x_raw = rng_range(
            seed,
            static_cast<uint32_t>(RngStream::AI_TIEBREAK),
            b.source_tick,
            i,
            10u,
            65536u,
            255u * 65536u,
            dummy
        );
        cmd.p.y_raw = rng_range(
            seed,
            static_cast<uint32_t>(RngStream::AI_TIEBREAK),
            b.source_tick,
            i,
            11u,
            65536u,
            255u * 65536u,
            dummy
        );
        cmd.p.speed_mtpt = 0;  // stub: no decide velocidad explícita

        b.result[count] = cmd;
        ++count;
    }

    b.result_count = count;
    b.state        = AiJobState::COMPLETED;
}

// ---------------------------------------------------------------------------
// 5a. ¿Está el job COMPLETED y exactamente en el tick de aplicación?
//     Devuelve true solo en el tick source_tick + AI_INPUT_DELAY_TICKS.
// ---------------------------------------------------------------------------
inline bool ai_due(const AiJobBox& b, uint32_t tick) noexcept {
    return b.state == AiJobState::COMPLETED
        && tick == b.source_tick + static_cast<uint32_t>(AI_INPUT_DELAY_TICKS);
}

// 5b. ¿Se quedó en vuelo (DISPATCHED o RUNNING) más allá del plazo?
//     Útil para que el scheduler aborte y re-despache.
inline bool ai_stalled(const AiJobBox& b, uint32_t tick) noexcept {
    const bool in_flight = (b.state == AiJobState::DISPATCHED)
                        || (b.state == AiJobState::RUNNING);
    return in_flight
        && tick >= b.source_tick + static_cast<uint32_t>(AI_INPUT_DELAY_TICKS);
}

// ---------------------------------------------------------------------------
// 6. Commit: consume el resultado, avanza runtime, libera slot.
//    Precondición: caller ya verificó ai_due() y copió result[] al batch
//    de comandos de este tick. Aquí solo cerramos el ciclo.
// ---------------------------------------------------------------------------
inline void ai_commit(AiJobBox& b, AiRuntimeV1& runtime) noexcept {
    runtime.ai_sequence    += b.result_count;
    runtime.decision_epoch += 1u;
    // COMMITTED es estado transitorio: tras consumir devolvemos EMPTY,
    // dejando el slot listo para el próximo dispatch.
    b.state                = AiJobState::EMPTY;
    b.result_count         = 0u;
}

// ---------------------------------------------------------------------------
// 7a. Serialización canónica campo a campo. Orden congelado por SPEC-001
//     §7.3: primero el AiRuntimeV1 (estado de continuación), luego la caja
//     del job, y por último los result[] en orden 0..result_count-1.
// ---------------------------------------------------------------------------
inline void ai_serialize(const AiJobBox& b, const AiRuntimeV1& rt,
                         ByteWriter& w) noexcept {
    w.u32(rt.decision_epoch);
    w.u64(rt.ai_sequence);

    w.u8(static_cast<uint8_t>(b.state));
    w.u8(b.ai_player);
    w.u32(b.source_tick);
    w.u32(b.decision_epoch);
    w.u32(b.runtime_before.decision_epoch);
    w.u64(b.runtime_before.ai_sequence);
    w.u32(b.result_count);

    for (uint32_t i = 0; i < b.result_count; ++i) {
        const RawCommand& c = b.result[i];
        w.u32(c.target_tick);
        w.u16(c.emitter);
        w.u16(static_cast<uint16_t>(c.type));
        w.u64(c.sequence);
        w.u32(c.p.handle.index);
        w.u32(c.p.handle.generation);
        w.i64(c.p.x_raw);
        w.i64(c.p.y_raw);
        w.i32(c.p.speed_mtpt);
    }
}

// ---------------------------------------------------------------------------
// 7b. Deserialización simétrica inversa. Cualquier fail del reader ⇒ false.
//
//    Validaciones de forma:
//      - state debe estar en [0..5]; fuera de rango ⇒ false.
//      - result_count debe estar en [0..AI_MAX_COMMANDS]; si no ⇒ false.
//      - si state == RUNNING al cargar, se degrada a DISPATCHED: el cómputo
//        se repite desde el input congelado (runtime_before + source_tick),
//        preservando determinismo (§7.3).
// ---------------------------------------------------------------------------
inline bool ai_deserialize(AiJobBox& b, AiRuntimeV1& rt,
                           ByteReader& r) noexcept {
    // Saneamiento defensivo del slot antes de poblarlo.
    b.state          = AiJobState::EMPTY;
    b.ai_player      = 0u;
    b.source_tick    = 0u;
    b.decision_epoch = 0u;
    b.runtime_before = AiRuntimeV1{0u, 0u};
    b.result_count   = 0u;

    rt.decision_epoch = r.u32();
    rt.ai_sequence = r.u64();

    if (r.fail) return false;
    uint8_t st_raw = r.u8();
    if (st_raw > static_cast<uint8_t>(AiJobState::FAILED)) return false;
    b.state = static_cast<AiJobState>(st_raw);
    // Degradación determinista: RUNNING ⇒ DISPATCHED.
    if (b.state == AiJobState::RUNNING) {
        b.state = AiJobState::DISPATCHED;
    }

    b.ai_player = r.u8();
    b.source_tick = r.u32();
    b.decision_epoch = r.u32();
    b.runtime_before.decision_epoch = r.u32();
    b.runtime_before.ai_sequence = r.u64();
    b.result_count = r.u32();
    if (r.fail) return false;
    if (b.result_count > AI_MAX_COMMANDS)            return false;

    for (uint32_t i = 0; i < b.result_count; ++i) {
        RawCommand& c = b.result[i];
        c.target_tick = r.u32();
        c.emitter = r.u16();
        c.type = static_cast<CommandType>(r.u16());
        c.sequence = r.u64();
        c.p.handle.index = r.u32();
        c.p.handle.generation = r.u32();
        c.p.x_raw = r.i64();
        c.p.y_raw = r.i64();
        c.p.speed_mtpt = r.i32();
    }

    if (r.fail) return false;
    return true;
}

} // namespace chunsa