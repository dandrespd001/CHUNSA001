#pragma once

// chunsa_sim_core — AI v1 de 3 capas: cuerpo real de ai_execute (Sprint 1.4 K2,
// SPEC-005 §4/§5). generado: minimax-m3 (lifecycle 0.1B) · cuerpo: sonnet-5
// (brief docs/briefs/SONNET_K2_IA_EXECUTE_SPRINT_1.4.md) · revisado: Arquitecto.
//
// El lifecycle del job (ai_box_init/ai_should_dispatch/ai_dispatch/ai_due/
// ai_stalled/ai_commit/ai_serialize/ai_deserialize) NO se toca (SPEC-005 §2):
// es andamiaje de Sprint 0.1B, ya verde en G4/G5. Este sprint reemplaza
// EXCLUSIVAMENTE el cuerpo de ai_execute (antes: MOVE_TO aleatorio) por la IA
// de 3 capas real. Bump AI_ALGO_VERSION 1->2 (el procedimiento cambió por
// completo).
//
// Sprint 1.6B (SPEC-004 §18/§19, brief SONNET_K2_GATHER_APERTURA): la IA
// gana consciencia económica sobre GATHER (redirige ociosos/excedentes por
// stock bajo) y `ai_find_trainer_type` se endurece con civ_id (dos civs
// reales en el mismo catálogo exigían filtrar por la civ del jugador — ver
// su comentario). Bump AI_ALGO_VERSION 2->3.
//
// REGLA DE ORO (SPEC-005 §0, criterio de rechazo #1 — auditado por Opus):
// ai_execute(box, g) es función PURA de (g, box.source_tick,
// box.runtime_before). CERO g.tick / reloj real / float / double / heap /
// STL dinámico dentro de esta función y sus helpers `ai_*` de este header;
// CERO entropía (de hecho, v1 no usa RNG en absoluto: todos los desempates
// son por MENOR ÍNDICE, determinista sin necesidad de tiebreak aleatorio —
// más simple de auditar que introducir AI_TIEBREAK sin necesidad real).
// Único punto donde una regla de kernel que SÍ depende de g.tick (el gate
// (b) de EPOCH_UP, SPEC-004 §12.3) queda deliberadamente SIN pre-verificar
// aquí — ver el comentario en el candidato TECH más abajo: la IA emite
// optimista (SPEC-005 §5) y deja que el kernel decida, exactamente el patrón
// que el propio spec describe para evitar leer g.tick por la puerta trasera.

#include <cstdint>

#include "chunsa/game_state.hpp"
#include "chunsa/rng.hpp"
#include "chunsa/commands.hpp"
#include "chunsa/serialize.hpp"
#include "chunsa/step.hpp"   // EPOCH_MAX_V1/EPOCH_COST_*/BUILD_ARRIVE_RADIUS_RAW (constantes v1, no duplicar)

namespace chunsa {

// ---------------------------------------------------------------------------
// Constantes de contrato (SPEC-001 §7.2 / §7.3). Versión del algoritmo; si
// cambia el procedimiento de decisión, bump AI_ALGO_VERSION y mantener
// compat en ai_deserialize.
// ---------------------------------------------------------------------------
// Sprint 1.6B (SPEC-004 §19): 2->3. El procedimiento de decisión cambió: (a)
// ai_find_trainer_type gana el filtro por civ_id (bug real con 2 civs reales
// en el mismo catálogo — antes resolvía SIEMPRE el primer edificio del
// catálogo sin importar la civ del jugador); (b) nueva capa 1.5 (económica
// adaptativa) que puede emitir GATHER. Ambos cambian qué comandos calcula
// ai_execute para el MISMO GameState — es, por contrato (SPEC-005 §7),
// exactamente el caso que exige el bump.
inline constexpr uint32_t AI_ALGO_VERSION     = 5;  // Sprint 1.19: la IA fabrica y ordena atacar
inline constexpr uint16_t AI_INPUT_DELAY_TICKS = 4;
inline constexpr uint32_t AI_DECISION_PHASE   = 7;     // dispatch cuando tick % 20 == 7
inline constexpr uint32_t AI_MAX_COMMANDS     = 64;

// ---------------------------------------------------------------------------
// Constantes v1 de la IA (brief K2, no re-litigar — mismo espíritu que
// POP_CAP_V1/EPOCH_MIN_TICKS en step.hpp): objetivo económico fijo de
// ciudadanos, ventana de barrido para sitio de construcción (tiles) y radio
// de "enemigo cerca de la base" para la capa reactiva (mili-tiles, mismo
// formato que AGGRO_RANGE_MT en step.hpp pero un radio propio y mayor — la
// IA reacciona ANTES de que el aggro_system del kernel ya esté trabando
// combate cuerpo a cuerpo).
// ---------------------------------------------------------------------------
inline constexpr int32_t  AI_ECON_TARGET_CITIZENS     = 6;
inline constexpr int32_t  AI_BASE_SEARCH_RADIUS_TILES = 24;
inline constexpr int64_t  AI_REACTIVE_RADIUS_MT       = 20000;  // 20 tiles

// Sprint 1.6B (SPEC-004 §19): umbral entero de stock "bajo" para la capa
// económica adaptativa — deriva de economy_focus_bp (basis points del
// perfil, mismo patrón bp que el resto de capas: MÁS foco económico ⇒
// umbral MÁS alto ⇒ la IA se sobre-abastece antes/más). Con el perfil v1
// (economy_focus_bp=5000, 50%) el umbral efectivo es la MITAD de la base.
inline constexpr int32_t AI_GATHER_STOCK_THRESHOLD_BASE = 150;

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
// Helpers de la IA v1 (SPEC-005 §3/§4). Todos PUROS de su(s) argumento(s)
// explícito(s) — ninguno lee g.tick, reloj real, ni asigna heap. Todas las
// búsquedas son barridos ascendentes por índice: el mismo (g, ai_player,
// [source_tick]) produce siempre el mismo resultado, sin importar cuántas
// veces se invoque ni desde qué runtime (regla de oro §0).
// ---------------------------------------------------------------------------

// Perfil por defecto (mismos valores literales que data/ai_profiles/
// base_demo_normal.yaml) — usado cuando el catálogo enlazado no tiene la
// sección ai-profile (p.ej. fixtures de test aislados de otros sprints que
// nunca la necesitaron) o no resuelve "base:demo_normal". La IA v1 SIEMPRE
// usa este único perfil para ambos jugadores IA (SPEC-005 §3).
inline constexpr AiProfileV1 AI_DEFAULT_PROFILE_V1 = AiProfileV1{
    INVALID_AI_PROFILE_ID,
    5000, 5000, 5000, 5000, 5000,
    20u, 4u,
    2500, 3000,
};

// Resuelve el perfil desde el catálogo YA enlazado (lectura pura de datos
// estáticos del match, invariantes durante todo el partido — no rompe la
// regla de oro: NO es reloj/heap/entropía, es el mismo binding de solo
// lectura que ya usan TRAIN_UNIT/PLACE_BUILDING vía g.catalog).
inline AiProfileV1 ai_resolve_profile(const GameState& g) noexcept {
    if (g.catalog != nullptr) {
        static constexpr char kName[] = "base:demo_normal";
        const AiProfileId id = catalog_find_ai_profile(*g.catalog, kName, sizeof(kName) - 1);
        if (id != INVALID_AI_PROFILE_ID) {
            return g.catalog->ai_profiles[id];
        }
    }
    return AI_DEFAULT_PROFILE_V1;
}

// Estado macro del jugador IA (SPEC-005 §4.1): un único barrido ascendente
// sobre entities.capacity acumula los contadores que parametrizan la capa
// estratégica y el centroide del ejército para la capa táctica.
struct AiMacroStateV1 {
    int32_t citizen_count = 0;         // ciudadanos vivos propios (unit_class==3)
    int32_t army_count    = 0;         // unidades de combate vivas propias (unit_class 0..2)
    int64_t army_sum_x    = 0;
    int64_t army_sum_y    = 0;         // suma de posiciones del ejército (centroide entero, §4.2)

