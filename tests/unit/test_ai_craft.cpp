// Sprint 1.19 — la IA se pone al día (auditoría del 2026-07-30).
//
// POR QUÉ EXISTE ESTE SPRINT. La auditoría midió `ai_stub.hpp` y encontró
// **cero** ocurrencias de `CRAFT`, recetas, armadura y efectos de tecnología.
// El oponente nunca fabricaría bronce ni entendería un contador: toda la
// profundidad de los sprints 1.9 y 1.18 era, en la práctica, **exclusiva del
// jugador humano**. Un adversario que juega a un juego más pobre no es un
// adversario difícil: es uno que se supera por acumulación.
//
// Fase A: que la IA FABRIQUE. Que ataque con órdenes y que elija mirando
// contadores va en fases siguientes.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>

#include "chunsa/data_catalog.hpp"
#include "chunsa/game_state.hpp"
#include "chunsa/step.hpp"
#include "chunsa/ai_stub.hpp"

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

namespace {

constexpr uint8_t R_COPPER = 3, R_TIN = 4, R_BRONZE = 5;

RecipeV1 g_recipes[1];
BuildingDefinitionV1 g_buildings[1];
AiProfileV1 g_profiles[1];

DataCatalogV1 make_catalog() {
    std::memset(g_recipes, 0, sizeof(g_recipes));
    g_recipes[0].id = 0; g_recipes[0].building_id = 0;
    g_recipes[0].input[R_COPPER] = 3;
    g_recipes[0].input[R_TIN] = 1;
    g_recipes[0].output_index = R_BRONZE;
    g_recipes[0].output_amount = 2;
    g_recipes[0].duration_ticks = 10;

    std::memset(g_buildings, 0, sizeof(g_buildings));
    BuildingDefinitionV1& b = g_buildings[0];
    b.id = 0; b.hp = 500; b.footprint_w = 2; b.footprint_h = 2;
    b.epoch_min = 1; b.epoch_max = 15;
    for (uint32_t k = 0; k < PROD_TRAINS_MAX; ++k) b.trains[k] = INVALID_UNIT_ID;
    for (uint32_t k = 0; k < PROD_TECHS_MAX; ++k) b.researches[k] = INVALID_TECH_ID;
    for (uint32_t k = 0; k < BUILDING_REQCAP_MAX; ++k) {
        b.required_capabilities[k] = INVALID_CAPABILITY_ID;
    }
    for (uint32_t k = 0; k < RECIPES_PER_BUILDING_MAX; ++k) b.recipes[k] = INVALID_RECIPE_ID;
    b.recipes[0] = 0; b.recipe_count = 1;
    b.civ_id = INVALID_CIV_ID;

    std::memset(g_profiles, 0, sizeof(g_profiles));
    g_profiles[0].economy_focus_bp = 6000;
    g_profiles[0].military_focus_bp = 3000;
    g_profiles[0].tech_focus_bp = 2000;
    g_profiles[0].decision_period_ticks = 1;
    g_profiles[0].reaction_latency_ticks = 0;

    DataCatalogV1 c{};
    c.building_count = 1; c.buildings = g_buildings;
    c.recipe_count = 1;   c.recipes = g_recipes;
    // Sin tabla de NOMBRES de perfil, catalog_find_ai_profile desreferencia
    // nullptr. Se deja el catalogo sin perfiles y la IA cae en
    // AI_DEFAULT_PROFILE_V1, que es lo que este test quiere ejercitar.
    c.ai_profile_count = 0; c.ai_profiles = nullptr;
    (void)g_profiles;
    return c;
}

MatchConfig01A cfg_of() {
    MatchConfig01A c{};
    c.max_entities = 64;
    c.player_count = 2;
    c.human_input_delay_ticks = 0;
    c.max_future_command_ticks = 20;
    c.checksum_every_ticks = 1;
    c.map_tiles_x = 256; c.map_tiles_y = 256;
    c.seed = 20260731ull;
    c.allow_debug_stat_payload = 0;
    return c;
}

uint32_t put_foundry(GameState& g, uint8_t owner) {
    const EntityHandle h = et_spawn(g.entities);
    const uint32_t i = h.index;
    zero_components(g, i);
    g.owner[i] = owner;
    g.entity_kind[i] = 1u;
    g.building_id[i] = 0u;
    g.build_progress[i] = 0;   // build_time 0 => nace completa
    g.hp[i] = 500; g.max_hp[i] = 500;
    g.pos_x[i] = 40 * FX_ONE_RAW;
    g.pos_y[i] = 40 * FX_ONE_RAW;
    g.bld_anchor_tx[i] = 40; g.bld_anchor_ty[i] = 40;
    return i;
}

// Ejecuta la decisión de la IA una vez y devuelve cuántos comandos CRAFT emitió.
uint32_t craft_commands_emitted(GameState& g, uint8_t ai_player) {
    // ai_box_init deja la caja en el estado que ai_execute espera; construirla
    // a mano dejaba runtime_before sin inicializar y segfaulteaba.
    AiJobBox box;
    ai_box_init(box, ai_player);
    AiRuntimeV1 rt{0, 0};
    ai_dispatch(box, g.tick, rt);
    ai_execute(box, g);
    uint32_t n = 0;
    for (uint32_t k = 0; k < box.result_count; ++k) {
        if (box.result[k].type == CommandType::CRAFT) ++n;
    }
    return n;
}

}  // namespace

