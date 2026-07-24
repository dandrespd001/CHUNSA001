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

// Construcción de edificios (Sprint 1.1, SPEC-004 §5): mismo radio de
// llegada que la economía (alias explícito, no una constante nueva
// independiente — comparten semántica "a un tile o menos del objetivo").
inline constexpr int64_t BUILD_ARRIVE_RADIUS_RAW = ECO_ARRIVE_RADIUS_RAW;

// EPOCH_UP (Sprint 1.2, SPEC-004 §12.3, ADR-015): constantes v1 (brief K2, no
// re-litigar). EPOCH_MIN_TICKS = 20 ticks/s * 300 s = 6000 (gate b: tiempo
// mínimo desde la época inicial). EPOCH_COST_*: coste fijo v1. EPOCH_MAX_V1:
// época máxima del slice — v1 la fija como constante literal (no derivada del
// catálogo pese a la prosa "de los datos del match" del spec; ver RESULT).
inline constexpr uint32_t EPOCH_MIN_TICKS = 6000;
inline constexpr int32_t EPOCH_COST_A = 200;
inline constexpr int32_t EPOCH_COST_B = 200;
inline constexpr int32_t EPOCH_COST_ME = 100;
inline constexpr uint8_t EPOCH_MAX_V1 = 7;

// Normalización del tick efectivo de un comando (SPEC-001 §6.2): un comando
// capturado en el tick `t` no puede surtir efecto antes de `t + delay` (retardo
// de input humano). Función PURA extraída de la fase (3) de step() para que el
// recorder de replays calcule EXACTAMENTE el mismo effective_tick que el kernel
// (agenda auto-verificada, sin duplicar la fórmula). No aplica el rechazo
// OUT_OF_WINDOW (eso depende de max_future y lo decide step); solo el piso.
//
// Ventana de setup (Sprint 1.2, SPEC-004 §10.3, refina la enmienda §4.1.2):
// caso explícito `target_tick == 0 && t == 0` -> `eff = 0`, SIN sumar `delay`.
// Con delay >= 1 (valor de producción) `eff = max(target, t+delay) >= 1`
// siempre, así que `effective_tick == 0` era INALCANZABLE (hallazgo Sprint
// 1.1) y forzaba a los escenarios que necesitaban la exención de §4.1.2/§4.3
// (p.ej. PLACE_BUILDING de los centros iniciales) a correr con delay=0 — un
// valor que NUNCA se usa en producción. Este caso lo hace alcanzable sin
// tocar delay: los comandos con target_tick=0 ingeridos en el PRIMER Step()
// (t==0) ejecutan en el tick 0 sin retardo. Para t>0 o target_tick!=0 la
// fórmula es EXACTAMENTE la de antes (casos existentes intactos).
//
// CONTRATO DEL HOST (driver/adaptador, no logic aquí — ver driver.hpp/
// cli_run.hpp/chunsa_sim_node.cpp): el host NUNCA debe ingerir input de
// JUGADOR en la llamada a step() de t==0 (la primera). Los comandos que un
// host meta en ese primer batch son, por definición de este contrato,
// EXCLUSIVAMENTE comandos de setup de escenario (spawns/edificios iniciales
// generados por el propio escenario, nunca por un humano) — de modo que la
// exención de tick 0 no abre una vía de escape para input real de jugador.
inline uint32_t command_effective_tick(uint32_t target_tick, uint32_t t,
                                       uint32_t delay) noexcept {
    if (target_tick == 0u && t == 0u) return 0u;  // §10.3: ventana de setup
    const uint32_t min_eff = t + delay;
    return target_tick < min_eff ? min_eff : target_tick;
}
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

// Sprint 0.4 (SPEC-002 §8.4): inicializa los componentes de una unidad de
// combate (infantry/cavalry/artillery) EXCLUSIVAMENTE desde el catálogo. Sin
// heap/lookup textual — `def` ya es un puntero validado fuera de Step().
inline void init_combat_unit_from_catalog(GameState& g, uint32_t i,
                                          const UnitDefinitionV1& def) noexcept {
    g.hp[i] = def.hp; g.max_hp[i] = def.hp;
    g.attack[i] = def.attack;
    g.range_mt[i] = def.range_millitiles;
    g.unit_class[i] = static_cast<uint8_t>(def.unit_class);
    g.atk_cd[i] = 0;
    g.speed_mtpt[i] = def.speed_millitile_tick;
    g.morale[i] = def.morale;
    g.fleeing[i] = 0;
}

