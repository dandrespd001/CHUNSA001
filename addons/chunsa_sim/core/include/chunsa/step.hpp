#pragma once
#include <cstdint>

#include "chunsa/game_state.hpp"
#include "chunsa/checksum.hpp"

namespace chunsa { inline constexpr uint32_t VIS_RADIUS_TILES = 8; }  // [DEFAULT] radio de visión v1
namespace chunsa { inline constexpr uint16_t ATK_COOLDOWN_TICKS = 10; }  // 0.5s @ 20Hz (Sprint 0.3)

namespace chunsa {
inline constexpr int32_t MORALE_MAX = 100;
inline constexpr int32_t MORALE_PANIC = 20;    // <= ⇒ entra en pánico
inline constexpr int32_t MORALE_RALLY = 50;    // >= ⇒ deja de huir
inline constexpr int32_t MORALE_DROP = 8;      // baja/tick si en desventaja
inline constexpr int32_t MORALE_REGEN = 2;     // sube/tick si a salvo
inline constexpr uint32_t MORALE_RADIUS_CELLS = 1;  // celda + vecinas

// Aggro v1 (Sprint 0.3+): radio de adquisición de objetivos para la
// persecución automática. 10 tiles → anillo de ±5 celdas del spatial hash
// (celda = 2 tiles). Solo unidades con attack > 0 participan, lo que excluye
// por construcción a SPAWN_DEBUG (attack queda en 0) y con ello preserva los
// vectores golden y los escenarios de record/verify sin cambio alguno.
inline constexpr int32_t  AGGRO_RANGE_MT    = 10000;  // 10 tiles
inline constexpr uint32_t AGGRO_RADIUS_CELLS = 5;
}  // namespace chunsa

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
        case CommandType::SPAWN_UNIT: {
            const Vec2Fx p{Fx{c.p.x_raw}, Fx{c.p.y_raw}};
            const bool combat_ok = c.p.hp > 0 && c.p.attack >= 0
                                 && c.p.range_mt >= 0 && c.p.unit_class <= 2;
            if (!world_contains(p) || !combat_ok) {
                return RejectReason::MALFORMED;
            }
            const EntityHandle h = et_spawn(g.entities);
            if (handle_eq(h, NULL_HANDLE)) return RejectReason::POOL_EXHAUSTED;
            const uint32_t i = h.index;
            g.pos_x[i] = c.p.x_raw; g.pos_y[i] = c.p.y_raw;
            g.tgt_x[i] = c.p.x_raw; g.tgt_y[i] = c.p.y_raw;
            g.vel_x[i] = 0; g.vel_y[i] = 0;
            g.owner[i] = static_cast<uint8_t>(c.emitter);
            g.hp[i] = g.max_hp[i] = c.p.hp;
            g.attack[i] = c.p.attack;
            g.range_mt[i] = c.p.range_mt;
            g.unit_class[i] = c.p.unit_class;
            g.atk_cd[i] = 0;
            // Endurecimiento del Arquitecto: v1 congelaba speed_mtpt=0, dejando
            // la huida de morale_system sin efecto (unidades de combate no se
            // desplazaban). Usar la velocidad del payload permite huir de verdad
            // sin romper el combate estático (SPAWN_UNIT también fija tgt=pos,
            // así que una unidad en reposo no migra hacia el origen por seek).
            g.speed_mtpt[i] = c.p.speed_mtpt;
            g.morale[i] = MORALE_MAX;
            g.fleeing[i] = 0;
            return RejectReason::ACCEPTED;
        }
        case CommandType::SPAWN_CITIZEN: {
            // Ciudadano económico: unit_class=3 lo excluye de combat_system (ambos
            // lados: atacante y objetivo — ver el guard `> 2` allí). hp nominal, no
            // se daña en v1 (nada apunta a class>2), pero se mantiene coherente con
            // la EntityTable (vivo/muerto) por si el futuro añade amenazas.
            const Vec2Fx p{Fx{c.p.x_raw}, Fx{c.p.y_raw}};
            if (!world_contains(p) || c.p.speed_mtpt <= 0) {
                return RejectReason::MALFORMED;
            }
            const EntityHandle h = et_spawn(g.entities);
            if (handle_eq(h, NULL_HANDLE)) return RejectReason::POOL_EXHAUSTED;
            const uint32_t i = h.index;
            g.pos_x[i] = c.p.x_raw; g.pos_y[i] = c.p.y_raw;
            g.tgt_x[i] = c.p.x_raw; g.tgt_y[i] = c.p.y_raw;
            g.vel_x[i] = 0; g.vel_y[i] = 0;
            g.owner[i] = static_cast<uint8_t>(c.emitter);
            g.hp[i] = g.max_hp[i] = 20;
            g.attack[i] = 0; g.range_mt[i] = 0;
            g.unit_class[i] = 3;  // citizen: excluido de combat_system
            g.atk_cd[i] = 0;
            g.speed_mtpt[i] = c.p.speed_mtpt;
            g.morale[i] = MORALE_MAX;
            g.fleeing[i] = 0;
            g.eco_state[i] = EcoState::SEEK;
            g.eco_assigned_deposit[i] = ECO_NO_DEPOSIT;
            g.eco_carry[i] = 0;
            g.eco_carry_resource[i] = 0;
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
        // Endurecimiento del Arquitecto (Sprint 0.3, economía): los ciudadanos
        // (unit_class==3) NO usan seek/flujo/huida — su movimiento es propiedad
        // exclusiva de economy_system (que corre más tarde en el mismo tick).
        // Sin este guard, tgt_x/tgt_y queda congelado en la posición de spawn y
        // esta rama de seek "corrige" cada tick el avance de economy_system de
        // vuelta hacia el spawn (efecto banda elástica) — bug real detectado
        // en la verificación del contrato de economía, no parte de él.
        if (g.unit_class[i] > 2) continue;
        if (g.fleeing[i]) {
            // Huir: moverse en dirección OPUESTA al enemigo vivo más cercano
            // (celda + 8 vecinas). Si no hay enemigo cerca, quedarse quieto.
            const uint32_t cell_i = sh_cell_index(g.shash, g.pos_x[i], g.pos_y[i]);
            const uint32_t cx = cell_i % g.shash.cells_x;
            const uint32_t cy = cell_i / g.shash.cells_x;

            uint32_t best = SH_EMPTY;
            uint64_t best_d2 = 0;
            const Vec2Fx pos_i{Fx{g.pos_x[i]}, Fx{g.pos_y[i]}};

            for (int32_t dcy = -1; dcy <= 1; ++dcy) {
                const int64_t ncy64 = static_cast<int64_t>(cy) + dcy;
                if (ncy64 < 0 || ncy64 >= static_cast<int64_t>(g.shash.cells_y)) continue;
                const uint32_t ncy = static_cast<uint32_t>(ncy64);
                for (int32_t dcx = -1; dcx <= 1; ++dcx) {
                    const int64_t ncx64 = static_cast<int64_t>(cx) + dcx;
                    if (ncx64 < 0 || ncx64 >= static_cast<int64_t>(g.shash.cells_x)) continue;
                    const uint32_t ncx = static_cast<uint32_t>(ncx64);
                    const uint32_t cell = ncy * g.shash.cells_x + ncx;

                    for (uint32_t j = sh_first(g.shash, cell); j != SH_EMPTY; j = sh_next(g.shash, j)) {
                        if (j == i) continue;
                        if (!t.alive[j]) continue;
                        if (g.hp[j] <= 0) continue;
                        if (g.owner[j] == g.owner[i]) continue;

                        FatalReason local_fatal = FatalReason::NONE;
                        const Vec2Fx pos_j{Fx{g.pos_x[j]}, Fx{g.pos_y[j]}};
                        const uint64_t d2 = dist_sq_raw(pos_i, pos_j, local_fatal);

                        if (best == SH_EMPTY || d2 < best_d2 || (d2 == best_d2 && j < best)) {
                            best = j;
                            best_d2 = d2;
                        }
                    }
                }
            }

            if (best != SH_EMPTY) {
                const int64_t step_fx = (int64_t)g.speed_mtpt[i] * FX_ONE_RAW / 1000;
                Vec2Fx away = normalize_v1(Vec2Fx{Fx{g.pos_x[i]-g.pos_x[best]},
                                                  Fx{g.pos_y[i]-g.pos_y[best]}}, g.fatal);
                Fx vx = fx_mul(away.x, Fx{step_fx}, g.fatal);
                Fx vy = fx_mul(away.y, Fx{step_fx}, g.fatal);
                g.vel_x[i]=vx.raw; g.vel_y[i]=vy.raw;
                g.pos_x[i]=fx_add(Fx{g.pos_x[i]},vx,g.fatal).raw;
                g.pos_y[i]=fx_add(Fx{g.pos_y[i]},vy,g.fatal).raw;
                if (g.pos_x[i] < 0) g.pos_x[i] = 0;
                if (g.pos_y[i] < 0) g.pos_y[i] = 0;
                if (g.pos_x[i] >= WORLD_RAW_MAX) g.pos_x[i] = WORLD_RAW_MAX - 1;
                if (g.pos_y[i] >= WORLD_RAW_MAX) g.pos_y[i] = WORLD_RAW_MAX - 1;
            } else { g.vel_x[i]=0; g.vel_y[i]=0; }
            continue;
        }
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

// Multiplicador RPS en basis points (10000 = 100%), tabla congelada (doc 07_COMBATE):
//            tgt=inf  tgt=cav  tgt=art
// atk=inf     10000    10000    10000
// atk=cav      8000    10000    13000
// atk=art     13000     8000    10000
inline int32_t rps_mult_bp(uint8_t atk_class, uint8_t tgt_class) noexcept {
    static constexpr int32_t TABLE[3][3] = {
        {10000, 10000, 10000},
        { 8000, 10000, 13000},
        {13000,  8000, 10000},
    };
    return TABLE[atk_class][tgt_class];
}

// Sistema de combate (Sprint 0.3). Cada tick, período 1: cada unidad viva en
// orden ascendente busca al enemigo más cercano en rango (celda propia + 8
// vecinas del spatial hash), le inflige daño RPS y entra en cooldown.
// Determinismo: el daño se aplica inmediatamente en orden ascendente de i.
inline void combat_system(GameState& g) noexcept {
    const EntityTable& t = g.entities;
    for (uint32_t i = 0; i < t.capacity; ++i) {
        if (!t.alive[i]) continue;
        if (g.hp[i] <= 0) continue;
        if (g.unit_class[i] > 2) continue;  // ciudadanos (Sprint 0.3): no atacan
        if (g.fleeing[i]) { if (g.atk_cd[i] > 0) --g.atk_cd[i]; continue; }
        if (g.atk_cd[i] > 0) { --g.atk_cd[i]; continue; }

        const uint32_t cell_i = sh_cell_index(g.shash, g.pos_x[i], g.pos_y[i]);
        const uint32_t cx = cell_i % g.shash.cells_x;
        const uint32_t cy = cell_i / g.shash.cells_x;

        const int64_t range_raw = static_cast<int64_t>(g.range_mt[i]) * 65536 / 1000;
        const uint64_t range_sq = static_cast<uint64_t>(range_raw) * static_cast<uint64_t>(range_raw);

        uint32_t best = SH_EMPTY;
        uint64_t best_d2 = 0;
        const Vec2Fx pos_i{Fx{g.pos_x[i]}, Fx{g.pos_y[i]}};

        for (int32_t dcy = -1; dcy <= 1; ++dcy) {
            const int64_t ncy64 = static_cast<int64_t>(cy) + dcy;
            if (ncy64 < 0 || ncy64 >= static_cast<int64_t>(g.shash.cells_y)) continue;
            const uint32_t ncy = static_cast<uint32_t>(ncy64);
            for (int32_t dcx = -1; dcx <= 1; ++dcx) {
                const int64_t ncx64 = static_cast<int64_t>(cx) + dcx;
                if (ncx64 < 0 || ncx64 >= static_cast<int64_t>(g.shash.cells_x)) continue;
                const uint32_t ncx = static_cast<uint32_t>(ncx64);
                const uint32_t cell = ncy * g.shash.cells_x + ncx;

                for (uint32_t j = sh_first(g.shash, cell); j != SH_EMPTY; j = sh_next(g.shash, j)) {
                    if (j == i) continue;
                    if (!t.alive[j]) continue;
                    if (g.hp[j] <= 0) continue;
                    if (g.owner[j] == g.owner[i]) continue;
                    if (g.unit_class[j] > 2) continue;  // ciudadanos: no son objetivo válido

                    FatalReason local_fatal = FatalReason::NONE;
                    const Vec2Fx pos_j{Fx{g.pos_x[j]}, Fx{g.pos_y[j]}};
                    const uint64_t d2 = dist_sq_raw(pos_i, pos_j, local_fatal);
                    if (d2 > range_sq) continue;

                    if (best == SH_EMPTY || d2 < best_d2 || (d2 == best_d2 && j < best)) {
                        best = j;
                        best_d2 = d2;
                    }
                }
            }
        }

        if (best != SH_EMPTY) {
            const int32_t mult = rps_mult_bp(g.unit_class[i], g.unit_class[best]);
            const int32_t dmg = static_cast<int32_t>(
                (static_cast<int64_t>(g.attack[i]) * mult) / 10000);
            g.hp[best] -= dmg;
            if (g.hp[best] <= 0 && t.alive[best]) {
                et_mark_dead(g.entities, best);
                if (g.destroy_count < PENDING_CAP) {
                    g.destroy_batch[g.destroy_count++] = best;
                }
            }
            g.atk_cd[i] = ATK_COOLDOWN_TICKS;
        }
    }
}

// Sistema de aggro/persecución (Sprint 0.3+). Sin esto el combate se estanca:
// combat_system solo dispara dentro de range_mt y nadie se re-acerca, así que
// tras el primer choque los supervivientes fuera de rango quedan inertes.
// Regla v1: una unidad de combate OCIOSA (pos == tgt — así una orden MOVE_TO
// del jugador en curso siempre tiene prioridad y jamás se redirige), viva, no
// huyendo y con attack > 0 busca al enemigo más cercano en AGGRO_RANGE_MT
// (anillo de ±AGGRO_RADIUS_CELLS del spatial hash, empate → j más bajo); si
// está más allá de su rango de arma, fija tgt a la posición del enemigo y
// movement_v1 la acerca en los ticks siguientes. Al llegar (movement_v1 hace
// snap exacto a tgt) vuelve a estar ociosa y re-adquiere, de modo que persigue
// blancos móviles por etapas. Sin estado nuevo: reutiliza tgt_x/tgt_y →
// checksum, serialización y versión de guardado intactos. Se ejecuta tras
// combat_system (hash fresco; los muertos del tick ya están marcados y no se
// adquieren). Determinismo: orden ascendente de i, lecturas post-movimiento.
inline void aggro_system(GameState& g) noexcept {
    const EntityTable& t = g.entities;
    for (uint32_t i = 0; i < t.capacity; ++i) {
        if (!t.alive[i]) continue;
        if (g.hp[i] <= 0) continue;
        if (g.unit_class[i] > 2) continue;   // ciudadanos: no persiguen
        if (g.attack[i] <= 0) continue;      // excluye SPAWN_DEBUG (golden intacto)
        if (g.fleeing[i]) continue;          // huir tiene prioridad
        if (g.pos_x[i] != g.tgt_x[i] || g.pos_y[i] != g.tgt_y[i]) continue;  // ocupada

        const uint32_t cell_i = sh_cell_index(g.shash, g.pos_x[i], g.pos_y[i]);
        const uint32_t cx = cell_i % g.shash.cells_x;
        const uint32_t cy = cell_i / g.shash.cells_x;

        const int64_t aggro_raw = static_cast<int64_t>(AGGRO_RANGE_MT) * 65536 / 1000;
        const uint64_t aggro_sq = static_cast<uint64_t>(aggro_raw) * static_cast<uint64_t>(aggro_raw);
        const int64_t range_raw = static_cast<int64_t>(g.range_mt[i]) * 65536 / 1000;
        const uint64_t range_sq = static_cast<uint64_t>(range_raw) * static_cast<uint64_t>(range_raw);

        uint32_t best = SH_EMPTY;
        uint64_t best_d2 = 0;
        const Vec2Fx pos_i{Fx{g.pos_x[i]}, Fx{g.pos_y[i]}};
        const int32_t R = static_cast<int32_t>(AGGRO_RADIUS_CELLS);

        for (int32_t dcy = -R; dcy <= R; ++dcy) {
            const int64_t ncy64 = static_cast<int64_t>(cy) + dcy;
            if (ncy64 < 0 || ncy64 >= static_cast<int64_t>(g.shash.cells_y)) continue;
            const uint32_t ncy = static_cast<uint32_t>(ncy64);
            for (int32_t dcx = -R; dcx <= R; ++dcx) {
                const int64_t ncx64 = static_cast<int64_t>(cx) + dcx;
                if (ncx64 < 0 || ncx64 >= static_cast<int64_t>(g.shash.cells_x)) continue;
                const uint32_t ncx = static_cast<uint32_t>(ncx64);
                const uint32_t cell = ncy * g.shash.cells_x + ncx;

                for (uint32_t j = sh_first(g.shash, cell); j != SH_EMPTY; j = sh_next(g.shash, j)) {
                    if (j == i) continue;
                    if (!t.alive[j]) continue;
                    if (g.hp[j] <= 0) continue;
                    if (g.owner[j] == g.owner[i]) continue;
                    if (g.unit_class[j] > 2) continue;  // ciudadanos: no se persiguen (v1)

                    FatalReason local_fatal = FatalReason::NONE;
                    const Vec2Fx pos_j{Fx{g.pos_x[j]}, Fx{g.pos_y[j]}};
                    const uint64_t d2 = dist_sq_raw(pos_i, pos_j, local_fatal);
                    if (d2 > aggro_sq) continue;

                    if (best == SH_EMPTY || d2 < best_d2 || (d2 == best_d2 && j < best)) {
                        best = j;
                        best_d2 = d2;
                    }
                }
            }
        }

        // Enemigo detectado fuera del rango de arma → perseguir. Dentro de
        // rango: quieta (combat_system ya le dispara donde está).
        if (best != SH_EMPTY && best_d2 > range_sq) {
            g.tgt_x[i] = g.pos_x[best];
            g.tgt_y[i] = g.pos_y[best];
        }
    }
}

// Sistema de moral (Sprint 0.3, doc 07_COMBATE §7.6). Se llama tras el
// combate del tick, antes del DESTROY, para reaccionar a lo que pasó.
// Cada unidad viva cuenta aliados/enemigos en su celda + 8 vecinas (mismo
// patrón que combat_system): en fuerte desventaja local pierde moral y,
// bajo histéresis, entra en pánico (huye, no ataca).
inline void morale_system(GameState& g) noexcept {
    const EntityTable& t = g.entities;
    for (uint32_t i = 0; i < t.capacity; ++i) {
        if (!t.alive[i]) continue;
        if (g.hp[i] <= 0) continue;

        const uint32_t cell_i = sh_cell_index(g.shash, g.pos_x[i], g.pos_y[i]);
        const uint32_t cx = cell_i % g.shash.cells_x;
        const uint32_t cy = cell_i / g.shash.cells_x;

        uint32_t allies = 0, enemies = 0;

        for (int32_t dcy = -1; dcy <= 1; ++dcy) {
            const int64_t ncy64 = static_cast<int64_t>(cy) + dcy;
            if (ncy64 < 0 || ncy64 >= static_cast<int64_t>(g.shash.cells_y)) continue;
            const uint32_t ncy = static_cast<uint32_t>(ncy64);
            for (int32_t dcx = -1; dcx <= 1; ++dcx) {
                const int64_t ncx64 = static_cast<int64_t>(cx) + dcx;
                if (ncx64 < 0 || ncx64 >= static_cast<int64_t>(g.shash.cells_x)) continue;
                const uint32_t ncx = static_cast<uint32_t>(ncx64);
                const uint32_t cell = ncy * g.shash.cells_x + ncx;

                for (uint32_t j = sh_first(g.shash, cell); j != SH_EMPTY; j = sh_next(g.shash, j)) {
                    if (j == i) continue;
                    if (!t.alive[j]) continue;
                    if (g.hp[j] <= 0) continue;
                    if (g.owner[j] == g.owner[i]) ++allies;
                    else ++enemies;
                }
            }
        }

        if (enemies > allies + 1) {
            g.morale[i] -= MORALE_DROP;
        } else if (enemies == 0) {
            g.morale[i] += MORALE_REGEN;
        }
        if (g.morale[i] < 0) g.morale[i] = 0;
        if (g.morale[i] > MORALE_MAX) g.morale[i] = MORALE_MAX;

        if (g.morale[i] <= MORALE_PANIC) g.fleeing[i] = 1;
        if (g.morale[i] >= MORALE_RALLY) g.fleeing[i] = 0;
    }
}

// Economía mínima (Sprint 0.3): pump del módulo autocontenido economy.hpp para
// cada ciudadano vivo (unit_class==3), en orden ascendente. economy.hpp NO muta
// deposits[]/player_stock (devuelve deltas); esta función es el único punto que
// los aplica, garantizando mutación en orden determinista y sin doble aplicación.
inline void economy_system(GameState& g) noexcept {
    const EntityTable& t = g.entities;
    for (uint32_t i = 0; i < t.capacity; ++i) {
        if (!t.alive[i]) continue;
        if (g.unit_class[i] != 3) continue;  // solo ciudadanos

        EcoCitizenIn in{};
        in.pos_x = g.pos_x[i];
        in.pos_y = g.pos_y[i];
        in.state = g.eco_state[i];
        in.assigned_deposit = g.eco_assigned_deposit[i];
        in.carry = g.eco_carry[i];
        in.carry_resource_idx = g.eco_carry_resource[i];
        in.speed_mtpt = g.speed_mtpt[i];

        const uint8_t owner_i = g.owner[i];
        const EcoCitizenOut out = eco_step_citizen(
                in, g.deposits, g.n_deposits,
                g.dropoff_x[owner_i], g.dropoff_y[owner_i], g.fatal);

        g.pos_x[i] = out.pos_x; g.pos_y[i] = out.pos_y;
        g.vel_x[i] = out.vel_x; g.vel_y[i] = out.vel_y;
        g.eco_state[i] = out.state;
        g.eco_assigned_deposit[i] = out.assigned_deposit;
        g.eco_carry[i] = out.carry;
        g.eco_carry_resource[i] = out.carry_resource_idx;

        if (out.did_harvest && out.assigned_deposit < g.n_deposits) {
            g.deposits[out.assigned_deposit].remaining -= out.harvested_amount;
        }
        if (out.did_dropoff && out.dropoff_resource_idx < 3u) {
            g.player_stock[owner_i][out.dropoff_resource_idx] += out.dropoff_amount;
        }
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

        // (5b) Combate (Sprint 0.3): cada tick, período 1. Antes de DESTROY.
        detail::combat_system(g);

        // (5b') Aggro/persecución (Sprint 0.3+): tras el combate (muertos del
        // tick ya marcados), fija tgt de las unidades ociosas hacia el enemigo
        // más cercano en radio de adquisición. Antes de moral y DESTROY.
        detail::aggro_system(g);

        // (5c) Moral (Sprint 0.3, doc 07 §7.6): tras el combate del tick,
        // para reaccionar a lo que pasó. Antes de DESTROY.
        detail::morale_system(g);

        // (5c) Economía mínima (Sprint 0.3): cada tick, período 1. Antes de DESTROY.
        detail::economy_system(g);

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
