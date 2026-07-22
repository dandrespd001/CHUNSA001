#pragma once
#include <cstdint>

// chunsa_sim_core — state_checksum_v1 sobre el stream canónico (SPEC-001 §10).
// Autor: Arquitecto. Algoritmo congelado: XXH3_64bits con seed y prefijo de
// dominio; fuente vendored (xxHash v0.8.3, sha256 en docs/TOOLCHAIN.md).
// Serialización canónica: CAMPO A CAMPO, little-endian implícito de x86-64,
// jamás memcpy de structs (evita padding — SPEC-001 §11.1).

#define XXH_INLINE_ALL
#include "../../third_party/xxhash/xxhash.h"

#include "chunsa/game_state.hpp"

namespace chunsa {

inline constexpr uint32_t CHECKSUM_ALGO_VERSION = 1;
inline constexpr uint64_t CHECKSUM_SEED = 0x4348554E5F535431ull;  // "CHUN_ST1"

namespace detail {

struct Hasher {
    XXH3_state_t st{};  // zero-init: evita -Werror=uninitialized de GCC16 en reset_withSeed
    void init() noexcept { XXH3_64bits_reset_withSeed(&st, CHECKSUM_SEED); }
    void bytes(const void* p, size_t n) noexcept { XXH3_64bits_update(&st, p, n); }
    void u8(uint8_t v) noexcept { bytes(&v, 1); }
    void u16(uint16_t v) noexcept { bytes(&v, 2); }
    void u32(uint32_t v) noexcept { bytes(&v, 4); }
    void u64(uint64_t v) noexcept { bytes(&v, 8); }
    void i32(int32_t v) noexcept { u32(static_cast<uint32_t>(v)); }
    void i64(int64_t v) noexcept { u64(static_cast<uint64_t>(v)); }
    uint64_t digest() noexcept { return XXH3_64bits_digest(&st); }
};

}  // namespace detail

// Dominio state_checksum_v1 (SPEC-001 §10): reproducible por replay.
// Cubre: tick, fatal, tabla de entidades CON generaciones y free-list (orden
// exacto), componentes de vivos en índice ascendente, PendingCommandState,
// last_seq y mailboxes. EXCLUYE presentación y telemetría de pared.
inline uint64_t state_checksum_v1(const GameState& g) noexcept {
    detail::Hasher h;
    h.init();
    h.bytes("CHUNSA_STATE_V1", 15);
    h.u32(CHECKSUM_ALGO_VERSION);
    h.u32(g.tick);
    h.u32(static_cast<uint32_t>(g.fatal));

    const EntityTable& t = g.entities;
    h.u32(t.capacity);
    h.u32(t.alive_count);
    h.u32(t.free_top);
    for (uint32_t i = 0; i < t.free_top; ++i) h.u32(t.free_stack[i]);
    for (uint32_t i = 0; i < t.capacity; ++i) {
        h.u32(t.generation[i]);
        h.u8(t.alive[i]);
        h.u8(t.retired[i]);
    }
    for (uint32_t i = 0; i < t.capacity; ++i) {
        if (!t.alive[i]) continue;
        h.u32(i);
        h.i64(g.pos_x[i]); h.i64(g.pos_y[i]);
        h.i64(g.vel_x[i]); h.i64(g.vel_y[i]);
        h.i64(g.tgt_x[i]); h.i64(g.tgt_y[i]);
        h.i32(g.speed_mtpt[i]);
        h.u8(g.owner[i]);
    }

    h.u32(g.pending.count);
    for (uint32_t i = 0; i < g.pending.count; ++i) {
        const ScheduledCommand& c = g.pending.items[i];
        h.u32(c.effective_tick);
        h.u16(c.emitter);
        h.u16(static_cast<uint16_t>(c.type));
        h.u64(c.sequence);
        h.u32(c.p.handle.index); h.u32(c.p.handle.generation);
        h.i64(c.p.x_raw); h.i64(c.p.y_raw);
        h.i32(c.p.speed_mtpt);
    }
    for (uint32_t e = 0; e < MAX_EMITTERS; ++e) {
        h.u64(g.last_seq[e]);
        const ReceiptMailbox& m = g.mailbox[e];
        h.u32(m.count);
        h.u64(m.dropped);
        for (uint32_t i = 0; i < m.count; ++i) {
            const CommandReceipt& r = m.ring[(m.head + i) % MAILBOX_CAP];
            h.u64(r.sequence);
            h.u32(r.processed_tick);
            h.u16(static_cast<uint16_t>(r.result));
        }
    }
    // Visión: solo `explored` (estado acumulativo); `visible` es derivada.
    for (uint32_t p = 0; p < VIS_MAX_PLAYERS; ++p) {
        for (uint32_t wgt = 0; wgt < VIS_WORDS; ++wgt) {
            h.u64(g.vision.explored[p][wgt]);
        }
    }
    // Flujo de navegación: cost_grid + flow_mode + goal. `flow` es derivada
    // (excluida) y `flow_dirty` es transitorio de cómputo (excluido).
    for (uint32_t i = 0; i < FF_CELLS; ++i) h.u8(g.cost_grid[i]);
    for (uint32_t i = 0; i < t.capacity; ++i) h.u8(g.flow_mode[i]);
    h.u32(g.flow_goal_cell);
    h.u8(g.flow_has_goal);
    // Combate (Sprint 0.3): componentes por índice ascendente, todos los slots.
    for (uint32_t i = 0; i < t.capacity; ++i) h.i32(g.hp[i]);
    for (uint32_t i = 0; i < t.capacity; ++i) h.i32(g.max_hp[i]);
    for (uint32_t i = 0; i < t.capacity; ++i) h.i32(g.attack[i]);
    for (uint32_t i = 0; i < t.capacity; ++i) h.i32(g.range_mt[i]);
    for (uint32_t i = 0; i < t.capacity; ++i) h.u8(g.unit_class[i]);
    for (uint32_t i = 0; i < t.capacity; ++i) h.u16(g.atk_cd[i]);
    // Moral (Sprint 0.3): componentes por índice ascendente, todos los slots.
    for (uint32_t i = 0; i < t.capacity; ++i) h.i32(g.morale[i]);
    for (uint32_t i = 0; i < t.capacity; ++i) h.u8(g.fleeing[i]);
    // Economía (Sprint 0.3): depósitos (todos los slots fijos), dropoffs y stock
    // por emisor, y componentes por-ciudadano por índice ascendente.
    h.u32(g.n_deposits);
    for (uint32_t i = 0; i < ECO_MAX_DEPOSITS; ++i) {
        h.i64(g.deposits[i].x_raw);
        h.i64(g.deposits[i].y_raw);
        h.u8(g.deposits[i].resource_idx);
        h.i32(g.deposits[i].remaining);
    }
    for (uint32_t e = 0; e < MAX_EMITTERS; ++e) {
        h.i64(g.dropoff_x[e]);
        h.i64(g.dropoff_y[e]);
        h.i64(g.player_stock[e][0]);
        h.i64(g.player_stock[e][1]);
        h.i64(g.player_stock[e][2]);
    }
    for (uint32_t i = 0; i < t.capacity; ++i) h.u8(static_cast<uint8_t>(g.eco_state[i]));
    for (uint32_t i = 0; i < t.capacity; ++i) h.u32(g.eco_assigned_deposit[i]);
    for (uint32_t i = 0; i < t.capacity; ++i) h.i32(g.eco_carry[i]);
    for (uint32_t i = 0; i < t.capacity; ++i) h.u8(g.eco_carry_resource[i]);
    return h.digest();
}

}  // namespace chunsa
