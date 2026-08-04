// Sprint 1.37 — BANCO de partida LARGA: ¿se llega a la época 15?
//
// POR QUÉ EXISTE. El Sprint 1.25 abrió las 15 épocas y el 1.28 hizo la comida
// renovable, pero NADIE ha medido si una partida llega DE VERDAD a la época 15
// ni cuánto tarda. La rampa del kernel exige `tick >= EPOCH_MIN_TICKS * pasos`
// con EPOCH_MIN_TICKS = 6000 (step.hpp §12.3): de la época 1 a la 15 son 14
// saltos, suelo teórico 84000 ticks. Lo que no sabemos es si la ECONOMÍA lo
// permite. Este banco mide eso con la economía real: partida IA contra IA de
// DOS jugadores arrancando en la época 1, catálogo real.
//
// QUÉ NO ES. No es una prueba de comportamiento: es un banco. Informa, no
// juzga. Devuelve 0 salvo error de carga del catálogo. Que la partida se quede
// en la época 6 es un dato valioso, no un fallo.
//
// Escenario (mismo montaje que test_ai_epoch_policy.cpp): skirmish_apertura
// resuelto del catálogo real, batch de setup en t==0, y el bucle de la IA
// (ai_should_dispatch/ai_dispatch/ai_execute/ai_due/ai_commit) pero con DOS
// cajas de IA — el escenario de la apertura solo mueve al jugador 1, y aquí
// los DOS jugadores son IA.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#include "chunsa/ai_stub.hpp"
#include "chunsa/data_catalog.hpp"
#include "chunsa/game_state.hpp"
#include "chunsa/skirmish_apertura.hpp"
#include "chunsa/step.hpp"

#ifndef CHUNSA_GOLDEN_CHDB_PATH
#error "CHUNSA_GOLDEN_CHDB_PATH debe definirse via CMake"
#endif

using namespace chunsa;

