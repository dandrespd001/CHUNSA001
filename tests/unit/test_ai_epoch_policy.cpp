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

    if (g_fails == 0) {
        std::printf("ai_epoch_policy OK\n");
        return 0;
    }
    std::printf("ai_epoch_policy: %d fallo(s)\n", g_fails);
    return 1;
}
