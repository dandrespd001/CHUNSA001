#pragma once
#include <cstdint>

#include "chunsa/game_state.hpp"
#include "chunsa/checksum.hpp"

namespace chunsa { inline constexpr uint32_t VIS_RADIUS_TILES = 8; }  // [DEFAULT] radio de visión v1

// chunsa_sim_core — ciclo normativo de Step() y MovementSystemV1.
// SPEC-001 §2 (orden total) y §12 (movimiento congelado). Autor: Arquitecto.
// Subconjunto 0.1A del pipeline: Ingesta → Aplicación de Commands →
// Movement → SpatialHashRebuild → Destroy → Checksum → tick++.
// (Combat/Moral/Vision/Economy llegan en 0.3-0.4 en sus fases de §8.)

namespace chunsa {

struct StepResult {
    uint32_t completed_tick;
    uint64_t checksum;        // válido solo si checksum_computed
    bool checksum_computed;
    uint32_t accepted;
    uint32_t rejected;
};

namespace detail {

inline void receipt(GameState& g, uint16_t emitter, uint64_t seq, RejectReason r) noexcept {
    if (emitter < MAX_EMITTERS) {
        mailbox_push(g.mailbox[emitter], CommandReceipt{seq, g.tick, r});
    }
}

// Validación y aplicación de UN comando debido (función pura de estado+comando).
inline RejectReason apply_command(GameState& g, const ScheduledCommand& c) noexcept {
    switch (c.type) {
        case CommandType::SPAWN_DEBUG: {
            const Vec2Fx p{Fx{c.p.x_raw}, Fx{c.p.y_raw}};
            if (!world_contains(p) || c.p.speed_mtpt <= 0 || c.p.speed_mtpt > 100000) {
                return RejectReason::MALFORMED;
            }
            const EntityHandle h = et_spawn(g.entities);
            if (handle_eq(h, NULL_HANDLE)) return RejectReason::POOL_EXHAUSTED;
            const uint32_t i = h.index;
            g.pos_x[i] = c.p.x_raw; g.pos_y[i] = c.p.y_raw;
            g.tgt_x[i] = c.p.x_raw; g.tgt_y[i] = c.p.y_raw;
            g.vel_x[i] = 0; g.vel_y[i] = 0;
            g.speed_mtpt[i] = c.p.speed_mtpt;
            g.owner[i] = static_cast<uint8_t>(c.emitter);
            return RejectReason::ACCEPTED;
        }
        case CommandType::MOVE_TO: {
            if (!et_is_alive(g.entities, c.p.handle)) return RejectReason::INVALID_ENTITY;
            const uint32_t i = c.p.handle.index;
            if (g.owner[i] != c.emitter) return RejectReason::NOT_OWNER;
            // Cota de mundo validada EN LA APLICACIÓN (Anexo B.2 de SPEC-001):
            // jamás llega una coordenada fuera de cota al sistema de movimiento.
            const Vec2Fx tgt{Fx{c.p.x_raw}, Fx{c.p.y_raw}};
            if (!world_contains(tgt)) return RejectReason::MALFORMED;
            g.tgt_x[i] = c.p.x_raw;  // un segundo target REEMPLAZA al anterior (§12)
            g.tgt_y[i] = c.p.y_raw;
            return RejectReason::ACCEPTED;
        }
        case CommandType::DESTROY_DEBUG: {
            if (!et_is_alive(g.entities, c.p.handle)) return RejectReason::INVALID_ENTITY;
            const uint32_t i = c.p.handle.index;
            if (g.owner[i] != c.emitter) return RejectReason::NOT_OWNER;
            if (g.destroy_count >= PENDING_CAP) return RejectReason::ILLEGAL_STATE;
            et_mark_dead(g.entities, i);           // los sistemas de este tick ya no la ven
            g.destroy_batch[g.destroy_count++] = i; // reciclaje al final del tick (paso 6)
            return RejectReason::ACCEPTED;
        }
        case CommandType::FLOW_MOVE: {
            const Vec2Fx goal{Fx{c.p.x_raw}, Fx{c.p.y_raw}};
            if (!world_contains(goal)) return RejectReason::MALFORMED;
            const uint32_t tx = static_cast<uint32_t>(c.p.x_raw >> 16);
            const uint32_t ty = static_cast<uint32_t>(c.p.y_raw >> 16);
            // El flow field es 256×256; un goal fuera de él no es representable.
            // (Endurecimiento del Arquitecto: el contrato usaba world_contains
            //  (cota 8192), pero el campo es FF_AXIS=256 — evita índice inválido.)
            if (tx >= FF_AXIS || ty >= FF_AXIS) return RejectReason::MALFORMED;
            g.flow_goal_cell = ty * FF_AXIS + tx;
            g.flow_has_goal = 1;
            g.flow_dirty = 1;
            for (uint32_t i = 0; i < g.entities.capacity; ++i) {
                if (g.entities.alive[i] && g.owner[i] == c.emitter) g.flow_mode[i] = 1u;
            }
            return RejectReason::ACCEPTED;
        }
    }
    return RejectReason::MALFORMED;
}

// MovementSystemV1 — CONGELADO (SPEC-001 §12).
inline void movement_v1(GameState& g) noexcept {
    if (g.flow_dirty && g.flow_has_goal) {
        ff_compute(g.flow, g.cost_grid, 256u, 256u,
                  g.flow_goal_cell % FF_AXIS, g.flow_goal_cell / FF_AXIS);
        g.flow_dirty = 0;
    }

    const EntityTable& t = g.entities;
    for (uint32_t i = 0; i < t.capacity; ++i) {
        if (!t.alive[i]) continue;
        if (g.flow_mode[i] == 1u && g.flow_has_goal) {
            // Clamp al rango del flow field (256): la cota de mundo (8192) es mayor,
            // así que una unidad más allá del tile 255 leería fuera de dir_x/dir_y.
            // (Endurecimiento del Arquitecto sobre el contrato original.)
            uint32_t tx = static_cast<uint32_t>(g.pos_x[i] >> 16);
            uint32_t ty = static_cast<uint32_t>(g.pos_y[i] >> 16);
            if (tx >= FF_AXIS) tx = FF_AXIS - 1u;
            if (ty >= FF_AXIS) ty = FF_AXIS - 1u;
            const uint32_t cell = ty * FF_AXIS + tx;
            const int8_t dx = g.flow.dir_x[cell];
            const int8_t dy = g.flow.dir_y[cell];
            if (dx == 0 && dy == 0) {           // goal o inalcanzable → detener
                g.vel_x[i] = 0; g.vel_y[i] = 0;
                continue;
            }
            const int64_t step_fx = (static_cast<int64_t>(g.speed_mtpt[i]) * FX_ONE_RAW) / 1000;
            const Vec2Fx dir = normalize_v1(Vec2Fx{Fx{static_cast<int64_t>(dx) * FX_ONE_RAW},
                                                   Fx{static_cast<int64_t>(dy) * FX_ONE_RAW}}, g.fatal);
            const Fx vx = fx_mul(dir.x, Fx{step_fx}, g.fatal);
            const Fx vy = fx_mul(dir.y, Fx{step_fx}, g.fatal);
            g.vel_x[i] = vx.raw; g.vel_y[i] = vy.raw;
            g.pos_x[i] = fx_add(Fx{g.pos_x[i]}, vx, g.fatal).raw;
            g.pos_y[i] = fx_add(Fx{g.pos_y[i]}, vy, g.fatal).raw;
            // Clamp defensivo a cota de mundo [0, WORLD_RAW_MAX) para no salir del grid.
            if (g.pos_x[i] < 0) g.pos_x[i] = 0;
            if (g.pos_y[i] < 0) g.pos_y[i] = 0;
            if (g.pos_x[i] >= WORLD_RAW_MAX) g.pos_x[i] = WORLD_RAW_MAX - 1;
            if (g.pos_y[i] >= WORLD_RAW_MAX) g.pos_y[i] = WORLD_RAW_MAX - 1;
            continue;
        }
        // step_fx = trunc_to_zero(speed_mtpt * FX_ONE / 1000) — enteros positivos.
        const int64_t step_fx = (static_cast<int64_t>(g.speed_mtpt[i]) * FX_ONE_RAW) / 1000;
        const Vec2Fx pos{Fx{g.pos_x[i]}, Fx{g.pos_y[i]}};
        const Vec2Fx tgt{Fx{g.tgt_x[i]}, Fx{g.tgt_y[i]}};
        const int64_t dx = tgt.x.raw - pos.x.raw;  // en-mundo: |delta| < 2^30, sin overflow
        const int64_t dy = tgt.y.raw - pos.y.raw;
        if (dx == 0 && dy == 0) {
            g.vel_x[i] = 0; g.vel_y[i] = 0;
            continue;
        }
        const uint64_t ax = mag_u64(dx), ay = mag_u64(dy);
        const uint64_t d2 = ax * ax + ay * ay;
        const uint64_t s2 = static_cast<uint64_t>(step_fx) * static_cast<uint64_t>(step_fx);
        if (d2 <= s2) {
            // SNAP: llega este tick (también evita normalizar vectores diminutos).
            g.pos_x[i] = tgt.x.raw; g.pos_y[i] = tgt.y.raw;
            g.vel_x[i] = 0; g.vel_y[i] = 0;
            continue;
        }
        const Vec2Fx dir = normalize_v1(Vec2Fx{Fx{dx}, Fx{dy}}, g.fatal);
        const Fx vx = fx_mul(dir.x, Fx{step_fx}, g.fatal);
        const Fx vy = fx_mul(dir.y, Fx{step_fx}, g.fatal);
        g.vel_x[i] = vx.raw; g.vel_y[i] = vy.raw;
        g.pos_x[i] = fx_add(pos.x, vx, g.fatal).raw;
        g.pos_y[i] = fx_add(pos.y, vy, g.fatal).raw;
    }
}

}  // namespace detail

// Ejecuta el tick t = g.tick con el corte de ingesta `batch` (RawCommands
// capturados por el caller — adaptador o CLI). Orden total de SPEC-001 §2.
inline StepResult step(GameState& g, const RawCommand* batch, uint32_t n) noexcept {
    StepResult res{};
    const uint32_t t = g.tick;
    res.completed_tick = t;

    if (g.fatal == FatalReason::NONE) {
        // (3) Normalizar RawCommands → agenda canónica.
        for (uint32_t k = 0; k < n; ++k) {
            const RawCommand& rc = batch[k];
            if (rc.emitter >= MAX_EMITTERS) { ++res.rejected; continue; }
            if (rc.sequence <= g.last_seq[rc.emitter]) {
                detail::receipt(g, rc.emitter, rc.sequence, RejectReason::SEQUENCE_REJECTED);
                ++res.rejected;
                continue;
            }
            uint32_t eff = rc.target_tick;
            const uint32_t min_eff = t + g.cfg.human_input_delay_ticks;  // §6.2
            if (eff < min_eff) eff = min_eff;
            if (eff > t + g.cfg.max_future_command_ticks) {
                detail::receipt(g, rc.emitter, rc.sequence, RejectReason::OUT_OF_WINDOW);
                ++res.rejected;
                continue;
            }
            ScheduledCommand sc{eff, rc.emitter, rc.type, rc.sequence, rc.p};
            if (!pcs_insert(g.pending, sc)) {
                detail::receipt(g, rc.emitter, rc.sequence, RejectReason::POOL_EXHAUSTED);
                ++res.rejected;
                continue;
            }
            g.last_seq[rc.emitter] = rc.sequence;
        }

        // (4) Aplicar los debidos en orden canónico (ya ordenados en la agenda).
        const uint32_t due_n = pcs_take_due(g.pending, t, g.due, PENDING_CAP);
        for (uint32_t k = 0; k < due_n; ++k) {
            const RejectReason r = detail::apply_command(g, g.due[k]);
            detail::receipt(g, g.due[k].emitter, g.due[k].sequence, r);
            if (r == RejectReason::ACCEPTED) ++res.accepted; else ++res.rejected;
        }

        // (5) Sistemas del tick (subconjunto 0.1A).
        detail::movement_v1(g);
        sh_rebuild(g.shash, g.pos_x, g.pos_y, g.entities.alive, g.entities.capacity);

        // Visión en su fase (SPEC-001 §8: t % 4 == 1). La actualización vive
        // aquí (no en vision.hpp) para evitar el ciclo de includes con GameState.
        if (t % 4u == 1u) {
            VisionGrid& vg = g.vision;
            for (uint32_t p = 0; p < VIS_MAX_PLAYERS; ++p)
                for (uint32_t wd = 0; wd < VIS_WORDS; ++wd)
                    vg.visible[p][wd] = 0;
            for (uint32_t i = 0; i < g.entities.capacity; ++i) {
                if (!g.entities.alive[i]) continue;
                vis_mark_circle(vg, g.owner[i], g.pos_x[i], g.pos_y[i], VIS_RADIUS_TILES);
            }
        }

        // (6) DESTROY: ordenar ASC por índice (inserción; batch pequeño) y reciclar.
        for (uint32_t a = 1; a < g.destroy_count; ++a) {
            const uint32_t v = g.destroy_batch[a];
            uint32_t b = a;
            while (b > 0 && g.destroy_batch[b - 1] > v) {
                g.destroy_batch[b] = g.destroy_batch[b - 1];
                --b;
            }
            g.destroy_batch[b] = v;
        }
        for (uint32_t a = 0; a < g.destroy_count; ++a) {
            const uint32_t i = g.destroy_batch[a];
            zero_components(g, i);
            et_release_index(g.entities, i);
        }
        g.destroy_count = 0;
    }

    // (7) Checksum en su fase: t % N == N-1 (SPEC-001 §8, fe de erratas).
    const uint16_t N = g.cfg.checksum_every_ticks;
    if (t % N == static_cast<uint32_t>(N - 1)) {
        res.checksum = state_checksum_v1(g);
        res.checksum_computed = true;
    }

    // (9) Avanza el reloj: el estado queda listo para el tick t+1.
    g.tick = t + 1;
    return res;
}

}  // namespace chunsa
