// Guardián de la POLÍTICA DE ÉPOCA de la IA (Sprint 1.23).
//
// POR QUÉ EXISTE. El 1.22 abrió las épocas 1-5 al jugador humano y dejó una
// pregunta sin responder: ¿sabe la MÁQUINA jugarlas? Al arrancar la apertura
// en la época 1 la partida dejó de terminar —36000 ticks, sin vencedor, cero
// construcciones militares— y se fijó el escenario en la 5 para no perder la
// prueba de regresión. Este guardián es la deuda de aquel día.
//
// CORRECCIÓN DE UN DIAGNÓSTICO MÍO. Dije entonces que «ai_execute nunca emite
// ADVANCE_EPOCH». Es FALSO: `ai_stub.hpp` calcula `epoch_up_try` y emite
// `CommandType::EPOCH_UP` cuando la intención de tecnología gana. Lo que
// ocurre es más fino, y sin medirlo no se ve:
//
//   La puerta de `ADVANCE_EPOCH` es `g.tick >= EPOCH_MIN_TICKS * pasos`, con
//   `pasos = epoca_actual - epoca_inicial + 1` y EPOCH_MIN_TICKS = 6000. La
//   rampa es ACUMULATIVA: 1->2 a los 6000, 2->3 a los 12000, 3->4 a los
//   18000, 4->5 a los 24000. Arrancar en la 1 cuesta 24000 ticks de reloj
//   ANTES de tocar la época 5, y la apertura entera se corta a 36000.
//
// Por eso este guardián NO comprueba que la IA gane desde la época 1 — eso
// mediría la rampa, no la política. Comprueba lo que sí depende de la IA:
//   (a) que SUBA de época cuando puede, en vez de estancarse;
//   (b) que en las épocas 1-2 use el contenido de las épocas 1-2 —construya
//       y entrene— en vez de quedarse esperando a que llegue la 5.
//
// (b) es lo que de verdad estaba roto: una IA que no hace nada durante 24000
// ticks no es una IA lenta, es un adversario ausente.

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

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

namespace {

MatchConfig01A cfg_of(uint64_t seed) {
    MatchConfig01A c{};
    c.max_entities = 512;
    c.player_count = 2;
    c.human_input_delay_ticks = 0;
    c.max_future_command_ticks = 20;
    c.checksum_every_ticks = 1;
    c.map_tiles_x = 256; c.map_tiles_y = 256;
    c.seed = seed;
    return c;
}

// Igual que el escenario de apertura pero SIN fijar la época: se hereda la
// del catálogo, que desde el 1.22 es la 1 para ambas civilizaciones.
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

}  // namespace

