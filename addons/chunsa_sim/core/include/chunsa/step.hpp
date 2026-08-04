#pragma once
#include <cstdint>

#include "chunsa/game_state.hpp"
#include "chunsa/market.hpp"
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

// Alcance de CONTACTO. Un arma con alcance cero no es un arma: con range_mt=0
// el filtro de combate exige d2==0, o sea que el enemigo este en la MISMA
// coordenada raw. Seis de las nueve unidades del catalogo estaban asi, y por
// eso dos ejercitos podian orbitar a 4,4 tiles cien mil ticks sin tocarse.
//
// El contacto es propiedad del MUNDO, no del arma: `range_millitiles` expresa
// el alcance MAS ALLA del contacto. Un tile, el mismo valor con el que
// ECO_ARRIVE_RADIUS_RAW y BUILD_ARRIVE_RADIUS_RAW dicen "he llegado".
inline constexpr int64_t MELEE_CONTACT_RAW = FX_ONE_RAW;   // 1 tile

// Construcción de edificios (Sprint 1.1, SPEC-004 §5): mismo radio de
// llegada que la economía (alias explícito, no una constante nueva
// independiente — comparten semántica "a un tile o menos del objetivo").
inline constexpr int64_t BUILD_ARRIVE_RADIUS_RAW = ECO_ARRIVE_RADIUS_RAW;

// GATHER (Sprint 1.6B, SPEC-004 §18): radio de "pick" del depósito objetivo —
// 1 tile, mismo valor que ECO_ARRIVE_RADIUS_RAW pero constante PROPIA (alias
// explícito, no la reutiliza directamente): semántica distinta ("¿hay un
// depósito aquí?" vs "¿llegué al depósito/dropoff?"), aunque el valor
// numérico coincida en v1.
inline constexpr int64_t GATHER_PICK_RADIUS_RAW = FX_ONE_RAW;

// EPOCH_UP (Sprint 1.2, SPEC-004 §12.3, ADR-015): constantes v1 (brief K2, no
// re-litigar). EPOCH_MIN_TICKS = 20 ticks/s * 300 s = 6000 (gate b: tiempo
// mínimo desde la época inicial). EPOCH_COST_*: coste fijo v1. EPOCH_MAX_V1:
// época máxima del slice — v1 la fija como constante literal (no derivada del
// catálogo pese a la prosa "de los datos del match" del spec; ver RESULT).
// Sprint 1.14 (SPEC-004 §11.3) — tope de poblacion DERIVADO de las viviendas.
//
// Funcion PURA del estado: suma `population_provided` de los edificios propios
// COMPLETOS y acota por POP_CAP_V1. Que sea derivada y no un campo nuevo evita
// el fallo clasico del contador incremental —desincronizarse al morir un
// edificio a medio construir, al cancelar una cola o al reciclar un slot— y
// ademas la deja FUERA del dominio del checksum, igual que `flow`: la
// informacion para recalcularla ya viaja en el estado.
inline int32_t player_pop_cap(const GameState& g, uint8_t player) noexcept {
    if (g.catalog == nullptr) return 0;
    int64_t total = 0;
    for (uint32_t i = 0; i < g.entities.capacity; ++i) {
        if (!g.entities.alive[i]) continue;
        if (g.owner[i] != player) continue;
        if (g.entity_kind[i] != 1u) continue;                    // solo edificios
        if (g.building_id[i] >= g.catalog->building_count) continue;
        const BuildingDefinitionV1& bdef = g.catalog->buildings[g.building_id[i]];
        // COMPLETO: una casa a medio construir no aloja a nadie. Si contara,
        // se podria entrenar contra poblacion inexistente y bastaria cancelar
        // la obra para quedarse por encima del tope.
        if (static_cast<uint32_t>(g.build_progress[i]) < bdef.build_time_ticks) continue;
        if (bdef.population_provided <= 0) continue;
        total += bdef.population_provided;
        if (total >= static_cast<int64_t>(POP_CAP_V1)) return static_cast<int32_t>(POP_CAP_V1);
    }
    return static_cast<int32_t>(total);
}

inline constexpr uint32_t EPOCH_MIN_TICKS = 6000;
inline constexpr int32_t EPOCH_COST_A = 200;
inline constexpr int32_t EPOCH_COST_B = 200;
inline constexpr int32_t EPOCH_COST_ME = 100;
// Sprint 1.25: 7 -> 15. El tope era el del "slice v1", cuando el juego llegaba
// a la Medieval y nada más. La escala de SPEC-007 §2 siempre tuvo QUINCE
// épocas, y los recursos de las últimas (uranio en la 14, silicio y tierras
// raras en la 15) llevaban meses declarados en `data/resources/` sin que
// ninguna partida pudiera alcanzarlos: eran filas de una tabla.
//
// AVISO SOBRE REPLAYS ANTIGUOS. Esto relaja una condición de aceptación: un
// ADVANCE_EPOCH que ANTES se rechazaba en la época 7 ahora se acepta. Un
// replay grabado con el tope viejo que contuviera ese comando divergiría. En
// la práctica no existe ninguno —ninguna civilización pasaba de la época 5
// hasta el 1.22— pero la condición queda dicha en vez de descubierta.
//
// LA RAMPA NO SE TOCA, y conviene ver lo que implica: el gate (b) exige
// `tick >= EPOCH_MIN_TICKS * pasos`, así que llegar a la época 15 desde la 1
// pide 6000*14 = 84000 ticks, unos 70 minutos a 20 ticks/s. Es una duración
// de partida coherente para recorrer toda la historia, pero es una decisión de
// BALANCE que hasta hoy nadie había tenido que tomar porque el tope lo hacía
// inalcanzable. Queda señalada para el Director, no cambiada por mi cuenta.
inline constexpr uint8_t EPOCH_MAX_V1 = 15;

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
    // citizen: attack=0 + el guard de ATACANTE `unit_class[i] > 2` en
    // combat_system/aggro_system lo mantienen sin atacar/perseguir. Como
    // OBJETIVO, desde SPEC-004 §7.1 (enmienda Sprint 1.4-cierre) SÍ es
    // vulnerable en combate — ya no está excluido en ese rol.
    g.unit_class[i] = 3;
    g.atk_cd[i] = 0;
    g.speed_mtpt[i] = def.speed_millitile_tick;
    g.morale[i] = def.morale;
    g.fleeing[i] = 0;
    g.eco_state[i] = EcoState::SEEK;
    g.eco_assigned_deposit[i] = ECO_NO_DEPOSIT;
    g.eco_carry[i] = 0;
    g.eco_carry_resource[i] = 0;
    // §22.4: todo ciudadano data-driven (SPAWN_UNIT, SPAWN_CITIZEN o
    // TRAIN_UNIT vía production_system) nace auto-asignado a GATHER.
    g.citizen_task[i] = CITIZEN_TASK_GATHER;
}

// Sprint 1.8A (SPEC-007 §9.3/§11): comprobación y deducción atómicas sobre el
// vector completo de costes. Ambos recorridos son ascendentes y no asignan.
inline bool resource_costs_affordable(
        const GameState& g, uint16_t emitter,
        const int32_t (&cost)[RESOURCE_COUNT]) noexcept {
    for (uint32_t resource = 0; resource < RESOURCE_COUNT; ++resource) {
        if (g.player_stock[emitter][resource] < cost[resource]) return false;
    }
    return true;
}

