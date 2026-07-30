// Sprint 1.18 fase 4 — efectos de tecnología sobre estadísticas (SPEC-004 §28).
//
// La regla de fondo: los efectos se aplican al VALOR EFECTIVO en el momento del
// cálculo, NUNCA mutando la definición del catálogo. La definición es inmutable
// y compartida entre jugadores; mutarla haría que la partida dependiera del
// orden de carga y del orden en que cada jugador investiga.

#include <cstdint>
#include <cstdio>

#include "chunsa/combat_damage.hpp"
#include "chunsa/data_catalog.hpp"
#include <cstring>

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

namespace {
constexpr uint8_t INFANTRY = 0, CAVALRY = 1, ARTILLERY = 2;
inline uint8_t mask_of(uint8_t cls) { return static_cast<uint8_t>(1u << cls); }
}  // namespace

int main() {
    // Sin efectos: cero.
    CHECK(tech_stat_bonus(nullptr, 0, StatEffectV1::Attack, INFANTRY) == 0);

    TechEffectV1 fx[4];
    fx[0] = {StatEffectV1::Attack,      2, mask_of(INFANTRY)};
    fx[1] = {StatEffectV1::ArmorCut,    1, static_cast<uint8_t>(mask_of(INFANTRY) | mask_of(CAVALRY))};
    fx[2] = {StatEffectV1::Attack,      3, mask_of(CAVALRY)};
    fx[3] = {StatEffectV1::ArmorPierce, 5, mask_of(ARTILLERY)};

    // Suma solo lo que corresponde a ESA estadística y ESA clase.
    CHECK(tech_stat_bonus(fx, 4, StatEffectV1::Attack, INFANTRY) == 2);
    CHECK(tech_stat_bonus(fx, 4, StatEffectV1::Attack, CAVALRY) == 3);
    CHECK(tech_stat_bonus(fx, 4, StatEffectV1::Attack, ARTILLERY) == 0);

    // Un efecto que cubre VARIAS clases las alcanza a todas.
    CHECK(tech_stat_bonus(fx, 4, StatEffectV1::ArmorCut, INFANTRY) == 1);
    CHECK(tech_stat_bonus(fx, 4, StatEffectV1::ArmorCut, CAVALRY) == 1);
    CHECK(tech_stat_bonus(fx, 4, StatEffectV1::ArmorCut, ARTILLERY) == 0);

    // Las estadísticas NO se mezclan entre sí: la armadura de perforación no
    // suma a la de corte aunque la clase coincida.
    CHECK(tech_stat_bonus(fx, 4, StatEffectV1::ArmorPierce, ARTILLERY) == 5);
    CHECK(tech_stat_bonus(fx, 4, StatEffectV1::ArmorImpact, ARTILLERY) == 0);

    // Varios efectos de la MISMA estadística y clase se ACUMULAN: es lo que
    // permite encadenar mejoras de herrería sin que una pise a la otra.
    TechEffectV1 stack[2];
    stack[0] = {StatEffectV1::Attack, 2, mask_of(INFANTRY)};
    stack[1] = {StatEffectV1::Attack, 3, mask_of(INFANTRY)};
    CHECK(tech_stat_bonus(stack, 2, StatEffectV1::Attack, INFANTRY) == 5);

    // Máscara vacía: un efecto que no aplica a nadie no hace nada. Es un dato
    // legítimo (una tech en preparación) y no debe sumar por accidente.
    TechEffectV1 nobody[1] = {{StatEffectV1::Attack, 99, 0}};
    CHECK(tech_stat_bonus(nobody, 1, StatEffectV1::Attack, INFANTRY) == 0);

    // Efectos negativos: una tecnología puede tener contrapartida.
    TechEffectV1 tradeoff[1] = {{StatEffectV1::ArmorCut, -1, mask_of(CAVALRY)}};
    CHECK(tech_stat_bonus(tradeoff, 1, StatEffectV1::ArmorCut, CAVALRY) == -1);

    // Y que los efectos LLEGAN DESDE EL BLOB REAL, no solo desde un fixture en
    // memoria: un parser que se olvide del campo pasaria todas las pruebas de
    // arriba y no serviria para nada en la partida.
    {
        DataCatalogStorageV1 store;
        const auto code = catalog_load_file_v1(CHUNSA_GOLDEN_CHDB_PATH,
                                               CatalogLoadProfile::Verified, store);
        CHECK(code == CatalogLoadCode::Ok);
        if (code == CatalogLoadCode::Ok && store.valid()) {
            const DataCatalogV1& cat = store.catalog();
            const TechId drill = catalog_find_tech(cat, "rome:marching_drill",
                                                   std::strlen("rome:marching_drill"));
            CHECK(drill != INVALID_TECH_ID);
            if (drill != INVALID_TECH_ID) {
                const TechDefinitionV1& td = cat.techs[drill];
                CHECK(td.stat_effect_count == 2);
                CHECK(tech_stat_bonus(td.stat_effects, td.stat_effect_count,
                                      StatEffectV1::ArmorCut, INFANTRY) == 1);
                CHECK(tech_stat_bonus(td.stat_effects, td.stat_effect_count,
                                      StatEffectV1::ArmorPierce, INFANTRY) == 1);
                // No alcanza a la caballería: la instrucción de marcha es de
                // infantería y el `applies_to` debe respetarse.
                CHECK(tech_stat_bonus(td.stat_effects, td.stat_effect_count,
                                      StatEffectV1::ArmorCut, CAVALRY) == 0);
            }
        }
    }

    if (g_fails == 0) {
        std::printf("tech_effects OK\n");
        return 0;
    }
    std::printf("tech_effects: %d fallo(s)\n", g_fails);
    return 1;
}
