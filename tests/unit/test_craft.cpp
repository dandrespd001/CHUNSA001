// Sprint 1.9 — recetas y CRAFT (SPEC-007 §12). Las once pruebas de §12.6.
//
// Fixture propio en memoria, mismo patrón que test_production_tech.cpp:
//
//   recipes[0] = "bronce": 3 de recurso 3 + 1 de recurso 4 -> 2 de recurso 5,
//                240 ticks, la ejecuta buildings[0].
//   recipes[1] = "cara":   9999 del recurso 3 -> 1 del recurso 5, para el
//                rechazo por stock.
//   buildings[0] = "fundicion" (nace completa, recipes=[0,1], epoch 1..15).
//   buildings[1] = "cuartel"   (nace completo, SIN recetas) — rechazo por
//                  "receta no listada en ESE edificio".
//   buildings[2] = "obra"      (constructible, build_time 10) — rechazo por
//                  edificio incompleto.
//
// NOTA: GameState SIEMPRE en heap — un GameState en pila segfaultea bajo ctest.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#include "chunsa/data_catalog.hpp"
#include "chunsa/game_state.hpp"
#include "chunsa/step.hpp"
#include "chunsa/checksum.hpp"
#include "chunsa/serialize.hpp"

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

namespace {

constexpr uint8_t R_COPPER = 3;
constexpr uint8_t R_TIN = 4;
constexpr uint8_t R_BRONZE = 5;

RecipeV1 g_recipes[2];
BuildingDefinitionV1 g_buildings[3];

DataCatalogV1 make_catalog() {
    std::memset(g_recipes, 0, sizeof(g_recipes));
    g_recipes[0].id = 0; g_recipes[0].building_id = 0;
    g_recipes[0].input[R_COPPER] = 3;
    g_recipes[0].input[R_TIN] = 1;
    g_recipes[0].output_index = R_BRONZE;
    g_recipes[0].output_amount = 2;
    g_recipes[0].duration_ticks = 240;

    g_recipes[1].id = 1; g_recipes[1].building_id = 0;
    g_recipes[1].input[R_COPPER] = 9999;
    g_recipes[1].output_index = R_BRONZE;
    g_recipes[1].output_amount = 1;
    g_recipes[1].duration_ticks = 10;

    std::memset(g_buildings, 0, sizeof(g_buildings));
    for (uint32_t b = 0; b < 3; ++b) {
        g_buildings[b].id = b;
        g_buildings[b].hp = 500;
        g_buildings[b].footprint_w = 2;
        g_buildings[b].footprint_h = 2;
        g_buildings[b].epoch_min = 1;
        g_buildings[b].epoch_max = 15;
        for (uint32_t k = 0; k < PROD_TRAINS_MAX; ++k) g_buildings[b].trains[k] = INVALID_UNIT_ID;
        for (uint32_t k = 0; k < PROD_TECHS_MAX; ++k) g_buildings[b].researches[k] = INVALID_TECH_ID;
        for (uint32_t k = 0; k < BUILDING_REQCAP_MAX; ++k) {
            g_buildings[b].required_capabilities[k] = INVALID_CAPABILITY_ID;
        }
        for (uint32_t k = 0; k < RECIPES_PER_BUILDING_MAX; ++k) {
            g_buildings[b].recipes[k] = INVALID_RECIPE_ID;
        }
    }
    g_buildings[0].recipes[0] = 0;
    g_buildings[0].recipes[1] = 1;
    g_buildings[0].recipe_count = 2;
    g_buildings[2].build_time_ticks = 10;
    g_buildings[2].constructible = 1;

    DataCatalogV1 c{};
    c.building_count = 3; c.buildings = g_buildings;
    c.recipe_count = 2; c.recipes = g_recipes;
    return c;
}

MatchConfig01A make_cfg() {
    MatchConfig01A cfg{};
    cfg.max_entities = 256;
    cfg.player_count = 2;
    cfg.human_input_delay_ticks = 0;
    cfg.max_future_command_ticks = 20;
    cfg.checksum_every_ticks = 1;
    cfg.map_tiles_x = 256; cfg.map_tiles_y = 256;
    cfg.seed = 20260730ull;
    cfg.allow_debug_stat_payload = 0;
    return cfg;
}

// Coloca un edificio ya existente en el estado, sin pasar por PLACE_BUILDING.
uint32_t spawn_building(GameState& g, BuildingId bid, uint8_t owner, uint32_t progress) {
    const EntityHandle h = et_spawn(g.entities);
    const uint32_t i = h.index;
    zero_components(g, i);
    g.owner[i] = owner;
    g.entity_kind[i] = 1u;
    g.building_id[i] = bid;
    g.build_progress[i] = progress;
    g.hp[i] = g.catalog->buildings[bid].hp;
    g.max_hp[i] = g.hp[i];
    g.pos_x[i] = 100 * FX_ONE_RAW;
    g.pos_y[i] = 100 * FX_ONE_RAW;
    return i;
}

RawCommand craft_cmd(uint32_t tick, uint16_t emitter, uint64_t seq,
                     const GameState& g, uint32_t slot, RecipeId rid) {
    RawCommand c{};
    std::memset(&c, 0, sizeof(c));
    c.target_tick = tick;
    c.emitter = static_cast<uint8_t>(emitter);
    c.sequence = seq;
    c.type = CommandType::CRAFT;
    c.p.handle = EntityHandle{slot, g.entities.generation[slot]};
    c.p.unit_id = rid;
    return c;
}

RejectReason submit(GameState& g, const RawCommand& c) {
    RawCommand batch[1] = {c};
    step(g, batch, 1);
    const ReceiptMailbox& m = g.mailbox[c.emitter];
    if (m.count == 0) return RejectReason::MALFORMED;
    return m.ring[(m.head + m.count - 1) % MAILBOX_CAP].result;
}

std::unique_ptr<GameState> fresh(const DataCatalogV1& cat) {
    auto g = std::make_unique<GameState>();
    gs_init(*g, make_cfg());
    g->catalog = &cat;
    // gs_init deja player_epoch en 0 ("sin epoca asignada"); el gate 6 de
    // §12.4 compara contra epoch_min=1, asi que sin esto TODO se rechazaria
    // por epoca y las demas pruebas no probarian nada.
    for (uint32_t p2 = 0; p2 < 8u; ++p2) g->player_epoch[p2] = 4u;
    return g;
}

}  // namespace