    bool     has_anchor = false;       // ancla fija = primer edificio propio vivo (ascendente)
    uint16_t anchor_tx  = 0;
    uint16_t anchor_ty  = 0;
    int64_t  anchor_x   = 0;
    int64_t  anchor_y   = 0;

    bool     has_idle_citizen  = false; // primer ciudadano propio SIN build_target (ascendente)
    uint32_t idle_citizen_index = 0;
    uint32_t idle_citizen_gen   = 0;
};

inline void ai_scan_macro(const GameState& g, uint8_t ai_player, AiMacroStateV1& m) noexcept {
    m = AiMacroStateV1{};
    const uint32_t cap = g.entities.capacity;
    for (uint32_t i = 0; i < cap; ++i) {
        if (!g.entities.alive[i]) continue;
        if (g.owner[i] != ai_player) continue;

        if (g.entity_kind[i] == 1u) {
            if (!m.has_anchor) {
                m.has_anchor = true;
                m.anchor_tx  = g.bld_anchor_tx[i];
                m.anchor_ty  = g.bld_anchor_ty[i];
                m.anchor_x   = g.pos_x[i];
                m.anchor_y   = g.pos_y[i];
            }
            continue;
        }
        if (g.unit_class[i] == 3u) {
            ++m.citizen_count;
            if (!m.has_idle_citizen && g.build_target[i] == BUILD_NO_TARGET) {
                m.has_idle_citizen   = true;
                m.idle_citizen_index = i;
                m.idle_citizen_gen   = g.entities.generation[i];
            }
            continue;
        }
        if (g.unit_class[i] <= 2u) {
            ++m.army_count;
            m.army_sum_x += g.pos_x[i];
            m.army_sum_y += g.pos_y[i];
        }
    }
}

// Primer edificio propio vivo INCOMPLETO sin ningún ciudadano propio con
// build_target apuntándole (SPEC-005 §4.1: "asigna un ciudadano al sitio",
// mantenimiento independiente de qué intención estratégica gane el ciclo).
inline bool ai_find_unassigned_site(const GameState& g, uint8_t ai_player, uint32_t& out_site) noexcept {
    if (g.catalog == nullptr) return false;
    const uint32_t cap = g.entities.capacity;
    for (uint32_t site = 0; site < cap; ++site) {
        if (!g.entities.alive[site]) continue;
        if (g.owner[site] != ai_player) continue;
        if (g.entity_kind[site] != 1u) continue;
        if (g.building_id[site] >= g.catalog->building_count) continue;
        const BuildingDefinitionV1& bdef = g.catalog->buildings[g.building_id[site]];
        if (g.build_progress[site] >= bdef.build_time_ticks) continue;  // ya completo

        bool assigned = false;
        for (uint32_t worker = 0; worker < cap; ++worker) {
            if (!g.entities.alive[worker]) continue;
            if (g.owner[worker] != ai_player) continue;
            if (g.unit_class[worker] != 3u) continue;
            if (g.build_target[worker] == site) { assigned = true; break; }
        }
        if (!assigned) { out_site = site; return true; }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Capa económica adaptativa (Sprint 1.6B, SPEC-004 §19): cuenta aldeanos
// propios por recurso ASIGNADO (para redirigir ociosos/excedentes cuando el
// stock de un recurso está bajo). Un único barrido ascendente adicional
// (mismo estilo que ai_scan_macro; separado de él porque su condición de
// "ocioso" es distinta — build_target==NO_TARGET es sobre TODOS los
// ciudadanos sin build_target, aquí importa además su estado de recolección).
//
// `skip_index`/`has_skip`: excluye un índice ya consumido este mismo ciclo
// por otra emisión (ASSIGN_BUILD, paso 1) — evita que el mismo ciudadano
// reciba DOS órdenes contradictorias en el mismo tick (ASSIGN_BUILD fija
// build_target; GATHER lo cancela, SPEC-004 §18). Sin este guard, un
// ciudadano recién asignado a un sitio de construcción podría ser
// "redirigido" a recolectar en la MISMA decisión, deshaciendo la asignación
// que la capa 1 acababa de emitir — desperdicio determinista, no un bug de
// determinismo, pero evitable con este chequeo barato.
struct AiEcoStateV1 {
    int32_t  resource_count[RESOURCE_COUNT] = {};
    bool     has_idle = false;               // primer ciudadano SIN depósito vivo asignado (ascendente)
    uint32_t idle_index = 0;
    uint32_t idle_gen   = 0;
    // Primer (menor índice) ciudadano gathering cada recurso — candidato
    // "donante" si ese recurso resulta ser el más abundante y hay que pulsar
    // uno hacia el recurso escaso.
    bool     has_first_of[RESOURCE_COUNT] = {};
    uint32_t first_of_index[RESOURCE_COUNT] = {};
    uint32_t first_of_gen[RESOURCE_COUNT] = {};
};

inline void ai_scan_economy(const GameState& g, uint8_t ai_player,
                            bool has_skip, uint32_t skip_index,
                            AiEcoStateV1& e) noexcept {
    e = AiEcoStateV1{};
    const uint32_t cap = g.entities.capacity;
    for (uint32_t i = 0; i < cap; ++i) {
        if (!g.entities.alive[i]) continue;
        if (g.owner[i] != ai_player) continue;
        if (g.unit_class[i] != 3u) continue;
        if (g.build_target[i] != BUILD_NO_TARGET) continue;   // en obra: fuera de la economía
        if (has_skip && i == skip_index) continue;             // ya consumido este ciclo (ASSIGN_BUILD)

        const uint32_t dep = g.eco_assigned_deposit[i];
        const bool valid = (dep != ECO_NO_DEPOSIT) && (dep < g.n_deposits)
                         && (g.deposits[dep].remaining > 0);
        if (!valid) {
            if (!e.has_idle) {
                e.has_idle   = true;
                e.idle_index = i;
                e.idle_gen   = g.entities.generation[i];
            }
            continue;
        }
        const uint8_t r = g.deposits[dep].resource_idx;
        if (r < RESOURCE_COUNT) {
            ++e.resource_count[r];
            if (!e.has_first_of[r]) {
                e.has_first_of[r]   = true;
                e.first_of_index[r] = i;
                e.first_of_gen[r]   = g.entities.generation[i];
            }
        }
    }
}

// Depósito VIVO de `resource_idx` más cercano a (from_x,from_y) — dist_sq
// entera, empate por menor índice (mismo criterio que
// economy.hpp::eco_find_nearest_deposit/find_building_dropoff/combat/aggro).
struct AiDepositPickV1 {
    bool     found = false;
    uint32_t index = 0;
    int64_t  x = 0, y = 0;
};

inline AiDepositPickV1 ai_find_deposit_for_resource(const GameState& g, uint8_t resource_idx,
                                                    int64_t from_x, int64_t from_y) noexcept {
    AiDepositPickV1 r{};
    uint64_t best_d2 = 0;
    const Vec2Fx here{Fx{from_x}, Fx{from_y}};
    for (uint32_t d = 0; d < g.n_deposits; ++d) {
        if (g.deposits[d].remaining <= 0) continue;
        if (g.deposits[d].resource_idx != resource_idx) continue;
        FatalReason local_fatal{};  // descartado a propósito, mismo patrón que combat/aggro/eco
        const Vec2Fx there{Fx{g.deposits[d].x_raw}, Fx{g.deposits[d].y_raw}};
        const uint64_t d2 = dist_sq_raw(here, there, local_fatal);
        if (!r.found || d2 < best_d2) {
            r.found = true;
            r.index = d;
            r.x = g.deposits[d].x_raw;
            r.y = g.deposits[d].y_raw;
            best_d2 = d2;
        }
    }
    return r;
}

// "Tipo entrenador" (SPEC-005 §4.1): primer edificio del catálogo (id
// ascendente) cuyo `trains[]` (índice ascendente) contiene una unidad de la
// clase pedida (ciudadano o no-ciudadano). Puro function-of-catalog: mismo
// resultado para cualquier `g` que comparta catálogo.
//
// Sprint 1.6B (SPEC-004 §19, endurecimiento): gana el parámetro `civ`. Antes
// de este sprint el catálogo real solo tenía datos de UNA civilización
// jugable a la vez (los fixtures sintéticos de skirmish.hpp/skirmish_eco.hpp,
// o el golden de Sprint 1.4 con un único bando); con `civ_id` tipado (K1) y
// DOS civs reales en el mismo catálogo (egipto + rome), un barrido
// civ-agnóstico devolvía SIEMPRE el primer edificio del catálogo por id
// ascendente sin importar de quién es el jugador — para el jugador de la
// SEGUNDA civ (mayor id) esto resolvía el edificio/unidad EQUIVOCADOS
// (el gate de civilización de TRAIN_UNIT/PLACE_BUILDING, SPEC-004 §17, los
// habría rechazado con ILLEGAL_STATE en cada ciclo, dejando a ese jugador sin
// economía/ejército posible). `civ == INVALID_CIV_ID` (sin asignar) preserva
// el barrido civ-agnóstico original — compatibilidad con todos los fixtures/
// tests previos que nunca llaman a gs_set_player_civ.
struct AiTrainerTypeV1 {
    bool     found         = false;
    BuildingId building_type = INVALID_BUILDING_ID;
    UnitId     unit_to_train = INVALID_UNIT_ID;
};

inline AiTrainerTypeV1 ai_find_trainer_type(const DataCatalogV1& cat, CivId civ,
                                            bool citizen_kind) noexcept {
    AiTrainerTypeV1 r{};
    for (uint32_t bt = 0; bt < cat.building_count; ++bt) {
        const BuildingDefinitionV1& bdef = cat.buildings[bt];
        if (civ != INVALID_CIV_ID && bdef.civ_id != civ) continue;
        for (uint8_t k = 0; k < bdef.train_count; ++k) {
            const UnitId uid = bdef.trains[k];
            if (uid >= cat.unit_count) continue;
            const bool is_citizen = (cat.units[uid].unit_class == UnitClassV1::Citizen);
            if (is_citizen == citizen_kind) {
                r.found = true;
                r.building_type = bt;
                r.unit_to_train = uid;
                return r;
            }
        }
    }
    return r;
}

// ¿El jugador IA ya posee (vivo) al menos una instancia del tipo de edificio
// `type_id`? Y, si sí, ¿la primera (ascendente) ya está COMPLETA?
struct AiOwnedBuildingV1 {
    bool     any_owned     = false;
    bool     has_complete  = false;
    uint32_t complete_index = 0;
};

inline AiOwnedBuildingV1 ai_find_owned_building_of_type(const GameState& g, uint8_t ai_player,
                                                        BuildingId type_id) noexcept {
    AiOwnedBuildingV1 r{};
    if (g.catalog == nullptr || type_id >= g.catalog->building_count) return r;
    const BuildingDefinitionV1& bdef = g.catalog->buildings[type_id];
    const uint32_t cap = g.entities.capacity;
    for (uint32_t i = 0; i < cap; ++i) {
        if (!g.entities.alive[i]) continue;
        if (g.owner[i] != ai_player) continue;
        if (g.entity_kind[i] != 1u) continue;
        if (g.building_id[i] != type_id) continue;
        r.any_owned = true;
        if (!r.has_complete && g.build_progress[i] >= bdef.build_time_ticks) {
            r.has_complete   = true;
            r.complete_index = i;
        }
    }
    return r;
}

inline bool ai_afford(const GameState& g, uint8_t player,
                      const int32_t (&cost)[RESOURCE_COUNT]) noexcept {
    for (uint32_t resource = 0; resource < RESOURCE_COUNT; ++resource) {
        if (g.player_stock[player][resource] < cost[resource]) return false;
    }
    return true;
}

inline bool ai_afford_epoch(const GameState& g, uint8_t player,
                            int32_t cost0, int32_t cost1,
                            int32_t cost2) noexcept {
    return g.player_stock[player][0] >= cost0
        && g.player_stock[player][1] >= cost1
        && g.player_stock[player][2] >= cost2;
}

inline bool ai_epoch_ok(uint8_t player_epoch, uint8_t epoch_min, uint8_t epoch_max) noexcept {
    return player_epoch >= epoch_min && player_epoch <= epoch_max;
}

inline bool ai_caps_ok(const GameState& g, uint8_t player, const BuildingDefinitionV1& bdef) noexcept {
    for (uint8_t k = 0; k < bdef.required_capabilities_count; ++k) {
        const CapabilityId cap_id = bdef.required_capabilities[k];
        const uint32_t word = cap_id / 64u;
        const uint32_t bit  = cap_id % 64u;
        if (word >= CAP_WORDS) return false;
        if (((g.player_caps[player][word] >> bit) & 1u) == 0u) return false;
    }
    return true;
}

// Sitio libre determinista (SPEC-005 §4.1: "barrido ascendente desde un
// ancla fija por jugador"): ventana [anchor-R, anchor+R] recorrida en orden
// (ty, tx) ascendente; primer bloque fw×fh íntegramente transitable
// (cost_grid != FF_WALL) y dentro de cota de mapa/flow-field gana. Lectura
// directa de g.cost_grid (la IA v1 lee todo el estado, SPEC-005 §1/§10): más
// eficaz que emitir a ciegas y esperar el rechazo del kernel.
struct AiFreeCellV1 {
    bool     found = false;
    uint16_t tx    = 0;
    uint16_t ty    = 0;
};

inline AiFreeCellV1 ai_find_free_cell(const GameState& g, uint16_t anchor_tx, uint16_t anchor_ty,
                                      uint8_t fw, uint8_t fh) noexcept {
    AiFreeCellV1 r{};
    const int32_t R  = AI_BASE_SEARCH_RADIUS_TILES;
    const int32_t ax = static_cast<int32_t>(anchor_tx);
    const int32_t ay = static_cast<int32_t>(anchor_ty);
    const int32_t map_hi_x = static_cast<int32_t>(g.cfg.map_tiles_x) - static_cast<int32_t>(fw);
    const int32_t map_hi_y = static_cast<int32_t>(g.cfg.map_tiles_y) - static_cast<int32_t>(fh);
    const int32_t ff_hi_x  = static_cast<int32_t>(FF_AXIS) - static_cast<int32_t>(fw);
    const int32_t ff_hi_y  = static_cast<int32_t>(FF_AXIS) - static_cast<int32_t>(fh);
    const int32_t hi_x = (map_hi_x < ff_hi_x) ? map_hi_x : ff_hi_x;
    const int32_t hi_y = (map_hi_y < ff_hi_y) ? map_hi_y : ff_hi_y;
    if (hi_x < 0 || hi_y < 0) return r;

    for (int32_t ty = ay - R; ty <= ay + R; ++ty) {
        if (ty < 0 || ty > hi_y) continue;
        for (int32_t tx = ax - R; tx <= ax + R; ++tx) {
            if (tx < 0 || tx > hi_x) continue;
            bool free_ok = true;
            for (int32_t cy = ty; cy < ty + static_cast<int32_t>(fh) && free_ok; ++cy) {
                for (int32_t cx = tx; cx < tx + static_cast<int32_t>(fw); ++cx) {
                    const uint32_t idx = static_cast<uint32_t>(cy) * FF_AXIS + static_cast<uint32_t>(cx);
                    if (g.cost_grid[idx] == FF_WALL) { free_ok = false; break; }
                }
            }
            if (free_ok) {
                r.found = true;
                r.tx = static_cast<uint16_t>(tx);
                r.ty = static_cast<uint16_t>(ty);
                return r;
            }
        }
    }
    return r;
}

// Objetivo táctico (SPEC-005 §4.2): edificio productor enemigo más cercano
// al centroide del ejército propio (dist_sq entera, empate por menor
// índice); si no hay ninguno, la unidad enemiga viva más cercana. Mismo
// criterio dist_sq/desempate que combat_system/aggro_system (step.hpp).
struct AiTargetV1 {
    bool    found = false;
    uint32_t index = 0;
    int64_t x = 0;
    int64_t y = 0;
};

inline AiTargetV1 ai_find_attack_target(const GameState& g, uint8_t ai_player,
                                        int64_t center_x, int64_t center_y) noexcept {
    const Vec2Fx center{Fx{center_x}, Fx{center_y}};
    AiTargetV1 building_best{};
    AiTargetV1 any_best{};
    uint64_t   building_d2 = 0;
    uint64_t   any_d2      = 0;
    const uint32_t cap = g.entities.capacity;
    for (uint32_t j = 0; j < cap; ++j) {
        if (!g.entities.alive[j]) continue;
        if (g.owner[j] == ai_player) continue;

        FatalReason local_fatal{};  // descartado a propósito, mismo patrón que combat/aggro
        const Vec2Fx pj{Fx{g.pos_x[j]}, Fx{g.pos_y[j]}};
        const uint64_t d2 = dist_sq_raw(center, pj, local_fatal);

        if (!any_best.found || d2 < any_d2 || (d2 == any_d2 && j < any_best.index)) {
            any_best = AiTargetV1{true, j, g.pos_x[j], g.pos_y[j]};
            any_d2 = d2;
        }
        if (g.entity_kind[j] == 1u) {
            if (!building_best.found || d2 < building_d2 || (d2 == building_d2 && j < building_best.index)) {
                building_best = AiTargetV1{true, j, g.pos_x[j], g.pos_y[j]};
                building_d2 = d2;
            }
        }
    }
    return building_best.found ? building_best : any_best;
}

// Capa reactiva (SPEC-005 §4.3): ¿alguna unidad de combate enemiga está
// dentro de AI_REACTIVE_RADIUS_MT del ancla de la base IA? (Solo unidades de
// combate: un ciudadano o edificio enemigo simplemente existiendo cerca no
// "amenaza" — coherente con qué puede efectivamente atacar en este kernel.)
inline bool ai_enemy_near_base(const GameState& g, uint8_t ai_player, int64_t anchor_x,
                               int64_t anchor_y) noexcept {
    const Vec2Fx anchor{Fx{anchor_x}, Fx{anchor_y}};
    const int64_t  r_raw = (AI_REACTIVE_RADIUS_MT * FX_ONE_RAW) / 1000;
    const uint64_t r_sq  = static_cast<uint64_t>(r_raw) * static_cast<uint64_t>(r_raw);
    const uint32_t cap = g.entities.capacity;
    for (uint32_t j = 0; j < cap; ++j) {
        if (!g.entities.alive[j]) continue;
        if (g.owner[j] == ai_player) continue;
        if (g.unit_class[j] > 2u) continue;  // solo combate: ciudadanos/edificios no "amenazan"
        FatalReason local_fatal{};
        const Vec2Fx pj{Fx{g.pos_x[j]}, Fx{g.pos_y[j]}};
        if (dist_sq_raw(anchor, pj, local_fatal) <= r_sq) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// 4. Ejecución determinista. Precondición: state == DISPATCHED.
//
//    REGLA DE ORO (§0/§7.1): jamás usar g.tick ni reloj real; solo
//    b.source_tick y b.runtime_before. Si se viola, la re-ejecución diverge
//    y se rompe la replay bit-a-bit. v1 NO usa RNG en absoluto (todo
//    desempate es por menor índice, ya determinista sin necesidad de
//    AI_TIEBREAK — ver el comentario de cabecera del archivo).
//
//    IA de 3 capas (SPEC-005 §4), orden fijo, presupuesto compartido
//    (AI_MAX_COMMANDS=64):
//      0) Barrido macro único (ai_scan_macro) + resolución del perfil.
//      1) Mantenimiento: ASSIGN_BUILD si hay un sitio propio sin ciudadano
//         asignado y uno ocioso disponible — independiente de la intención
//         estratégica del ciclo (SPEC-005 §4.1).
//      1.5) Capa ECONÓMICA ADAPTATIVA (Sprint 1.6B, SPEC-004 §19): cuenta
//         aldeanos por recurso asignado + stock A/B/Me; si el recurso con
//         mayor déficit bajo el umbral (bp del perfil) tiene CERO aldeanos
//         asignados o hay uno ocioso, emite GATHER redirigiendo un ocioso
//         (prioridad) o un excedente (del recurso con más aldeanos) hacia
//         ese recurso. Como máximo UN GATHER por ciclo (v1, conservador).
//      2) Capa ESTRATÉGICA (§4.1): utility entera (bp) sobre 4 intenciones
//         mutuamente excluyentes — economía (TRAIN_UNIT ciudadano) / construir
//         (PLACE_BUILDING) / militarizar (TRAIN_UNIT combate) / tech-época
//         (RESEARCH_TECH o EPOCH_UP). Gana la de mayor utilidad; empate ->
//         menor índice de intención (economía < construir < militarizar <
//         tech). Solo se emite la ganadora (si utilidad > 0).
//      3) Capa REACTIVA (§4.3) vs TÁCTICA (§4.2): si hay enemigos de combate
//         cerca del ancla de la base -> el ejército DEFIENDE (MOVE_TO al
//         ancla); si no, y el ejército alcanza el umbral derivado de
//         expansion_aggressiveness_bp -> ATACA (MOVE_TO al objetivo táctico).
//         La reactiva tiene prioridad estricta sobre la táctica (§4.3: "antes
//         de gastar presupuesto en ataque").
//
//    INVARIANTE DEL SCHEDULER (auditoría Opus, Sprint 1.4, P2): esta función
//    lee el `g` VIVO, no una copia congelada en source_tick. Es correcto SOLO
//    porque el driver invoca dispatch→execute en la MISMA iteración de tick
//    (source_tick == g.tick al ejecutar), y la degradación RUNNING→DISPATCHED
//    de ai_deserialize re-ejecuta contra el mismo `g` cargado. El contrato §0
//    exige que la salida dependa solo de (g, source_tick, runtime_before) — lo
//    cumple —, no que `g` sea una copia. Cualquier refactor futuro del
//    scheduler DEBE preservar "execute en el tick de dispatch": jamás llamar a
//    ai_execute con un `g` ya avanzado respecto a b.source_tick.
// ---------------------------------------------------------------------------
inline void ai_execute(AiJobBox& b, const GameState& g) noexcept {
    b.state        = AiJobState::RUNNING;
    b.result_count = 0u;

    const uint8_t  ai_player   = b.ai_player;
    const uint32_t target_tick = b.source_tick + AI_INPUT_DELAY_TICKS;
    const uint64_t seq_base    = b.runtime_before.ai_sequence;
    uint32_t       count       = 0u;

    // Emisor canónico único: target_tick/emitter/sequence fijos por contrato
    // (§4), orden de emisión = orden de llamada (ascendente por construcción
    // en todo el cuerpo de la función). Corta en cuanto count==AI_MAX_COMMANDS
    // (presupuesto, determinista: SIEMPRE el mismo prefijo para el mismo
    // estado de entrada).
    auto emit = [&](CommandType type, EntityHandle h, int64_t xr, int64_t yr,
                    UnitId uid) noexcept -> bool {
        if (count >= AI_MAX_COMMANDS) return false;
        RawCommand cmd{};
        cmd.target_tick = target_tick;
        cmd.emitter     = static_cast<uint16_t>(ai_player);
        cmd.type        = type;
        cmd.sequence    = seq_base + static_cast<uint64_t>(count + 1u);
        cmd.p.handle    = h;
        cmd.p.x_raw     = xr;
        cmd.p.y_raw     = yr;
        cmd.p.unit_id   = uid;
        b.result[count] = cmd;
        ++count;
        return true;
    };

    // ---- 0. Barrido macro + perfil ----------------------------------------
    AiMacroStateV1 macro{};
    ai_scan_macro(g, ai_player, macro);
    const AiProfileV1 profile = ai_resolve_profile(g);

    // ---- 1. Mantenimiento: ASSIGN_BUILD (SPEC-005 §4.1) --------------------
    bool build_assign_emitted = false;
    {
        uint32_t unassigned_site = 0;
        if (macro.has_idle_citizen && ai_find_unassigned_site(g, ai_player, unassigned_site)) {
            build_assign_emitted = emit(CommandType::ASSIGN_BUILD,
                 EntityHandle{macro.idle_citizen_index, macro.idle_citizen_gen},
                 static_cast<int64_t>(g.bld_anchor_tx[unassigned_site]),
                 static_cast<int64_t>(g.bld_anchor_ty[unassigned_site]),
                 0u);
        }
    }

    // ---- 1.5. Capa económica adaptativa (Sprint 1.6B, SPEC-004 §19) -------
    {
        AiEcoStateV1 eco{};
        ai_scan_economy(g, ai_player, build_assign_emitted, macro.idle_citizen_index, eco);

        // Umbral entero derivado de economy_focus_bp (mismo patrón bp que el
        // resto de capas): sin float, basis points 0..10000. Reutiliza
        // `profile`, ya resuelto en el paso 0 (mismo perfil que el resto de
        // capas de este ciclo — no re-consultar el catálogo dos veces).
        const int64_t threshold = (static_cast<int64_t>(AI_GATHER_STOCK_THRESHOLD_BASE)
                                   * profile.economy_focus_bp) / 10000;

        // Recurso con MAYOR déficit (umbral - stock, solo si positivo);
        // empate -> menor índice de recurso (0=A,1=B,2=Me, recorrido ascendente).
        int32_t best_r = -1;
        int64_t best_deficit = 0;
        for (uint8_t r = 0; r < RESOURCE_COUNT; ++r) {
            // 1.8A reserva 3..31 pero no activa recursos nuevos. La política
            // de IA ampliada pertenece a 1.8B (CONCORDANCIA C6).
            if (r > RESOURCE_INDEX_STONE) continue;
            const int64_t deficit = threshold - g.player_stock[ai_player][r];
            if (deficit > 0 && deficit > best_deficit) { best_deficit = deficit; best_r = static_cast<int32_t>(r); }
        }

        if (best_r >= 0) {
            const uint8_t need_r = static_cast<uint8_t>(best_r);
            bool     do_redirect = false;
            uint32_t redirect_idx = 0, redirect_gen = 0;
            if (eco.has_idle) {
                do_redirect  = true;
                redirect_idx = eco.idle_index;
                redirect_gen = eco.idle_gen;
            } else {
                // Excedente: tomar del recurso (!= need_r) con MÁS aldeanos
                // asignados, empate -> menor índice de recurso; exige > 1
                // asignado (no vaciar por completo esa recolección).
                int32_t donor_r = -1;
                int32_t donor_count = 0;
                for (uint8_t r = 0; r < RESOURCE_COUNT; ++r) {
                    if (r > RESOURCE_INDEX_STONE) continue;
                    if (r == need_r) continue;
                    if (eco.resource_count[r] > 1 && eco.resource_count[r] > donor_count) {
                        donor_count = eco.resource_count[r];
                        donor_r = static_cast<int32_t>(r);
                    }
                }
                if (donor_r >= 0 && eco.has_first_of[static_cast<uint8_t>(donor_r)]) {
                    const uint8_t dr = static_cast<uint8_t>(donor_r);
                    do_redirect  = true;
                    redirect_idx = eco.first_of_index[dr];
                    redirect_gen = eco.first_of_gen[dr];
                }
            }

            if (do_redirect) {
                const AiDepositPickV1 pick = ai_find_deposit_for_resource(
                        g, need_r, g.pos_x[redirect_idx], g.pos_y[redirect_idx]);
                if (pick.found) {
                    emit(CommandType::GATHER, EntityHandle{redirect_idx, redirect_gen},
                         pick.x, pick.y, 0u);
                }
            }
        }
    }

    // ---- 2. Capa estratégica (SPEC-005 §4.1) ------------------------------
    if (g.catalog != nullptr) {
        const DataCatalogV1& cat = *g.catalog;
        const uint8_t epoch = g.player_epoch[ai_player];
        // Sprint 1.6B (SPEC-004 §19): civ del jugador IA — ver el comentario
        // de ai_find_trainer_type sobre por qué esto es ahora indispensable
        // con dos civs reales en el mismo catálogo.
        const CivId ai_civ = g.player_civ[ai_player];

        // Candidato ECONOMÍA: entrenar un ciudadano en el primer edificio
        // propio COMPLETO capaz de hacerlo.
        bool     econ_ok = false;
        uint32_t econ_building = 0;
        UnitId   econ_unit = INVALID_UNIT_ID;
        if (macro.citizen_count < AI_ECON_TARGET_CITIZENS) {
            const AiTrainerTypeV1 tt = ai_find_trainer_type(cat, ai_civ, /*citizen_kind=*/true);
            if (tt.found) {
                const AiOwnedBuildingV1 own = ai_find_owned_building_of_type(g, ai_player, tt.building_type);
                if (own.has_complete) {
                    const UnitDefinitionV1& udef = cat.units[tt.unit_to_train];
                    if (ai_epoch_ok(epoch, udef.epoch_min, udef.epoch_max)
                        && ai_afford(g, ai_player, udef.cost)
                        && g.pop_used[ai_player] + udef.pop_cost <= static_cast<int32_t>(POP_CAP_V1)
                        && g.prod_count[own.complete_index] < PROD_QUEUE_CAP) {
                        econ_ok = true;
                        econ_building = own.complete_index;
                        econ_unit = tt.unit_to_train;
                    }
                }
            }
        }

        // Candidato CONSTRUIR: falta un edificio "cuartel" (entrena una
        // unidad no-ciudadano) y hay sitio libre + stock.
        bool       build_ok = false;
        BuildingId build_type = INVALID_BUILDING_ID;
        AiFreeCellV1 build_cell{};
        {
            const AiTrainerTypeV1 mt = ai_find_trainer_type(cat, ai_civ, /*citizen_kind=*/false);
            if (mt.found) {
                const AiOwnedBuildingV1 own = ai_find_owned_building_of_type(g, ai_player, mt.building_type);
                if (!own.any_owned && macro.has_anchor) {
                    const BuildingDefinitionV1& bdef = cat.buildings[mt.building_type];
                    if (bdef.constructible == 1u
                        && ai_epoch_ok(epoch, bdef.epoch_min, bdef.epoch_max)
                        && ai_caps_ok(g, ai_player, bdef)
                        && ai_afford(g, ai_player, bdef.cost)) {
                        build_cell = ai_find_free_cell(g, macro.anchor_tx, macro.anchor_ty,
                                                       bdef.footprint_w, bdef.footprint_h);
                        if (build_cell.found) {
                            build_ok = true;
                            build_type = mt.building_type;
                        }
                    }
                }
            }
        }

        // Candidato MILITARIZAR: entrenar una unidad de combate en el primer
        // edificio propio COMPLETO capaz de hacerlo.
        bool     mil_ok = false;
        uint32_t mil_building = 0;
        UnitId   mil_unit = INVALID_UNIT_ID;
        {
            const AiTrainerTypeV1 mt = ai_find_trainer_type(cat, ai_civ, /*citizen_kind=*/false);
            if (mt.found) {
                const AiOwnedBuildingV1 own = ai_find_owned_building_of_type(g, ai_player, mt.building_type);
                if (own.has_complete) {
                    const UnitDefinitionV1& udef = cat.units[mt.unit_to_train];
                    if (ai_epoch_ok(epoch, udef.epoch_min, udef.epoch_max)
                        && ai_afford(g, ai_player, udef.cost)
                        && g.pop_used[ai_player] + udef.pop_cost <= static_cast<int32_t>(POP_CAP_V1)
                        && g.prod_count[own.complete_index] < PROD_QUEUE_CAP) {
                        mil_ok = true;
                        mil_building = own.complete_index;
                        mil_unit = mt.unit_to_train;
                    }
                }
            }
        }

        // Candidato TECH/ÉPOCA: investigar la primera tech disponible de un
        // edificio propio COMPLETO, o subir de época. REGLA DE ORO: el gate
        // (b) de EPOCH_UP (SPEC-004 §12.3, tiempo mínimo desde la época
        // inicial) depende de g.tick — DELIBERADAMENTE no se pre-verifica
        // aquí (sería leer g.tick por la puerta trasera). Solo se verifica el
        // gate (a) (recuento de edificios), que es función pura de g sin
        // tick; el kernel decide el resto — la IA emite optimista y lee el
        // rechazo el ciclo siguiente (SPEC-005 §5), reintentando cada 20
        // ticks hasta que el tiempo real haya pasado.
        bool     research_ok = false;
        uint32_t research_building = 0;
        TechId   research_tech_id = INVALID_TECH_ID;
        bool     epoch_up_try = false;
        {
            uint32_t epoch_building_count = 0;
            const uint32_t cap = g.entities.capacity;
            for (uint32_t bi = 0; bi < cap; ++bi) {
                if (!g.entities.alive[bi]) continue;
                if (g.owner[bi] != ai_player) continue;
                if (g.entity_kind[bi] != 1u) continue;
                if (g.building_id[bi] >= cat.building_count) continue;
                const BuildingDefinitionV1& bdef = cat.buildings[g.building_id[bi]];
                if (g.build_progress[bi] < bdef.build_time_ticks) continue;  // no completo
                if (epoch >= bdef.epoch_min && epoch <= bdef.epoch_max) ++epoch_building_count;

                if (research_ok) continue;               // ya se eligió candidato, sigue contando (a)
                if (g.research_tech[bi] != INVALID_TECH_ID) continue;  // edificio ocupado

                for (uint8_t rk = 0; rk < bdef.research_count && !research_ok; ++rk) {
                    const TechId tid = bdef.researches[rk];
                    if (tid >= cat.tech_count) continue;
                    const uint32_t tw = tid / 64u, tb = tid % 64u;
                    if (tw < TECH_WORDS && ((g.player_techs[ai_player][tw] >> tb) & 1u) != 0u) continue;

                    bool in_progress = false;
                    for (uint32_t oi = 0; oi < cap; ++oi) {
                        if (!g.entities.alive[oi]) continue;
                        if (g.owner[oi] != ai_player) continue;
                        if (g.research_tech[oi] == tid) { in_progress = true; break; }
                    }
                    if (in_progress) continue;

                    const TechDefinitionV1& tdef = cat.techs[tid];
                    if (tdef.epoch > epoch) continue;

                    bool prereq_ok = true;
                    for (uint8_t pk = 0; pk < tdef.prereq_count; ++pk) {
                        const TechId pr = tdef.prerequisites[pk];
                        const uint32_t pw = pr / 64u, pb = pr % 64u;
                        if (!(pw < TECH_WORDS && ((g.player_techs[ai_player][pw] >> pb) & 1u) != 0u)) {
                            prereq_ok = false; break;
                        }
                    }
                    if (!prereq_ok) continue;

                    bool mutex_clear = true;
                    for (uint8_t mk = 0; mk < tdef.mutex_count; ++mk) {
                        const TechId mx = tdef.mutually_exclusive_with[mk];
                        const uint32_t mw = mx / 64u, mb = mx % 64u;
                        if (mw < TECH_WORDS && ((g.player_techs[ai_player][mw] >> mb) & 1u) != 0u) {
                            mutex_clear = false; break;
                        }
                    }
                    if (!mutex_clear) continue;

                    if (!ai_afford(g, ai_player, tdef.cost)) continue;

                    research_ok = true;
                    research_building = bi;
                    research_tech_id = tid;
                }
            }
            if (epoch < EPOCH_MAX_V1 && epoch_building_count >= 2u
                && ai_afford_epoch(g, ai_player,
                                   EPOCH_COST_A, EPOCH_COST_B, EPOCH_COST_ME)) {
                epoch_up_try = true;
            }
        }
        const bool tech_ok = research_ok || epoch_up_try;

        // Candidato FABRICAR (Sprint 1.19, auditoria del 2026-07-30): el
        // primer edificio propio COMPLETO y OCIOSO con una receta que el
        // jugador puede pagar. Antes de esto la IA no fabricaba nunca, asi que
        // el bronce y todo lo que dependa de el eran exclusivos del humano.
        //
        // Recorrido ascendente por slot y por receta: determinista, desempate
        // por indice bajo, como todo lo demas de esta funcion.
        bool     craft_ok = false;
        uint32_t craft_building = 0;
        uint32_t craft_recipe_id = 0;
        if (cat.recipes != nullptr) {
            for (uint32_t bi = 0; bi < g.entities.capacity && !craft_ok; ++bi) {
                if (!g.entities.alive[bi]) continue;
                if (g.owner[bi] != ai_player) continue;
                if (g.entity_kind[bi] != 1u) continue;
                if (g.building_id[bi] >= cat.building_count) continue;
                const BuildingDefinitionV1& bd = cat.buildings[g.building_id[bi]];
                if (g.build_progress[bi] < bd.build_time_ticks) continue;
                // Ocupada: el kernel lo rechazaria (§12.4 paso 7) y emitir un
                // comando condenado es ruido en el mailbox.
                if (g.craft_recipe[bi] != INVALID_RECIPE_ID) continue;
                if (bd.epoch_min > epoch) continue;
                for (uint8_t k = 0; k < bd.recipe_count && !craft_ok; ++k) {
                    const RecipeId rid = bd.recipes[k];
                    if (rid >= cat.recipe_count) continue;
                    if (!ai_afford(g, ai_player, cat.recipes[rid].input)) continue;
                    craft_ok = true;
                    craft_building = bi;
                    craft_recipe_id = rid;
                }
            }
        }

        // ---- utilidad entera (bp), empate -> menor índice de intención ----
        const int32_t u_econ  = econ_ok  ? profile.economy_focus_bp  : 0;
        const int32_t u_build = build_ok ? profile.military_focus_bp : 0;
        const int32_t u_mil   = mil_ok   ? profile.military_focus_bp : 0;
        const int32_t u_tech  = tech_ok  ? profile.tech_focus_bp     : 0;
        // Fabricar es economia: convierte materia prima en material util, que
        // es lo mismo que hace recolectar un tick despues.
        const int32_t u_craft = craft_ok ? profile.economy_focus_bp   : 0;

        int32_t best_u = u_econ;
        int32_t best_idx = 0;
        if (u_build > best_u) { best_u = u_build; best_idx = 1; }
        if (u_mil   > best_u) { best_u = u_mil;   best_idx = 2; }
        if (u_tech  > best_u) { best_u = u_tech;  best_idx = 3; }
        if (u_craft > best_u) { best_u = u_craft; best_idx = 4; }

        if (best_u > 0) {
            if (best_idx == 0) {
                emit(CommandType::TRAIN_UNIT,
                     EntityHandle{econ_building, g.entities.generation[econ_building]},
                     0, 0, econ_unit);
            } else if (best_idx == 1) {
                emit(CommandType::PLACE_BUILDING, EntityHandle{0u, 0u},
                     static_cast<int64_t>(build_cell.tx), static_cast<int64_t>(build_cell.ty),
                     build_type);
            } else if (best_idx == 2) {
                emit(CommandType::TRAIN_UNIT,
                     EntityHandle{mil_building, g.entities.generation[mil_building]},
                     0, 0, mil_unit);
            } else if (best_idx == 4) {
                // CRAFT reutiliza unit_id para el RecipeId, igual que
                // RESEARCH_TECH hace con el TechId.
                emit(CommandType::CRAFT,
                     EntityHandle{craft_building, g.entities.generation[craft_building]},
                     0, 0, craft_recipe_id);
            } else {  // best_idx == 3
                if (research_ok) {
                    emit(CommandType::RESEARCH_TECH,
                         EntityHandle{research_building, g.entities.generation[research_building]},
                         0, 0, research_tech_id);
                } else {
                    emit(CommandType::EPOCH_UP, EntityHandle{0u, 0u}, 0, 0, 0u);
                }
            }
        }
    }

    // ---- 3. Reactiva vs táctica (SPEC-005 §4.2/§4.3) -----------------------
    if (macro.army_count > 0) {
        const bool defend = macro.has_anchor
                          && ai_enemy_near_base(g, ai_player, macro.anchor_x, macro.anchor_y);

        int32_t threshold = 10 - (profile.expansion_aggressiveness_bp / 1000);
        if (threshold < 1) threshold = 1;
        if (threshold > 10) threshold = 10;

        // Sprint 1.19 fase B: la IA usa las ORDENES DE COMBATE del 1.13 en vez
        // de caminar y esperar al enganche automatico. Antes emitia MOVE_TO y
        // dependia del aggro para pelear; con proyectiles que fallan eso la
        // castiga mas que antes, porque llega desordenada y dispara a lo que
        // pilla en vez de a lo que decidio atacar.
        if (defend) {
            // Volver a casa PELEANDO con lo que se encuentre, no atravesandolo.
            const uint32_t cap = g.entities.capacity;
            for (uint32_t i = 0; i < cap && count < AI_MAX_COMMANDS; ++i) {
                if (!g.entities.alive[i]) continue;
                if (g.owner[i] != ai_player) continue;
                if (g.unit_class[i] > 2u) continue;
                emit(CommandType::ATTACK_MOVE, EntityHandle{i, g.entities.generation[i]},
                     macro.anchor_x, macro.anchor_y, 0u);
            }
        } else if (macro.army_count >= threshold) {
            const int64_t centroid_x = macro.army_sum_x / macro.army_count;
            const int64_t centroid_y = macro.army_sum_y / macro.army_count;
            const AiTargetV1 tgt = ai_find_attack_target(g, ai_player, centroid_x, centroid_y);
            if (tgt.found) {
                // FUEGO FOCALIZADO sobre el objetivo que la capa tactica ya
                // elegia: ATTACK sobre ESA entidad, no MOVE_TO a sus
                // coordenadas. El objetivo va en unit_id (indice) y
                // speed_mtpt (generacion), como fija SPEC-004 §24.2.
                const uint32_t cap = g.entities.capacity;
                for (uint32_t i = 0; i < cap && count < AI_MAX_COMMANDS; ++i) {
                    if (!g.entities.alive[i]) continue;
                    if (g.owner[i] != ai_player) continue;
                    if (g.unit_class[i] > 2u) continue;
                    if (count >= AI_MAX_COMMANDS) break;
                    RawCommand cmd{};
                    cmd.target_tick = target_tick;
                    cmd.emitter     = static_cast<uint16_t>(ai_player);
                    cmd.type        = CommandType::ATTACK;
                    cmd.sequence    = seq_base + static_cast<uint64_t>(count + 1u);
                    cmd.p.handle    = EntityHandle{i, g.entities.generation[i]};
                    cmd.p.unit_id   = tgt.index;
                    cmd.p.speed_mtpt =
                        static_cast<int32_t>(g.entities.generation[tgt.index]);
                    b.result[count] = cmd;
                    ++count;
                }
            }
        }
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
