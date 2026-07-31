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

// Sprint 0.4 (SPEC-002 §8.5): bump a v2 — el stream gana `unit_id` (identidad
// del dato con el que se spawneó cada entidad). Cambio INTENCIONAL: el dominio
// v1 nunca incluyó unit_id (no existía), así que dos GameState solo pueden
// diferir en checksum si unit_id realmente difiere; el resto del stream v1
// se conserva sin reordenar. Deviación documentada frente al brief: se
// mantiene el símbolo `state_checksum_v1` (no se añade un `state_checksum_v2`
// separado) para no tocar los ~6 call sites existentes — el propio dominio
// hasheado ya se identifica como "CHUNSA_STATE_V2" y CHECKSUM_ALGO_VERSION=2.
//
// Sprint 1.1 (SPEC-004 §8): bump a v3 — el stream gana los 6 arrays de
// edificios (§3), añadidos AL FINAL tras todo lo existente (orden intencional,
// nada reordenado). Cambio de dominio DELIBERADO: los checksums golden se
// regeneran una vez (mismo procedimiento del bump v1→v2 de Sprint 0.4); ver
// RESULT del sprint — la TRAYECTORIA de los escenarios previos no cambia, solo
// el dominio hasheado. Se mantiene igual el símbolo `state_checksum_v1` por la
// misma razón que en Sprint 0.4 (no tocar call sites).
//
// Sprint 1.2 (SPEC-004 §10.2): bump a v4 — VERIFICADO que el dominio v3 NO
// cubría `pending.items[].p.unit_id` (el bucle de la agenda solo hasheaba
// effective_tick/emitter/type/sequence/handle/x_raw/y_raw/speed_mtpt; nunca
// unit_id, aun cuando el resto de CmdPayload — hp/attack/range_mt/unit_class —
// tampoco se hasheaba y sigue fuera de alcance de este bump, que se limita
// estrictamente al gap contratado). Un SPAWN_UNIT/SPAWN_CITIZEN/PLACE_BUILDING
// con unit_id != 0 aún PENDIENTE (no aplicado) no se distinguía por checksum
// de uno con unit_id == 0 — gap gemelo del D8 de Sprint 1.1 pero en la agenda
// del ESTADO EN MEMORIA, no en un formato de archivo. Se añade `h.u32(unit_id)`
// al final de cada ítem de la agenda (mismo patrón append que el resto de este
// bucle). No hay golden-checksum de estado persistido en este repo (a
// diferencia de los vectores Fixed64 de tests/determinism/golden, que son
// puramente aritméticos y no tocan GameState) — todos los tests de estado
// comparan dos corridas EN VIVO entre sí, así que "regenerar" el golden no
// requiere tocar ningún archivo aparte de este bump de versión/dominio (ver
// RESULT del sprint, punto 2).
// Sprint 1.2 (SPEC-004 §13): bump a v5 — el stream gana los arrays de
// producción/tecnología/épocas (§11.2/§12.2: prod_queue, prod_count,
// prod_progress, rally_x/y, rally_set, pop_used, player_techs, player_caps,
// player_epoch, epoch_initial —deviación documentada, ver game_state.hpp—,
// research_tech, research_progress), añadidos AL FINAL tras todo lo v4 (orden
// intencional). Cambio de dominio DELIBERADO: no hay golden-checksum de
// estado persistido en este repo (mismo precedente que los bumps v1→v2,
// v2→v3 y v3→v4 — ver checksum.hpp de esos sprints); todos los tests de
// estado comparan dos corridas EN VIVO entre sí. Se mantiene el símbolo
// `state_checksum_v1` por la misma razón que en sprints anteriores (no tocar
// call sites).
//
// Sprint 1.4 (SPEC-005 §6/§7): bump a v6 — el stream gana `game_over`,
// `winner` y `participants_mask` (condición de victoria/derrota + la
// definición operativa de "jugador activo", ver step.hpp::detail::
// victory_check y el RESULT del sprint), añadidos AL FINAL tras todo lo v5
// (orden intencional). Cambio de dominio DELIBERADO: mismo precedente que
// los bumps v1→v5 anteriores — no hay golden-checksum de estado persistido
// en este repo, todos los tests de estado comparan dos corridas EN VIVO
// entre sí, así que "regenerar" el golden no requiere tocar ningún archivo
// aparte de este bump. La TRAYECTORIA de los escenarios previos (posiciones/
// aceptación/rechazo de comandos) no cambia — solo el dominio hasheado y los
// 3 campos nuevos, que para cualquier escenario que nunca alcance game_over
// quedan en su valor inicial (0/0xFF/0) durante toda la corrida. Se mantiene
// el símbolo `state_checksum_v1` por la misma razón que en sprints
// anteriores (no tocar call sites).
//
// Sprint 1.6B (SPEC-004 §17/§20): bump a v7 — el stream gana `player_civ`
// (identidad de civilización por jugador), añadido AL FINAL tras todo lo v6
// (orden intencional). Cambio de dominio DELIBERADO: mismo precedente que
// los bumps v1→v6 anteriores — no hay golden-checksum de estado persistido
// en este repo, todos los tests de estado comparan dos corridas EN VIVO
// entre sí (nunca contra un valor de checksum hardcodeado de `main`), así
// que "regenerar" el golden no requiere tocar ningún archivo aparte de este
// bump. La TRAYECTORIA de los escenarios que no asignan civ (sintéticos,
// economía 0.3, skirmish militar y eco) NO cambia — player_civ queda en
// INVALID_CIV_ID (gs_init) durante toda la corrida para esos escenarios, así
// que el ÚNICO efecto observable del bump es el valor del checksum en sí
// (esperado e intencional), no la trayectoria subyacente (posiciones/hp/
// stock), que sigue siendo bit-idéntica — ver RESULT del sprint para el
// dump pre/post. Se mantiene el símbolo `state_checksum_v1` por la misma
// razón que en sprints anteriores (no tocar call sites).
//
// Sprint 1.7 (SPEC-004 §22.5): bump a v8 — el stream gana citizen_task al
// final para todos los slots y usa universalmente el dominio V8, sin depender
// del contenido del estado. El bump V7→V8 invalida por diseño todos los
// baselines previos, aunque la trayectoria funcional permanezca bit-idéntica.
//
// Sprint 1.8A (SPEC-007 §9.3/§11): bump a v9 — el dominio de player_stock
// pasa universalmente de 3 a RESOURCE_COUNT entradas por emisor. Incluso un
// estado con 3..31 en cero usa CHUNSA_STATE_V9; no hay rutas condicionales por
// contenido. La trayectoria sigue intacta y todos los baselines cambian solo
// por este nuevo dominio.
inline constexpr uint32_t CHECKSUM_ALGO_VERSION = 12;  // Sprint 1.9C: RESOURCE_COUNT 32->64
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
    const EntityTable& t = g.entities;
    h.bytes("CHUNSA_STATE_V9", 15);
    h.u32(CHECKSUM_ALGO_VERSION);
    h.u32(g.tick);
    h.u32(static_cast<uint32_t>(g.fatal));

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
        h.u32(c.p.unit_id);  // Sprint 1.2 (SPEC-004 §10.2): gap cerrado, ver bump v4 arriba.
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
    // Catálogo (Sprint 0.4, SPEC-002 §8.5): unit_id por índice, todos los
    // slots (misma convención que combate/moral). `catalog` (puntero binding)
    // NUNCA se hashea — no es estado simulado.
    for (uint32_t i = 0; i < t.capacity; ++i) h.u32(g.unit_id[i]);
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
        for (uint32_t resource = 0; resource < RESOURCE_COUNT; ++resource) {
            h.i64(g.player_stock[e][resource]);
        }
    }
    for (uint32_t i = 0; i < t.capacity; ++i) h.u8(static_cast<uint8_t>(g.eco_state[i]));
    for (uint32_t i = 0; i < t.capacity; ++i) h.u32(g.eco_assigned_deposit[i]);
    for (uint32_t i = 0; i < t.capacity; ++i) h.i32(g.eco_carry[i]);
    for (uint32_t i = 0; i < t.capacity; ++i) h.u8(g.eco_carry_resource[i]);
    // Edificios (Sprint 1.1, SPEC-004 §3/§8): AL FINAL, tras todo lo v7, en el
    // mismo orden en que aparecen en §3. Todos los slots (misma convención que
    // unit_id/combate/moral), sin gate de alive[].
    for (uint32_t i = 0; i < t.capacity; ++i) h.u8(g.entity_kind[i]);
    for (uint32_t i = 0; i < t.capacity; ++i) h.u32(g.building_id[i]);
    for (uint32_t i = 0; i < t.capacity; ++i) h.u32(g.build_progress[i]);
    for (uint32_t i = 0; i < t.capacity; ++i) h.u16(g.bld_anchor_tx[i]);
    for (uint32_t i = 0; i < t.capacity; ++i) h.u16(g.bld_anchor_ty[i]);
    for (uint32_t i = 0; i < t.capacity; ++i) h.u32(g.build_target[i]);
    // Producción y tecnología (Sprint 1.2, SPEC-004 §11.2/§12.2/§13): AL
    // FINAL, tras todo lo v4, en el mismo orden que aparecen en §11.2/§12.2
    // (+ epoch_initial, deviación documentada en game_state.hpp). Todos los
    // slots (misma convención que unit_id/edificios), sin gate de alive[].
    for (uint32_t i = 0; i < t.capacity; ++i) {
        for (uint32_t k = 0; k < PROD_QUEUE_CAP; ++k) h.u32(g.prod_queue[i][k]);
    }
    for (uint32_t i = 0; i < t.capacity; ++i) h.u8(g.prod_count[i]);
    for (uint32_t i = 0; i < t.capacity; ++i) h.u32(g.prod_progress[i]);
    for (uint32_t i = 0; i < t.capacity; ++i) h.i64(g.rally_x[i]);
    for (uint32_t i = 0; i < t.capacity; ++i) h.i64(g.rally_y[i]);
    for (uint32_t i = 0; i < t.capacity; ++i) h.u8(g.rally_set[i]);
    for (uint32_t e = 0; e < MAX_EMITTERS; ++e) h.i32(g.pop_used[e]);
    for (uint32_t e = 0; e < MAX_EMITTERS; ++e) {
        for (uint32_t w = 0; w < TECH_WORDS; ++w) h.u64(g.player_techs[e][w]);
    }
    for (uint32_t e = 0; e < MAX_EMITTERS; ++e) {
        for (uint32_t w = 0; w < CAP_WORDS; ++w) h.u64(g.player_caps[e][w]);
    }
    for (uint32_t e = 0; e < MAX_EMITTERS; ++e) h.u8(g.player_epoch[e]);
    for (uint32_t e = 0; e < MAX_EMITTERS; ++e) h.u8(g.epoch_initial[e]);
    for (uint32_t i = 0; i < t.capacity; ++i) h.u32(g.research_tech[i]);
    for (uint32_t i = 0; i < t.capacity; ++i) h.u32(g.research_progress[i]);
    // Sprint 1.9 (SPEC-007 §12.5): la fabricacion entra en el dominio del
    // checksum, AL FINAL como todo lo anadido despues (append-only).
    for (uint32_t i = 0; i < t.capacity; ++i) h.u32(g.craft_recipe[i]);
    for (uint32_t i = 0; i < t.capacity; ++i) h.u32(g.craft_progress[i]);
    // Sprint 1.13 (SPEC-004 §24.6): las ordenes entran en el dominio.
    for (uint32_t i = 0; i < t.capacity; ++i) h.u32(g.attack_target[i].index);
    for (uint32_t i = 0; i < t.capacity; ++i) h.u32(g.attack_target[i].generation);
    for (uint32_t i = 0; i < t.capacity; ++i) h.u8(g.order_mode[i]);
    // Proyectiles: SOLO los vivos, en orden. Hashear el array entero metería
    // basura de slots retirados y haría el checksum dependiente de la historia
    // en vez del estado.
    h.u32(g.n_projectiles);
    for (uint32_t k = 0; k < g.n_projectiles && k < PROJECTILE_HARD_CAP; ++k) {
        const Projectile& p = g.projectiles[k];
        h.u64(static_cast<uint64_t>(p.x_raw));
        h.u64(static_cast<uint64_t>(p.y_raw));
        h.u64(static_cast<uint64_t>(p.vel_x));
        h.u64(static_cast<uint64_t>(p.vel_y));
        h.u32(p.target.index);
        h.u32(p.target.generation);
        h.u64(static_cast<uint64_t>(p.aim_x));
        h.u64(static_cast<uint64_t>(p.aim_y));
        h.u32(static_cast<uint32_t>(p.damage));
        h.u8(p.owner);
        h.u8(p.guided);
    }
    // Victoria/derrota (Sprint 1.4, SPEC-005 §6/§7): AL FINAL, tras todo lo
    // v5. Escalares del partido (no por-slot): un único u8/u8/u16.
    h.u8(g.game_over);
    h.u8(g.winner);
    h.u16(g.participants_mask);
    // Civilización por jugador (Sprint 1.6B, SPEC-004 §17/§20): AL FINAL,
    // tras todo lo v6. Escalar por jugador (no por-entidad), todos los
    // MAX_EMITTERS slots (misma convención que pop_used/player_epoch).
    for (uint32_t e = 0; e < MAX_EMITTERS; ++e) h.u32(g.player_civ[e]);
    // Tarea explícita del ciudadano (Sprint 1.7, SPEC-004 §22.5): AL FINAL,
    // todos los slots, sin condicionales por contenido.
    for (uint32_t i = 0; i < t.capacity; ++i) h.u8(g.citizen_task[i]);
    return h.digest();
}

}  // namespace chunsa