inline void deduct_resource_costs(
        GameState& g, uint16_t emitter,
        const int32_t (&cost)[RESOURCE_COUNT]) noexcept {
    for (uint32_t resource = 0; resource < RESOURCE_COUNT; ++resource) {
        g.player_stock[emitter][resource] -= cost[resource];
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
            // Ciudadano económico: unit_class=3 lo excluye de combat_system/
            // aggro_system como ATACANTE (guard `unit_class[i] > 2` intacto:
            // no ataca ni persigue). Como OBJETIVO SÍ es vulnerable desde
            // SPEC-004 §7.1 (enmienda Sprint 1.4-cierre) — ver el guard de
            // targeting allí. Alias restringido a clase Citizen del mismo
            // camino data-driven que SPAWN_UNIT (SPEC-002 §8.4).
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
            g.unit_class[i] = 3;  // citizen: no ataca; SÍ vulnerable (SPEC-004 §7.1)
            g.atk_cd[i] = 0;
            g.speed_mtpt[i] = c.p.speed_mtpt;
            g.morale[i] = MORALE_MAX;
            g.fleeing[i] = 0;
            g.unit_id[i] = INVALID_UNIT_ID;
            g.eco_state[i] = EcoState::SEEK;
            g.eco_assigned_deposit[i] = ECO_NO_DEPOSIT;
            g.eco_carry[i] = 0;
            g.eco_carry_resource[i] = 0;
            g.citizen_task[i] = CITIZEN_TASK_GATHER;
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
                // Sprint 1.6B (SPEC-004 §17): gate de civilización — rechaza
                // si el edificio es de OTRA civ que la asignada al jugador.
                // Exención doble (ambas obligatorias, brief K1): (a)
                // INVALID_CIV_ID ("sin asignar") NO aplica el gate —
                // compatibilidad con todos los escenarios/tests existentes
                // que nunca llaman a gs_set_player_civ; (b) la MISMA ventana
                // de setup del tick 0 que el resto de gates de este bloque
                // (scenario_exempt ya cubre esta rama entera).
                if (g.player_civ[c.emitter] != INVALID_CIV_ID
                    && def.civ_id != g.player_civ[c.emitter]) {
                    return RejectReason::ILLEGAL_STATE;
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

            if (!scenario_exempt
                && !resource_costs_affordable(g, c.emitter, def.cost)) {
                return RejectReason::ILLEGAL_STATE;
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
                deduct_resource_costs(g, c.emitter, def.cost);
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
            g.citizen_task[ci] = CITIZEN_TASK_BUILD;
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

            // Sprint 1.6B (SPEC-004 §17): gate de civilización, mismo
            // patrón/exención que PLACE_BUILDING (INVALID_CIV_ID no aplica).
            // TRAIN_UNIT no tiene ventana de setup de tick 0 (§4.1.2 es
            // exclusiva de PLACE_BUILDING — los edificios iniciales del
            // escenario, nunca unidades entrenadas), así que no hace falta
            // ninguna exención adicional aquí.
            if (g.player_civ[c.emitter] != INVALID_CIV_ID
                && udef.civ_id != g.player_civ[c.emitter]) {
                return RejectReason::ILLEGAL_STATE;
            }

            if (g.prod_count[bi] >= PROD_QUEUE_CAP) return RejectReason::ILLEGAL_STATE;

            const int32_t pop_cost = udef.pop_cost;  // constante v1 = 1
            // Sprint 1.14: el tope ya no es la constante regalada, sino lo que
            // el jugador HAYA CONSTRUIDO. POP_CAP_V1 sigue dentro, como cota
            // dura, en player_pop_cap.
            // El cast es explícito a propósito: `emitter` es uint16_t y
            // `player_pop_cap` toma uint8_t. MSVC avisa (C4244) donde gcc y
            // clang callan, y con -Werror eso tumba TODA la matriz de Windows.
            // La conversión es segura y está probada arriba: el despacho
            // rechaza cualquier orden con `emitter >= MAX_EMITTERS`, y
            // MAX_EMITTERS es 16.
            if (g.pop_used[c.emitter] + pop_cost
                > player_pop_cap(g, static_cast<uint8_t>(c.emitter))) {
                return RejectReason::ILLEGAL_STATE;
            }

            if (!resource_costs_affordable(g, c.emitter, udef.cost)) {
                return RejectReason::ILLEGAL_STATE;
            }

            deduct_resource_costs(g, c.emitter, udef.cost);
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

            // Sprint 1.6B (SPEC-004 §17): gate de civilización, mismo
            // patrón/exención que PLACE_BUILDING/TRAIN_UNIT.
            if (g.player_civ[c.emitter] != INVALID_CIV_ID
                && tdef.civ_id != g.player_civ[c.emitter]) {
                return RejectReason::ILLEGAL_STATE;
            }

            if (g.research_tech[bi] != INVALID_TECH_ID) return RejectReason::ILLEGAL_STATE;  // edificio ocupado

            if (!resource_costs_affordable(g, c.emitter, tdef.cost)) {
                return RejectReason::ILLEGAL_STATE;
            }

            deduct_resource_costs(g, c.emitter, tdef.cost);
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
        case CommandType::ATTACK: {
            // SPEC-004 §24.2: handle vivo, propio, es unidad, objetivo vivo,
            // objetivo NO propio. El orden es el del contrato.
            if (!et_is_alive(g.entities, c.p.handle)) return RejectReason::INVALID_ENTITY;
            const uint32_t i = c.p.handle.index;
            if (g.owner[i] != c.emitter) return RejectReason::NOT_OWNER;
            if (g.entity_kind[i] != 0u) return RejectReason::ILLEGAL_STATE;

            const EntityHandle tgt{c.p.unit_id,
                                   static_cast<uint32_t>(c.p.speed_mtpt)};
            if (!et_is_alive(g.entities, tgt)) return RejectReason::INVALID_ENTITY;
            // Atacar a los tuyos no es una orden valida: es un descuido, y el
            // juego no debe ejecutarlo en silencio.
            if (g.owner[tgt.index] == c.emitter) return RejectReason::NOT_OWNER;

            g.attack_target[i] = tgt;
            g.order_mode[i] = ORDER_MODE_ATTACK;
            return RejectReason::ACCEPTED;
        }
        case CommandType::ATTACK_MOVE: {
            if (!et_is_alive(g.entities, c.p.handle)) return RejectReason::INVALID_ENTITY;
            const uint32_t i = c.p.handle.index;
            if (g.owner[i] != c.emitter) return RejectReason::NOT_OWNER;
            if (g.entity_kind[i] != 0u) return RejectReason::ILLEGAL_STATE;
            const Vec2Fx dest{Fx{c.p.x_raw}, Fx{c.p.y_raw}};
            if (!world_contains(dest)) return RejectReason::MALFORMED;

            g.tgt_x[i] = c.p.x_raw;
            g.tgt_y[i] = c.p.y_raw;
            g.attack_target[i] = NULL_HANDLE;
            g.order_mode[i] = ORDER_MODE_ATTACK_MOVE;
            return RejectReason::ACCEPTED;
        }
        case CommandType::TRADE: {
            // Sprint 1.33 (SPEC-010) — mercado. El orden de los rechazos es
            // contractual, como en CRAFT: el jugador debe recibir el motivo
            // MAS ESPECIFICO primero.
            //
            // 1) El handle debe ser un EDIFICIO propio, vivo y COMPLETO. Sin
            //    mercado no se comercia: es lo que ata la mecanica a una
            //    decision de construccion y no a un menu siempre disponible.
            if (!et_is_alive(g.entities, c.p.handle)) return RejectReason::INVALID_ENTITY;
            const uint32_t bi = c.p.handle.index;
            if (g.owner[bi] != c.emitter) return RejectReason::NOT_OWNER;
            if (g.entity_kind[bi] != 1u) return RejectReason::ILLEGAL_STATE;
            if (g.catalog == nullptr || g.building_id[bi] >= g.catalog->building_count) {
                return RejectReason::ILLEGAL_STATE;
            }
            const BuildingDefinitionV1& mdef = g.catalog->buildings[g.building_id[bi]];
            if (static_cast<uint32_t>(g.build_progress[bi]) < mdef.build_time_ticks) {
                return RejectReason::ILLEGAL_STATE;
            }
            if (mdef.can_trade == 0u) return RejectReason::ILLEGAL_STATE;

            // 2) Recurso valido, y NO el oro: cambiar oro por oro no significa
            //    nada y seria una via silenciosa de mover el precio sin coste.
            // El indice del ORO no es fijo: lo asigna el compilador (solo
            // comida/madera/piedra tienen indice reservado). Se resuelve del
            // catalogo en vez de cablearlo, o un cambio de datos romperia el
            // mercado en silencio.
            const ResourceId oro_id = catalog_find_resource(*g.catalog, "chunsa:gold", 11);
            if (oro_id == INVALID_RESOURCE_ID) return RejectReason::ILLEGAL_STATE;
            const uint32_t oro = g.catalog->resources[oro_id].index;

            const uint32_t ridx = c.p.unit_id;
            if (ridx >= RESOURCE_COUNT || ridx == oro) return RejectReason::MALFORMED;
            const int32_t signo = c.p.hp;
            if (signo == 0) return RejectReason::MALFORMED;

            int32_t precio = g.market_price_bp[c.emitter][ridx];
            if (precio == 0) precio = MARKET_BASE_BP;   // sin inicializar = base

            if (signo < 0) {
                // VENDER un lote: hay que tenerlo.
                if (g.player_stock[c.emitter][ridx] < MARKET_LOT) {
                    return RejectReason::ILLEGAL_STATE;
                }
                g.player_stock[c.emitter][ridx] -= MARKET_LOT;
                g.player_stock[c.emitter][oro] += market_sell_gold(precio);
                g.market_price_bp[c.emitter][ridx] = market_price_after_sell(precio);
            } else {
                // COMPRAR un lote: hay que poder pagarlo.
                const int32_t coste = market_buy_gold(precio);
                if (g.player_stock[c.emitter][oro] < coste) {
                    return RejectReason::ILLEGAL_STATE;
                }
                g.player_stock[c.emitter][oro] -= coste;
                g.player_stock[c.emitter][ridx] += MARKET_LOT;
                g.market_price_bp[c.emitter][ridx] = market_price_after_buy(precio);
            }
            return RejectReason::ACCEPTED;
        }
        case CommandType::CRAFT: {
            // SPEC-007 §12.4. EL ORDEN DE ESTOS OCHO PASOS ES CONTRACTUAL: la
            // prueba 8 de §12.6 comprueba que un comando que viola varias
            // reglas devuelve el codigo de la PRIMERA. Cambiarlo de sitio
            // cambia el mensaje que ve el jugador.
            // 1) handle vivo
            if (!et_is_alive(g.entities, c.p.handle)) return RejectReason::INVALID_ENTITY;
            const uint32_t bi = c.p.handle.index;
            // 2) propio
            if (g.owner[bi] != c.emitter) return RejectReason::NOT_OWNER;
            // 3) es edificio
            if (g.entity_kind[bi] != 1u) return RejectReason::ILLEGAL_STATE;
            if (g.catalog == nullptr || g.building_id[bi] >= g.catalog->building_count) {
                return RejectReason::ILLEGAL_STATE;
            }
            const BuildingDefinitionV1& bdef = g.catalog->buildings[g.building_id[bi]];
            // 4) completo
            if (g.build_progress[bi] < bdef.build_time_ticks) return RejectReason::ILLEGAL_STATE;
            // 5) la receta esta en la lista de ESE edificio
            if (c.p.unit_id >= g.catalog->recipe_count) return RejectReason::ILLEGAL_STATE;
            const RecipeId rid = c.p.unit_id;
            bool in_recipes = false;
            for (uint8_t k = 0; k < bdef.recipe_count; ++k) {
                if (bdef.recipes[k] == rid) { in_recipes = true; break; }
            }
            if (!in_recipes) return RejectReason::ILLEGAL_STATE;
            const RecipeV1& rdef = g.catalog->recipes[rid];
            // 6) epoca alcanzada. La receta no lleva epoca propia: la hereda
            //    del edificio que la ejecuta, que ya tiene su ventana. Un
            //    campo duplicado seria una segunda verdad que mantener.
            if (bdef.epoch_min > g.player_epoch[c.emitter]) return RejectReason::ILLEGAL_STATE;
            // 7) edificio ocioso de produccion
            if (g.craft_recipe[bi] != INVALID_RECIPE_ID) return RejectReason::ILLEGAL_STATE;
            // 8) el stock cubre TODOS los inputs
            for (uint32_t r = 0; r < RESOURCE_COUNT; ++r) {
                if (rdef.input[r] <= 0) continue;
                if (g.player_stock[c.emitter][r] < rdef.input[r]) {
                    return RejectReason::ILLEGAL_STATE;
                }
            }
            // Deduccion POR ADELANTADO y de golpe (§12.4). Si se dedujera al
            // terminar, el jugador podria encolar lo que no puede pagar y el
            // sistema tendria que decidir que hacer al final; asi esa clase
            // entera de casos no existe.
            for (uint32_t r = 0; r < RESOURCE_COUNT; ++r) {
                if (rdef.input[r] > 0) g.player_stock[c.emitter][r] -= rdef.input[r];
            }
            g.craft_recipe[bi] = rid;
            g.craft_progress[bi] = 0;
            return RejectReason::ACCEPTED;
        }
        case CommandType::GATHER: {
            // SPEC-004 §18: p.handle = ciudadano propio; p.x_raw/p.y_raw =
            // punto raw del depósito objetivo. Orden de validación es
            // CONTRATO (testeado): handle vivo/propio, unit_class==3, resolver
            // depósito (índice más bajo con remaining>0 dentro de
            // GATHER_PICK_RADIUS_RAW del punto; ninguno -> INVALID_ENTITY).
            if (!et_is_alive(g.entities, c.p.handle)) return RejectReason::INVALID_ENTITY;
            const uint32_t ci = c.p.handle.index;
            if (g.owner[ci] != c.emitter) return RejectReason::NOT_OWNER;
            if (g.unit_class[ci] != 3u) return RejectReason::ILLEGAL_STATE;

            const Vec2Fx point{Fx{c.p.x_raw}, Fx{c.p.y_raw}};
            const uint64_t pick_r_sq = static_cast<uint64_t>(GATHER_PICK_RADIUS_RAW)
                                      * static_cast<uint64_t>(GATHER_PICK_RADIUS_RAW);
            uint32_t found = ECO_NO_DEPOSIT;
            for (uint32_t d = 0; d < g.n_deposits; ++d) {
                if (g.deposits[d].remaining <= 0) continue;
                const Vec2Fx there{Fx{g.deposits[d].x_raw}, Fx{g.deposits[d].y_raw}};
                FatalReason local_fatal{};  // descartado a propósito, mismo patrón que combat/aggro/eco
                const uint64_t d_sq = dist_sq_raw(point, there, local_fatal);
                if (d_sq <= pick_r_sq) { found = d; break; }  // índice más bajo: primer match ascendente
            }
            if (found == ECO_NO_DEPOSIT) return RejectReason::INVALID_ENTITY;

            g.eco_assigned_deposit[ci] = found;
            // Auditoría multimodelo 2026-07-27, F-01: si la redirección cambia
            // de recurso, la carga previa debe volver al dropoff antes de
            // buscar el depósito nuevo. RETURN conserva assigned_deposit y,
            // tras descargar, pasa a SEEK sin convertir el tipo transportado.
            const bool changes_loaded_resource =
                g.eco_carry[ci] > 0
                && g.deposits[found].resource_idx != g.eco_carry_resource[ci];
            g.eco_state[ci] = changes_loaded_resource ? EcoState::RETURN : EcoState::SEEK;
            // Recolectar cancela construir — decisión explícita del contrato
            // (SPEC-004 §18), mismo patrón que el resto del kernel usa
            // BUILD_NO_TARGET como centinela de "sin objetivo".
            g.build_target[ci] = BUILD_NO_TARGET;
            g.citizen_task[ci] = CITIZEN_TASK_GATHER;
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
            // SPEC-004 §22.2: MOVE_TO conserva toda la carga/asignación
            // económica. Solo en ciudadanos cambia la autoridad de locomoción
            // y cancela cualquier obra previa.
            if (g.unit_class[i] == 3u) {
                g.citizen_task[i] = CITIZEN_TASK_MOVE;
                g.build_target[i] = BUILD_NO_TARGET;
            }
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
                // Acorralada (Arquitecto 2026-08-04): si tras el paso de huida
                // la distancia al MISMO enemigo del que huía (best) NO ha
                // aumentado, es que no consigue alejarse — la empujan contra el
                // borde del mundo (clamps de arriba) o no hay a dónde ir. En
                // vez de huir contra el clamp para siempre, se planta y pelea:
                // deja de huir y la moral sube a MORALE_RALLY (a tope y no un
                // poco: por debajo del rally, el tick siguiente volvería a
                // entrar en pánico y el atasco solo cambiaría de nombre — una
                // oscilación). Entero y determinista: se compara la distancia
                // al cuadrado al mismo best, antes (best_d2) y después del paso.
                FatalReason local_fatal = FatalReason::NONE;
                const uint64_t d2_after = dist_sq_raw(
                    Vec2Fx{Fx{g.pos_x[i]}, Fx{g.pos_y[i]}},
                    Vec2Fx{Fx{g.pos_x[best]}, Fx{g.pos_y[best]}}, local_fatal);
                if (d2_after <= best_d2) {
                    g.fleeing[i] = 0;
                    g.morale[i] = MORALE_RALLY;
                }
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

// Locomoción individual del ciudadano (Sprint 1.7, SPEC-004 §22.3). Fase
// propia, antes de economía y construcción. movement_v1 permanece congelado
// y sigue excluyendo unit_class>2; este sistema es el único dueño de pos para
// un ciudadano cuya tarea explícita sea MOVE.
inline void citizen_move_system(GameState& g) noexcept {
    const EntityTable& t = g.entities;
    const uint64_t arrive_r_sq =
        static_cast<uint64_t>(ECO_ARRIVE_RADIUS_RAW)
        * static_cast<uint64_t>(ECO_ARRIVE_RADIUS_RAW);

    for (uint32_t i = 0; i < t.capacity; ++i) {
        if (!t.alive[i]) continue;
        if (g.unit_class[i] != 3u) continue;
        if (g.citizen_task[i] != CITIZEN_TASK_MOVE) continue;

        const Vec2Fx here{Fx{g.pos_x[i]}, Fx{g.pos_y[i]}};
        const Vec2Fx there{Fx{g.tgt_x[i]}, Fx{g.tgt_y[i]}};
        const uint64_t d_sq = dist_sq_raw(here, there, g.fatal);
        if (d_sq <= arrive_r_sq) {
            g.citizen_task[i] = CITIZEN_TASK_IDLE;
            g.vel_x[i] = 0;
            g.vel_y[i] = 0;
            continue;
        }

        // Forma idéntica a economy.hpp::try_move: step entero, snap si el
        // paso cubre la distancia, si no normalize_v1 + multiplicación fija.
        const int64_t step_i64 =
            (static_cast<int64_t>(g.speed_mtpt[i]) * FX_ONE_RAW) / 1000;
        if (step_i64 <= 0) {
            g.vel_x[i] = 0;
            g.vel_y[i] = 0;
            continue;
        }

        uint64_t step_sq;
        if (static_cast<uint64_t>(step_i64) > UINT32_MAX) {
            step_sq = UINT64_MAX;
        } else {
            const uint64_t s = static_cast<uint64_t>(step_i64);
            step_sq = s * s;
        }
        if (d_sq <= step_sq) {
            g.pos_x[i] = g.tgt_x[i];
            g.pos_y[i] = g.tgt_y[i];
            g.vel_x[i] = 0;
            g.vel_y[i] = 0;
            g.citizen_task[i] = CITIZEN_TASK_IDLE;
            continue;
        }

        const Vec2Fx d{Fx{g.tgt_x[i] - g.pos_x[i]},
                       Fx{g.tgt_y[i] - g.pos_y[i]}};
        const Vec2Fx dir = normalize_v1(d, g.fatal);
        const Fx step_fx{step_i64};
        const Fx vx = fx_mul(dir.x, step_fx, g.fatal);
        const Fx vy = fx_mul(dir.y, step_fx, g.fatal);
        g.pos_x[i] = fx_add(Fx{g.pos_x[i]}, vx, g.fatal).raw;
        g.pos_y[i] = fx_add(Fx{g.pos_y[i]}, vy, g.fatal).raw;
        g.vel_x[i] = vx.raw;
        g.vel_y[i] = vy.raw;
    }
}

// SPEC-004 §28: bono de tecnologia sobre una estadistica, para el jugador dado
// y la clase de unidad dada. Recorre las tecnologias YA INVESTIGADAS por ese
// jugador y suma los efectos que casan.
//
// Se calcula aqui, sobre el valor EFECTIVO, y NUNCA se muta la definicion del
// catalogo: es inmutable y COMPARTIDA entre jugadores, asi que mutarla le
// aplicaria a un bando las mejoras del otro.
//
// Coste: O(tecnologias del catalogo) por golpe. Con 4 techs es ruido; cuando
// haya cientos habra que precalcularlo una vez por tick por jugador, que es el
// patron que ya usan la zona aliada y la lista de objetivos economicos.
inline int32_t player_tech_bonus(const GameState& g, uint8_t player,
                                 StatEffectV1 which, uint8_t unit_class) noexcept {
    if (g.catalog == nullptr || g.catalog->techs == nullptr) return 0;
    if (player >= MAX_EMITTERS) return 0;
    int32_t total = 0;
    for (uint32_t t = 0; t < g.catalog->tech_count; ++t) {
        const uint32_t w = t / 64u, b = t % 64u;
        if (w >= TECH_WORDS) break;
        if (((g.player_techs[player][w] >> b) & 1u) == 0u) continue;
        const TechDefinitionV1& td = g.catalog->techs[t];
        total += tech_stat_bonus(td.stat_effects, td.stat_effect_count, which, unit_class);
    }
    return total;
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

// RPS contra aldeano defensor (Sprint 1.4-cierre, SPEC-004 §7.1 — enmienda del
// Director): la tabla `rps_mult_bp` es 3×3 (clases 0..2); un aldeano
// (unit_class==3) como `tgt_class` haría OOB. Rama análoga a
// `rps_mult_vs_building_bp`: cualquier atacante inflige ×1.0 (10000 bp)
// neutro contra un aldeano. Balance v1 ajustable — documentado, no es
// decisión final de diseño.
inline int32_t rps_mult_vs_citizen_bp(uint8_t atk_class) noexcept {
    (void)atk_class;
    return 10000;
}

// SPEC-004 §24.5: los proyectiles VIAJAN. Movimiento entero por tick, misma
// aritmetica que movement_v1: sin balistica, sin gravedad, sin float.
//
// Compactacion DETERMINISTA: al retirar uno se desplazan los siguientes, en
// orden ascendente. Un "swap con el ultimo" seria mas rapido y cambiaria el
// orden segun que se retire, que es exactamente como se rompe el determinismo.
inline void projectile_system(GameState& g) noexcept {
    uint32_t write = 0;
    for (uint32_t k = 0; k < g.n_projectiles; ++k) {
        Projectile p = g.projectiles[k];

        // Objetivo muerto en vuelo: el proyectil SE DESCARTA. No redirige ni
        // cae al suelo; la flecha no busca otra victima.
        if (!et_is_alive(g.entities, p.target) || g.hp[p.target.index] <= 0) {
            continue;
        }

        // SIN PERSECUCION (correccion del Director, 2026-07-31): la flecha vuela
        // hacia el punto PREDICHO al disparar y no corrige. Si el objetivo se
        // aparta, falla — que es lo que hace que moverse sirva de algo contra
        // los tiradores, y como funciona AoE2.
        //
        // Solo los proyectiles GUIADOS corrigen rumbo. Hoy ninguno lo es; el
        // campo existe para que un misil o un dron de las edades tardias solo
        // tengan que ponerlo a 1 desde datos.
        if (p.guided != 0u) {
            const int64_t ddx = g.pos_x[p.target.index] - p.x_raw;
            const int64_t ddy = g.pos_y[p.target.index] - p.y_raw;
            const int64_t addx = ddx < 0 ? -ddx : ddx;
            const int64_t addy = ddy < 0 ? -ddy : ddy;
            const int64_t dom = addx > addy ? addx : addy;
            if (dom > 0) {
                const int64_t step_raw = dom < PROJECTILE_SPEED_RAW ? dom
                                                                    : PROJECTILE_SPEED_RAW;
                p.vel_x = ddx * step_raw / dom;
                p.vel_y = ddy * step_raw / dom;
            }
        }
        p.x_raw += p.vel_x;
        p.y_raw += p.vel_y;

        // RESOLUCION EN EL PUNTO DE IMPACTO, no contra el objetivo en vuelo.
        //
        // La version anterior comprobaba en cada tick si el proyectil estaba
        // encima del objetivo, y con 2 tiles por tick y medio tile de radio la
        // flecha SALTABA POR ENCIMA sin tocarlo nunca: nadie moria. El paso
        // discreto no admite una comprobacion continua barata.
        //
        // Asi que la flecha CAE DONDE SE APUNTO y ahi se resuelve: si el
        // objetivo sigue en el radio, impacta; si se aparto, falla. Es la
        // semantica de AoE2 y es la que hace que moverse valga de algo.
        const int64_t to_aim_x = p.aim_x - p.x_raw;
        const int64_t to_aim_y = p.aim_y - p.y_raw;
        const uint64_t to_aim2 =
                static_cast<uint64_t>(to_aim_x < 0 ? -to_aim_x : to_aim_x) *
                static_cast<uint64_t>(to_aim_x < 0 ? -to_aim_x : to_aim_x) +
                static_cast<uint64_t>(to_aim_y < 0 ? -to_aim_y : to_aim_y) *
                static_cast<uint64_t>(to_aim_y < 0 ? -to_aim_y : to_aim_y);
        const uint64_t one_step = static_cast<uint64_t>(PROJECTILE_SPEED_RAW) *
                                  static_cast<uint64_t>(PROJECTILE_SPEED_RAW);

        const bool guided = p.guided != 0u;
        bool resolve = false;
        if (guided) {
            // Guiado (misil, dron): se resuelve al alcanzar al objetivo.
            FatalReason gf = FatalReason::NONE;
            const uint64_t d2g = dist_sq_raw(Vec2Fx{Fx{p.x_raw}, Fx{p.y_raw}},
                                             Vec2Fx{Fx{g.pos_x[p.target.index]},
                                                    Fx{g.pos_y[p.target.index]}}, gf);
            resolve = d2g <= static_cast<uint64_t>(PROJECTILE_HIT_RADIUS_RAW) *
                              static_cast<uint64_t>(PROJECTILE_HIT_RADIUS_RAW);
        } else {
            // ESTRICTO, no <=: con la distancia justa de dos pasos, un <=
            // resolvia UN TICK ANTES de llegar y el dano se adelantaba. Con <
            // la flecha da el ultimo paso y cae exactamente donde se apunto.
            resolve = to_aim2 < one_step;
        }

        if (resolve) {
            FatalReason lf = FatalReason::NONE;
            const uint64_t d2 = dist_sq_raw(Vec2Fx{Fx{p.aim_x}, Fx{p.aim_y}},
                                            Vec2Fx{Fx{g.pos_x[p.target.index]},
                                                   Fx{g.pos_y[p.target.index]}}, lf);
            const uint64_t hit_sq = static_cast<uint64_t>(PROJECTILE_HIT_RADIUS_RAW) *
                                    static_cast<uint64_t>(PROJECTILE_HIT_RADIUS_RAW);
            if (guided || d2 <= hit_sq) {
                g.hp[p.target.index] -= p.damage;
                if (g.hp[p.target.index] <= 0 && g.entities.alive[p.target.index]) {
                    et_mark_dead(g.entities, p.target.index);
                    if (g.destroy_count < PENDING_CAP) {
                        g.destroy_batch[g.destroy_count++] = p.target.index;
                    }
                }
            }
            continue;   // impacte o falle, la flecha desaparece
        }
        g.projectiles[write++] = p;
    }
    g.n_projectiles = write;
}

// Sistema de combate (Sprint 0.3/1.6A). Cada tick, período 1: cada unidad viva
// en orden ascendente busca al enemigo más cercano en rango, le inflige daño
// RPS y entra en cooldown. La consulta recorre todas las celdas que intersectan
// el AABB del círculo de range_mt; el filtro final por dist_sq conserva el
// círculo exacto. No hay un límite fijo de 3x3: armas de alcance mayor que una
// celda del spatial hash siguen encontrando blancos válidos.
// Determinismo: el daño se aplica inmediatamente en orden ascendente de i.
inline void combat_system(GameState& g) noexcept {
    const EntityTable& t = g.entities;
    for (uint32_t i = 0; i < t.capacity; ++i) {
        if (!t.alive[i]) continue;
        if (g.hp[i] <= 0) continue;
        if (g.unit_class[i] > 2) continue;  // ciudadanos (Sprint 0.3): no atacan
        if (g.fleeing[i]) { if (g.atk_cd[i] > 0) --g.atk_cd[i]; continue; }
        // ATK_COOLDOWN_TICKS es el número exacto de ticks ENTRE impactos:
        // ataque en t=0 => cd=10; en t=1..9 queda 9..1; en t=10 pasa a 0 y
        // puede atacar de nuevo en este mismo tick.
        if (g.atk_cd[i] > 0) --g.atk_cd[i];
        if (g.atk_cd[i] > 0) continue;

        int64_t range_raw = static_cast<int64_t>(g.range_mt[i]) * 65536 / 1000;
        if (range_raw < MELEE_CONTACT_RAW) range_raw = MELEE_CONTACT_RAW;
        const uint64_t range_sq = static_cast<uint64_t>(range_raw) * static_cast<uint64_t>(range_raw);

        // Las posiciones/rangos válidos de v1 caben holgadamente en int64_t.
        // sh_cell_index aplica el clamp normativo a los límites reales del mapa;
        // con ello también cubrimos unidades pegadas a cualquiera de los bordes.
        const int64_t min_x = range_raw > g.pos_x[i] ? 0 : g.pos_x[i] - range_raw;
        const int64_t min_y = range_raw > g.pos_y[i] ? 0 : g.pos_y[i] - range_raw;
        const int64_t max_x = g.pos_x[i] + range_raw;
        const int64_t max_y = g.pos_y[i] + range_raw;
        const uint32_t min_x_cell = sh_cell_index(g.shash, min_x, g.pos_y[i]);
        const uint32_t max_x_cell = sh_cell_index(g.shash, max_x, g.pos_y[i]);
        const uint32_t min_y_cell = sh_cell_index(g.shash, g.pos_x[i], min_y);
        const uint32_t max_y_cell = sh_cell_index(g.shash, g.pos_x[i], max_y);
        const uint32_t min_cx = min_x_cell % g.shash.cells_x;
        const uint32_t max_cx = max_x_cell % g.shash.cells_x;
        const uint32_t min_cy = min_y_cell / g.shash.cells_x;
        const uint32_t max_cy = max_y_cell / g.shash.cells_x;

        uint32_t best = SH_EMPTY;
        uint64_t best_d2 = 0;
        const Vec2Fx pos_i{Fx{g.pos_x[i]}, Fx{g.pos_y[i]}};

        for (uint32_t ncy = min_cy; ncy <= max_cy; ++ncy) {
            for (uint32_t ncx = min_cx; ncx <= max_cx; ++ncx) {
                const uint32_t cell = ncy * g.shash.cells_x + ncx;

                for (uint32_t j = sh_first(g.shash, cell); j != SH_EMPTY; j = sh_next(g.shash, j)) {
                    if (j == i) continue;
                    if (!t.alive[j]) continue;
                    if (g.hp[j] <= 0) continue;
                    if (g.owner[j] == g.owner[i]) continue;
                    // Objetivo válido: unidad de combate (unit_class 0..2),
                    // aldeano (unit_class==3 — SPEC-004 §7.1, enmienda del
                    // Director Sprint 1.4-cierre: ahora vulnerable en combate)
                    // O edificio (entity_kind==1, aunque su unit_class==255
                    // lo deje fuera del rango 0..3). Excluido: unit_class>3
                    // que no sea edificio (no hay entidades vivas así hoy).
                    if (g.unit_class[j] > 2 && g.unit_class[j] != 3u
                        && g.entity_kind[j] != 1u) continue;

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
            // SPEC-004 Parte VI. La piedra-papel-tijera deja de ser un
            // multiplicador opaco por clase y pasa a ser DATOS LEGIBLES:
            // armadura del que recibe y bono del que ataca. El bono sale de
            // bonus_vs_bp, que hasta hoy se cargaba del blob y NO se usaba en
            // ninguna parte de la simulacion — dato muerto que ahora vive.
            int32_t armor_of_type = 0;
            int32_t bonus_bp = 0;
            const UnitDefinitionV1* adef = nullptr;
            if (g.catalog != nullptr && g.unit_id[i] < g.catalog->unit_count) {
                adef = &g.catalog->units[g.unit_id[i]];
            }
            if (adef != nullptr) {
                const uint32_t dt = static_cast<uint32_t>(adef->attack_type);
                if (g.entity_kind[best] == 1u) {
                    if (g.catalog->buildings != nullptr &&
                        g.building_id[best] < g.catalog->building_count &&
                        dt < DAMAGE_TYPE_COUNT) {
                        armor_of_type = g.catalog->buildings[g.building_id[best]].armor[dt];
                    }
                } else {
                    if (g.unit_id[best] < g.catalog->unit_count && dt < DAMAGE_TYPE_COUNT) {
                        armor_of_type = g.catalog->units[g.unit_id[best]].armor[dt];
                    }
                    // El bono va contra la CLASE del objetivo; los edificios no
                    // son una clase de unidad, asi que no lo reciben.
                    const uint32_t tc = static_cast<uint32_t>(g.unit_class[best]);
                    if (tc < 6u) bonus_bp = adef->bonus_vs_bp[tc];
                }
            }
            // Efectos de tecnologia sobre el valor EFECTIVO (SPEC-004 §28):
            // el ataque lo mejora quien golpea, la armadura quien recibe.
            int32_t eff_attack = g.attack[i];
            if (adef != nullptr) {
                eff_attack += player_tech_bonus(g, g.owner[i], StatEffectV1::Attack,
                                                g.unit_class[i]);
                if (eff_attack < 0) eff_attack = 0;
                if (g.entity_kind[best] == 0u) {
                    static constexpr StatEffectV1 kArmorOf[DAMAGE_TYPE_COUNT] = {
                        StatEffectV1::ArmorCut, StatEffectV1::ArmorPierce,
                        StatEffectV1::ArmorImpact,
                    };
                    const uint32_t dt2 = static_cast<uint32_t>(adef->attack_type);
                    if (dt2 < DAMAGE_TYPE_COUNT) {
                        armor_of_type += player_tech_bonus(g, g.owner[best], kArmorOf[dt2],
                                                           g.unit_class[best]);
                        if (armor_of_type < 0) armor_of_type = 0;
                    }
                }
            }
            const int32_t dmg = compute_damage(eff_attack, armor_of_type, bonus_bp);

            // SPEC-004 §24.5: con alcance, el dano NO es instantaneo — sale un
            // proyectil y llega cuando llega. Cuerpo a cuerpo sigue siendo
            // inmediato, que es lo que distingue a las dos formas de pelear.
            if (g.range_mt[i] > 0) {
                if (g.n_projectiles < PROJECTILE_HARD_CAP) {
                    Projectile p{};
                    p.x_raw = g.pos_x[i];
                    p.y_raw = g.pos_y[i];
                    p.target = EntityHandle{best, g.entities.generation[best]};
                    p.damage = dmg;
                    p.owner = g.owner[i];
                    // PREDICCION DE TIRO: se apunta a donde ESTARA el objetivo,
                    // no a donde esta. Tiempo de vuelo estimado = distancia
                    // dominante / velocidad, y se adelanta la posicion con la
                    // velocidad actual del objetivo. Todo entero.
                    const int64_t raw_dx = g.pos_x[best] - p.x_raw;
                    const int64_t raw_dy = g.pos_y[best] - p.y_raw;
                    const int64_t araw_dx = raw_dx < 0 ? -raw_dx : raw_dx;
                    const int64_t araw_dy = raw_dy < 0 ? -raw_dy : raw_dy;
                    const int64_t rough = araw_dx > araw_dy ? araw_dx : araw_dy;
                    const int64_t flight = rough / PROJECTILE_SPEED_RAW;
                    p.aim_x = g.pos_x[best] + g.vel_x[best] * flight;
                    p.aim_y = g.pos_y[best] + g.vel_y[best] * flight;
                    p.guided = 0u;
                    const int64_t dx = p.aim_x - p.x_raw;
                    const int64_t dy = p.aim_y - p.y_raw;
                    const int64_t adx = dx < 0 ? -dx : dx;
                    const int64_t ady = dy < 0 ? -dy : dy;
                    const int64_t dom = adx > ady ? adx : ady;
                    const int64_t speed = PROJECTILE_SPEED_RAW;
                    if (dom > 0) {
                        p.vel_x = dx * speed / dom;
                        p.vel_y = dy * speed / dom;
                    }
                    g.projectiles[g.n_projectiles++] = p;
                }
                // Si el array esta lleno, el disparo NO se crea. Cota dura,
                // determinista y documentada: nunca desbordamiento.
            } else {
                g.hp[best] -= dmg;
            }
            // La muerte por golpe cuerpo a cuerpo se resuelve aqui; la de un
            // proyectil la resuelve projectile_system al impactar.
            if (g.range_mt[i] == 0 && g.hp[best] <= 0 && t.alive[best]) {
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
// está más allá de su rango de arma, fija tgt en un punto de standoff frente al
// enemigo (1 mili-tile dentro del alcance para absorber truncamiento Q47.16).
// Así movement_v1 hace la aproximación sin reconsultar el hash cada tick y la
// unidad queda ociosa al alcanzar su distancia de fuego. Sin estado nuevo:
// reutiliza tgt_x/tgt_y →
// checksum, serialización y versión de guardado intactos. Se ejecuta tras
// combat_system (hash fresco; los muertos del tick ya están marcados y no se
// adquieren). Determinismo: orden ascendente de i, lecturas post-movimiento.
// SPEC-004 §24.4: las ordenes del jugador MANDAN sobre el aggro automatico.
// Sin esta precedencia, ATTACK seria una sugerencia que el aggro pisa y el
// jugador lo notaria como que el juego no le obedece.
//
// Corre ANTES de aggro_system y persigue al objetivo fijado; si el objetivo
// muere, la unidad vuelve a NINGUNA y el aggro recupera el mando.
inline void order_system(GameState& g) noexcept {
    const EntityTable& t = g.entities;
    for (uint32_t i = 0; i < t.capacity; ++i) {
        if (!t.alive[i]) continue;
        if (g.order_mode[i] != ORDER_MODE_ATTACK) continue;
        const EntityHandle tgt = g.attack_target[i];
        if (!et_is_alive(g.entities, tgt) || g.hp[tgt.index] <= 0) {
            // Objetivo muerto: la orden se cumplio y el aggro vuelve a mandar.
            g.attack_target[i] = NULL_HANDLE;
            g.order_mode[i] = ORDER_MODE_NONE;
            continue;
        }
        // Perseguir: el destino de movimiento es la posicion del objetivo.
        g.tgt_x[i] = g.pos_x[tgt.index];
        g.tgt_y[i] = g.pos_y[tgt.index];
    }
}

inline void aggro_system(GameState& g) noexcept {
    const EntityTable& t = g.entities;
    for (uint32_t i = 0; i < t.capacity; ++i) {
        if (!t.alive[i]) continue;
        if (g.hp[i] <= 0) continue;
        if (g.unit_class[i] > 2) continue;   // ciudadanos: no persiguen
        if (g.attack[i] <= 0) continue;      // excluye SPAWN_DEBUG (golden intacto)
        if (g.fleeing[i]) continue;          // huir tiene prioridad
        // SPEC-004 §24.4: con ATTACK activo el jugador mando, y el aggro NO
        // reasigna objetivo aunque pase otro enemigo mas cerca.
        if (g.order_mode[i] == ORDER_MODE_ATTACK) continue;
        // Sprint 1.50: el ATAQUE-MOVIMIENTO engancha AUNQUE se este moviendo.
        // Un attack-move que no ataca mientras se mueve es un MOVE, y entonces
        // la orden no significa nada. MOVE conserva la exigencia de ociosidad
        // —quien va a un sitio con MOVE no se para a pelear, y esa es justo la
        // diferencia entre las dos ordenes— y ATTACK sigue excluido arriba.
        if (g.order_mode[i] != ORDER_MODE_ATTACK_MOVE
            && (g.pos_x[i] != g.tgt_x[i] || g.pos_y[i] != g.tgt_y[i])) continue;

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
                    // Mismo criterio que combat_system (SPEC-004 §7.1,
                    // enmienda Sprint 1.4-cierre): edificios y aldeanos
                    // (unit_class==3) sí son objetivo válido de aggro.
                    if (g.unit_class[j] > 2 && g.unit_class[j] != 3u
                        && g.entity_kind[j] != 1u) continue;

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

        // Enemigo detectado fuera del rango de arma → aproximarse directamente
        // a un punto de standoff. Dentro de rango: quieta (combat_system ya le
        // dispara donde está).
        // Sprint 1.50: este comentario decia "este bloque solo corre ociosa" y
        // el cambio de arriba lo dejo FALSO, asi que se corrige aqui: en
        // ATTACK_MOVE corre tambien en marcha. Una orden MOVE_TO humana en
        // curso sigue teniendo prioridad, que era lo que el comentario queria
        // proteger — pero ahora por el modo de orden, no por la ociosidad.
        if (best != SH_EMPTY && best_d2 > range_sq) {
            const int64_t step_fx = (static_cast<int64_t>(g.speed_mtpt[i]) * FX_ONE_RAW) / 1000;
            if (step_fx <= 0) continue;
            const Vec2Fx toward = normalize_v1(
                Vec2Fx{Fx{g.pos_x[best] - g.pos_x[i]}, Fx{g.pos_y[best] - g.pos_y[i]}}, g.fatal);
            const int64_t standoff_mt = g.range_mt[i] > 0 ? g.range_mt[i] - 1 : 0;
            const int64_t standoff_raw = (standoff_mt * FX_ONE_RAW) / 1000;
            int64_t next_x = fx_sub(
                Fx{g.pos_x[best]}, fx_mul(toward.x, Fx{standoff_raw}, g.fatal), g.fatal).raw;
            int64_t next_y = fx_sub(
                Fx{g.pos_y[best]}, fx_mul(toward.y, Fx{standoff_raw}, g.fatal), g.fatal).raw;
            if (next_x < 0) next_x = 0;
            if (next_y < 0) next_y = 0;
            if (next_x >= WORLD_RAW_MAX) next_x = WORLD_RAW_MAX - 1;
            if (next_y >= WORLD_RAW_MAX) next_y = WORLD_RAW_MAX - 1;
            g.tgt_x[i] = next_x;
            g.tgt_y[i] = next_y;
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

        // Todo estado debe tener salida, o el sistema admite estados
        // absorbentes que congelan la partida. La rama original era
        // `else if (enemies == 0)`: con enemigos cerca y números parejos no se
        // ejecutaba NINGUNA rama y la moral quedaba congelada en valores como
        // 26 — por encima del pánico (20), muy por debajo del rally (50),
        // huyendo para siempre (diagnóstico del Arquitecto 2026-08-04).
        // Aguantar la línea con números parejos es precisamente lo que
        // sostiene la moral de una tropa: solo se pierde moral en desventaja
        // clara.
        if (enemies > allies + 1) {
            g.morale[i] -= MORALE_DROP;
        } else {
            g.morale[i] += MORALE_REGEN;
        }
        if (g.morale[i] < 0) g.morale[i] = 0;
        if (g.morale[i] > MORALE_MAX) g.morale[i] = MORALE_MAX;

        if (g.morale[i] <= MORALE_PANIC) g.fleeing[i] = 1;
        if (g.morale[i] >= MORALE_RALLY) g.fleeing[i] = 0;
    }
}

// Criterio compartido de edificio aliado completo (SPEC-004 §6/§23). Tanto
// el dropoff como la zona de auto-recolección deben decidirlo en un único
// sitio: entidad viva, edificio, dueño correcto, definición válida y progreso
// suficiente.
inline bool is_complete_owned_building(const GameState& g, uint32_t entity,
                                       uint8_t owner) noexcept {
    if (g.catalog == nullptr || entity >= g.entities.capacity) return false;
    if (!g.entities.alive[entity]) return false;
    if (g.entity_kind[entity] != 1u || g.owner[entity] != owner) return false;
    const BuildingId bid = g.building_id[entity];
    if (bid >= g.catalog->building_count) return false;
    return g.build_progress[entity] >= g.catalog->buildings[bid].build_time_ticks;
}

// Máscaras de depósitos en zona aliada para la auto-asignación económica
// (Sprint 1.7, SPEC-004 §23). Bit d de out[owner] está activo si deposits[d]
// queda a <=32 tiles del centro de algún edificio aliado completo. Se recorre
// la tabla de entidades una sola vez y, por cada edificio completo, los <=32
// depósitos: evita multiplicar el barrido por MAX_EMITTERS. Todo es ascendente,
// entero y fijo: cero heap/STL dentro de Step().
inline void allied_auto_gather_deposit_masks(
        const GameState& g, uint64_t out[MAX_EMITTERS]) noexcept {
    for (uint32_t owner = 0; owner < MAX_EMITTERS; ++owner) out[owner] = 0u;
    const uint64_t radius_sq =
        static_cast<uint64_t>(ECO_AUTO_GATHER_RADIUS_RAW)
        * static_cast<uint64_t>(ECO_AUTO_GATHER_RADIUS_RAW);

    for (uint32_t j = 0; j < g.entities.capacity; ++j) {
        const uint8_t owner = g.owner[j];
        if (owner >= MAX_EMITTERS
            || !is_complete_owned_building(g, j, owner)) continue;
        const Vec2Fx building{Fx{g.pos_x[j]}, Fx{g.pos_y[j]}};
        for (uint32_t d = 0; d < g.n_deposits && d < ECO_MAX_DEPOSITS; ++d) {
            FatalReason local_fatal = FatalReason::NONE;
            const Vec2Fx deposit{Fx{g.deposits[d].x_raw}, Fx{g.deposits[d].y_raw}};
            // Sprint 1.45-bis: el ALCANCE se mide al BORDE de la zona, igual
            // que la llegada y que la eleccion de candidato. Sin esto el
            // sistema quedaria incoherente consigo mismo: un bosque cuyo borde
            // esta a tiro pero cuyo centro cae a 38 tiles seria inalcanzable
            // pese a que el aldeano puede talarlo perfectamente. Un bosque
            // grande ES mas alcanzable que uno pequeno; esa es justo la
            // propiedad que lo hace un bosque y no una mina.
            //
            // Fallo de MI diseno, no del delegado: cambie llegada y seleccion
            // al borde y me deje la alcanzabilidad midiendo al centro. Es
            // exactamente como `eco_available_for` acabo siendo codigo muerto
            // —probado y llamado por nadie—, asi que va con prueba.
            const int64_t zona = eco_zone_radius(g.deposits[d]);
            const uint64_t alcance = static_cast<uint64_t>(ECO_AUTO_GATHER_RADIUS_RAW)
                                   + static_cast<uint64_t>(zona);
            const uint64_t alcance_sq = (zona > 0) ? (alcance * alcance) : radius_sq;
            if (dist_sq_raw(deposit, building, local_fatal) > alcance_sq) continue;
            // Sprint 1.28 (SPEC-007 §4): la RESERVA la abre la tecnologia, y
            // por jugador. Un yacimiento agotado deja de ser elegible para
            // quien no sabe extraer lo que queda, y sigue siendolo para quien
            // si. Es aqui —en la mascara POR JUGADOR— donde esa ventaja se
            // puede expresar; en el modulo de economia, que no conoce
            // GameState, no habria a quien preguntarle.
            //
            // El corpus lo sostiene: la flotacion de 1916 hizo rentable lo que
            // antes se abandonaba en la mina. La misma roca, otra tecnica.
            if (eco_available_for(g.deposits[d], g.player_caps[owner][0]) <= 0) continue;
            out[owner] |= (uint64_t{1} << d);
        }
    }
}

// Conveniencia de consulta aislada para pruebas del predicado dinámico.
inline uint64_t allied_auto_gather_deposit_mask(const GameState& g,
                                                uint8_t owner) noexcept {
    if (owner >= MAX_EMITTERS) return 0u;
    uint64_t eligible[MAX_EMITTERS] = {};
    allied_auto_gather_deposit_masks(g, eligible);
    return eligible[owner];
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
    if (g.catalog == nullptr || resource_idx >= RESOURCE_COUNT) return false;
    const EntityTable& t = g.entities;
    uint32_t best = t.capacity;
    uint64_t best_d2 = 0;
    const Vec2Fx here{Fx{citizen_x}, Fx{citizen_y}};

    for (uint32_t j = 0; j < t.capacity; ++j) {
        if (!is_complete_owned_building(g, j, owner)) continue;
        const BuildingDefinitionV1& def = g.catalog->buildings[g.building_id[j]];
        if ((def.dropoff_mask & (uint64_t{1} << resource_idx)) == 0u) continue;

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
    // El módulo puro economy.hpp no conoce edificios ni GameState. El wiring
    // precalcula una máscara fija por jugador una sola vez por fase y se la
    // entrega a cada ciudadano. La máscara solo interviene si SEEK necesita
    // auto-reasignar: un GATHER explícito ya válido puede seguir fuera de zona
    // hasta que su depósito se agote (§23.3).
    uint64_t auto_gather_eligible[MAX_EMITTERS] = {};
    allied_auto_gather_deposit_masks(g, auto_gather_eligible);

    for (uint32_t i = 0; i < t.capacity; ++i) {
        if (!t.alive[i]) continue;
        if (g.unit_class[i] != 3) continue;  // solo ciudadanos
        // §22.1: la tarea, no build_target, es la única autoridad. Un
        // build_target obsoleto es solo dato y no puede secuestrar GATHER.
        if (g.citizen_task[i] != CITIZEN_TASK_GATHER) continue;

        EcoCitizenIn in{};
        in.pos_x = g.pos_x[i];
        in.pos_y = g.pos_y[i];
        in.state = g.eco_state[i];
        in.assigned_deposit = g.eco_assigned_deposit[i];
        in.carry = g.eco_carry[i];
        in.carry_resource_idx = g.eco_carry_resource[i];
        in.speed_mtpt = g.speed_mtpt[i];
        // Sprint 1.32: cuantos trabajan ESTE deposito. Barrido ascendente por
        // indice, como todo lo demas. Se cuentan TODOS los duenos, no solo los
        // propios: un filon no distingue de quien es la pala, y disputar un
        // yacimiento saturado debe ser malo para los dos.
        {
            int32_t cuantos = 0;
            const uint32_t d = g.eco_assigned_deposit[i];
            if (d != ECO_NO_DEPOSIT) {
                for (uint32_t k = 0; k < g.entities.capacity; ++k) {
                    if (!g.entities.alive[k]) continue;
                    if (g.eco_assigned_deposit[k] == d) ++cuantos;
                }
            }
            in.gatherers_here = cuantos;
        }
        // Sprint 1.29: la tecnologia y las herramientas del JUGADOR entran
        // aqui. `unit_class` 3 es Citizen, que es quien recolecta; una tech de
        // herramientas de minero no debe mejorar a la caballeria.
        //
        // Se suman a la constante base en vez de sustituirla: una tech que
        // diera un valor absoluto haria que investigar dos veces empeorara.
        {
            const uint8_t o = g.owner[i];
            if (o < MAX_EMITTERS) {
            const int32_t d_rate = player_tech_bonus(g, o, StatEffectV1::HarvestRate, 3u);
            const int32_t d_cap  = player_tech_bonus(g, o, StatEffectV1::CarryCap, 3u);
            in.harvest_per_tick = d_rate > 0 ? ECO_HARVEST_PER_TICK + d_rate : 0;
            in.carry_cap        = d_cap  > 0 ? ECO_CARRY_CAP + d_cap : 0;
            in.recovery_bp      = player_tech_bonus(g, o, StatEffectV1::Recovery, 3u);
            }
        }

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
                in, g.deposits, g.n_deposits,
                auto_gather_eligible[owner_i], drop_x, drop_y, g.fatal);

        g.pos_x[i] = out.pos_x; g.pos_y[i] = out.pos_y;
        g.vel_x[i] = out.vel_x; g.vel_y[i] = out.vel_y;
        g.eco_state[i] = out.state;
        g.eco_assigned_deposit[i] = out.assigned_deposit;
        g.eco_carry[i] = out.carry;
        g.eco_carry_resource[i] = out.carry_resource_idx;

        // §22.2: no queda depósito alcanzable. eco_step_citizen ya deja el
        // ciudadano quieto y sin asignación; la tarea explícita pasa a IDLE
        // para que no vuelva a girar indefinidamente en SEEK.
        if (out.state == EcoState::SEEK
            && out.assigned_deposit == ECO_NO_DEPOSIT) {
            g.citizen_task[i] = CITIZEN_TASK_IDLE;
            g.vel_x[i] = 0;
            g.vel_y[i] = 0;
        }

        if (out.did_harvest && out.assigned_deposit < g.n_deposits) {
            // Sprint 1.29: el yacimiento pierde lo EXTRAIDO, no lo ganado. Con
            // recuperacion no son lo mismo, y ahi esta la ventaja de la
            // tecnologia: de la misma roca sale mas material util.
            g.deposits[out.assigned_deposit].remaining -= out.deposit_decrement;
        }
        if (out.did_dropoff && out.dropoff_resource_idx < RESOURCE_COUNT) {
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
// Sprint 1.28 (SPEC-007 §15) — granjas y bosques plantados.
//
// Un sistema propio, y no un enganche dentro de `construction_system`, por dos
// razones: es IDEMPOTENTE —comprueba si el deposito ya existe antes de
// crearlo— y asi cubre tambien los edificios que nacen COMPLETOS, como los
// centros de escenario, que nunca pasan por el constructor.
//
// Orden dentro del tick: va con el resto de sistemas, en barrido ASCENDENTE
// por indice y con desempate por indice bajo, como todo lo demas.
inline void farm_system(GameState& g) noexcept {
    if (g.catalog == nullptr) return;
    const EntityTable& t = g.entities;

    // (a) Alta: todo edificio COMPLETO que declare deposito y aun no lo tenga.
    for (uint32_t i = 0; i < t.capacity; ++i) {
        if (!t.alive[i]) continue;
        if (g.entity_kind[i] != 1u) continue;
        if (g.building_id[i] >= g.catalog->building_count) continue;
        const BuildingDefinitionV1& bd = g.catalog->buildings[g.building_id[i]];
        if (bd.creates_amount <= 0) continue;
        // Una obra a medias no alimenta a nadie.
        if (static_cast<uint32_t>(g.build_progress[i]) < bd.build_time_ticks) continue;

        bool ya = false;
        for (uint32_t d = 0; d < g.n_deposits && !ya; ++d) {
            if (g.deposits[d].owner_building == i) ya = true;
        }
        if (ya) continue;
        if (g.n_deposits >= ECO_MAX_DEPOSITS) continue;  // cota dura, sin desbordar

        EcoDeposit& nd = g.deposits[g.n_deposits];
        nd.x_raw = g.pos_x[i];
        nd.y_raw = g.pos_y[i];
        nd.resource_idx = bd.creates_resource_idx;
        nd.remaining = bd.creates_amount;
        nd.regen_milli_per_tick = bd.creates_regen_per_tick;
        nd.regen_accum = 0;
        nd.cap = bd.creates_cap;
        nd.owner_building = i;
        nd.reserve = 0;
        nd.reserve_capability = ECO_NO_CAPABILITY;
        ++g.n_deposits;
    }

    // (b) Baja: si el edificio dueno ya no esta, su deposito deja de dar.
    //     NO se borra la entrada —quitarla desplazaria indices y romperia las
    //     asignaciones de los aldeanos— sino que se agota y se marca sin
    //     regeneracion. Un campo sin quien lo trabaje deja de producir.
    for (uint32_t d = 0; d < g.n_deposits; ++d) {
        EcoDeposit& dep = g.deposits[d];
        if (dep.owner_building == ECO_NO_OWNER) continue;
        if (dep.owner_building < t.capacity && t.alive[dep.owner_building]
            && g.entity_kind[dep.owner_building] == 1u) {
            continue;
        }
        dep.remaining = 0;
        dep.regen_milli_per_tick = 0;
        // Sprint 1.44 — hallazgo F1 de la auditoria externa, y era un fallo
        // mio de verdad. Apagar el deposito no bastaba: hay que SOLTAR el
        // dueno. La free-list de entidades es LIFO, asi que el indice de la
        // granja destruida se reutiliza enseguida; si el deposito seguia
        // apuntando a ese indice, el edificio NUEVO que lo ocupara heredaba un
        // deposito muerto —y, peor, el alta lo daba por "ya registrado" y no le
        // creaba el suyo.
        //
        // Al soltarlo queda como un yacimiento agotado del mapa: inerte, sin
        // regeneracion y sin dueno. Es lo correcto — el campo ya no es de
        // nadie.
        dep.owner_building = ECO_NO_OWNER;
    }

    // (c) Regeneracion, un tick. Los yacimientos del mapa tienen regen 0 y no
    //     se enteran de que este sistema existe.
    for (uint32_t d = 0; d < g.n_deposits; ++d) eco_regen_deposit(g.deposits[d]);
}

inline void construction_system(GameState& g) noexcept {
    const EntityTable& t = g.entities;
    for (uint32_t i = 0; i < t.capacity; ++i) {
        if (!t.alive[i]) continue;
        if (g.unit_class[i] != 3u) continue;
        // §22.1: BUILD es el selector exclusivo; build_target solo describe
        // el objetivo de esa tarea y puede ser inválido/obsoleto.
        if (g.citizen_task[i] != CITIZEN_TASK_BUILD) continue;

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
            g.citizen_task[i] = CITIZEN_TASK_IDLE;
            g.vel_x[i] = 0;
            g.vel_y[i] = 0;
            continue;
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
            if (g.build_progress[b] >= T) {
                g.build_target[i] = BUILD_NO_TARGET;
                g.citizen_task[i] = CITIZEN_TASK_IDLE;
            }
        }
    }
}

// Sistema de producción (Sprint 1.2, SPEC-004 §11.4). Fase propia, después de
// construction_system y antes del destroy batch, iteración ascendente por
// índice sobre edificios vivos COMPLETOS con cola no vacía.
// SPEC-007 §12.4: fabricacion. Misma fase que production_system y mismo
// recorrido ascendente por slot. Al completar se acredita la salida y el
// edificio vuelve a ocioso. Si el edificio muere a mitad, el slot se recicla
// (zero_components deja craft_recipe en INVALID) y NO se acredita nada ni se
// devuelven los inputs: fabricar es un riesgo, no un deposito a plazo.
inline void craft_system(GameState& g) noexcept {
    const EntityTable& t = g.entities;
    for (uint32_t i = 0; i < t.capacity; ++i) {
        if (!t.alive[i]) continue;
        if (g.entity_kind[i] != 1u) continue;
        if (g.craft_recipe[i] == INVALID_RECIPE_ID) continue;
        if (g.catalog == nullptr || g.craft_recipe[i] >= g.catalog->recipe_count) continue;
        const RecipeV1& rdef = g.catalog->recipes[g.craft_recipe[i]];
        ++g.craft_progress[i];
        if (g.craft_progress[i] < rdef.duration_ticks) continue;
        if (rdef.output_index < RESOURCE_COUNT) {
            g.player_stock[g.owner[i]][rdef.output_index] += rdef.output_amount;
        }
        g.craft_recipe[i] = INVALID_RECIPE_ID;
        g.craft_progress[i] = 0;
    }
}

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
        // (5a) Control del ciudadano (Sprint 1.7, SPEC-004 §22.3): fase
        // propia antes de economía y construcción. movement_v1 sigue
        // congelado y no toca ciudadanos.
        detail::citizen_move_system(g);
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
        detail::order_system(g);
        detail::aggro_system(g);
        // Los proyectiles se mueven DESPUES del combate del tick: el que
        // sale ahora vuela a partir del tick siguiente, no impacta el mismo.
        detail::projectile_system(g);

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
        // (5e-bis) Granjas y bosques plantados (Sprint 1.28, SPEC-007 §15):
        // tras el constructor, para que una granja terminada ESTE TICK ya
        // registre su deposito.
        detail::farm_system(g);

        detail::production_system(g);
        detail::craft_system(g);
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