int main() {
    static DataCatalogV1 cat = make_catalog();

    // 1) CRAFT con inputs suficientes es ACEPTADO y deduce TODOS los inputs en
    //    el mismo tick de aceptacion (deduccion por adelantado, §12.4).
    {
        auto g = fresh(cat);
        const uint32_t b = spawn_building(*g, 0, 0, 0);
        g->player_stock[0][R_COPPER] = 10;
        g->player_stock[0][R_TIN] = 5;
        CHECK(submit(*g, craft_cmd(0, 0, 1, *g, b, 0)) == RejectReason::ACCEPTED);
        CHECK(g->player_stock[0][R_COPPER] == 7);
        CHECK(g->player_stock[0][R_TIN] == 4);
        CHECK(g->craft_recipe[b] == 0u);
    }

    // 2) Un input insuficiente ⇒ ILLEGAL_STATE y NO deduce NADA. Una deduccion
    //    parcial seria un robo silencioso al jugador.
    {
        auto g = fresh(cat);
        const uint32_t b = spawn_building(*g, 0, 0, 0);
        g->player_stock[0][R_COPPER] = 10;
        g->player_stock[0][R_TIN] = 0;          // falta SOLO el estano
        CHECK(submit(*g, craft_cmd(0, 0, 1, *g, b, 0)) == RejectReason::ILLEGAL_STATE);
        CHECK(g->player_stock[0][R_COPPER] == 10);
        CHECK(g->player_stock[0][R_TIN] == 0);
        CHECK(g->craft_recipe[b] == INVALID_RECIPE_ID);
    }

    // 3) Al completarse, la salida sube exactamente output_amount.
    {
        auto g = fresh(cat);
        const uint32_t b = spawn_building(*g, 0, 0, 0);
        g->player_stock[0][R_COPPER] = 3;
        g->player_stock[0][R_TIN] = 1;
        CHECK(submit(*g, craft_cmd(0, 0, 1, *g, b, 0)) == RejectReason::ACCEPTED);
        for (uint32_t t = 0; t < 240u; ++t) step(*g, nullptr, 0);
        CHECK(g->player_stock[0][R_BRONZE] == 2);
        CHECK(g->craft_recipe[b] == INVALID_RECIPE_ID);   // vuelve a ocioso
    }

    // 4) Edificio NO completo ⇒ ILLEGAL_STATE.
    {
        auto g = fresh(cat);
        const uint32_t b = spawn_building(*g, 2, 0, 0);   // build_time 10, progreso 0
        g->player_stock[0][R_COPPER] = 10;
        g->player_stock[0][R_TIN] = 10;
        CHECK(submit(*g, craft_cmd(0, 0, 1, *g, b, 0)) == RejectReason::ILLEGAL_STATE);
    }

    // 5) Receta NO listada en ESE edificio ⇒ ILLEGAL_STATE.
    {
        auto g = fresh(cat);
        const uint32_t b = spawn_building(*g, 1, 0, 0);   // cuartel, sin recetas
        g->player_stock[0][R_COPPER] = 10;
        g->player_stock[0][R_TIN] = 10;
        CHECK(submit(*g, craft_cmd(0, 0, 1, *g, b, 0)) == RejectReason::ILLEGAL_STATE);
    }

    // 6) Edificio de OTRO jugador ⇒ no es propio.
    {
        auto g = fresh(cat);
        const uint32_t b = spawn_building(*g, 0, 1, 0);   // owner 1
        g->player_stock[0][R_COPPER] = 10;
        g->player_stock[0][R_TIN] = 10;
        CHECK(submit(*g, craft_cmd(0, 0, 1, *g, b, 0)) != RejectReason::ACCEPTED);
    }

    // 7) Edificio ya ocupado fabricando ⇒ ILLEGAL_STATE.
    {
        auto g = fresh(cat);
        const uint32_t b = spawn_building(*g, 0, 0, 0);
        g->player_stock[0][R_COPPER] = 20;
        g->player_stock[0][R_TIN] = 20;
        CHECK(submit(*g, craft_cmd(0, 0, 1, *g, b, 0)) == RejectReason::ACCEPTED);
        CHECK(submit(*g, craft_cmd(0, 0, 2, *g, b, 0)) == RejectReason::ILLEGAL_STATE);
    }

    // 8) ORDEN de rechazos: un comando que viola varias reglas devuelve el
    //    codigo de la PRIMERA. Edificio incompleto Y sin stock ⇒ manda el
    //    estado del edificio, no el stock.
    {
        auto g = fresh(cat);
        const uint32_t b = spawn_building(*g, 2, 0, 0);   // incompleto
        g->player_stock[0][R_COPPER] = 0;                 // y sin stock
        CHECK(submit(*g, craft_cmd(0, 0, 1, *g, b, 0)) == RejectReason::ILLEGAL_STATE);
        CHECK(g->player_stock[0][R_COPPER] == 0);         // nada deducido
    }

    // 9) Save/load conserva craft_recipe y craft_progress a MITAD de produccion.
    {
        auto g = fresh(cat);
        const uint32_t b = spawn_building(*g, 0, 0, 0);
        g->player_stock[0][R_COPPER] = 3;
        g->player_stock[0][R_TIN] = 1;
        CHECK(submit(*g, craft_cmd(0, 0, 1, *g, b, 0)) == RejectReason::ACCEPTED);
        for (uint32_t t = 0; t < 100u; ++t) step(*g, nullptr, 0);
        CHECK(g->craft_recipe[b] == 0u);
        CHECK(g->craft_progress[b] > 0u);

        std::vector<uint8_t> buf(GS_SERIALIZE_MAX);
        const size_t len = gs_serialize(*g, buf.data(), buf.size());
        CHECK(len > 0);
        auto g2 = std::make_unique<GameState>();
        CHECK(gs_deserialize(*g2, buf.data(), len));
        CHECK(g2->craft_recipe[b] == g->craft_recipe[b]);
        CHECK(g2->craft_progress[b] == g->craft_progress[b]);
    }

    // 10) Mutar craft_recipe CAMBIA el checksum: prueba de pertenencia al
    //     dominio. Sin esto, la fabricacion quedaria fuera del determinismo.
    {
        auto g = fresh(cat);
        const uint32_t b = spawn_building(*g, 0, 0, 0);
        const uint64_t before = state_checksum_v1(*g);
        g->craft_recipe[b] = 0u;
        const uint64_t after_recipe = state_checksum_v1(*g);
        CHECK(before != after_recipe);
        g->craft_progress[b] = 42u;
        CHECK(after_recipe != state_checksum_v1(*g));
    }

    // 11) Destruir el edificio a mitad NO acredita la salida NI devuelve los
    //     inputs. Fabricar es un riesgo, no un deposito a plazo.
    {
        auto g = fresh(cat);
        const uint32_t b = spawn_building(*g, 0, 0, 0);
        g->player_stock[0][R_COPPER] = 3;
        g->player_stock[0][R_TIN] = 1;
        CHECK(submit(*g, craft_cmd(0, 0, 1, *g, b, 0)) == RejectReason::ACCEPTED);
        for (uint32_t t = 0; t < 100u; ++t) step(*g, nullptr, 0);
        et_mark_dead(g->entities, b);
        for (uint32_t t = 0; t < 300u; ++t) step(*g, nullptr, 0);
        CHECK(g->player_stock[0][R_BRONZE] == 0);
        CHECK(g->player_stock[0][R_COPPER] == 0);
        CHECK(g->player_stock[0][R_TIN] == 0);
    }

    // Sprint 1.9B: la cadena del hierro llega DESDE EL BLOB REAL. Un fixture
    // en memoria no prueba que el dato haya viajado; esto si.
    {
        DataCatalogStorageV1 store;
        const auto code = catalog_load_file_v1(CHUNSA_GOLDEN_CHDB_PATH,
                                               CatalogLoadProfile::Verified, store);
        CHECK(code == CatalogLoadCode::Ok);
        if (code == CatalogLoadCode::Ok && store.valid()) {
            const DataCatalogV1& rc = store.catalog();
            const BuildingId f = catalog_find_building(rc, "rome:foundry",
                                                       std::strlen("rome:foundry"));
            CHECK(f != INVALID_BUILDING_ID);
            if (f != INVALID_BUILDING_ID) {
                const BuildingDefinitionV1& fd = rc.buildings[f];
                // Bronce y hierro forjado: dos recetas en la misma fundicion.
                // Sprint 1.9B: bronce, carboneo y hierro forjado — la cadena real
                // madera -> carbon vegetal -> hierro, ya con profundidad 5.
                CHECK(fd.recipe_count == 3);
                bool halla_hierro = false;
                for (uint8_t k = 0; k < fd.recipe_count; ++k) {
                    const RecipeV1& r = rc.recipes[fd.recipes[k]];
                    // Toda receta debe tener entradas y salida util: una receta
                    // sin entradas seria una fabrica de recursos de la nada.
                    int32_t total_in = 0;
                    for (uint32_t x = 0; x < RESOURCE_COUNT; ++x) total_in += r.input[x];
                    CHECK(total_in > 0);
                    CHECK(r.output_amount >= 1);
                    CHECK(r.duration_ticks >= 1);
                    if (r.duration_ticks == 300u) halla_hierro = true;
                }
                CHECK(halla_hierro);
            }
        }
    }

    // GUARDIAN: toda entrada de receta debe ser OBTENIBLE.
    //
    // Este patron ya ha mordido TRES VECES —estaño en el 1.9, mena de hierro en
    // el 1.9B, caliza en el 1.9C—: se anade una receta y sus entradas no
    // existen en el mapa, asi que el producto es infabricable y nadie se entera
    // hasta que alguien intenta jugarlo. Tres veces es un patron, no un
    // descuido, y se cierra con estructura en vez de con cuidado.
    //
    // Un recurso es obtenible si esta en el mapa (recolectado) o si alguna
    // receta lo produce. Lo segundo permite las cadenas de hasta 5 pasos que
    // SPEC-007 §12.1 autoriza desde el 2026-07-31.
    {
        DataCatalogStorageV1 store;
        const auto code = catalog_load_file_v1(CHUNSA_GOLDEN_CHDB_PATH,
                                               CatalogLoadProfile::Verified, store);
        CHECK(code == CatalogLoadCode::Ok);
        if (code == CatalogLoadCode::Ok && store.valid()) {
            const DataCatalogV1& rc = store.catalog();
            bool obtenible[RESOURCE_COUNT] = {};
            for (uint32_t k = 0; k < rc.map_resource_spawn_count; ++k) {
                const uint8_t idx = rc.map_resource_spawns[k].resource_idx;
                if (idx < RESOURCE_COUNT) obtenible[idx] = true;
            }
            for (uint32_t k = 0; k < rc.recipe_count; ++k) {
                const uint8_t out = rc.recipes[k].output_index;
                if (out < RESOURCE_COUNT) obtenible[out] = true;
            }
            for (uint32_t k = 0; k < rc.recipe_count; ++k) {
                for (uint32_t r = 0; r < RESOURCE_COUNT; ++r) {
                    if (rc.recipes[k].input[r] <= 0) continue;
                    if (!obtenible[r]) {
                        std::printf("RECETA %u pide el recurso %u y NO es obtenible: "
                                    "no esta en el mapa y ninguna receta lo produce\n",
                                    k, r);
                        ++g_fails;
                    }
                }
            }
        }
    }

    if (g_fails == 0) {
        std::printf("craft OK\n");
        return 0;
    }
    std::printf("craft: %d fallo(s)\n", g_fails);
    return 1;
}
