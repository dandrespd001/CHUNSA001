#pragma once
#include <cstdint>

#include "chunsa/entity_table.hpp"

// chunsa_sim_core — Commands, PendingCommandState y mailbox de receipts.
// SPEC-001 §6. Autor: Arquitecto.
// Orden canónico único: (effective_tick, emitter, sequence).

namespace chunsa {

enum class CommandType : uint16_t {
    SPAWN_DEBUG = 1,
    MOVE_TO = 2,
    DESTROY_DEBUG = 3,
    FLOW_MOVE = 4,
    // append-only; jamás renumerar (SPEC-001 §5.1)
};

enum class RejectReason : uint16_t {
    ACCEPTED = 0,
    MALFORMED = 1,
    INVALID_ENTITY = 2,
    NOT_OWNER = 3,
    ILLEGAL_STATE = 4,
    OUT_OF_WINDOW = 5,
    POOL_EXHAUSTED = 6,
    RATE_LIMITED = 7,
    SEQUENCE_REJECTED = 8,
};

// Payload plano de tamaño fijo (0.1A). Campos no usados por un tipo = 0.
struct CmdPayload {
    EntityHandle handle;   // MOVE_TO / DESTROY_DEBUG
    int64_t x_raw;         // SPAWN_DEBUG / MOVE_TO
    int64_t y_raw;
    int32_t speed_mtpt;    // SPAWN_DEBUG (mili-tiles por tick)
};

struct RawCommand {
    uint32_t target_tick;
    uint16_t emitter;      // 0..MAX_EMITTERS-1
    CommandType type;
    uint64_t sequence;     // estrictamente creciente por emisor (u64, SPEC-001 §6.1)
    CmdPayload p;
};

struct ScheduledCommand {
    uint32_t effective_tick;
    uint16_t emitter;
    CommandType type;
    uint64_t sequence;
    CmdPayload p;
};

inline constexpr uint32_t PENDING_CAP = 4096;
inline constexpr uint32_t MAX_EMITTERS = 16;

// Agenda canónica de capacidad fija, SIEMPRE ordenada por
// (effective_tick, emitter, sequence). Serializada en el save (0.1B).
struct PendingCommandState {
    uint32_t count;
    ScheduledCommand items[PENDING_CAP];
};

inline bool sched_less(const ScheduledCommand& a, const ScheduledCommand& b) noexcept {
    if (a.effective_tick != b.effective_tick) return a.effective_tick < b.effective_tick;
    if (a.emitter != b.emitter) return a.emitter < b.emitter;
    return a.sequence < b.sequence;
}

// Inserción manteniendo el orden canónico (memmove; cap fija). false = llena.
inline bool pcs_insert(PendingCommandState& s, const ScheduledCommand& c) noexcept {
    if (s.count >= PENDING_CAP) return false;
    uint32_t i = s.count;
    while (i > 0 && sched_less(c, s.items[i - 1])) {
        s.items[i] = s.items[i - 1];
        --i;
    }
    s.items[i] = c;
    ++s.count;
    return true;
}

// Extrae el prefijo con effective_tick == tick (ya está al frente por el orden).
// Devuelve cuántos escribió en out (≤ out_cap; el resto queda para el caller —
// con PENDING_CAP == out_cap nunca trunca).
inline uint32_t pcs_take_due(PendingCommandState& s, uint32_t tick,
                             ScheduledCommand* out, uint32_t out_cap) noexcept {
    uint32_t n = 0;
    while (n < s.count && n < out_cap && s.items[n].effective_tick == tick) {
        out[n] = s.items[n];
        ++n;
    }
    if (n > 0) {
        for (uint32_t i = n; i < s.count; ++i) s.items[i - n] = s.items[i];
        s.count -= n;
    }
    return n;
}

// ---- Mailbox de receipts por emisor (SPEC-001 §7.6; en GameState, dominio state) ----

struct CommandReceipt {
    uint64_t sequence;
    uint32_t processed_tick;
    RejectReason result;
};

inline constexpr uint32_t MAILBOX_CAP = 256;

struct ReceiptMailbox {
    uint32_t head;    // índice lógico del más antiguo
    uint32_t count;
    uint64_t dropped; // monotónico: receipts sobrescritos (la IA detecta huecos)
    CommandReceipt ring[MAILBOX_CAP];
};

inline void mailbox_push(ReceiptMailbox& m, CommandReceipt r) noexcept {
    if (m.count < MAILBOX_CAP) {
        m.ring[(m.head + m.count) % MAILBOX_CAP] = r;
        ++m.count;
    } else {
        m.ring[m.head] = r;               // sobrescribe el más antiguo
        m.head = (m.head + 1) % MAILBOX_CAP;
        ++m.dropped;
    }
}

}  // namespace chunsa
