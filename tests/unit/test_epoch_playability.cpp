// Guardián de JUGABILIDAD POR ÉPOCA (Sprint 1.22).
//
// POR QUÉ EXISTE. «La época 2 existe» era falso y nadie lo detectaba, porque
// nada lo comprobaba. Las épocas 1 y 2 estaban declaradas en la escala de 15,
// tenían recursos asignados (comida, madera, piedra, lana en la 1; arcilla,
// lino, paño en la 2) y NINGUNA civilización podía jugarlas: Egipto abría en
// la 3 y Roma en la 5. Eran números en una tabla, no juego.
//
// QUÉ SIGNIFICA «JUGABLE», Y POR QUÉ ESTA DEFINICIÓN Y NO OTRA. No es una
// opinión de diseño: sale del kernel. `ADVANCE_EPOCH` (step.hpp) exige
// **≥2 edificios COMPLETOS propios cuya ventana incluya la época ACTUAL**
// antes de dejar subir. Una civilización en una época donde sólo tiene un
// edificio válido queda ATRAPADA — no por una regla de contenido, sino por la
// puerta del kernel. Y sin una unidad trabajadora no puede construir ninguno,
// así que tampoco llega a los dos.
//
// De ahí los tres requisitos:
//   (a) ≥2 edificios cuya ventana incluya la época — el gate literal;
//   (b) ≥1 unidad de clase Citizen que la incluya, o no puede construir nada;
//   (c) ≥1 de esos edificios CONSTRUIBLE.
//
// El (c) merece explicación porque NO sale del gate. Los centros de
// asentamiento son `constructible: false`: los coloca el escenario al arrancar
// y el kernel SÍ los cuenta. Una época con dos edificios no construibles
// pasaría el gate del kernel y aun así sería una época muerta — el jugador no
// tendría nada que hacer en ella. El (c) exige que quede al menos una decisión
// de construcción en manos de quien juega.
//
// LO QUE ESTA PRUEBA NO DICE. Que la época sea divertida, esté equilibrada o
// tenga variedad militar. Comprueba que NO ESTÁ MUERTA. Es un suelo, no un
// techo, y conviene no leer de más en un verde.
//
// AL AÑADIR UNA CIV NUEVA hay que declararla aquí abajo; el recuento contra
// `civ_count` hace que olvidarlo falle en vez de pasar en silencio, igual que
// en test_civ_start_epoch.

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "chunsa/data_catalog.hpp"

#ifndef CHUNSA_GOLDEN_CHDB_PATH
#error "CHUNSA_GOLDEN_CHDB_PATH debe definirse via CMake"
#endif

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

namespace {

// Rango que el Director pidió dejar COMPLETO. El 1.22 lo dejó en 5; el 1.25 lo
// sube a 15, que es la escala entera de SPEC-007 §2 y el encargo literal: dos
// civilizaciones jugables de la época 1 a la 15.
//
// Subir esta constante es lo que convierte «época declarada» en «época
// jugable»: mientras valía 5, las épocas 6-15 podían estar vacías sin que nada
// protestara. Ahora cada casilla civilización×época tiene que sostenerse.
constexpr uint8_t kEpochFirst = 1;
constexpr uint8_t kEpochLast  = 15;

struct Civ {
    const char* record_id;
};

constexpr Civ kCivs[] = {
    {"egipto:dynastic_nile"},
    {"rome:republic_imperial"},
};

bool covers(uint8_t emin, uint8_t emax, uint8_t e) noexcept {
    return emin <= e && e <= emax;
}

}  // namespace

int main() {
    DataCatalogStorageV1 store;
    const auto code = catalog_load_file_v1(CHUNSA_GOLDEN_CHDB_PATH,
                                           CatalogLoadProfile::Verified, store);
    CHECK(code == CatalogLoadCode::Ok);
    if (code != CatalogLoadCode::Ok || !store.valid()) {
        std::printf("epoch_playability: no se pudo cargar el catálogo\n");
        return 1;
    }
    const DataCatalogV1& cat = store.catalog();

    CHECK(cat.civ_count == sizeof(kCivs) / sizeof(kCivs[0]));

    for (const Civ& c : kCivs) {
        const CivId civ = catalog_find_civ(cat, c.record_id,
                                           static_cast<uint16_t>(std::strlen(c.record_id)));
        CHECK(civ != INVALID_CIV_ID);
        if (civ == INVALID_CIV_ID) continue;

        for (uint8_t e = kEpochFirst; e <= kEpochLast; ++e) {
            uint32_t buildings = 0;      // (a) los que cuenta el kernel
            uint32_t constructible = 0;  // (c) los que el jugador puede levantar
            for (uint32_t b = 0; b < cat.building_count; ++b) {
                const BuildingDefinitionV1& d = cat.buildings[b];
                if (d.civ_id != civ) continue;
                if (!covers(d.epoch_min, d.epoch_max, e)) continue;
                ++buildings;
                if (d.constructible != 0) ++constructible;
            }

            uint32_t workers = 0;
            for (uint32_t u = 0; u < cat.unit_count; ++u) {
                const UnitDefinitionV1& d = cat.units[u];
                if (d.civ_id != civ) continue;
                if (!covers(d.epoch_min, d.epoch_max, e)) continue;
                if (d.unit_class != UnitClassV1::Citizen) continue;
                ++workers;
            }

            // El «2» no es un número redondo elegido a ojo: es literalmente el
            // `count_ok < 2u` de ADVANCE_EPOCH. Si esa puerta cambiara, este
            // guardián debe cambiar con ella o dejaría de guardar nada.
            if (buildings < 2) {
                std::printf("CIV %s epoca %u: %u edificio(s) construible(s) — "
                            "ADVANCE_EPOCH exige 2, la civilizacion queda ATRAPADA\n",
                            c.record_id, static_cast<unsigned>(e), buildings);
                ++g_fails;
            }
            if (constructible < 1) {
                std::printf("CIV %s epoca %u: ningun edificio CONSTRUIBLE — la "
                            "epoca pasaria el gate del kernel y aun asi no habria "
                            "nada que decidir en ella\n",
                            c.record_id, static_cast<unsigned>(e));
                ++g_fails;
            }
            if (workers < 1) {
                std::printf("CIV %s epoca %u: sin unidad Citizen — no puede "
                            "construir, asi que tampoco llega a los 2 edificios\n",
                            c.record_id, static_cast<unsigned>(e));
                ++g_fails;
            }
        }
    }

    if (g_fails == 0) {
        std::printf("epoch_playability OK\n");
        return 0;
    }
    std::printf("epoch_playability: %d fallo(s)\n", g_fails);
    return 1;
}
