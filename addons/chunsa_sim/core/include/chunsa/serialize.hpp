#pragma once

// chunsa_sim_core — serialización canónica campo-a-campo, little-endian (SPEC-001 §11.1)
// generado: minimax-m3 · revisado: Arquitecto 2026-07-17

#include "chunsa/game_state.hpp"
#include <cstdint>
#include <cstring>

namespace chunsa {

// =============================================================================
// ByteWriter — escritor bounded, little-endian, campo a campo.
// Sin memcpy de multibyte: cada valor se descompone byte a byte con shifts para
// garantizar el orden canónico independientemente del endianness del host.
// Si no cabe, marca `overflow=true` y no escribe; el caller devuelve 0.
// =============================================================================
struct ByteWriter {
    uint8_t* buf;
    size_t   cap;
    size_t   len;
    bool     overflow;

    void u8(uint8_t v) noexcept {
        if (overflow) return;
        if (len + 1 > cap) { overflow = true; return; }
        buf[len++] = v;
    }

    void u16(uint16_t v) noexcept {
        if (overflow) return;
        if (len + 2 > cap) { overflow = true; return; }
        buf[len++] = static_cast<uint8_t>(v & 0xFFu);
        buf[len++] = static_cast<uint8_t>((v >> 8) & 0xFFu);
    }

    void u32(uint32_t v) noexcept {
        if (overflow) return;
        if (len + 4 > cap) { overflow = true; return; }
        buf[len++] = static_cast<uint8_t>(v & 0xFFu);
        buf[len++] = static_cast<uint8_t>((v >> 8) & 0xFFu);
        buf[len++] = static_cast<uint8_t>((v >> 16) & 0xFFu);
        buf[len++] = static_cast<uint8_t>((v >> 24) & 0xFFu);
    }

    void u64(uint64_t v) noexcept {
        if (overflow) return;
        if (len + 8 > cap) { overflow = true; return; }
        for (int i = 0; i < 8; ++i) {
            buf[len++] = static_cast<uint8_t>((v >> (i * 8)) & 0xFFu);
        }
    }

    void i32(int32_t v) noexcept { u32(static_cast<uint32_t>(v)); }
    void i64(int64_t v) noexcept { u64(static_cast<uint64_t>(v)); }

    // Copia byte a byte (no se permite usar memcpy sobre structs).
    void bytes(const void* src, size_t n) noexcept {
        if (overflow) return;
        if (len + n > cap) { overflow = true; return; }
        const uint8_t* s = static_cast<const uint8_t*>(src);
        for (size_t i = 0; i < n; ++i) buf[len++] = s[i];
    }
};

// =============================================================================
// ByteReader — lector bounded, little-endian. Una vez `fail=true`, todo
// subsiguiente devuelve 0 y no avanza el cursor (estado terminal).
// =============================================================================
struct ByteReader {
    const uint8_t* buf;
    size_t         len;
    size_t         pos;
    bool           fail;

    uint8_t u8() noexcept {
        if (fail || pos + 1 > len) { fail = true; return 0; }
        return buf[pos++];
    }

    uint16_t u16() noexcept {
        if (fail || pos + 2 > len) { fail = true; return 0; }
        uint16_t v = 0;
        v |= static_cast<uint16_t>(buf[pos]);
        v |= static_cast<uint16_t>(buf[pos + 1]) << 8;
        pos += 2;
        return v;
    }

    uint32_t u32() noexcept {
        if (fail || pos + 4 > len) { fail = true; return 0; }
        uint32_t v = 0;
        v |= static_cast<uint32_t>(buf[pos]);
        v |= static_cast<uint32_t>(buf[pos + 1]) << 8;
        v |= static_cast<uint32_t>(buf[pos + 2]) << 16;
        v |= static_cast<uint32_t>(buf[pos + 3]) << 24;
        pos += 4;
        return v;
    }