namespace {

constexpr uint32_t kN_TICKS = 120000u;   // cota dura del banco
constexpr uint32_t kN_PLAYERS = 2u;
constexpr uint32_t kREPORT_EVERY = 6000u;

MatchConfig01A cfg_of(uint64_t seed) {
    MatchConfig01A c{};
    c.max_entities = 512;
    c.player_count = static_cast<uint8_t>(kN_PLAYERS);
    c.human_input_delay_ticks = 0;
    c.max_future_command_ticks = 20;
    c.checksum_every_ticks = 1;
    c.map_tiles_x = 256; c.map_tiles_y = 256;
    c.seed = seed;
    return c;
}

std::unique_ptr<GameState> make_state(const DataCatalogV1& cat,
                                      const SkirmishAperturaSetup& setup,
                                      uint64_t seed) {
    auto g = std::make_unique<GameState>();
    gs_init(*g, cfg_of(seed));
    gs_bind_catalog(*g, cat);
    gs_set_player_civ(*g, 0, setup.civ_egipto);
    gs_set_player_civ(*g, 1, setup.civ_rome);
    gs_init_epoch_from_catalog_per_player(*g);
    gs_init_economy_from_catalog(*g);
    return g;
}

// ¿Es un edificio de recurso renovable (granja o bosque plantado)?
inline bool is_farm_building(const BuildingDefinitionV1& bd) {
    return bd.creates_regen_per_tick > 0
        && bd.creates_resource_idx == RESOURCE_INDEX_FOOD;
}

inline bool is_grove_building(const BuildingDefinitionV1& bd) {
    return bd.creates_regen_per_tick > 0
        && bd.creates_resource_idx == RESOURCE_INDEX_WOOD;
}

struct FarmGroveCount {
    uint32_t farms = 0;   // granjas COMPLETAS y vivas
    uint32_t groves = 0;  // bosques plantados COMPLETOS y vivos
};

struct Census {
    uint32_t citizens = 0;  // unit_class == 3 vivos
    uint32_t army = 0;      // unit_class 0..2 vivos
    uint32_t buildings = 0; // entity_kind == 1 vivos
};

inline Census census(const GameState& g, uint32_t player) {
    Census out{};
    for (uint32_t i = 0; i < g.entities.capacity; ++i) {
        if (!g.entities.alive[i]) continue;
        if (g.owner[i] != player) continue;
        if (g.entity_kind[i] == 1u) { ++out.buildings; continue; }
        if (g.unit_class[i] == 3u) ++out.citizens;
        else if (g.unit_class[i] <= 2u) ++out.army;
    }
    return out;
}

// Censo instantáneo de granjas/bosques plantados COMPLETOS y vivos de un
// jugador. Es un snapshot (lo que está en pie en este tick): un campo derribado
// por el enemigo deja de contar aquí; los pedidos ACUMULADOS los lleva el banco
// aparte (ordenados_*).
inline FarmGroveCount count_farms_groves(const GameState& g, uint32_t player,
                                         const DataCatalogV1& cat) {
    FarmGroveCount out{};
    for (uint32_t i = 0; i < g.entities.capacity; ++i) {
        if (!g.entities.alive[i]) continue;
        if (g.owner[i] != player) continue;
        if (g.entity_kind[i] != 1u) continue;
        if (g.building_id[i] >= cat.building_count) continue;
        const BuildingDefinitionV1& bd = cat.buildings[g.building_id[i]];
        if (bd.creates_regen_per_tick <= 0) continue;
        if (static_cast<uint32_t>(g.build_progress[i]) < bd.build_time_ticks) continue;
        if (is_farm_building(bd)) ++out.farms;
        else if (is_grove_building(bd)) ++out.groves;
    }
    return out;
}

// ---- Replicación del barrido `research_ok` (ai_stub.hpp, candidato TECH) ----
//
// INSTRUMENTACIÓN EXTERNA DE SOLO LECTURA: copia exacta de los filtros del
// bucle (~líneas 1508-1581 de ai_stub.hpp) para CONTAR por qué se descarta
// cada tecnología candidata. El banco lee el catálogo y el estado igual que la
// IA, sin tocar el kernel. Motivos, en orden de evaluación: (0) edificio no
// completo, (1) edificio ocupado investigando, (2) tech fuera de catálogo,
// (3) ya investigada, (4) en curso en otro edificio, (5) época no alcanzada,
// (6) prerequisitos, (7) exclusión mutua, (8) asequibilidad (ai_afford). Lo
// que pasa TODOS los filtros se cuenta en `eligible`. `cost_short` desglosa
// los fallos de (8) por recurso corto.
struct ResearchWhy {
    uint64_t epoch = 0;         // tdef.epoch > época actual
    uint64_t prereq = 0;        // prerequisitos sin cumplir
    uint64_t mutex = 0;         // excluida por una ya investigada
    uint64_t cost = 0;          // ai_afford falla
    uint64_t already = 0;       // ya investigada
    uint64_t in_progress = 0;   // en curso en otro edificio
    uint64_t invalid_tech = 0;  // tid fuera de catálogo
    uint64_t bld_not_complete = 0;  // edificio sin terminar (salto por edificio)
    uint64_t bld_occupied = 0;      // edificio ya investigando (salto por edificio)
    uint64_t eligible = 0;      // pasó TODOS los filtros
    uint64_t cost_short[RESOURCE_COUNT] = {};  // por slot: nº de candidatos que fallan por él
};

inline void scan_research_reasons(const GameState& g, uint8_t player,
                                  const DataCatalogV1& cat, uint8_t epoch,
                                  ResearchWhy& out) {
    const uint32_t cap = g.entities.capacity;
    for (uint32_t bi = 0; bi < cap; ++bi) {
        if (!g.entities.alive[bi]) continue;
        if (g.owner[bi] != player) continue;
        if (g.entity_kind[bi] != 1u) continue;
        if (g.building_id[bi] >= cat.building_count) continue;
        const BuildingDefinitionV1& bdef = cat.buildings[g.building_id[bi]];
        if (g.build_progress[bi] < bdef.build_time_ticks) { ++out.bld_not_complete; continue; }
        if (g.research_tech[bi] != INVALID_TECH_ID) { ++out.bld_occupied; continue; }

        for (uint8_t rk = 0; rk < bdef.research_count; ++rk) {
            const TechId tid = bdef.researches[rk];
            if (tid >= cat.tech_count) { ++out.invalid_tech; continue; }
            const uint32_t tw = tid / 64u, tb = tid % 64u;
            if (tw < TECH_WORDS && ((g.player_techs[player][tw] >> tb) & 1u) != 0u) {
                ++out.already; continue;
            }

            bool in_progress = false;
            for (uint32_t oi = 0; oi < cap; ++oi) {
                if (!g.entities.alive[oi]) continue;
                if (g.owner[oi] != player) continue;
                if (g.research_tech[oi] == tid) { in_progress = true; break; }
            }
            if (in_progress) { ++out.in_progress; continue; }

            const TechDefinitionV1& tdef = cat.techs[tid];
            if (tdef.epoch > epoch) { ++out.epoch; continue; }

            bool prereq_ok = true;
            for (uint8_t pk = 0; pk < tdef.prereq_count; ++pk) {
                const TechId pr = tdef.prerequisites[pk];
                const uint32_t pw = pr / 64u, pb = pr % 64u;
                if (!(pw < TECH_WORDS && ((g.player_techs[player][pw] >> pb) & 1u) != 0u)) {
                    prereq_ok = false; break;
                }
            }
            if (!prereq_ok) { ++out.prereq; continue; }

            bool mutex_clear = true;
            for (uint8_t mk = 0; mk < tdef.mutex_count; ++mk) {
                const TechId mx = tdef.mutually_exclusive_with[mk];
                const uint32_t mw = mx / 64u, mb = mx % 64u;
                if (mw < TECH_WORDS && ((g.player_techs[player][mw] >> mb) & 1u) != 0u) {
                    mutex_clear = false; break;
                }
            }
            if (!mutex_clear) { ++out.mutex; continue; }

            bool afford = true;
            for (uint32_t r = 0; r < RESOURCE_COUNT; ++r) {
                if (g.player_stock[player][r] < tdef.cost[r]) {
                    afford = false;
                    ++out.cost_short[r];
                }
            }
            if (!afford) { ++out.cost; continue; }

            ++out.eligible;
        }
    }
}

struct Counters {
    uint64_t ordered_farms = 0;   // PLACE_BUILDING de granja emitido por la IA
    uint64_t ordered_groves = 0;  // PLACE_BUILDING de bosque plantado emitido por la IA
    uint64_t emitted_total = 0;   // comandos emitidos por la IA (todo tipo)
    uint64_t emitted_epoch_up = 0;
    uint64_t emitted_build = 0;
    uint64_t emitted_train = 0;
    uint64_t emitted_gather = 0;
    uint64_t emitted_attack = 0;
    uint64_t emitted_research = 0;
    uint64_t emitted_craft = 0;
    uint64_t emitted_trade = 0;
};

// Escanea los comandos resultantes de un job de IA: cuenta los PLACE_BUILDING
// de granja/bosque plantado que la IA pidió (aceptados o no — es la intención
// de construcción acumulada, el numerador de "¿construye granjas?"), y
// clasifica el resto de tipos de comando para ver si la IA sigue intentando
// algo o se quedó muda en el estancamiento.
inline void tally_orders(const RawCommand* result, uint32_t result_count,
                         const DataCatalogV1& cat, Counters& out) {
    for (uint32_t k = 0; k < result_count; ++k) {
        const RawCommand& rc = result[k];
        ++out.emitted_total;
        switch (rc.type) {
            case CommandType::EPOCH_UP:     ++out.emitted_epoch_up; break;
            case CommandType::PLACE_BUILDING: ++out.emitted_build; break;
            case CommandType::TRAIN_UNIT:   ++out.emitted_train; break;
            case CommandType::GATHER:       ++out.emitted_gather; break;
            case CommandType::ATTACK:       ++out.emitted_attack; break;
            case CommandType::RESEARCH_TECH: ++out.emitted_research; break;
            case CommandType::CRAFT:        ++out.emitted_craft; break;
            case CommandType::TRADE:        ++out.emitted_trade; break;
            default: break;
        }
        if (rc.type != CommandType::PLACE_BUILDING) continue;
        if (rc.p.unit_id >= cat.building_count) continue;
        const BuildingDefinitionV1& bd = cat.buildings[rc.p.unit_id];
        if (bd.creates_regen_per_tick <= 0) continue;
        if (is_farm_building(bd)) ++out.ordered_farms;
        else if (is_grove_building(bd)) ++out.ordered_groves;
    }
}

// Firma de "¿cambió algo?" usada para detectar el tick exacto de estancamiento
// (stock + censo + épocas). El banco no juzga: solo informa dónde se congeló.
inline uint64_t change_signature(const GameState& g) {
    uint64_t sig = 0;
    for (uint32_t p = 0; p < kN_PLAYERS; ++p) {
        sig += static_cast<uint64_t>(g.player_epoch[p]) * 1000000009ull;
        sig += static_cast<uint64_t>(g.player_stock[p][RESOURCE_INDEX_FOOD]);
        sig += static_cast<uint64_t>(g.player_stock[p][RESOURCE_INDEX_WOOD]);
        const Census c = census(g, p);
        sig += c.citizens * 1000003ull + c.army * 1009ull + c.buildings * 17ull;
    }
    return sig;
}

// Vuelco de diagnóstico del estado económico en un tick dado (NO alimenta
// ninguna decisión: es instrumentación externa de solo lectura, como los
// printf de los escenarios CLI).
inline void dump_economy(const GameState& g, const DataCatalogV1& cat,
                         uint32_t tick) {
    std::printf("bench_partida_larga: --- diagnostico economico en tick %u ---\n", tick);
    for (uint32_t d = 0; d < g.n_deposits; ++d) {
        std::printf("  deposito %2u: recurso_idx=%u remaining=%6d (x=%lld y=%lld)\n",
                    d, g.deposits[d].resource_idx, g.deposits[d].remaining,
                    static_cast<long long>(g.deposits[d].x_raw),
                    static_cast<long long>(g.deposits[d].y_raw));
    }
    for (uint32_t p = 0; p < kN_PLAYERS; ++p) {
        // cast explicito: `p` es uint32_t y la funcion toma uint8_t (MSVC
        // C4244). kN_PLAYERS es 2.
        const uint64_t mask =
            detail::allied_auto_gather_deposit_mask(g, static_cast<uint8_t>(p));
        std::printf("  jugador %u: mascara auto-gather=%016llx\n",
                    p, static_cast<unsigned long long>(mask));
        for (uint32_t i = 0; i < g.entities.capacity; ++i) {
            if (!g.entities.alive[i]) continue;
            if (g.owner[i] != p) continue;
            if (g.unit_class[i] != 3u) continue;
            std::printf("    ciudadano %u: task=%u eco_state=%u deposito=%u carry=%d carry_res=%u\n",
                        i, g.citizen_task[i], static_cast<unsigned>(g.eco_state[i]),
                        g.eco_assigned_deposit[i], g.eco_carry[i],
                        g.eco_carry_resource[i]);
        }
    }
    (void)cat;
}

// ¿La granja de la civilización es ENCONTRABLE por la IA en la época actual?
// Diagnóstico del porqué del "cero granjas pedidas" en el estancamiento.
inline void dump_farm_trigger(const GameState& g, const DataCatalogV1& cat,
                              const SkirmishAperturaSetup& setup) {
    const BuildingId f0 = ai_find_farm_type(cat, setup.civ_egipto, g.player_epoch[0]);
    const BuildingId f1 = ai_find_farm_type(cat, setup.civ_rome, g.player_epoch[1]);
    const int64_t outlook0 = ai_food_outlook(g, 0u);
    const int64_t outlook1 = ai_food_outlook(g, 1u);
    std::printf("bench_partida_larga: granja_findable p0(ep%u)=%u p1(ep%u)=%u | "
                "outlook comida p0=%lld p1=%lld (umbral granja=%d)\n",
                static_cast<unsigned>(g.player_epoch[0]), static_cast<unsigned>(f0),
                static_cast<unsigned>(g.player_epoch[1]), static_cast<unsigned>(f1),
                static_cast<long long>(outlook0),
                static_cast<long long>(outlook1),
                static_cast<int>(AI_FARM_FOOD_OUTLOOK_THRESHOLD));
}

// Posición del centroide de cada ejército (para ver si los ejércitos están
// separados por el muro del mapa o simplemente no se alcanzan).
inline void dump_armies(const GameState& g) {
    for (uint32_t p = 0; p < kN_PLAYERS; ++p) {
        int64_t sx = 0, sy = 0;
        uint32_t count = 0;
        for (uint32_t i = 0; i < g.entities.capacity; ++i) {
            if (!g.entities.alive[i]) continue;
            if (g.owner[i] != p) continue;
            if (g.entity_kind[i] != 0u || g.unit_class[i] > 2u) continue;
            sx += g.pos_x[i]; sy += g.pos_y[i]; ++count;
        }
        if (count > 0u) {
            std::printf("bench_partida_larga: ejercio p%u: %u unidades, centroide mt=(%.1f, %.1f)\n",
                        p, count,
                        static_cast<double>(sx / count) / 65536.0,
                        static_cast<double>(sy / count) / 65536.0);
        }
    }
    // Muestra las 3 primeras unidades de combate de cada bando: posición,
    // alcance, orden actual y moral — para leer el estancamiento sin abrir el
    // debugger.
    for (uint32_t p = 0; p < kN_PLAYERS; ++p) {
        uint32_t shown = 0;
        for (uint32_t i = 0; i < g.entities.capacity && shown < 3u; ++i) {
            if (!g.entities.alive[i]) continue;
            if (g.owner[i] != p) continue;
            if (g.entity_kind[i] != 0u || g.unit_class[i] > 2u) continue;
            std::printf("  p%u unidad %u: mt=(%.1f, %.1f) rango=%d hp=%d/%d moral=%d "
                        "order=%u atk_target=%u atk_cd=%d huyendo=%d\n",
                        p, i,
                        static_cast<double>(g.pos_x[i]) / 65536.0,
                        static_cast<double>(g.pos_y[i]) / 65536.0,
                        static_cast<int>(g.range_mt[i]),
                        static_cast<int>(g.hp[i]), static_cast<int>(g.max_hp[i]),
                        static_cast<int>(g.morale[i]),
                        static_cast<unsigned>(g.order_mode[i]),
                        static_cast<unsigned>(g.attack_target[i].index),
                        static_cast<int>(g.atk_cd[i]),
                        static_cast<int>(g.fleeing[i]));
            ++shown;
        }
    }
}

}  // namespace