// Misma state machine económica que el SPAWN_CITIZEN histórico, pero con hp
// y velocidad tomados del catálogo (no hardcodeados). Compartida por
// SPAWN_UNIT(class=Citizen) y SPAWN_CITIZEN data-driven (SPEC-002 §8.4).
inline void init_citizen_from_catalog(GameState& g, uint32_t i,
                                      const UnitDefinitionV1& def) noexcept {
    g.hp[i] = def.hp; g.max_hp[i] = def.hp;
    g.attack[i] = 0; g.range_mt[i] = 0;
    g.unit_class[i] = 3;  // citizen: excluido de combat_system
    g.atk_cd[i] = 0;
    g.speed_mtpt[i] = def.speed_millitile_tick;
    g.morale[i] = def.morale;
    g.fleeing[i] = 0;
    g.eco_state[i] = EcoState::SEEK;
    g.eco_assigned_deposit[i] = ECO_NO_DEPOSIT;
    g.eco_carry[i] = 0;
    g.eco_carry_resource[i] = 0;
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
            // Sprint 0.4 (SPEC-002 §8.4): data-driven por defecto. `unit_id`
            // decide el camino; jamás se hace lookup textual aquí (el binding
            // ya resolvió record_id → UnitId fuera de Step()).
            const Vec2Fx p{Fx{c.p.x_raw}, Fx{c.p.y_raw}};
            if (!world_contains(p)) return RejectReason::MALFORMED;

            if (c.p.unit_id != INVALID_UNIT_ID) {
                if (g.catalog == nullptr || c.p.unit_id >= g.catalog->unit_count) {
                    return RejectReason::MALFORMED;
                }
                // Camino normal: TODOS los campos de stats del payload deben
                // ser cero (SPEC-002 §8.4) — las stats vienen solo del dato.
                const bool payload_clean = c.p.hp == 0 && c.p.attack == 0
                                        && c.p.range_mt == 0 && c.p.unit_class == 0
                                        && c.p.speed_mtpt == 0;
                if (!payload_clean) return RejectReason::MALFORMED;
                const UnitDefinitionV1& def = g.catalog->units[c.p.unit_id];
                if (def.unit_class == UnitClassV1::Siege
                    || def.unit_class == UnitClassV1::NavalLight) {
                    return RejectReason::ILLEGAL_STATE;  // compilados, spawn aún no soportado
                }
                const EntityHandle h = et_spawn(g.entities);
                if (handle_eq(h, NULL_HANDLE)) return RejectReason::POOL_EXHAUSTED;
                const uint32_t i = h.index;
                g.pos_x[i] = c.p.x_raw; g.pos_y[i] = c.p.y_raw;
                g.tgt_x[i] = c.p.x_raw; g.tgt_y[i] = c.p.y_raw;
                g.vel_x[i] = 0; g.vel_y[i] = 0;
                g.owner[i] = static_cast<uint8_t>(c.emitter);
                g.unit_id[i] = c.p.unit_id;
                if (def.unit_class == UnitClassV1::Citizen) {
                    detail::init_citizen_from_catalog(g, i, def);
                } else {
                    detail::init_combat_unit_from_catalog(g, i, def);
                }
                return RejectReason::ACCEPTED;
            }

            // Camino debug LEGADO: solo si el match lo habilita explícitamente.
            if (g.cfg.allow_debug_stat_payload != 1u) return RejectReason::MALFORMED;
            const bool combat_ok = c.p.hp > 0 && c.p.attack >= 0
                                 && c.p.range_mt >= 0 && c.p.unit_class <= 2;
            if (!combat_ok) return RejectReason::MALFORMED;
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
            g.unit_id[i] = INVALID_UNIT_ID;
            return RejectReason::ACCEPTED;
        }
        case CommandType::SPAWN_CITIZEN: {
            // Ciudadano económico: unit_class=3 lo excluye de combat_system (ambos
            // lados: atacante y objetivo — ver el guard `> 2` allí). Alias
            // restringido a clase Citizen del mismo camino data-driven que
            // SPAWN_UNIT (SPEC-002 §8.4).
            const Vec2Fx p{Fx{c.p.x_raw}, Fx{c.p.y_raw}};
            if (!world_contains(p)) return RejectReason::MALFORMED;

            if (c.p.unit_id != INVALID_UNIT_ID) {
                if (g.catalog == nullptr || c.p.unit_id >= g.catalog->unit_count) {
                    return RejectReason::MALFORMED;
                }
                const bool payload_clean = c.p.hp == 0 && c.p.attack == 0
                                        && c.p.range_mt == 0 && c.p.unit_class == 0
                                        && c.p.speed_mtpt == 0;
                if (!payload_clean) return RejectReason::MALFORMED;
                const UnitDefinitionV1& def = g.catalog->units[c.p.unit_id];
                if (def.unit_class != UnitClassV1::Citizen) return RejectReason::ILLEGAL_STATE;
                const EntityHandle h = et_spawn(g.entities);
                if (handle_eq(h, NULL_HANDLE)) return RejectReason::POOL_EXHAUSTED;
                const uint32_t i = h.index;
                g.pos_x[i] = c.p.x_raw; g.pos_y[i] = c.p.y_raw;
                g.tgt_x[i] = c.p.x_raw; g.tgt_y[i] = c.p.y_raw;
                g.vel_x[i] = 0; g.vel_y[i] = 0;
                g.owner[i] = static_cast<uint8_t>(c.emitter);
                g.unit_id[i] = c.p.unit_id;
                detail::init_citizen_from_catalog(g, i, def);
                return RejectReason::ACCEPTED;
            }

            // Camino debug LEGADO (deviación documentada frente a SPEC-002
            // §8.4: se conserva hp=20 hardcodeado del comportamiento previo a
            // este sprint en lugar de exigir hp>0 del payload, para no
            // perturbar los supuestos numéricos de test_economy.cpp; ver
            // RESULT del sprint).
            if (g.cfg.allow_debug_stat_payload != 1u) return RejectReason::MALFORMED;
            if (c.p.speed_mtpt <= 0) return RejectReason::MALFORMED;
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
            g.unit_id[i] = INVALID_UNIT_ID;
            g.eco_state[i] = EcoState::SEEK;
            g.eco_assigned_deposit[i] = ECO_NO_DEPOSIT;
            g.eco_carry[i] = 0;
            g.eco_carry_resource[i] = 0;
            return RejectReason::ACCEPTED;
        }
        case CommandType::PLACE_BUILDING: {
            // SPEC-004 §4.1: orden de validación es CONTRATO (testeado). Payload:
            // p.unit_id = BuildingId del catálogo; p.x_raw/p.y_raw = tile ancla en
            // unidades ENTERAS de tile (no raw).
            if (g.catalog == nullptr || c.p.unit_id >= g.catalog->building_count) {
                return RejectReason::MALFORMED;
            }
            const BuildingDefinitionV1& def = g.catalog->buildings[c.p.unit_id];

            // Enmienda del Arquitecto 2026-07-23 (SPEC-004 §4.1.2/§4.3): exención
            // de escenario en effective_tick==0 — omite el paso 2 (constructible)
            // y el paso 6 (costes/stock). Ventana de setup exclusiva del
            // driver/adaptador: los comandos de jugador jamás llegan a
            // effective_tick==0 en producción (human_input_delay_ticks>=1),
            // así que esto no abre una vía de escape para PLACE_BUILDING normal.
            const bool scenario_exempt = (c.effective_tick == 0u);

            if (!scenario_exempt && def.constructible != 1u) {
                return RejectReason::ILLEGAL_STATE;
            }

            // Sprint 1.2 (SPEC-004 §12.4): gating de época/capacidades —
            // retro-aplica a Parte I. Exento en la MISMA ventana de setup que
            // constructible/costes (mismo motivo: los edificios iniciales de
            // escenario nacen antes de que player_epoch/player_caps tengan un
            // valor con sentido para el jugador que los coloca).
            if (!scenario_exempt) {
                if (g.player_epoch[c.emitter] < def.epoch_min || g.player_epoch[c.emitter] > def.epoch_max) {
                    return RejectReason::ILLEGAL_STATE;
                }
                for (uint8_t k = 0; k < def.required_capabilities_count; ++k) {
                    const CapabilityId cap = def.required_capabilities[k];
                    const uint32_t word = cap / 64u, bit = cap % 64u;
                    if (word >= CAP_WORDS || ((g.player_caps[c.emitter][word] >> bit) & 1u) == 0u) {
                        return RejectReason::ILLEGAL_STATE;
                    }
                }
            }

            // Resto de campos de stats del payload == 0 (misma disciplina
            // payload-limpio que SPAWN_UNIT), + handle == NULL-ish (índice y
            // generación en 0, valor por defecto de un CmdPayload sin usar).
            const bool payload_clean = c.p.hp == 0 && c.p.attack == 0
                                    && c.p.range_mt == 0 && c.p.unit_class == 0
                                    && c.p.speed_mtpt == 0
                                    && c.p.handle.index == 0 && c.p.handle.generation == 0;
            if (!payload_clean) return RejectReason::MALFORMED;

            if (c.p.x_raw < 0 || c.p.y_raw < 0) return RejectReason::MALFORMED;
            const uint64_t tx = static_cast<uint64_t>(c.p.x_raw);
            const uint64_t ty = static_cast<uint64_t>(c.p.y_raw);
            const uint64_t fw = def.footprint_w;
            const uint64_t fh = def.footprint_h;
            // Footprint dentro del mapa. Endurecimiento (mismo patrón que
            // FLOW_MOVE más abajo): el cost_grid de navegación es fijo 256×256
            // (FF_AXIS) sin importar map_tiles_x/y; un footprint que excediera
            // ese rango leería fuera de cost_grid en el chequeo siguiente.
            if (tx + fw > g.cfg.map_tiles_x || ty + fh > g.cfg.map_tiles_y
                || tx + fw > FF_AXIS || ty + fh > FF_AXIS) {
                return RejectReason::MALFORMED;
            }

            // Todas las celdas del footprint transitables. NOTA: esto YA
            // implica "sin footprint de otro edificio vivo" (§4.1.5): colocar
            // un edificio marca sus celdas FF_WALL (efecto de este mismo
            // comando, más abajo), así que solapar con uno existente falla
            // este MISMO chequeo — no hace falta un segundo barrido O(edificios).
            for (uint64_t cy = ty; cy < ty + fh; ++cy) {
                for (uint64_t cx = tx; cx < tx + fw; ++cx) {
                    if (g.cost_grid[cy * FF_AXIS + cx] == FF_WALL) {
                        return RejectReason::ILLEGAL_STATE;
                    }
                }
            }

            if (!scenario_exempt) {
                if (g.player_stock[c.emitter][0] < def.cost_a
                    || g.player_stock[c.emitter][1] < def.cost_b
                    || g.player_stock[c.emitter][2] < def.cost_me) {
                    return RejectReason::ILLEGAL_STATE;
                }
            }

            const EntityHandle h = et_spawn(g.entities);
            if (handle_eq(h, NULL_HANDLE)) return RejectReason::POOL_EXHAUSTED;
            const uint32_t i = h.index;

            // Efecto (atómico): deducir costes, spawn de la entidad, marcar
            // footprint en cost_grid. La exención de escenario (§4.1.2) exime
            // TAMBIÉN la deducción, no solo el chequeo de stock: un escenario
            // que pre-coloque en tick 0 un edificio con coste no debe dejar el
            // stock del jugador en negativo (endurecimiento del Arquitecto en
            // revisión; con los datos actuales —centros coste 0— es un no-op).
            if (!scenario_exempt) {
                g.player_stock[c.emitter][0] -= def.cost_a;
                g.player_stock[c.emitter][1] -= def.cost_b;
                g.player_stock[c.emitter][2] -= def.cost_me;
            }

            // Posición = centro geométrico del footprint (SPEC-004 §3):
            // anchor*T + (w*T)/2, raw exacto en Q47.16 con T=FX_ONE_RAW.
            const int64_t T = FX_ONE_RAW;
            g.pos_x[i] = static_cast<int64_t>(tx) * T + (static_cast<int64_t>(fw) * T) / 2;
            g.pos_y[i] = static_cast<int64_t>(ty) * T + (static_cast<int64_t>(fh) * T) / 2;
            g.vel_x[i] = 0; g.vel_y[i] = 0;
            g.tgt_x[i] = g.pos_x[i]; g.tgt_y[i] = g.pos_y[i];
            g.owner[i] = static_cast<uint8_t>(c.emitter);
            g.unit_id[i] = INVALID_UNIT_ID;  // no es una unidad del catálogo de unidades

            g.hp[i] = g.max_hp[i] = def.hp;
            g.attack[i] = 0; g.range_mt[i] = 0;
            g.unit_class[i] = 255u;  // edificio: nunca 0..3 (SPEC-004 §3)
            g.atk_cd[i] = 0;
            g.speed_mtpt[i] = 0;
            g.morale[i] = 0; g.fleeing[i] = 0;
            // eco_state queda en SEEK (default de zero_components/gs_init):
            // los sistemas de unidades saltan esta entidad por entity_kind.

            g.entity_kind[i] = 1u;
            g.building_id[i] = c.p.unit_id;
            g.build_progress[i] = 0u;  // 0 >= T solo si T==0 (nace completo)
            g.bld_anchor_tx[i] = static_cast<uint16_t>(tx);
            g.bld_anchor_ty[i] = static_cast<uint16_t>(ty);

            for (uint64_t cy = ty; cy < ty + fh; ++cy) {
                for (uint64_t cx = tx; cx < tx + fw; ++cx) {
                    g.cost_grid[cy * FF_AXIS + cx] = FF_WALL;
                }
            }
            g.flow_dirty = 1;
            return RejectReason::ACCEPTED;
        }
        case CommandType::ASSIGN_BUILD: {
            // Payload: p.handle = ciudadano propio; p.x_raw/p.y_raw = tile
            // entero contenido en el footprint del sitio objetivo.
            if (!et_is_alive(g.entities, c.p.handle)) return RejectReason::INVALID_ENTITY;
            const uint32_t ci = c.p.handle.index;
            if (g.owner[ci] != c.emitter) return RejectReason::NOT_OWNER;
            if (g.unit_class[ci] != 3u) return RejectReason::ILLEGAL_STATE;

            // Resolver el edificio: entidad viva propia con entity_kind==1,
            // build_progress < T y cuyo footprint contiene el tile — recorrido
            // ASCENDENTE, primer match gana (== menor índice; el no-solape de
            // §4.1.5 hace "varias" imposible en la práctica, esto es robustez).
            uint32_t found = g.entities.capacity;
            for (uint32_t j = 0; j < g.entities.capacity; ++j) {
                if (!g.entities.alive[j]) continue;
                if (g.owner[j] != c.emitter) continue;
                if (g.entity_kind[j] != 1u) continue;
                if (g.catalog == nullptr || g.building_id[j] >= g.catalog->building_count) continue;
                const BuildingDefinitionV1& bdef = g.catalog->buildings[g.building_id[j]];
                if (g.build_progress[j] >= bdef.build_time_ticks) continue;
                const int64_t bx0 = static_cast<int64_t>(g.bld_anchor_tx[j]);
                const int64_t by0 = static_cast<int64_t>(g.bld_anchor_ty[j]);
                if (c.p.x_raw < bx0 || c.p.x_raw >= bx0 + static_cast<int64_t>(bdef.footprint_w)) continue;
                if (c.p.y_raw < by0 || c.p.y_raw >= by0 + static_cast<int64_t>(bdef.footprint_h)) continue;
                found = j;
                break;
            }
            if (found == g.entities.capacity) return RejectReason::INVALID_ENTITY;

            g.build_target[ci] = found;
            return RejectReason::ACCEPTED;
        }
        case CommandType::TRAIN_UNIT: {
            // SPEC-004 §11.3: p.handle = edificio propio COMPLETO; p.unit_id =
            // UnitId. Orden de validación es CONTRATO (testeado).
            if (!et_is_alive(g.entities, c.p.handle)) return RejectReason::INVALID_ENTITY;
            const uint32_t bi = c.p.handle.index;
            if (g.owner[bi] != c.emitter) return RejectReason::NOT_OWNER;
            if (g.entity_kind[bi] != 1u) return RejectReason::ILLEGAL_STATE;
            if (g.catalog == nullptr || g.building_id[bi] >= g.catalog->building_count) {
                return RejectReason::ILLEGAL_STATE;
            }
            const BuildingDefinitionV1& bdef = g.catalog->buildings[g.building_id[bi]];
            if (g.build_progress[bi] < bdef.build_time_ticks) return RejectReason::ILLEGAL_STATE;

            if (c.p.unit_id >= g.catalog->unit_count) return RejectReason::MALFORMED;
            bool in_trains = false;
            for (uint8_t k = 0; k < bdef.train_count; ++k) {
                if (bdef.trains[k] == c.p.unit_id) { in_trains = true; break; }
            }
            if (!in_trains) return RejectReason::MALFORMED;
            const UnitDefinitionV1& udef = g.catalog->units[c.p.unit_id];

            // §12.4: player_epoch ∈ epoch_window de la unidad. (unit.schema.json
            // no declara required_capabilities — ese sub-gate pasa trivialmente
            // sobre el conjunto vacío, deviación documentada en data_catalog.hpp).
            if (g.player_epoch[c.emitter] < udef.epoch_min || g.player_epoch[c.emitter] > udef.epoch_max) {
                return RejectReason::ILLEGAL_STATE;
            }

            if (g.prod_count[bi] >= PROD_QUEUE_CAP) return RejectReason::ILLEGAL_STATE;

            const int32_t pop_cost = udef.pop_cost;  // constante v1 = 1
            if (g.pop_used[c.emitter] + pop_cost > static_cast<int32_t>(POP_CAP_V1)) {
                return RejectReason::ILLEGAL_STATE;
            }

            if (g.player_stock[c.emitter][0] < udef.cost_a
                || g.player_stock[c.emitter][1] < udef.cost_b
                || g.player_stock[c.emitter][2] < udef.cost_me) {
                return RejectReason::ILLEGAL_STATE;
            }

            g.player_stock[c.emitter][0] -= udef.cost_a;
            g.player_stock[c.emitter][1] -= udef.cost_b;
            g.player_stock[c.emitter][2] -= udef.cost_me;
            g.prod_queue[bi][g.prod_count[bi]] = c.p.unit_id;
            ++g.prod_count[bi];
            g.pop_used[c.emitter] += pop_cost;
            return RejectReason::ACCEPTED;
        }
        case CommandType::SET_RALLY: {
            // SPEC-004 §11.3: p.handle = edificio propio (completo o no);
            // p.x_raw/p.y_raw = punto raw dentro de la cota del mundo.
            if (!et_is_alive(g.entities, c.p.handle)) return RejectReason::INVALID_ENTITY;
            const uint32_t bi = c.p.handle.index;
            if (g.owner[bi] != c.emitter) return RejectReason::NOT_OWNER;
            if (g.entity_kind[bi] != 1u) return RejectReason::ILLEGAL_STATE;
            const Vec2Fx p{Fx{c.p.x_raw}, Fx{c.p.y_raw}};
            if (!world_contains(p)) return RejectReason::MALFORMED;
            g.rally_x[bi] = c.p.x_raw;
            g.rally_y[bi] = c.p.y_raw;
            g.rally_set[bi] = 1u;
            return RejectReason::ACCEPTED;
        }
        case CommandType::RESEARCH_TECH: {
            // SPEC-004 §12.3: p.handle = edificio propio completo con tech ∈
            // researches; p.unit_id = TechId. Orden análogo a TRAIN_UNIT +
            // los gates propios de research (prereq/mutex/época/ocioso/stock).
            if (!et_is_alive(g.entities, c.p.handle)) return RejectReason::INVALID_ENTITY;
            const uint32_t bi = c.p.handle.index;
            if (g.owner[bi] != c.emitter) return RejectReason::NOT_OWNER;
            if (g.entity_kind[bi] != 1u) return RejectReason::ILLEGAL_STATE;
            if (g.catalog == nullptr || g.building_id[bi] >= g.catalog->building_count) {
                return RejectReason::ILLEGAL_STATE;
            }
            const BuildingDefinitionV1& bdef = g.catalog->buildings[g.building_id[bi]];
            if (g.build_progress[bi] < bdef.build_time_ticks) return RejectReason::ILLEGAL_STATE;

            if (c.p.unit_id >= g.catalog->tech_count) return RejectReason::MALFORMED;
            bool in_researches = false;
            for (uint8_t k = 0; k < bdef.research_count; ++k) {
                if (bdef.researches[k] == c.p.unit_id) { in_researches = true; break; }
            }
            if (!in_researches) return RejectReason::MALFORMED;
            const TechDefinitionV1& tdef = g.catalog->techs[c.p.unit_id];
            const TechId tid = c.p.unit_id;

            // No investigada ya ni en curso por este jugador (en CUALQUIERA de
            // sus edificios, no solo el edificio `bi` de este comando).
            {
                const uint32_t tw = tid / 64u, tb = tid % 64u;
                if (tw < TECH_WORDS && ((g.player_techs[c.emitter][tw] >> tb) & 1u) != 0u) {
                    return RejectReason::ILLEGAL_STATE;
                }
            }
            for (uint32_t j = 0; j < g.entities.capacity; ++j) {
                if (!g.entities.alive[j]) continue;
                if (g.owner[j] != c.emitter) continue;
                if (g.research_tech[j] == tid) return RejectReason::ILLEGAL_STATE;
            }

            for (uint8_t k = 0; k < tdef.prereq_count; ++k) {
                const TechId pr = tdef.prerequisites[k];
                const uint32_t pw = pr / 64u, pb = pr % 64u;
                const bool has = (pw < TECH_WORDS) && (((g.player_techs[c.emitter][pw] >> pb) & 1u) != 0u);
                if (!has) return RejectReason::ILLEGAL_STATE;
            }
            for (uint8_t k = 0; k < tdef.mutex_count; ++k) {
                const TechId mx = tdef.mutually_exclusive_with[k];
                const uint32_t mw = mx / 64u, mb = mx % 64u;
                const bool has = (mw < TECH_WORDS) && (((g.player_techs[c.emitter][mw] >> mb) & 1u) != 0u);
                if (has) return RejectReason::ILLEGAL_STATE;
            }

            if (tdef.epoch > g.player_epoch[c.emitter]) return RejectReason::ILLEGAL_STATE;

            if (g.research_tech[bi] != INVALID_TECH_ID) return RejectReason::ILLEGAL_STATE;  // edificio ocupado

            if (g.player_stock[c.emitter][0] < tdef.cost_a
                || g.player_stock[c.emitter][1] < tdef.cost_b
                || g.player_stock[c.emitter][2] < tdef.cost_me) {
                return RejectReason::ILLEGAL_STATE;
            }

            g.player_stock[c.emitter][0] -= tdef.cost_a;
            g.player_stock[c.emitter][1] -= tdef.cost_b;
            g.player_stock[c.emitter][2] -= tdef.cost_me;
            g.research_tech[bi] = tid;
            g.research_progress[bi] = 0;
            return RejectReason::ACCEPTED;
        }
        case CommandType::EPOCH_UP: {
            // SPEC-004 §12.3: comando de JUGADOR, no de entidad (p.handle=0).
            // Disciplina payload-limpio: TODOS los campos sin uso deben ser 0
            // (mismo patrón que PLACE_BUILDING/SPAWN_UNIT).
            const bool payload_clean = c.p.handle.index == 0 && c.p.handle.generation == 0
                                    && c.p.x_raw == 0 && c.p.y_raw == 0
                                    && c.p.speed_mtpt == 0 && c.p.hp == 0
                                    && c.p.attack == 0 && c.p.range_mt == 0
                                    && c.p.unit_class == 0 && c.p.unit_id == 0;
            if (!payload_clean) return RejectReason::MALFORMED;

            const uint8_t cur_epoch = g.player_epoch[c.emitter];
            if (cur_epoch >= EPOCH_MAX_V1) return RejectReason::ILLEGAL_STATE;

            // Gate (a): >= 2 edificios COMPLETOS propios cuya epoch_window
            // incluye la época ACTUAL (antes de subir).
            uint32_t count_ok = 0;
            if (g.catalog != nullptr) {
                for (uint32_t j = 0; j < g.entities.capacity; ++j) {
                    if (!g.entities.alive[j]) continue;
                    if (g.owner[j] != c.emitter) continue;
                    if (g.entity_kind[j] != 1u) continue;
                    if (g.building_id[j] >= g.catalog->building_count) continue;
                    const BuildingDefinitionV1& bdef = g.catalog->buildings[g.building_id[j]];
                    if (g.build_progress[j] < bdef.build_time_ticks) continue;  // no completo
                    if (cur_epoch < bdef.epoch_min || cur_epoch > bdef.epoch_max) continue;
                    ++count_ok;
                }
            }
            if (count_ok < 2u) return RejectReason::ILLEGAL_STATE;

            // Gate (b): tiempo mínimo desde la época inicial.
            const uint8_t initial = g.epoch_initial[c.emitter];
            const uint32_t steps = static_cast<uint32_t>(cur_epoch) - static_cast<uint32_t>(initial) + 1u;
            if (g.tick < EPOCH_MIN_TICKS * steps) return RejectReason::ILLEGAL_STATE;

            if (g.player_stock[c.emitter][0] < EPOCH_COST_A
                || g.player_stock[c.emitter][1] < EPOCH_COST_B
                || g.player_stock[c.emitter][2] < EPOCH_COST_ME) {
                return RejectReason::ILLEGAL_STATE;
            }

            g.player_stock[c.emitter][0] -= EPOCH_COST_A;
            g.player_stock[c.emitter][1] -= EPOCH_COST_B;
            g.player_stock[c.emitter][2] -= EPOCH_COST_ME;
            g.player_epoch[c.emitter] = cur_epoch + 1u;
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

// RPS contra edificios (Sprint 1.1, SPEC-004 §7): clase defensora "edificio"
// — artillery ×2.0 (20000 bp), resto ×1.0 (10000 bp). unit_class==255 (el de
// los propios edificios) nunca llega aquí como ATACANTE (excluido más abajo
// por el mismo guard `unit_class[i] > 2` que ya excluye a los ciudadanos).
// Siege (unit_class==4 del catálogo) se incluye por completitud del contrato
// aunque SPAWN_UNIT todavía no admite esa clase (SPEC-002 §8.4).
inline int32_t rps_mult_vs_building_bp(uint8_t atk_class) noexcept {
    return (atk_class == 2u || atk_class == 4u) ? 20000 : 10000;
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
                    // Objetivo válido: unidad de combate (unit_class 0..2) O
                    // edificio (entity_kind==1, aunque su unit_class==255 lo
                    // deje fuera del rango 0..2). Ciudadanos (unit_class==3)
                    // siguen sin ser objetivo (Sprint 1.1, SPEC-004 §7).
                    if (g.unit_class[j] > 2 && g.entity_kind[j] != 1u) continue;

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
            const int32_t mult = (g.entity_kind[best] == 1u)
                                ? rps_mult_vs_building_bp(g.unit_class[i])
                                : rps_mult_bp(g.unit_class[i], g.unit_class[best]);
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
                    // Mismo criterio que combat_system (Sprint 1.1, SPEC-004
                    // §7): edificios sí son objetivo de aggro; ciudadanos no.
                    if (g.unit_class[j] > 2 && g.entity_kind[j] != 1u) continue;

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

// Dropoff-edificio (Sprint 1.1, SPEC-004 §6). Wiring en step.hpp (NO en
// economy.hpp, que sigue autocontenido y sin conocer GameState): resuelve el
// punto de entrega para el ciudadano `citizen_x/y` del jugador `owner` que
// carga el recurso `resource_idx` (0=A,1=B,2=Me). Busca el edificio PROPIO,
// COMPLETO (build_progress >= build_time_ticks), con dropoff_mask incluyendo
// `resource_idx`, más cercano (dist_sq_raw centro-a-centro, empate ⇒ menor
// índice — misma métrica/desempate que combat/aggro/eco_find_nearest_deposit).
// El punto de entrega es el clamp de la posición del ciudadano al rectángulo
// del footprint del edificio elegido. Devuelve false si el jugador no tiene
// ninguno (el caller aplica el fallback legacy: dropoff_x/y[owner]).
inline bool find_building_dropoff(const GameState& g, uint8_t owner, uint8_t resource_idx,
                                  int64_t citizen_x, int64_t citizen_y,
                                  int64_t& out_x, int64_t& out_y) noexcept {
    if (g.catalog == nullptr) return false;
    const EntityTable& t = g.entities;
    uint32_t best = t.capacity;
    uint64_t best_d2 = 0;
    const Vec2Fx here{Fx{citizen_x}, Fx{citizen_y}};

    for (uint32_t j = 0; j < t.capacity; ++j) {
        if (!t.alive[j]) continue;
        if (g.entity_kind[j] != 1u) continue;
        if (g.owner[j] != owner) continue;
        const BuildingId bid = g.building_id[j];
        if (bid >= g.catalog->building_count) continue;  // defensivo: catálogo desalineado
        const BuildingDefinitionV1& def = g.catalog->buildings[bid];
        if (g.build_progress[j] < def.build_time_ticks) continue;      // no completo
        if ((def.dropoff_mask & (1u << resource_idx)) == 0u) continue; // no acepta este recurso

        FatalReason local_fatal = FatalReason::NONE;  // descartado a propósito, mismo patrón que combat/aggro
        const Vec2Fx there{Fx{g.pos_x[j]}, Fx{g.pos_y[j]}};
        const uint64_t d2 = dist_sq_raw(here, there, local_fatal);
        if (best == t.capacity || d2 < best_d2 || (d2 == best_d2 && j < best)) {
            best = j;
            best_d2 = d2;
        }
    }
    if (best == t.capacity) return false;

    const BuildingDefinitionV1& def = g.catalog->buildings[g.building_id[best]];
    const int64_t T = FX_ONE_RAW;
    const int64_t bx0 = static_cast<int64_t>(g.bld_anchor_tx[best]) * T;
    const int64_t by0 = static_cast<int64_t>(g.bld_anchor_ty[best]) * T;
    const int64_t bw = static_cast<int64_t>(def.footprint_w) * T;
    const int64_t bh = static_cast<int64_t>(def.footprint_h) * T;
    int64_t cx = citizen_x;
    if (cx < bx0) cx = bx0; else if (cx > bx0 + bw) cx = bx0 + bw;
    int64_t cy = citizen_y;
    if (cy < by0) cy = by0; else if (cy > by0 + bh) cy = by0 + bh;
    out_x = cx;
    out_y = cy;
    return true;
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
        // Sprint 1.1 (SPEC-004 §4.2): mientras build_target esté activo el
        // ciudadano queda FUERA del pipeline económico (construction_system,
        // más abajo, es quien lo mueve/hace avanzar el progreso).
        if (g.build_target[i] != BUILD_NO_TARGET) continue;

        EcoCitizenIn in{};
        in.pos_x = g.pos_x[i];
        in.pos_y = g.pos_y[i];
        in.state = g.eco_state[i];
        in.assigned_deposit = g.eco_assigned_deposit[i];
        in.carry = g.eco_carry[i];
        in.carry_resource_idx = g.eco_carry_resource[i];
        in.speed_mtpt = g.speed_mtpt[i];

        const uint8_t owner_i = g.owner[i];
        // Dropoff resuelto (Sprint 1.1, SPEC-004 §6): edificio propio completo
        // con el bit del recurso cargado, si existe; si no, fallback legacy
        // EXACTO (dropoff_x/y[owner]) — preserva bit a bit la trayectoria de
        // los escenarios golden sin edificios (§9.1). Solo importa en RETURN
        // (economy.hpp únicamente lee dropoff_x/y en esa rama del switch).
        int64_t drop_x = g.dropoff_x[owner_i];
        int64_t drop_y = g.dropoff_y[owner_i];
        if (in.state == EcoState::RETURN) {
            int64_t bx = 0, by = 0;
            if (detail::find_building_dropoff(g, owner_i, g.eco_carry_resource[i],
                                              in.pos_x, in.pos_y, bx, by)) {
                drop_x = bx;
                drop_y = by;
            }
        }
        const EcoCitizenOut out = eco_step_citizen(
                in, g.deposits, g.n_deposits, drop_x, drop_y, g.fatal);

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

// Sistema constructor (Sprint 1.1, SPEC-004 §5). Fase propia, después de
// economía y antes del destroy batch, iteración ascendente por índice.
//
// Desviación documentada frente a la prosa literal del §5 ("tgt[i] = p_cerca
// // el movement system lo lleva"): `movement_v1` está marcada CONGELADA
// (SPEC-001 §12) y excluye incondicionalmente unit_class>2 (ciudadanos), así
// que delegarle el desplazamiento habría exigido tocar código congelado. En
// vez de eso, este sistema mueve al ciudadano DIRECTAMENTE (mismo patrón
// snap-si-el-paso-cubre/normalize+step que economy.hpp::try_move), sin pasar
// por tgt_x/tgt_y ni por movement_v1. El comportamiento observable — el
// ciudadano converge a p_cerca y, al llegar, suma progreso — es idéntico al
// descrito; solo cambia el mecanismo interno de locomoción.
inline void construction_system(GameState& g) noexcept {
    const EntityTable& t = g.entities;
    for (uint32_t i = 0; i < t.capacity; ++i) {
        if (!t.alive[i]) continue;
        if (g.unit_class[i] != 3u) continue;
        if (g.build_target[i] == BUILD_NO_TARGET) continue;

        const uint32_t b = g.build_target[i];
        bool invalid = (b >= t.capacity) || !t.alive[b] || (g.entity_kind[b] != 1u);
        uint32_t T = 0;
        const BuildingDefinitionV1* bdef = nullptr;
        if (!invalid) {
            if (g.catalog == nullptr || g.building_id[b] >= g.catalog->building_count) {
                invalid = true;
            } else {
                bdef = &g.catalog->buildings[g.building_id[b]];
                T = bdef->build_time_ticks;
                if (g.build_progress[b] >= T) invalid = true;
            }
        }
        if (invalid) {
            g.build_target[i] = BUILD_NO_TARGET;
            continue;  // vuelve a economía en el siguiente tick
        }

        const int64_t Traw = FX_ONE_RAW;
        const int64_t bx0 = static_cast<int64_t>(g.bld_anchor_tx[b]) * Traw;
        const int64_t by0 = static_cast<int64_t>(g.bld_anchor_ty[b]) * Traw;
        const int64_t bw = static_cast<int64_t>(bdef->footprint_w) * Traw;
        const int64_t bh = static_cast<int64_t>(bdef->footprint_h) * Traw;

        int64_t cx = g.pos_x[i];
        if (cx < bx0) cx = bx0; else if (cx > bx0 + bw) cx = bx0 + bw;
        int64_t cy = g.pos_y[i];
        if (cy < by0) cy = by0; else if (cy > by0 + bh) cy = by0 + bh;

        const Vec2Fx here{Fx{g.pos_x[i]}, Fx{g.pos_y[i]}};
        const Vec2Fx there{Fx{cx}, Fx{cy}};
        FatalReason local_fatal = FatalReason::NONE;  // descartado, mismo patrón que combat/aggro
        const uint64_t d_sq = dist_sq_raw(here, there, local_fatal);
        const uint64_t arrive_sq =
            static_cast<uint64_t>(BUILD_ARRIVE_RADIUS_RAW) * static_cast<uint64_t>(BUILD_ARRIVE_RADIUS_RAW);

        if (d_sq > arrive_sq) {
            // Mover hacia p_cerca: snap si el paso cubre la distancia, si no
            // normalize+step (idéntico a economy.hpp::try_move).
            const int64_t step_i64 = (static_cast<int64_t>(g.speed_mtpt[i]) * FX_ONE_RAW) / 1000;
            if (step_i64 <= 0) {
                g.vel_x[i] = 0; g.vel_y[i] = 0;
            } else {
                uint64_t step_sq;
                if (static_cast<uint64_t>(step_i64) > UINT32_MAX) {
                    step_sq = UINT64_MAX;
                } else {
                    const uint64_t s = static_cast<uint64_t>(step_i64);
                    step_sq = s * s;
                }
                if (d_sq <= step_sq) {
                    g.pos_x[i] = cx; g.pos_y[i] = cy;
                    g.vel_x[i] = 0; g.vel_y[i] = 0;
                } else {
                    const Vec2Fx d{Fx{cx - g.pos_x[i]}, Fx{cy - g.pos_y[i]}};
                    const Vec2Fx dir = normalize_v1(d, g.fatal);
                    const Fx vx = fx_mul(dir.x, Fx{step_i64}, g.fatal);
                    const Fx vy = fx_mul(dir.y, Fx{step_i64}, g.fatal);
                    g.vel_x[i] = vx.raw; g.vel_y[i] = vy.raw;
                    g.pos_x[i] = fx_add(Fx{g.pos_x[i]}, vx, g.fatal).raw;
                    g.pos_y[i] = fx_add(Fx{g.pos_y[i]}, vy, g.fatal).raw;
                }
            }
        } else {
            g.build_progress[b] += 1u;
            if (g.build_progress[b] > T) g.build_progress[b] = T;
            g.vel_x[i] = 0; g.vel_y[i] = 0;
        }
    }
}

// Sistema de producción (Sprint 1.2, SPEC-004 §11.4). Fase propia, después de
// construction_system y antes del destroy batch, iteración ascendente por
// índice sobre edificios vivos COMPLETOS con cola no vacía.
inline void production_system(GameState& g) noexcept {
    const EntityTable& t = g.entities;
    for (uint32_t i = 0; i < t.capacity; ++i) {
        if (!t.alive[i]) continue;
        if (g.entity_kind[i] != 1u) continue;
        if (g.catalog == nullptr || g.building_id[i] >= g.catalog->building_count) continue;
        const BuildingDefinitionV1& bdef = g.catalog->buildings[g.building_id[i]];
        if (g.build_progress[i] < bdef.build_time_ticks) continue;  // edificio no completo
        if (g.prod_count[i] == 0u) continue;

        const UnitId head_uid = g.prod_queue[i][0];
        const UnitDefinitionV1& udef = g.catalog->units[head_uid];
        ++g.prod_progress[i];
        if (g.prod_progress[i] < static_cast<uint32_t>(udef.build_time_ticks)) continue;

        // Posición = punto medio del lado inferior del footprint + medio
        // tile, exacto en raw (§11.4 literal).
        const int64_t T = FX_ONE_RAW;
        const int64_t bx0 = static_cast<int64_t>(g.bld_anchor_tx[i]) * T;
        const int64_t by0 = static_cast<int64_t>(g.bld_anchor_ty[i]) * T;
        const int64_t bw = static_cast<int64_t>(bdef.footprint_w) * T;
        const int64_t bh = static_cast<int64_t>(bdef.footprint_h) * T;
        const int64_t spawn_x = bx0 + bw / 2;
        const int64_t spawn_y = by0 + bh + T / 2;

        const EntityHandle h = et_spawn(g.entities);
        if (handle_eq(h, NULL_HANDLE)) {
            // Sin slot de entidad libre: el ítem espera (reintenta cada tick,
            // sin perder progreso — determinista). Revertir el incremento de
            // este tick para no rebasar build_time_ticks mientras se reintenta.
            --g.prod_progress[i];
            continue;
        }
        const uint32_t ni = h.index;
        g.pos_x[ni] = spawn_x; g.pos_y[ni] = spawn_y;
        g.vel_x[ni] = 0; g.vel_y[ni] = 0;
        g.owner[ni] = g.owner[i];
        g.unit_id[ni] = head_uid;
        if (udef.unit_class == UnitClassV1::Citizen) {
            detail::init_citizen_from_catalog(g, ni, udef);
        } else {
            detail::init_combat_unit_from_catalog(g, ni, udef);
        }
        if (g.rally_set[i]) {
            g.tgt_x[ni] = g.rally_x[i]; g.tgt_y[ni] = g.rally_y[i];
        } else {
            g.tgt_x[ni] = spawn_x; g.tgt_y[ni] = spawn_y;
        }

        // Desplazar la cola una posición (FIFO): el ítem 1 pasa a ser el 0.
        for (uint8_t k = 1; k < g.prod_count[i]; ++k) g.prod_queue[i][k - 1] = g.prod_queue[i][k];
        g.prod_queue[i][g.prod_count[i] - 1] = INVALID_UNIT_ID;
        --g.prod_count[i];
        g.prod_progress[i] = 0;
    }
}

// Sistema de investigación (Sprint 1.2, SPEC-004 §12.3: "misma fase que
// production"). Completa el research en curso de cada edificio vivo,
// iteración ascendente por índice: al alcanzar research_time_ticks, marca el
// bit de la tech en player_techs y hace OR de sus grants en player_caps.
inline void research_system(GameState& g) noexcept {
    const EntityTable& t = g.entities;
    for (uint32_t i = 0; i < t.capacity; ++i) {
        if (!t.alive[i]) continue;
        if (g.entity_kind[i] != 1u) continue;
        if (g.research_tech[i] == INVALID_TECH_ID) continue;
        if (g.catalog == nullptr || g.building_id[i] >= g.catalog->building_count) continue;

        const TechId tid = g.research_tech[i];
        if (tid >= g.catalog->tech_count) {
            // Defensivo (catálogo desalineado): abandona el research sin
            // completar en vez de leer fuera de rango.
            g.research_tech[i] = INVALID_TECH_ID;
            g.research_progress[i] = 0;
            continue;
        }
        const TechDefinitionV1& tdef = g.catalog->techs[tid];
        ++g.research_progress[i];
        if (g.research_progress[i] < tdef.research_time_ticks) continue;

        const uint8_t owner_i = g.owner[i];
        const uint32_t tw = tid / 64u, tb = tid % 64u;
        if (tw < TECH_WORDS) g.player_techs[owner_i][tw] |= (1ull << tb);
        for (uint8_t k = 0; k < tdef.grant_count; ++k) {
            const CapabilityId cap = tdef.grants[k];
            const uint32_t cw = cap / 64u, cb = cap % 64u;
            if (cw < CAP_WORDS) g.player_caps[owner_i][cw] |= (1ull << cb);
        }
        g.research_tech[i] = INVALID_TECH_ID;
        g.research_progress[i] = 0;
    }
}

// Condición de victoria/derrota v1 (Sprint 1.4, SPEC-005 §6). Llamada AL
// FINAL de step(), tras el destroy batch, barrido ascendente. Sin
// RNG/float/heap (dos arrays locales fijos de MAX_EMITTERS==16 elementos,
// en pila).
//
// "Jugador activo" (definición operativa, ver RESULT del sprint): un emisor
// p ∈ [0, cfg.player_count) es activo ⟺ en algún momento del partido tuvo
// AL MENOS UN edificio (entity_kind==1) o UN ciudadano (unit_class==3) vivo
// simultáneamente con game_over==0 — se registra de forma monótona en
// g.participants_mask (bit p, solo se pone a 1, nunca se limpia) la PRIMERA
// vez que se observa. Un emisor configurado en player_count que nunca llegó
// a tener edificio/ciudadano (p.ej. el emisor 1 en una corrida sin IA,
// player_count==1; o un jugador de fixture nunca poblado) JAMÁS entra en el
// conjunto activo — SPEC-005 §6 exige explícitamente no marcarlo "ganador"
// (ni contarlo como "derrotado" para forzar un empate espurio).
//
// Salvaguarda adicional (deviación documentada frente a la letra literal de
// SPEC-005 §6, ver RESULT — "conservador ante huecos"): la evaluación solo
// se dispara con >= 2 jugadores activos. Con 0 o 1 activo no hay adversario
// posible y la partida NUNCA puede "terminar" por este mecanismo (evita que
// escenarios de un único jugador real —sintéticos de movimiento con
// SPAWN_DEBUG, benchmarks, o un fixture de 2 jugadores donde solo uno de
// ellos llega a tener producción real— declaren un "ganador"/"empate"
// espurio con game_over==1 desde el primer tick).
//
// Derrota (regla v1 concreta y testeable de SPEC-005 §6): un jugador ACTIVO
// está derrotado cuando, EN ESTE INSTANTE, no tiene ningún edificio vivo
// propio NI ningún ciudadano vivo propio (no puede producir ni reconstruir).
// Nota: esto es la observación INSTANTÁNEA (no monótona) — a diferencia de
// participants_mask, un jugador puede pasar de derrotado a no-derrotado si
// reconstruye (irrelevante en la práctica: una vez game_over==1 se congela).
//
// Congelado: si g.game_over ya es 1, esta función es un no-op inmediato
// (SPEC-005 §6: "una vez game_over==1, step deja de evaluar").
inline void victory_check(GameState& g) noexcept {
    if (g.game_over != 0u) return;  // congelado: no reevaluar jamás

    bool has_building[MAX_EMITTERS] = {};
    bool has_citizen[MAX_EMITTERS] = {};
    const EntityTable& t = g.entities;
    for (uint32_t i = 0; i < t.capacity; ++i) {
        if (!t.alive[i]) continue;
        const uint8_t p = g.owner[i];
        if (p >= MAX_EMITTERS) continue;  // defensivo: owner fuera de rango nunca debería darse
        if (g.entity_kind[i] == 1u) has_building[p] = true;
        else if (g.unit_class[i] == 3u) has_citizen[p] = true;
    }

    // Actualiza la máscara monótona de "jugador activo" (solo bits que se
    // encienden, nunca se apagan) ANTES de evaluar derrota/victoria.
    for (uint32_t p = 0; p < g.cfg.player_count; ++p) {
        if (has_building[p] || has_citizen[p]) {
            g.participants_mask = static_cast<uint16_t>(g.participants_mask | (uint16_t{1} << p));
        }
    }

    uint32_t active_count = 0;
    for (uint32_t p = 0; p < g.cfg.player_count; ++p) {
        if ((g.participants_mask & (uint16_t{1} << p)) != 0u) ++active_count;
    }
    if (active_count < 2u) return;  // sin adversario posible: nunca "termina" por este mecanismo

    uint32_t not_defeated_count = 0;
    uint8_t sole_survivor = 0xFFu;
    for (uint32_t p = 0; p < g.cfg.player_count; ++p) {
        if ((g.participants_mask & (uint16_t{1} << p)) == 0u) continue;  // nunca jugó: fuera del cómputo
        const bool defeated = !has_building[p] && !has_citizen[p];
        if (!defeated) {
            ++not_defeated_count;
            sole_survivor = static_cast<uint8_t>(p);
        }
    }
    if (not_defeated_count == 0u) {
        g.game_over = 1u;
        g.winner = 0xFFu;  // empate: todos los activos derrotados el mismo tick
    } else if (not_defeated_count == 1u) {
        g.game_over = 1u;
        g.winner = sole_survivor;
    }
    // else: la partida sigue (>= 2 activos siguen sin ser derrotados).
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
            const uint32_t eff = command_effective_tick(
                    rc.target_tick, t, g.cfg.human_input_delay_ticks);  // §6.2
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

        // (5d) Constructor (Sprint 1.1, SPEC-004 §5): después de economía,
        // antes del destroy batch (contrato de orden de fases).
        detail::construction_system(g);

        // (5e) Producción + investigación (Sprint 1.2, SPEC-004 §11.4/§12.3):
        // después de construction_system, antes del destroy batch.
        detail::production_system(g);
        detail::research_system(g);

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
            // Sprint 1.1 (SPEC-004 §7): al reciclar un edificio, restaurar las
            // celdas de su footprint a cost_grid=1 (transitable) y marcar
            // flow_dirty — ANTES de zero_components, que resetea
            // building_id/anclas y perdería la información del footprint.
            if (g.entity_kind[i] == 1u) {
                const uint64_t tx = g.bld_anchor_tx[i];
                const uint64_t ty = g.bld_anchor_ty[i];
                uint64_t fw = 0, fh = 0;
                if (g.catalog != nullptr && g.building_id[i] < g.catalog->building_count) {
                    const BuildingDefinitionV1& def = g.catalog->buildings[g.building_id[i]];
                    fw = def.footprint_w;
                    fh = def.footprint_h;
                }
                for (uint64_t cy = ty; cy < ty + fh && cy < FF_AXIS; ++cy) {
                    for (uint64_t cx = tx; cx < tx + fw && cx < FF_AXIS; ++cx) {
                        g.cost_grid[cy * FF_AXIS + cx] = 1u;
                    }
                }
                g.flow_dirty = 1;
                // Sprint 1.2 (SPEC-004 §11.4): la muerte del edificio pierde su
                // cola de producción (costes ya pagados se pierden); la
                // población reservada de los ítems NO entrenados se libera
                // (pop_cost=1 constante v1, uno por ítem restante en la cola).
                // El research en curso (si lo hubiera) también se pierde sin
                // reembolso — zero_components lo resetea a continuación
                // (mismo espíritu "sin CANCEL" de Parte II, ver RESULT).
                if (g.prod_count[i] > 0u) {
                    const uint8_t owner_i = g.owner[i];
                    int32_t freed = static_cast<int32_t>(g.prod_count[i]);
                    g.pop_used[owner_i] -= freed;
                    if (g.pop_used[owner_i] < 0) g.pop_used[owner_i] = 0;
                }
            } else {
                // Sprint 1.2 (SPEC-004 §11.4): la muerte de una unidad reduce
                // pop_used (pop_cost=1 constante v1, para TODA unidad —
                // deviación documentada: unidades creadas fuera de la cola de
                // producción, p.ej. SPAWN_UNIT/SPAWN_CITIZEN/debug, nunca
                // incrementaron pop_used; se clampa a 0 para no ir negativo).
                const uint8_t owner_i = g.owner[i];
                g.pop_used[owner_i] -= 1;
                if (g.pop_used[owner_i] < 0) g.pop_used[owner_i] = 0;
            }
            zero_components(g, i);
            et_release_index(g.entities, i);
        }
        g.destroy_count = 0;

        // (6b) Sprint 1.4 (SPEC-005 §6): condición de victoria/derrota, tras
        // el destroy batch, barrido ascendente. Sin RNG/float/heap. Congela
        // en cuanto game_over==1 (no se reevalúa jamás — SPEC-005 §6).
        detail::victory_check(g);
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
