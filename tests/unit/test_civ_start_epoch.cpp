// Guardián de la época inicial por civilización (Sprint 1.9).
//
// POR QUÉ EXISTE. Al añadir las fundiciones del 1.9 puse la egipcia y la romana
// con `epoch_window: [4, 6]`. La romana entra en el catálogo de Roma, y la
// época inicial de un jugador es el MÍNIMO `epoch_min` de los datos de SU
// civilización (`gs_init_epoch_from_catalog_per_player`). Resultado: Roma
// empezaba en la época 4 en vez de la 5, sus unidades y edificios de época 5
// quedaban fuera de alcance, y la apertura DEJÓ DE TERMINAR — el escenario
// pasó de 9438 ticks a agotar el tope de 36000 sin vencedor.
//
// El síntoma no señalaba a la causa por ninguna parte: un fichero de datos
// nuevo, en otra civilización, apagó la partida. Costó tres experimentos de
// bisección encontrarlo.
//
// Esto empeora con el catálogo: el plan son 4–6 civilizaciones para la 1.0 y
// 15 épocas. Cada dato nuevo puede mover en silencio la época inicial de su
// civilización. Este guardián convierte ese acoplamiento invisible en un fallo
// ruidoso: si añades datos que cambian la época de arranque de una civ, ESTA
// prueba te lo dice, en vez de que se caiga la IA a 36000 ticks de distancia.
//
// Si el cambio es INTENCIONADO, se actualiza el valor esperado aquí y se dice
// por qué en el commit. Lo que no puede pasar es que ocurra sin que nadie lo
// note.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>

#include "chunsa/data_catalog.hpp"
#include "chunsa/game_state.hpp"

#ifndef CHUNSA_GOLDEN_CHDB_PATH
#error "CHUNSA_GOLDEN_CHDB_PATH debe definirse via CMake"
#endif

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

namespace {

// Época de arranque esperada por civilización, por record_id.
struct Expected {
    const char* civ_record_id;
    uint8_t     start_epoch;
};

// CAMBIO INTENCIONADO — Sprint 1.22, 2026-07-31. Antes: Egipto 3, Roma 5.
// Ahora AMBAS arrancan en la 1, y este guardián lo detectó en cuanto ocurrió,
// que es exactamente para lo que se escribió.
//
// El motivo es que las épocas 1 (Paleolítica) y 2 (Neolítica) existían en la
// escala de SPEC-007 §3 y NO LAS JUGABA NADIE: Egipto abría en la 3, Roma en
// la 5, y Roma además no tenía ningún dato en las épocas 1-4. Siete de las
// diez casillas civilización×época del rango 1-5 estaban muertas.
//
// El 1.22 les da contenido —campamentos qadan y epigravetiense, talleres
// líticos, almacenes neolíticos, aldea Remedello, taller Terramare— y amplía
// a [1,5] la ventana de los obreros y los centros. Al bajar el `epoch_min`
// de esos datos a 1, la época inicial baja con ellos: es el mismo mecanismo
// que en el 1.9 nos costó tres bisecciones, sólo que esta vez es lo que se
// quería y está escrito.
//
// La partida empieza ahora en el Paleolítico, no en la Edad del Bronce.
constexpr Expected kExpected[] = {
    {"egipto:dynastic_nile", 1},
    {"rome:republic_imperial", 1},
};

}  // namespace

int main() {
    DataCatalogStorageV1 store;
    const auto code = catalog_load_file_v1(CHUNSA_GOLDEN_CHDB_PATH,
                                           CatalogLoadProfile::Verified, store);
    CHECK(code == CatalogLoadCode::Ok);
    if (code != CatalogLoadCode::Ok || !store.valid()) {
        std::printf("civ_start_epoch: no se pudo cargar el catálogo\n");
        return 1;
    }
    const DataCatalogV1& cat = store.catalog();

    // Toda civilización del catálogo debe estar cubierta por el guardián: si
    // se añade una civ nueva y nadie declara aquí su época, el guardián no
    // serviria de nada justo cuando mas falta hace.
    CHECK(cat.civ_count == sizeof(kExpected) / sizeof(kExpected[0]));

    for (const Expected& e : kExpected) {
        const CivId civ = catalog_find_civ(cat, e.civ_record_id,
                                           static_cast<uint16_t>(std::strlen(e.civ_record_id)));
        CHECK(civ != INVALID_CIV_ID);
        if (civ == INVALID_CIV_ID) continue;

        auto g = std::make_unique<GameState>();
        MatchConfig01A cfg{};
        cfg.max_entities = 64;
        cfg.player_count = 2;
        cfg.human_input_delay_ticks = 0;
        cfg.max_future_command_ticks = 20;
        cfg.checksum_every_ticks = 1;
        cfg.map_tiles_x = 256; cfg.map_tiles_y = 256;
        cfg.seed = 1ull;
        cfg.allow_debug_stat_payload = 0;
        gs_init(*g, cfg);
        gs_bind_catalog(*g, cat);
        gs_set_player_civ(*g, 0, civ);
        gs_init_epoch_from_catalog_per_player(*g);

        if (g->player_epoch[0] != e.start_epoch) {
            std::printf("CIV %s: época inicial %u, esperada %u — "
                        "algún dato de esta civilización tiene un epoch_min por "
                        "debajo de su ventana\n",
                        e.civ_record_id, static_cast<unsigned>(g->player_epoch[0]),
                        static_cast<unsigned>(e.start_epoch));
            ++g_fails;
        }
    }

    if (g_fails == 0) {
        std::printf("civ_start_epoch OK\n");
        return 0;
    }
    std::printf("civ_start_epoch: %d fallo(s)\n", g_fails);
    return 1;
}