    uint64_t u64() noexcept {
        if (fail || pos + 8 > len) { fail = true; return 0; }
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) {
            v |= static_cast<uint64_t>(buf[pos + i]) << (i * 8);
        }
        pos += 8;
        return v;
    }

    int32_t i32() noexcept { return static_cast<int32_t>(u32()); }
    int64_t i64() noexcept { return static_cast<int64_t>(u64()); }

    void bytes(void* dst, size_t n) noexcept {
        if (fail || pos + n > len) { fail = true; return; }
        uint8_t* d = static_cast<uint8_t*>(dst);
        for (size_t i = 0; i < n; ++i) d[i] = buf[pos++];
    }
};

// =============================================================================
// Cota superior del buffer de save (16 MiB). El caller dimensiona con este
// tamaño o verifica overflow==true al recibir 0.
// =============================================================================
inline constexpr size_t GS_SERIALIZE_MAX = 16u * 1024u * 1024u;

// =============================================================================
// gs_serialize — escribe el GameState en ORDEN CANÓNICO EXACTO (congelado).
// Precondición: g.destroy_count == 0 (frontera de save = inicio de tick).
// Devuelve bytes escritos; 0 si overflow (el contenido del buf es parcial).
// =============================================================================
inline size_t gs_serialize(const GameState& g, uint8_t* buf, size_t cap) noexcept {
    ByteWriter w{buf, cap, 0, false};

    // (a) MatchConfig01A — orden canónico exacto
    w.u32(g.cfg.max_entities);
    w.u8 (g.cfg.player_count);
    w.u16(g.cfg.human_input_delay_ticks);
    w.u16(g.cfg.max_future_command_ticks);
    w.u16(g.cfg.checksum_every_ticks);
    w.u32(g.cfg.map_tiles_x);
    w.u32(g.cfg.map_tiles_y);
    w.u64(g.cfg.seed);
    w.u8 (g.cfg.allow_debug_stat_payload);  // Sprint 0.4 (SPEC-002 §8.3)

    // (b) tick + fatal
    w.u32(g.tick);
    w.u32(static_cast<uint32_t>(g.fatal));

    // (c) EntityTable: header + free_stack[0..free_top) + filas generation/alive/retired
    const uint32_t cap_e = g.entities.capacity;
    const uint32_t alive = g.entities.alive_count;
    const uint32_t ftop  = g.entities.free_top;
    w.u32(cap_e);
    w.u32(alive);
    w.u32(ftop);
    for (uint32_t k = 0; k < ftop; ++k) {
        w.u32(g.entities.free_stack[k]);
    }
    for (uint32_t i = 0; i < cap_e; ++i) {
        w.u32(g.entities.generation[i]);
        w.u8 (g.entities.alive[i]);
        w.u8 (g.entities.retired[i]);
    }

    // (d) Componentes SOLO de los vivos, índice ascendente
    for (uint32_t i = 0; i < cap_e; ++i) {
        if (g.entities.alive[i] != 0) {
            w.u32(i);                  // índice, para validación al cargar
            w.i64(g.pos_x[i]);
            w.i64(g.pos_y[i]);
            w.i64(g.vel_x[i]);
            w.i64(g.vel_y[i]);
            w.i64(g.tgt_x[i]);
            w.i64(g.tgt_y[i]);
            w.i32(g.speed_mtpt[i]);
            w.u8 (g.owner[i]);
        }
    }

    // (e) PendingCommandState — save v9 (Sprint 1.2, SPEC-004 §10.2): +u32
    // unit_id TRAS unit_class, append-only, mismo patrón que v7/v8. Sin
    // migración v8→v9 (precedente D7): un save v8 real ya falla antes de
    // llegar aquí, en el check SAVE_FORMAT_VERSION del envelope (save_io.hpp).
    const uint32_t pcount = g.pending.count;
    w.u32(pcount);
    for (uint32_t k = 0; k < pcount; ++k) {
        const auto& it = g.pending.items[k];
        w.u32(it.effective_tick);
        w.u16(it.emitter);
        w.u16(static_cast<uint16_t>(it.type));
        w.u64(it.sequence);
        w.u32(it.p.handle.index);
        w.u32(it.p.handle.generation);
        w.i64(it.p.x_raw);
        w.i64(it.p.y_raw);
        w.i32(it.p.speed_mtpt);
        w.i32(it.p.hp);
        w.i32(it.p.attack);
        w.i32(it.p.range_mt);
        w.u8 (it.p.unit_class);
        w.u32(it.p.unit_id);  // v9 (SPEC-004 §10.2)
    }

    // (f) Por emisor (0..15): last_seq, mailbox count/dropped, ring en orden lógico
    for (uint32_t e = 0; e < 16; ++e) {
        const auto& mb = g.mailbox[e];
        w.u64(g.last_seq[e]);
        const uint32_t mcount = mb.count;
        w.u32(mcount);
        w.u64(mb.dropped);
        for (uint32_t j = 0; j < mcount; ++j) {
            const auto& rc = mb.ring[(mb.head + j) % MAILBOX_CAP];
            w.u64(rc.sequence);
            w.u32(rc.processed_tick);
            w.u16(static_cast<uint16_t>(rc.result));
        }
    }

    // (g) Visión: solo explored (visible se reconstruye en la próxima fase).
    for (uint32_t p = 0; p < VIS_MAX_PLAYERS; ++p) {
        for (uint32_t wd = 0; wd < VIS_WORDS; ++wd) {
            w.u64(g.vision.explored[p][wd]);
        }
    }

    // (h) Flujo de navegación: cost_grid + flow_mode + goal. `flow` (derivada)
    // NO se serializa; se recalcula al cargar (ver flow_dirty en deserialize).
    for (uint32_t i = 0; i < FF_CELLS; ++i) w.u8(g.cost_grid[i]);
    for (uint32_t i = 0; i < cap_e; ++i) w.u8(g.flow_mode[i]);
    w.u32(g.flow_goal_cell);
    w.u8(g.flow_has_goal);

    // (i) Combate (Sprint 0.3): 6 arrays, todos los slots, en este orden.
    for (uint32_t i = 0; i < cap_e; ++i) w.i32(g.hp[i]);
    for (uint32_t i = 0; i < cap_e; ++i) w.i32(g.max_hp[i]);
    for (uint32_t i = 0; i < cap_e; ++i) w.i32(g.attack[i]);
    for (uint32_t i = 0; i < cap_e; ++i) w.i32(g.range_mt[i]);
    for (uint32_t i = 0; i < cap_e; ++i) w.u8 (g.unit_class[i]);
    for (uint32_t i = 0; i < cap_e; ++i) w.u16(g.atk_cd[i]);

    // (j) Moral (Sprint 0.3): 2 arrays, todos los slots, en este orden.
    for (uint32_t i = 0; i < cap_e; ++i) w.i32(g.morale[i]);
    for (uint32_t i = 0; i < cap_e; ++i) w.u8 (g.fleeing[i]);

    // (j2) Catálogo (Sprint 0.4): unit_id, todos los slots (misma convención
    // que combate/moral). `catalog` (puntero binding runtime) NUNCA se
    // serializa — se re-enlaza fuera del lifecycle de save/load.
    for (uint32_t i = 0; i < cap_e; ++i) w.u32(g.unit_id[i]);

    // (k) Economía (Sprint 0.3): depósitos (fijos, ECO_MAX_DEPOSITS slots),
    // dropoffs+stock por emisor, y componentes por-ciudadano, mismo orden que checksum.
    w.u32(g.n_deposits);
    for (uint32_t i = 0; i < ECO_MAX_DEPOSITS; ++i) {
        w.i64(g.deposits[i].x_raw);
        w.i64(g.deposits[i].y_raw);
        w.u8 (g.deposits[i].resource_idx);
        w.i32(g.deposits[i].remaining);
    }
    for (uint32_t e = 0; e < MAX_EMITTERS; ++e) {
        w.i64(g.dropoff_x[e]);
        w.i64(g.dropoff_y[e]);
        w.i64(g.player_stock[e][0]);
        w.i64(g.player_stock[e][1]);
        w.i64(g.player_stock[e][2]);
    }
    for (uint32_t i = 0; i < cap_e; ++i) w.u8 (static_cast<uint8_t>(g.eco_state[i]));
    for (uint32_t i = 0; i < cap_e; ++i) w.u32(g.eco_assigned_deposit[i]);
    for (uint32_t i = 0; i < cap_e; ++i) w.i32(g.eco_carry[i]);
    for (uint32_t i = 0; i < cap_e; ++i) w.u8 (g.eco_carry_resource[i]);

    // (l) Edificios (Sprint 1.1, SPEC-004 §3/§8): 6 arrays, AL FINAL, tras todo
    // lo v7, en el mismo orden que aparecen en §3 (== orden del checksum v3).
    for (uint32_t i = 0; i < cap_e; ++i) w.u8 (g.entity_kind[i]);
    for (uint32_t i = 0; i < cap_e; ++i) w.u32(g.building_id[i]);
    for (uint32_t i = 0; i < cap_e; ++i) w.u32(g.build_progress[i]);
    for (uint32_t i = 0; i < cap_e; ++i) w.u16(g.bld_anchor_tx[i]);
    for (uint32_t i = 0; i < cap_e; ++i) w.u16(g.bld_anchor_ty[i]);
    for (uint32_t i = 0; i < cap_e; ++i) w.u32(g.build_target[i]);

    // (m) Producción y tecnología (Sprint 1.2, SPEC-004 §11.2/§12.2 — save
    // v10): AL FINAL, tras todo lo v9, mismo orden que checksum.hpp
    // (+ epoch_initial, deviación documentada en game_state.hpp).
    for (uint32_t i = 0; i < cap_e; ++i) {
        for (uint32_t k = 0; k < PROD_QUEUE_CAP; ++k) w.u32(g.prod_queue[i][k]);
    }
    for (uint32_t i = 0; i < cap_e; ++i) w.u8(g.prod_count[i]);
    for (uint32_t i = 0; i < cap_e; ++i) w.u32(g.prod_progress[i]);
    for (uint32_t i = 0; i < cap_e; ++i) w.i64(g.rally_x[i]);
    for (uint32_t i = 0; i < cap_e; ++i) w.i64(g.rally_y[i]);
    for (uint32_t i = 0; i < cap_e; ++i) w.u8(g.rally_set[i]);
    for (uint32_t e = 0; e < MAX_EMITTERS; ++e) w.i32(g.pop_used[e]);
    for (uint32_t e = 0; e < MAX_EMITTERS; ++e) {
        for (uint32_t k = 0; k < TECH_WORDS; ++k) w.u64(g.player_techs[e][k]);
    }
    for (uint32_t e = 0; e < MAX_EMITTERS; ++e) {
        for (uint32_t k = 0; k < CAP_WORDS; ++k) w.u64(g.player_caps[e][k]);
    }
    for (uint32_t e = 0; e < MAX_EMITTERS; ++e) w.u8(g.player_epoch[e]);
    for (uint32_t e = 0; e < MAX_EMITTERS; ++e) w.u8(g.epoch_initial[e]);
    for (uint32_t i = 0; i < cap_e; ++i) w.u32(g.research_tech[i]);
    for (uint32_t i = 0; i < cap_e; ++i) w.u32(g.research_progress[i]);

    // (n) Victoria/derrota (Sprint 1.4, SPEC-005 §6/§7 — save v11): AL FINAL
    // del stream, tras todo lo v10 (precedente D7: append-only, sin
    // migración — ver save_io.hpp). Escalares del partido, no por-slot.
    w.u8(g.game_over);
    w.u8(g.winner);
    w.u16(g.participants_mask);

    if (w.overflow) return 0;
    return w.len;
}