int main() {
    DataCatalogStorageV1 store;
    const auto code = catalog_load_file_v1(CHUNSA_GOLDEN_CHDB_PATH,
                                           CatalogLoadProfile::Verified, store);
    CHECK(code == CatalogLoadCode::Ok);
    if (code != CatalogLoadCode::Ok || !store.valid()) {
        std::printf("ai_epoch_policy: no se pudo cargar el catálogo\n");
        return 1;
    }
    const DataCatalogV1& cat = store.catalog();
    const SkirmishAperturaSetup setup = skirmish_apertura_resolve(cat);
    CHECK(setup.ok);
    if (!setup.ok) return 1;

    auto g = make_state(cat, setup, 20260731ull);

    // Pre-condición del sprint: se arranca DE VERDAD en la época 1. Si esto
    // falla, lo que sigue no mide lo que dice medir.
    CHECK(g->player_epoch[1] == 1u);

    // El escenario necesita su BATCH DE SETUP en t==0 (centro + 3 aldeanos por
    // bando, exencion de SPEC-004 §10.3). Sin el no hay ancla ni ciudadanos y
    // la IA no tendria nada con que jugar — se estaria midiendo un mapa vacio,
    // no una politica de epoca.
    {
        std::vector<RawCommand> batch(16);
        const uint32_t n = build_apertura_batch(batch, 0u, setup);
        step(*g, batch.data(), n);
        CHECK(n > 0u);
    }

    AiJobBox box{}; ai_box_init(box, 1);
    AiRuntimeV1 rt{0u, 4u};   // el setup de rome ya gasto 4 sequences

    const uint32_t kTicks = 30000u;
    uint8_t  epoch_max_seen = g->player_epoch[1];
    uint32_t tick_first_advance = 0u;
    uint32_t buildings_epoch12 = 0u;   // construidos por la IA con ventana <= 2
    uint32_t units_epoch12 = 0u;

    for (uint32_t t = 0; t < kTicks; ++t) {
        if (ai_should_dispatch(box, g->tick)) ai_dispatch(box, g->tick, rt);
        if (box.state == AiJobState::DISPATCHED) {
            ai_execute(box, *g);
            step(*g, box.result, box.result_count);
            rt.decision_epoch += 1u;
            rt.ai_sequence += box.result_count;
            ai_box_init(box, 1);
        } else {
            step(*g, nullptr, 0);
        }

        if (g->player_epoch[1] > epoch_max_seen) {
            epoch_max_seen = g->player_epoch[1];
            if (tick_first_advance == 0u) tick_first_advance = g->tick;
        }
    }

    // Censo final de lo que la IA levantó/entrenó del contenido temprano.
    for (uint32_t i = 0; i < g->entities.capacity; ++i) {
        if (!g->entities.alive[i]) continue;
        if (g->owner[i] != 1u) continue;
        if (g->entity_kind[i] == 1u) {
            if (g->building_id[i] < cat.building_count
                && cat.buildings[g->building_id[i]].epoch_max <= 2u) ++buildings_epoch12;
        } else {
            if (g->unit_id[i] != INVALID_UNIT_ID && g->unit_id[i] < cat.unit_count
                && cat.units[g->unit_id[i]].epoch_max <= 2u) ++units_epoch12;
        }
    }

    std::printf("ai_epoch_policy: epoca_final=%u primera_subida_tick=%u "
                "edificios_ep1-2=%u unidades_ep1-2=%u\n",
                static_cast<unsigned>(epoch_max_seen), tick_first_advance,
                buildings_epoch12, units_epoch12);

    // (a) La IA SUBE de época. Con 30000 ticks y una rampa de 6000 acumulativos
    //     le da tiempo de sobra a varios saltos; exigir >= 3 deja margen para
    //     que la economía tarde en pagar sin volver la prueba frágil.
    if (epoch_max_seen < 3u) {
        std::printf("La IA se ESTANCO en la epoca %u: no usa ADVANCE_EPOCH o no "
                    "reune los 2 edificios de la ventana actual\n",
                    static_cast<unsigned>(epoch_max_seen));
        ++g_fails;
    }

    // (b) Y mientras tanto NO se queda quieta: usa el contenido temprano.
    if (buildings_epoch12 == 0u) {
        std::printf("La IA no construyo NINGUN edificio de las epocas 1-2 — "
                    "el contenido que abrio el 1.22 le es invisible\n");
        ++g_fails;
    }


    // ========================================================================
    // LA IA CONSTRUYE GRANJAS CUANDO ESCASEA LA COMIDA (Sprint 1.34)
    //
    // POR QUE ESTA PRUEBA EXISTE. El delegado que implemento el candidato
    // señalo el hueco el mismo: "ningun test existente ejerce la granja de la
    // IA". Y tenia razon — la suite entera pasaba en verde con el candidato
    // recien anadido, porque en los escenarios actuales la comida nunca
    // escasea de verdad. Un verde que no prueba nada.
    //
    // Ya me mordio antes con `eco_available_for`: una funcion con pruebas
    // propias que NADIE llamaba. Aqui se cierra el hueco de entrada.
    // ========================================================================
    {
        auto g2 = make_state(cat, setup, 20260803ull);
        {
            std::vector<RawCommand> batch(16);
            const uint32_t n = build_apertura_batch(batch, 0u, setup);
            step(*g2, batch.data(), n);
        }
        // Se AGOTA el mapa a mano y se deja al jugador sin reservas de comida,
        // pero con madera de sobra para pagar la granja. Es el escenario que
        // hace falta y que ningun test producia.
        for (uint32_t d = 0; d < g2->n_deposits; ++d) {
            if (g2->deposits[d].resource_idx == 0u) g2->deposits[d].remaining = 0;
        }
        // La granja es de epoca [2,15] y este escenario arranca en la 1, donde
        // NO existe. Se fija la epoca 5, que es donde la apertura vive. Sin
        // esto la prueba mediria "no hay granja en el Paleolitico", que es
        // cierto y no es lo que interesa.
        for (uint8_t e = 0; e < 2u; ++e) g2->player_epoch[e] = 5u;
        g2->player_stock[1][0] = 30;      // comida casi agotada
        g2->player_stock[1][1] = 500;     // madera de sobra

        AiJobBox box2{}; ai_box_init(box2, 1);
        AiRuntimeV1 rt2{0u, 0u};
        bool granja_emitida = false;
        for (uint32_t t = 0; t < 400u && !granja_emitida; ++t) {
            if (ai_should_dispatch(box2, g2->tick)) ai_dispatch(box2, g2->tick, rt2);
            if (box2.state == AiJobState::DISPATCHED) {
                ai_execute(box2, *g2);
                for (uint32_t k = 0; k < box2.result_count; ++k) {
                    const RawCommand& rc = box2.result[k];
                    if (rc.type != CommandType::PLACE_BUILDING) continue;
                    if (rc.p.unit_id >= cat.building_count) continue;
                    if (cat.buildings[rc.p.unit_id].creates_regen_per_tick > 0) {
                        granja_emitida = true;   // eso es una granja
                    }
                }
                step(*g2, box2.result, box2.result_count);
                rt2.decision_epoch += 1u;
                rt2.ai_sequence += box2.result_count;
                ai_box_init(box2, 1);
            } else {
                step(*g2, nullptr, 0);
            }
        }
        if (!granja_emitida) {
            std::printf("La IA NO construyo granja con la comida agotada y madera de "
                        "sobra: el candidato del 1.34 no se dispara\n");
            ++g_fails;
        }
    }

    // ========================================================================
    // LA IA COMERCIA CUANDO UN RECURSO LA BLOQUEA (Sprint 1.35)
    //
    // POR QUE ESTA PRUEBA EXISTE. El Sprint 1.33 anadió el mercado (TRADE) y
    // la IA no lo usaba: podía quedarse bloqueada sin estaño —con oro de sobra
    // y un mercado propio construido— mirándolo. Ningún test ejercita el
    // comercio: test_market solo prueba la aritmética del lote, y sin un
    // escenario donde la IA DEBA comprar, la suite pasaría en verde con un
    // candidato que nunca se dispara (la lección del 1.34).
    //
    // Escenario: mercado propio COMPLETO, taller propio COMPLETO con la
    // receta rome:terramare_bronze_casting (cobre 3 + estaño 1 -> bronce 2),
    // oro de sobra, cobre de sobra y ESTAÑO A CERO. La IA quiere ejecutar la
    // receta, no puede pagarla, y el mercado la desatasca. Tiene que emitir
    // TRADE de COMPRA de estaño. Sin comida/madera/piedra, además, ningún otro
    // intento (construir/entrenar/época) puede ganarle al comercio.
    // ========================================================================
    {
        auto g3 = make_state(cat, setup, 20260804ull);
        {
            std::vector<RawCommand> batch(16);
            const uint32_t n = build_apertura_batch(batch, 0u, setup);
            step(*g3, batch.data(), n);
        }
        // Se AGOTA TODO el mapa a mano para que ningún ciudadano recolecte y
        // los stocks se queden exactamente donde los pone esta prueba. Sin
        // esto, los 3 aldeanos de la apertura podrían reunir comida/madera/
        // piedra y destapar EPOCH_UP, que (5000 bp) le ganaría al comercio.
        for (uint32_t d = 0; d < g3->n_deposits; ++d) g3->deposits[d].remaining = 0;
        // El taller [4,5] y el mercado [3,15] de Roma viven en la época 5,
        // que es donde la apertura fija el escenario. Sin esto no habría ni
        // receta que pagar ni mercado con el que pagarla.
        for (uint8_t e = 0; e < 2u; ++e) g3->player_epoch[e] = 5u;

        // Indices DINAMICOS del catálogo real — el oro NO tiene índice fijo y
        // cablearlo aquí rompería la prueba si cambian los datos.
        const ResourceId gold_id   = catalog_find_resource(cat, "chunsa:gold", 11);
        const ResourceId tin_id    = catalog_find_resource(cat, "chunsa:tin", 10);
        const ResourceId copper_id = catalog_find_resource(cat, "chunsa:copper", 13);
        const BuildingId market_b  = catalog_find_building(cat, "rome:market", 11);
        const BuildingId workshop_b = catalog_find_building(cat, "rome:terramare_workshop",
                                                           sizeof("rome:terramare_workshop") - 1);
        CHECK(gold_id != INVALID_RESOURCE_ID && tin_id != INVALID_RESOURCE_ID
              && copper_id != INVALID_RESOURCE_ID
              && market_b != INVALID_BUILDING_ID && workshop_b != INVALID_BUILDING_ID);
        if (gold_id == INVALID_RESOURCE_ID || tin_id == INVALID_RESOURCE_ID
            || copper_id == INVALID_RESOURCE_ID
            || market_b == INVALID_BUILDING_ID || workshop_b == INVALID_BUILDING_ID) {
            std::printf("comercio: el catálogo real no resolvió oro/estaño/cobre/"
                        "mercado/taller\n");
            ++g_fails;
        } else {
            const uint8_t oro    = static_cast<uint8_t>(cat.resources[gold_id].index);
            const uint8_t tin    = static_cast<uint8_t>(cat.resources[tin_id].index);
            const uint8_t copper = static_cast<uint8_t>(cat.resources[copper_id].index);

            // Mercado y taller propios y COMPLETOS (build_progress = su
            // build_time: nacer completos, mismo patrón que el put_foundry de
            // test_ai_craft pero con los tiempos reales del catálogo).
            auto put_complete_building = [&](BuildingId bid, int64_t tx, int64_t ty) -> void {
                const EntityHandle h = et_spawn(g3->entities);
                const uint32_t i = h.index;
                zero_components(*g3, i);
                g3->owner[i] = 1u;
                g3->entity_kind[i] = 1u;
                g3->building_id[i] = bid;
                g3->build_progress[i] = cat.buildings[bid].build_time_ticks;
                g3->hp[i] = 500; g3->max_hp[i] = 500;
                g3->pos_x[i] = tx * FX_ONE_RAW;
                g3->pos_y[i] = ty * FX_ONE_RAW;
                g3->bld_anchor_tx[i] = static_cast<uint16_t>(tx);
                g3->bld_anchor_ty[i] = static_cast<uint16_t>(ty);
            };
            put_complete_building(market_b, 200, 100);
            put_complete_building(workshop_b, 204, 100);

            // Oro de sobra, cobre de sobra, ESTAÑO a cero. Sin comida/madera/
            // piedra, todos los demás intentos mueren por no poder pagar y el
            // comercio (3000 bp) queda como única salida.
            g3->player_stock[1][0] = 0;
            g3->player_stock[1][1] = 0;
            g3->player_stock[1][2] = 0;
            g3->player_stock[1][oro] = 500;
            g3->player_stock[1][copper] = 30;
            g3->player_stock[1][tin] = 0;

            AiJobBox box3{}; ai_box_init(box3, 1);
            AiRuntimeV1 rt3{0u, 0u};
            bool trade_emitido = false;
            bool trade_compra = false;
            uint8_t trade_recurso = 0;
            for (uint32_t t = 0; t < 400u && !trade_emitido; ++t) {
                if (ai_should_dispatch(box3, g3->tick)) ai_dispatch(box3, g3->tick, rt3);
                if (box3.state == AiJobState::DISPATCHED) {
                    ai_execute(box3, *g3);
                    for (uint32_t k = 0; k < box3.result_count; ++k) {
                        const RawCommand& rc = box3.result[k];
                        if (rc.type != CommandType::TRADE) continue;
                        trade_emitido = true;
                        trade_compra = rc.p.hp > 0;
                        trade_recurso = static_cast<uint8_t>(rc.p.unit_id);
                    }
                    step(*g3, box3.result, box3.result_count);
                    rt3.decision_epoch += 1u;
                    rt3.ai_sequence += box3.result_count;
                    ai_box_init(box3, 1);
                } else {
                    step(*g3, nullptr, 0);
                }
            }
            if (!trade_emitido) {
                std::printf("La IA NO emitio TRADE con mercado completo, oro de "
                            "sobra y estaño a cero: el candidato del 1.35 no se dispara\n");
                ++g_fails;
            } else if (!trade_compra || trade_recurso != tin) {
                // Y cuando comercia, COMPRA el estaño que le falta, no vende
                // otra cosa. hp > 0 es la compra (SPEC-010); vender con una
                // receta bloqueada sería sabotearse.
                std::printf("La IA emitio TRADE pero no la compra de estaño "
                            "(hp=%d recurso=%u, esperado estaño=%u)\n",
                            static_cast<int>(trade_compra ? 1 : -1),
                            static_cast<unsigned>(trade_recurso),
                            static_cast<unsigned>(tin));
                ++g_fails;
            }
        }
    }

    if (g_fails == 0) {
        std::printf("ai_epoch_policy OK\n");
        return 0;
    }
    std::printf("ai_epoch_policy: %d fallo(s)\n", g_fails);
    return 1;
}