int main() {
    DataCatalogStorageV1 store;
    const auto code = catalog_load_file_v1(CHUNSA_GOLDEN_CHDB_PATH,
                                           CatalogLoadProfile::Verified, store);
    if (code != CatalogLoadCode::Ok || !store.valid()) {
        std::printf("bench_partida_larga: no se pudo cargar el catalogo (%d)\n",
                    static_cast<int>(code));
        return 1;
    }
    const DataCatalogV1& cat = store.catalog();
    const SkirmishAperturaSetup setup = skirmish_apertura_resolve(cat);
    if (!setup.ok) {
        std::printf("bench_partida_larga: no se pudo resolver el setup de apertura\n");
        return 1;
    }

    auto g = make_state(cat, setup, 20260804ull);

    // Batch de setup en t==0 (centro + 3 aldeanos por bando, exención SPEC-004
    // §10.3). Sin el no hay ancla ni ciudadanos: no se mediría nada.
    {
        std::vector<RawCommand> batch(16);
        const uint32_t n = build_apertura_batch(batch, 0u, setup);
        step(*g, batch.data(), n);
    }

    std::printf("bench_partida_larga: catalogo=%s\n", CHUNSA_GOLDEN_CHDB_PATH);
    std::printf("bench_partida_larga: epocas iniciales p0=%u p1=%u (suelo epoca15 = %u ticks)\n",
                static_cast<unsigned>(g->player_epoch[0]),
                static_cast<unsigned>(g->player_epoch[1]),
                static_cast<unsigned>(EPOCH_MIN_TICKS * (EPOCH_MAX_V1 - 1u)));

    // DOS cajas de IA, una por jugador. El batch de setup ya gastó 4 sequences
    // de CADA emisor, así que ambas runtimes arrancan en ai_sequence=4.
    AiJobBox box0{}; ai_box_init(box0, 0);
    AiJobBox box1{}; ai_box_init(box1, 1);
    AiRuntimeV1 rt0{0u, 4u};
    AiRuntimeV1 rt1{0u, 4u};

    uint8_t epoch_seen[2] = {g->player_epoch[0], g->player_epoch[1]};
    uint8_t epoch_max[2] = {g->player_epoch[0], g->player_epoch[1]};
    Counters cnt[2] = {};
    ResearchWhy why[2] = {};

    // Snapshot de TODO el stock (no solo comida/madera) de ambos jugadores en
    // un tick dado: instrumentación de solo lectura, no decide nada.
    auto print_snapshot = [&](uint32_t t) {
        const FarmGroveCount fg0 = count_farms_groves(*g, 0u, cat);
        const FarmGroveCount fg1 = count_farms_groves(*g, 1u, cat);
        const Census c0 = census(*g, 0u);
        const Census c1 = census(*g, 1u);
        std::printf("bench_partida_larga: --- stock tick %u | ep p0=%u p1=%u | "
                    "ciudadanos p0=%u p1=%u | ejercito p0=%u p1=%u | edificios p0=%u p1=%u ---\n",
                    t,
                    static_cast<unsigned>(g->player_epoch[0]),
                    static_cast<unsigned>(g->player_epoch[1]),
                    c0.citizens, c1.citizens, c0.army, c1.army, c0.buildings, c1.buildings);
        for (uint32_t r = 0; r < cat.resource_count; ++r) {
            const uint32_t slot = cat.resources[r].index;
            if (slot >= RESOURCE_COUNT) continue;
            const char* name = (cat.resource_names != nullptr)
                                   ? cat.resource_names[r].record_id_utf8
                                   : nullptr;
            std::printf("  %-24s p0=%12lld p1=%12lld\n",
                        (name != nullptr) ? name : "?",
                        static_cast<long long>(g->player_stock[0][slot]),
                        static_cast<long long>(g->player_stock[1][slot]));
        }
        (void)fg0; (void)fg1;
    };

    print_snapshot(0u);

    const auto t_start = std::chrono::steady_clock::now();

    std::vector<RawCommand> batch(8 + 2u * AI_MAX_COMMANDS);
    uint32_t ai_executions = 0u;
    uint64_t accepted = 0u, rejected = 0u;
    uint64_t sig_prev = change_signature(*g);
    uint32_t tick_last_change = 0u;

    while (g->tick < kN_TICKS && g->game_over == 0u && g->fatal == FatalReason::NONE) {
        const uint32_t t = g->tick;
        uint32_t n = 0;
        n += build_apertura_batch(batch, t, setup);  // solo t==0 devuelve comandos

        auto pump = [&](AiJobBox& box, AiRuntimeV1& rt, uint8_t player) {
            if (ai_should_dispatch(box, t)) ai_dispatch(box, t, rt);
            if (box.state == AiJobState::DISPATCHED) {
                ai_execute(box, *g);
                ++ai_executions;
            }
            if (ai_stalled(box, t)) {
                ai_execute(box, *g);
                ++ai_executions;
            }
            if (ai_due(box, t)) {
                tally_orders(box.result, box.result_count, cat, cnt[player]);
                for (uint32_t k = 0; k < box.result_count && n < batch.size(); ++k)
                    batch[n++] = box.result[k];
                ai_commit(box, rt);
            }
        };

        pump(box0, rt0, 0u);
        pump(box1, rt1, 1u);

        const StepResult res = step(*g, batch.data(), n);
        accepted += res.accepted;
        rejected += res.rejected;

        // Barrido de investigación REPLICADO: contar por qué el bucle de
        // `research_ok` descarta cada candidato (solo lectura, no decide).
        scan_research_reasons(*g, static_cast<uint8_t>(0u), cat,
                              g->player_epoch[0], why[0]);
        scan_research_reasons(*g, static_cast<uint8_t>(1u), cat,
                              g->player_epoch[1], why[1]);

        // Subidas de época: registrar el tick exacto del salto.
        for (uint32_t p = 0; p < kN_PLAYERS; ++p) {
            if (g->player_epoch[p] > epoch_seen[p]) {
                std::printf("bench_partida_larga: jugador %u alcanza epoca %u en tick %u\n",
                            p,
                            static_cast<unsigned>(g->player_epoch[p]),
                            g->tick);
                epoch_seen[p] = g->player_epoch[p];
                if (epoch_seen[p] > epoch_max[p]) epoch_max[p] = epoch_seen[p];
            }
        }

        const uint64_t sig = change_signature(*g);
        if (sig != sig_prev) { sig_prev = sig; tick_last_change = g->tick; }

        if (g->tick % kREPORT_EVERY == 0u) print_snapshot(g->tick);
    }

    const auto t_end = std::chrono::steady_clock::now();
    const double wall_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();

    // Stock COMPLETO de cada jugador al final de la partida (aunque el tick
    // final no caiga en un corte de 6000).
    print_snapshot(g->tick);

    // Por qué no se investiga: desglose de descartes del barrido replicado de
    // `research_ok`. `coste` es el único motivo que puede hablar del recurso
    // corto; los demás (epoca/prereqs/mutex/ya/en_curso) son estructurales.
    for (uint32_t p = 0; p < kN_PLAYERS; ++p) {
        std::printf("bench_partida_larga: P%u barrido investigacion: "
                    "epoca=%llu prereqs=%llu mutex=%llu coste=%llu | "
                    "ya_investigada=%llu en_curso=%llu tech_invalida=%llu "
                    "edif_incompleto=%llu edif_ocupado=%llu | elegibles=%llu\n",
                    p,
                    static_cast<unsigned long long>(why[p].epoch),
                    static_cast<unsigned long long>(why[p].prereq),
                    static_cast<unsigned long long>(why[p].mutex),
                    static_cast<unsigned long long>(why[p].cost),
                    static_cast<unsigned long long>(why[p].already),
                    static_cast<unsigned long long>(why[p].in_progress),
                    static_cast<unsigned long long>(why[p].invalid_tech),
                    static_cast<unsigned long long>(why[p].bld_not_complete),
                    static_cast<unsigned long long>(why[p].bld_occupied),
                    static_cast<unsigned long long>(why[p].eligible));
        std::printf("bench_partida_larga: P%u fallos de coste por recurso corto:", p);
        for (uint32_t r = 0; r < cat.resource_count; ++r) {
            const uint32_t slot = cat.resources[r].index;
            if (slot >= RESOURCE_COUNT) continue;
            if (why[p].cost_short[slot] == 0u) continue;
            const char* name = (cat.resource_names != nullptr)
                                   ? cat.resource_names[r].record_id_utf8
                                   : nullptr;
            std::printf(" %s=%llu", (name != nullptr) ? name : "?",
                        static_cast<unsigned long long>(why[p].cost_short[slot]));
        }
        std::printf("\n");
    }

    std::printf("----------------------------------------------------------------------------------------------------------\n");

    // Resultado: quién ganó (si la partida terminó), en qué tick, y la época
    // MÁXIMA alcanzada por cada jugador. Esto es un BANCO: nada de esto falla.
    if (g->game_over != 0u) {
        if (g->winner == 0xFFu) {
            std::printf("bench_partida_larga: partida TERMINADA en tick %u (empate)\n", g->tick);
        } else {
            std::printf("bench_partida_larga: partida TERMINADA en tick %u, gana el jugador %u\n",
                        g->tick, static_cast<unsigned>(g->winner));
        }
    } else {
        std::printf("bench_partida_larga: la partida NO termino en %u ticks (limite del banco)\n",
                    kN_TICKS);
    }
    if (g->fatal != FatalReason::NONE) {
        std::printf("bench_partida_larga: fatal=%d en tick %u\n",
                    static_cast<int>(g->fatal), g->tick);
    }
    std::printf("bench_partida_larga: epoca MAXIMA alcanzada p0=%u p1=%u\n",
                static_cast<unsigned>(epoch_max[0]),
                static_cast<unsigned>(epoch_max[1]));
    std::printf("bench_partida_larga: granjas PEDIDAS p0=%llu p1=%llu | bosques PEDIDOS p0=%llu p1=%llu\n",
                static_cast<unsigned long long>(cnt[0].ordered_farms),
                static_cast<unsigned long long>(cnt[1].ordered_farms),
                static_cast<unsigned long long>(cnt[0].ordered_groves),
                static_cast<unsigned long long>(cnt[1].ordered_groves));
    const FarmGroveCount fg0 = count_farms_groves(*g, 0u, cat);
    const FarmGroveCount fg1 = count_farms_groves(*g, 1u, cat);
    std::printf("bench_partida_larga: granjas EN PIE p0=%u p1=%u | bosques EN PIE p0=%u p1=%u\n",
                fg0.farms, fg1.farms, fg0.groves, fg1.groves);
    std::printf("bench_partida_larga: comida en caja p0=%lld p1=%lld | madera p0=%lld p1=%lld\n",
                static_cast<long long>(g->player_stock[0][RESOURCE_INDEX_FOOD]),
                static_cast<long long>(g->player_stock[1][RESOURCE_INDEX_FOOD]),
                static_cast<long long>(g->player_stock[0][RESOURCE_INDEX_WOOD]),
                static_cast<long long>(g->player_stock[1][RESOURCE_INDEX_WOOD]));
    const Census e0 = census(*g, 0u);
    const Census e1 = census(*g, 1u);
    std::printf("bench_partida_larga: censo final p0={ciudadanos=%u ejercito=%u edificios=%u} "
                "p1={ciudadanos=%u ejercito=%u edificios=%u}\n",
                e0.citizens, e0.army, e0.buildings,
                e1.citizens, e1.army, e1.buildings);
    std::printf("bench_partida_larga: ultimo cambio de estado en tick %u (desde ahi, congelado)\n",
                tick_last_change);
    std::printf("bench_partida_larga: comandos IA aceptados=%llu rechazados=%llu\n",
                static_cast<unsigned long long>(accepted),
                static_cast<unsigned long long>(rejected));
    for (uint32_t p = 0; p < kN_PLAYERS; ++p) {
        std::printf("bench_partida_larga: P%u emitio=%llu (epoca=%llu construir=%llu entrenar=%llu "
                    "recolectar=%llu atacar=%llu investigar=%llu fabricar=%llu comerciar=%llu)\n",
                    p,
                    static_cast<unsigned long long>(cnt[p].emitted_total),
                    static_cast<unsigned long long>(cnt[p].emitted_epoch_up),
                    static_cast<unsigned long long>(cnt[p].emitted_build),
                    static_cast<unsigned long long>(cnt[p].emitted_train),
                    static_cast<unsigned long long>(cnt[p].emitted_gather),
                    static_cast<unsigned long long>(cnt[p].emitted_attack),
                    static_cast<unsigned long long>(cnt[p].emitted_research),
                    static_cast<unsigned long long>(cnt[p].emitted_craft),
                    static_cast<unsigned long long>(cnt[p].emitted_trade));
    }
    dump_economy(*g, cat, g->tick);
    dump_farm_trigger(*g, cat, setup);
    dump_armies(*g);
    std::printf("bench_partida_larga: %u ticks en %.1f ms (%.1f us/tick), %u ejecuciones de IA\n",
                g->tick, wall_ms, wall_ms * 1000.0 / static_cast<double>(g->tick),
                ai_executions);

    return 0;
}