int main() {
    static DataCatalogV1 cat = make_catalog();

    // 1) Con fundición propia completa y materiales de sobra, la IA EMITE CRAFT.
    //    Hoy no lo hace: es el hallazgo de la auditoría.
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, cfg_of());
        g->catalog = &cat;
        for (uint32_t p = 0; p < 8u; ++p) g->player_epoch[p] = 4u;
        put_foundry(*g, 1);
        g->player_stock[1][R_COPPER] = 30;
        g->player_stock[1][R_TIN] = 10;
        CHECK(craft_commands_emitted(*g, 1) > 0u);
    }

    // 2) SIN materiales no lo intenta: emitir un comando que el kernel va a
    //    rechazar es ruido en el mailbox y una decisión mal tomada.
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, cfg_of());
        g->catalog = &cat;
        for (uint32_t p = 0; p < 8u; ++p) g->player_epoch[p] = 4u;
        put_foundry(*g, 1);
        g->player_stock[1][R_COPPER] = 1;   // insuficiente
        g->player_stock[1][R_TIN] = 0;
        CHECK(craft_commands_emitted(*g, 1) == 0u);
    }

    // 3) Con la fundición YA fabricando no encola otra sobre el mismo edificio:
    //    el kernel lo rechazaría (§12.4 paso 7) y la IA no debe insistir.
    {
        auto g = std::make_unique<GameState>();
        gs_init(*g, cfg_of());
        g->catalog = &cat;
        for (uint32_t p = 0; p < 8u; ++p) g->player_epoch[p] = 4u;
        const uint32_t f = put_foundry(*g, 1);
        g->player_stock[1][R_COPPER] = 30;
        g->player_stock[1][R_TIN] = 10;
        g->craft_recipe[f] = 0u;            // ya ocupada
        CHECK(craft_commands_emitted(*g, 1) == 0u);
    }

    // 4) DETERMINISMO: la misma entrada produce la misma decisión. Es la regla
    //    de oro de SPEC-005 §0 y se comprueba, no se supone.
    {
        auto make = []() {
            auto g = std::make_unique<GameState>();
            gs_init(*g, cfg_of());
            g->catalog = &cat;
            for (uint32_t p = 0; p < 8u; ++p) g->player_epoch[p] = 4u;
            put_foundry(*g, 1);
            g->player_stock[1][R_COPPER] = 30;
            g->player_stock[1][R_TIN] = 10;
            return g;
        };
        auto a = make();
        auto b = make();
        AiJobBox ba, bb;
        ai_box_init(ba, 1); ai_box_init(bb, 1);
        AiRuntimeV1 rt_a{0, 0}, rt_b{0, 0};
        ai_dispatch(ba, 0u, rt_a);
        ai_dispatch(bb, 0u, rt_b);
        ai_execute(ba, *a);
        ai_execute(bb, *b);
        CHECK(ba.result_count == bb.result_count);
        if (ba.result_count == bb.result_count) {
            for (uint32_t k = 0; k < ba.result_count; ++k) {
                CHECK(ba.result[k].type == bb.result[k].type);
                CHECK(ba.result[k].p.unit_id == bb.result[k].p.unit_id);
            }
        }
    }

    if (g_fails == 0) {
        std::printf("ai_craft OK\n");
        return 0;
    }
    std::printf("ai_craft: %d fallo(s)\n", g_fails);
    return 1;
}