// =============================================================================
// gs_deserialize — reconstruye un GameState en ORDEN ESPEJO exacto.
// Lee cfg → valida → gs_init(g, cfg) → hidrata resto. Ante cualquier
// inconsistencia devuelve false; el caller descarta el GameState.
// =============================================================================
inline bool gs_deserialize(GameState& g, const uint8_t* buf, size_t len) noexcept {
    ByteReader r{buf, len, 0, false};

    // (a) cfg a variable local
    MatchConfig01A cfg{};
    cfg.max_entities             = r.u32();
    cfg.player_count             = r.u8();
    cfg.human_input_delay_ticks  = r.u16();
    cfg.max_future_command_ticks = r.u16();
    cfg.checksum_every_ticks     = r.u16();
    cfg.map_tiles_x              = r.u32();
    cfg.map_tiles_y              = r.u32();
    cfg.seed                     = r.u64();
    cfg.allow_debug_stat_payload = r.u8();  // Sprint 0.4 (SPEC-002 §8.3)

    if (r.fail) return false;
    if (!config_validate(cfg)) return false;

    // Inicializa GameState con la cfg validada (zeros, arrays dimensionados, etc.)
    gs_init(g, cfg);

    // (b) tick + fatal
    g.tick  = r.u32();
    g.fatal = static_cast<FatalReason>(r.u32());

    // (c) entities — encabezado
    const uint32_t cap_e = r.u32();
    const uint32_t alive = r.u32();
    const uint32_t ftop  = r.u32();
    if (r.fail)                       return false;
    if (cap_e != cfg.max_entities)    return false;
    if (cap_e > ENTITY_HARD_CAP)      return false;
    if (alive > cap_e)                return false;
    if (ftop  > cap_e)                return false;

    g.entities.capacity    = cap_e;
    g.entities.alive_count = alive;
    g.entities.free_top    = ftop;

    for (uint32_t k = 0; k < ftop; ++k) {
        const uint32_t idx = r.u32();
        if (r.fail) return false;
        if (idx >= cap_e) return false;
        g.entities.free_stack[k] = idx;
    }
    for (uint32_t i = 0; i < cap_e; ++i) {
        g.entities.generation[i] = r.u32();
        g.entities.alive[i]      = r.u8();
        g.entities.retired[i]    = r.u8();
    }
    if (r.fail) return false;

    // Conteo real de vivos debe coincidir con el header
    uint32_t actual_alive = 0;
    for (uint32_t i = 0; i < cap_e; ++i) {
        if (g.entities.alive[i] != 0) ++actual_alive;
    }
    if (actual_alive != alive) return false;

    // (d) Componentes de vivos — exactamente `alive` registros
    for (uint32_t n = 0; n < alive; ++n) {
        const uint32_t i = r.u32();
        if (r.fail) return false;
        if (i >= cap_e) return false;
        if (g.entities.alive[i] == 0) return false; // inconsistencia índice vs alive[]
        g.pos_x[i]      = r.i64();
        g.pos_y[i]      = r.i64();
        g.vel_x[i]      = r.i64();
        g.vel_y[i]      = r.i64();
        g.tgt_x[i]      = r.i64();
        g.tgt_y[i]      = r.i64();
        g.speed_mtpt[i] = r.i32();
        g.owner[i]      = r.u8();
        if (r.fail) return false;
    }

    // (e) pending — save v9 (Sprint 1.2, SPEC-004 §10.2): +u32 unit_id tras
    // unit_class, espejo exacto de gs_serialize. NOTA (deviación documentada,
    // mismo espíritu que unit_id de entidad/building_id/build_target en este
    // archivo): no se revalida aquí contra el catálogo (acepta cualquier u32;
    // apply_command ya comprueba bounds en caliente al aplicarse el comando).
    const uint32_t pcount = r.u32();
    if (r.fail) return false;
    if (pcount > PENDING_CAP) return false;
    g.pending.count = pcount;
    for (uint32_t k = 0; k < pcount; ++k) {
        auto& it = g.pending.items[k];
        it.effective_tick        = r.u32();
        const uint16_t emitter_raw = r.u16();
        const uint16_t type_raw    = r.u16();
        it.sequence              = r.u64();
        it.p.handle.index        = r.u32();
        it.p.handle.generation   = r.u32();
        it.p.x_raw               = r.i64();
        it.p.y_raw               = r.i64();
        it.p.speed_mtpt          = r.i32();
        it.p.hp                  = r.i32();
        it.p.attack              = r.i32();
        it.p.range_mt            = r.i32();
        it.p.unit_class          = r.u8();
        it.p.unit_id             = r.u32();  // v9 (SPEC-004 §10.2)
        if (r.fail) return false;
        if (emitter_raw >= 16)               return false;
        // CommandType ∈ {1..12} (Sprint 1.1: +PLACE_BUILDING/ASSIGN_BUILD;
        // Sprint 1.2, SPEC-004 §11.3/§12.3: +TRAIN_UNIT/SET_RALLY/
        // RESEARCH_TECH/EPOCH_UP).
        if (type_raw < 1 || type_raw > 12)   return false;
        it.emitter = emitter_raw;
        it.type    = static_cast<CommandType>(type_raw);
    }

    // (f) Por emisor: last_seq, mailbox count/dropped, ring reconstruido
    for (uint32_t e = 0; e < 16; ++e) {
        g.last_seq[e]               = r.u64();
        const uint32_t mcount       = r.u32();
        const uint64_t dropped      = r.u64();
        if (r.fail) return false;
        if (mcount > MAILBOX_CAP)   return false;
        g.mailbox[e].head    = 0;     // canónico al cargar: head siempre 0
        g.mailbox[e].count   = mcount;
        g.mailbox[e].dropped = dropped;
        for (uint32_t j = 0; j < mcount; ++j) {
            auto& rc = g.mailbox[e].ring[j];
            rc.sequence       = r.u64();
            rc.processed_tick = r.u32();
            rc.result         = static_cast<RejectReason>(r.u16());
        }
    }

    if (r.fail) return false;

    // (g) Visión: explored; visible queda a cero (derivada, se reconstruye).
    for (uint32_t p = 0; p < VIS_MAX_PLAYERS; ++p) {
        for (uint32_t wd = 0; wd < VIS_WORDS; ++wd) {
            g.vision.explored[p][wd] = r.u64();
        }
    }
    if (r.fail) return false;

    // (h) Flujo de navegación: cost_grid + flow_mode + goal. `flow` (derivada)
    // se fuerza a recomputar (flow_dirty = flow_has_goal) en vez de leerse.
    for (uint32_t i = 0; i < FF_CELLS; ++i) g.cost_grid[i] = r.u8();
    for (uint32_t i = 0; i < cap_e; ++i) g.flow_mode[i] = r.u8();
    g.flow_goal_cell = r.u32();
    g.flow_has_goal  = r.u8();
    if (r.fail) return false;
    g.flow_dirty = g.flow_has_goal;

    // (i) Combate (Sprint 0.3): 6 arrays, mismo orden que gs_serialize.
    for (uint32_t i = 0; i < cap_e; ++i) g.hp[i]         = r.i32();
    for (uint32_t i = 0; i < cap_e; ++i) g.max_hp[i]     = r.i32();
    for (uint32_t i = 0; i < cap_e; ++i) g.attack[i]     = r.i32();
    for (uint32_t i = 0; i < cap_e; ++i) g.range_mt[i]   = r.i32();
    for (uint32_t i = 0; i < cap_e; ++i) g.unit_class[i] = r.u8();
    for (uint32_t i = 0; i < cap_e; ++i) g.atk_cd[i]     = r.u16();
    if (r.fail) return false;

    // (j) Moral (Sprint 0.3): 2 arrays, mismo orden que gs_serialize.
    for (uint32_t i = 0; i < cap_e; ++i) g.morale[i]  = r.i32();
    for (uint32_t i = 0; i < cap_e; ++i) g.fleeing[i] = r.u8();
    if (r.fail) return false;

    // (j2) Catálogo (Sprint 0.4): unit_id, mismo orden que gs_serialize. NOTA
    // (deviación documentada): esta versión no re-valida unit_id contra un
    // catálogo en el momento del load (gs_deserialize no recibe
    // `DataCatalogV1` — ver RESULT del sprint); acepta cualquier u32. El
    // binding real (`g.catalog`) se re-enlaza aparte vía `gs_bind_catalog`
    // antes de que el estado vuelva a correr Step() con SPAWN_UNIT pendiente.
    for (uint32_t i = 0; i < cap_e; ++i) g.unit_id[i] = r.u32();
    if (r.fail) return false;

    // (k) Economía (Sprint 0.3): mismo orden que gs_serialize. Validación:
    // resource_idx/eco_carry_resource < 3, eco_state <= 2 (RETURN), assigned_deposit
    // < ECO_MAX_DEPOSITS o == ECO_NO_DEPOSIT.
    g.n_deposits = r.u32();
    if (r.fail || g.n_deposits > ECO_MAX_DEPOSITS) return false;
    for (uint32_t i = 0; i < ECO_MAX_DEPOSITS; ++i) {
        g.deposits[i].x_raw = r.i64();
        g.deposits[i].y_raw = r.i64();
        const uint8_t ridx = r.u8();
        if (ridx > 2u) return false;
        g.deposits[i].resource_idx = ridx;
        g.deposits[i].remaining = r.i32();
    }
    for (uint32_t e = 0; e < MAX_EMITTERS; ++e) {
        g.dropoff_x[e] = r.i64();
        g.dropoff_y[e] = r.i64();
        g.player_stock[e][0] = r.i64();
        g.player_stock[e][1] = r.i64();
        g.player_stock[e][2] = r.i64();
    }
    if (r.fail) return false;
    for (uint32_t i = 0; i < cap_e; ++i) {
        const uint8_t st = r.u8();
        if (st > static_cast<uint8_t>(EcoState::RETURN)) return false;
        g.eco_state[i] = static_cast<EcoState>(st);
    }
    for (uint32_t i = 0; i < cap_e; ++i) {
        const uint32_t ad = r.u32();
        if (ad != ECO_NO_DEPOSIT && ad >= ECO_MAX_DEPOSITS) return false;
        g.eco_assigned_deposit[i] = ad;
    }
    for (uint32_t i = 0; i < cap_e; ++i) g.eco_carry[i] = r.i32();
    for (uint32_t i = 0; i < cap_e; ++i) {
        const uint8_t cr = r.u8();
        if (cr > 2u) return false;
        g.eco_carry_resource[i] = cr;
    }
    if (r.fail) return false;

    // (l) Edificios (Sprint 1.1, SPEC-004 §3/§8): mismo orden que gs_serialize.
    // NOTA (deviación documentada, mismo espíritu que unit_id en Sprint 0.4):
    // building_id/build_target no se revalidan aquí contra un catálogo/rango
    // de entidades — aceptan cualquier valor u32; el binding real y los
    // sistemas que los consumen (construction_system, dropoff) ya comprueban
    // bounds/alive en caliente. entity_kind SÍ se valida (0/1: es un flag
    // estructural barato de comprobar, no una referencia externa).
    for (uint32_t i = 0; i < cap_e; ++i) {
        const uint8_t ek = r.u8();
        if (ek > 1u) return false;
        g.entity_kind[i] = ek;
    }
    for (uint32_t i = 0; i < cap_e; ++i) g.building_id[i]   = r.u32();
    for (uint32_t i = 0; i < cap_e; ++i) g.build_progress[i] = r.u32();
    for (uint32_t i = 0; i < cap_e; ++i) g.bld_anchor_tx[i] = r.u16();
    for (uint32_t i = 0; i < cap_e; ++i) g.bld_anchor_ty[i] = r.u16();
    for (uint32_t i = 0; i < cap_e; ++i) g.build_target[i]  = r.u32();
    if (r.fail) return false;

    // (m) Producción y tecnología (Sprint 1.2, SPEC-004 §11.2/§12.2 — save
    // v10): mismo orden que gs_serialize. NOTA (deviación documentada, mismo
    // espíritu que unit_id/building_id/build_target en este archivo):
    // prod_queue/research_tech no se revalidan aquí contra el catálogo —
    // aceptan cualquier u32; production_system/research_system ya comprueban
    // bounds en caliente al consumirlos.
    for (uint32_t i = 0; i < cap_e; ++i) {
        for (uint32_t k = 0; k < PROD_QUEUE_CAP; ++k) g.prod_queue[i][k] = r.u32();
    }
    for (uint32_t i = 0; i < cap_e; ++i) {
        const uint8_t pc = r.u8();
        if (pc > PROD_QUEUE_CAP) return false;
        g.prod_count[i] = pc;
    }
    for (uint32_t i = 0; i < cap_e; ++i) g.prod_progress[i] = r.u32();
    for (uint32_t i = 0; i < cap_e; ++i) g.rally_x[i] = r.i64();
    for (uint32_t i = 0; i < cap_e; ++i) g.rally_y[i] = r.i64();
    for (uint32_t i = 0; i < cap_e; ++i) g.rally_set[i] = r.u8();
    if (r.fail) return false;
    for (uint32_t e = 0; e < MAX_EMITTERS; ++e) g.pop_used[e] = r.i32();
    for (uint32_t e = 0; e < MAX_EMITTERS; ++e) {
        for (uint32_t k = 0; k < TECH_WORDS; ++k) g.player_techs[e][k] = r.u64();
    }
    for (uint32_t e = 0; e < MAX_EMITTERS; ++e) {
        for (uint32_t k = 0; k < CAP_WORDS; ++k) g.player_caps[e][k] = r.u64();
    }
    for (uint32_t e = 0; e < MAX_EMITTERS; ++e) g.player_epoch[e] = r.u8();
    for (uint32_t e = 0; e < MAX_EMITTERS; ++e) g.epoch_initial[e] = r.u8();
    if (r.fail) return false;
    for (uint32_t i = 0; i < cap_e; ++i) g.research_tech[i] = r.u32();
    for (uint32_t i = 0; i < cap_e; ++i) g.research_progress[i] = r.u32();
    if (r.fail) return false;

    // (n) Victoria/derrota (Sprint 1.4, SPEC-005 §6/§7 — save v11): mismo
    // orden que gs_serialize. Validación: game_over booleano estricto;
    // winner es o bien 0xFF (empate/pendiente) o un índice de jugador dentro
    // de cfg.player_count (ya validado arriba por config_validate);
    // participants_mask no debe tener bits fuera de [0, player_count) —
    // cualquier otro valor es un archivo corrupto/hostil, mismo rigor que
    // entity_kind/eco_state/resource_idx más arriba en esta función.
    const uint8_t go = r.u8();
    if (go > 1u) return false;
    g.game_over = go;
    const uint8_t win = r.u8();
    if (win != 0xFFu && win >= cfg.player_count) return false;
    g.winner = win;
    const uint16_t pmask = r.u16();
    if (r.fail) return false;
    if ((pmask >> cfg.player_count) != 0u) return false;
    g.participants_mask = pmask;

    // Frontera de save = inicio de tick → no hay destrucciones pendientes
    g.destroy_count = 0;
    return true;
}

} // namespace chunsa
