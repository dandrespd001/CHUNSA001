// La IA COMPRA lo que la bloquea y VENDE lo que le sobra para pagarlo
// (Sprint 1.47).
//
// POR QUÉ EXISTE. El banco de partida larga midió a p1 sentado encima de
// 10 984 de comida mientras se moría sin madera. El mercado estaba construido
// y era inútil para ese caso por dos agujeros:
//
//   · sólo compraba para desatascar RECETAS, nunca para CONSTRUIR — y lo que
//     bloquea la partida es no poder levantar el edificio de época;
//   · sólo vendía "excedente claro", definido como un recurso que NADA en la
//     civilización consume. La comida la consume medio catálogo, así que jamás
//     contaba como excedente por mucha que hubiera.
//
// LA REGLA QUE VIGILA ESTE FICHERO, y es la que da sentido a todo lo demás:
// VENDER NO ES UNA POLÍTICA, ES EL PAGO DE UNA COMPRA CONCRETA. Una IA que
// venda cada vez que le sobre algo mueve el precio en su contra y no obtiene
// nada. La prueba 2 es la que lo comprueba.

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

constexpr uint8_t R_FOOD = 0, R_WOOD = 1, R_GOLD = 2;

BuildingDefinitionV1 g_buildings[2];
UnitDefinitionV1     g_units[1];
ResourceDefinitionV1 g_resources[3];
ResourceNameIndexV1  g_resource_names[3];

// Catálogo mínimo: un MERCADO (can_trade) y un CUARTEL que cuesta madera y
// entrena un soldado. Nada más: cuanto más pequeño el catálogo, más claro es
// qué provoca cada decisión.
DataCatalogV1 make_catalog() {
    std::memset(g_buildings, 0, sizeof(g_buildings));
    std::memset(g_units, 0, sizeof(g_units));
    std::memset(g_resources, 0, sizeof(g_resources));
    std::memset(g_resource_names, 0, sizeof(g_resource_names));

    auto init_building = [](BuildingDefinitionV1& b, uint32_t id) {
        b.id = id; b.hp = 500; b.footprint_w = 2; b.footprint_h = 2;
        b.epoch_min = 1; b.epoch_max = 15;
        b.constructible = 1u;
        b.civ_id = INVALID_CIV_ID;
        for (uint32_t k = 0; k < PROD_TRAINS_MAX; ++k) b.trains[k] = INVALID_UNIT_ID;
        for (uint32_t k = 0; k < PROD_TECHS_MAX; ++k) b.researches[k] = INVALID_TECH_ID;
        for (uint32_t k = 0; k < BUILDING_REQCAP_MAX; ++k) {
            b.required_capabilities[k] = INVALID_CAPABILITY_ID;
        }
        for (uint32_t k = 0; k < RECIPES_PER_BUILDING_MAX; ++k) {
            b.recipes[k] = INVALID_RECIPE_ID;
        }
    };

    // 0 = MERCADO. Sin él, ai_find_trade no emite nada en absoluto.
    init_building(g_buildings[0], 0);
    g_buildings[0].can_trade = 1u;
    g_buildings[0].build_time_ticks = 10;

    // 1 = CUARTEL: cuesta 60 de MADERA y entrena un soldado. Es el edificio
    //     que la IA quiere y no puede pagar — el caso del banco, literal.
    init_building(g_buildings[1], 1);
    g_buildings[1].build_time_ticks = 20;
    g_buildings[1].cost[R_WOOD] = 60;
    g_buildings[1].trains[0] = 0; g_buildings[1].train_count = 1;

    g_units[0].id = 0;
    g_units[0].unit_class = UnitClassV1::Infantry;   // no-ciudadano: hace del edificio 1 un cuartel
    g_units[0].hp = 60; g_units[0].attack = 10;
    g_units[0].speed_millitile_tick = 200;
    g_units[0].morale = 100;
    g_units[0].build_time_ticks = 5;
    g_units[0].pop_cost = 1;
    g_units[0].epoch_min = 1; g_units[0].epoch_max = 15;
    g_units[0].civ_id = INVALID_CIV_ID;
    g_units[0].attack_type = DamageTypeV1::Cut;
    // El soldado CUESTA COMIDA, y hace falta que asi sea. Si nada del catalogo
    // consumiera comida, la regla VIEJA de "excedente claro" (stock muerto)
    // la venderia por su cuenta y estas pruebas medirian esa regla en vez de
    // la nueva. Con este coste la comida es stock VIVO, que es justo el caso
    // que el Sprint 1.47 existe para resolver: sobra algo que SI se usa.
    g_units[0].cost[R_FOOD] = 50;

    // El índice del ORO se resuelve del catálogo por su record_id, no está
    // fijado en el kernel. Sin esta tabla, ai_find_trade sale sin hacer nada.
    for (uint32_t i = 0; i < 3; ++i) {
        g_resources[i].index = static_cast<uint8_t>(i);
    }
    static const char kFood[] = "chunsa:food";
    static const char kWood[] = "chunsa:wood";
    static const char kGold[] = "chunsa:gold";
    // ORDENADA BYTEWISE, y no es un detalle: catalog_find_resource hace
    // BUSQUEDA BINARIA. Con la tabla desordenada el oro sencillamente no se
    // encuentra, ai_find_trade sale sin hacer nada y las pruebas fallan sin
    // decir por que. El orden es food < gold < wood, no el de los indices.
    g_resource_names[0].record_id_utf8 = kFood;
    g_resource_names[0].record_id_bytes = sizeof(kFood) - 1;
    g_resource_names[0].id = 0;
    g_resource_names[1].record_id_utf8 = kGold;
    g_resource_names[1].record_id_bytes = sizeof(kGold) - 1;
    g_resource_names[1].id = 2;
    g_resource_names[2].record_id_utf8 = kWood;
    g_resource_names[2].record_id_bytes = sizeof(kWood) - 1;
    g_resource_names[2].id = 1;

    DataCatalogV1 c{};
    c.building_count = 2; c.buildings = g_buildings;
    c.unit_count = 1;     c.units = g_units;
    c.resource_count = 3; c.resources = g_resources;
    c.resource_names = g_resource_names;
    c.ai_profile_count = 0; c.ai_profiles = nullptr;
    return c;
}

MatchConfig01A cfg_of() {
    MatchConfig01A c{};
    c.max_entities = 32;
    c.player_count = 2;
    c.human_input_delay_ticks = 0;
    c.max_future_command_ticks = 20;
    c.checksum_every_ticks = 1;
    c.map_tiles_x = 256; c.map_tiles_y = 256;
    c.seed = 20260804ull;
    c.allow_debug_stat_payload = 0;
    return c;
}

DataCatalogV1 g_cat;

// Estado con UN mercado completo del jugador 0 y las cajas a cero.
std::unique_ptr<GameState> make_state() {
    auto g = std::make_unique<GameState>();
    gs_init(*g, cfg_of());
    g_cat = make_catalog();
    g->catalog = &g_cat;
    g->player_epoch[0] = 1;
    g->player_civ[0] = INVALID_CIV_ID;

    const EntityHandle h = et_spawn(g->entities);
    const uint32_t i = h.index;
    zero_components(*g, i);
    g->owner[i] = 0u;
    g->entity_kind[i] = 1u;                 // edificio
    g->building_id[i] = 0u;                 // el MERCADO
    g->build_progress[i] = 10;              // COMPLETO
    g->hp[i] = 500; g->max_hp[i] = 500;
    g->pos_x[i] = 40 * FX_ONE_RAW;
    g->pos_y[i] = 40 * FX_ONE_RAW;

    for (uint8_t rr = 0; rr < RESOURCE_COUNT; ++rr) g->player_stock[0][rr] = 0;
    return g;
}

}  // namespace

// ============================================================================
// 1) EL CASO DEL BANCO, LITERAL. Montaña de comida, cero madera, cero oro, y
//    un cuartel que cuesta madera. La IA debe VENDER comida: es la única forma
//    de conseguir el oro con el que comprar la madera que la bloquea.
// ============================================================================
static void test_vende_comida_para_pagar_la_madera() {
    auto g = make_state();
    g->player_stock[0][R_FOOD] = 10000;   // el excedente medido en el banco
    g->player_stock[0][R_WOOD] = 0;
    g->player_stock[0][R_GOLD] = 0;

    const AiTradeV1 t = ai_find_trade(*g, 0u);
    CHECK(t.found);
    CHECK(!t.buy);                        // VENDE
    CHECK(t.resource == R_FOOD);          // vende la comida, que es lo que sobra
}

// ============================================================================
// 2) LA PRUEBA QUE DEFINE LA REGLA. El mismo jugador con la misma montaña de
//    comida, pero SIN NADA QUE COMPRAR (ya tiene madera de sobra): no vende.
//
//    Sin esta prueba, el cambio del 1.47 sería "vender cuando sobre", que es
//    la forma lenta de arruinarse: el mercado cobra horquilla y el precio baja
//    con cada lote. La venta sólo existe como pago de una compra concreta.
// ============================================================================
static void test_no_vende_si_no_hay_nada_que_comprar() {
    auto g = make_state();
    g->player_stock[0][R_FOOD] = 10000;
    g->player_stock[0][R_WOOD] = 5000;    // el cuartel ya es pagable
    g->player_stock[0][R_GOLD] = 0;

    const AiTradeV1 t = ai_find_trade(*g, 0u);
    CHECK(!t.found);                      // no hay nada que hacer: no se toca el mercado
}

// ============================================================================
// 3) CON ORO, COMPRA DIRECTAMENTE. No hace falta vender nada si ya se puede
//    pagar: vender sería gastar precio a cambio de un oro que ya se tiene.
// ============================================================================
static void test_con_oro_compra_sin_vender() {
    auto g = make_state();
    g->player_stock[0][R_FOOD] = 10000;
    g->player_stock[0][R_WOOD] = 0;
    g->player_stock[0][R_GOLD] = 100000;  // de sobra

    const AiTradeV1 t = ai_find_trade(*g, 0u);
    CHECK(t.found);
    CHECK(t.buy);                         // COMPRA
    CHECK(t.resource == R_WOOD);          // justo lo que la bloquea
    CHECK(t.cost_gold > 0);
}

// ============================================================================
// 4) NUNCA VENDE LO QUE QUIERE COMPRAR. Vender la madera que te falta para
//    conseguir oro con el que comprar madera es un círculo, y uno caro: cada
//    vuelta paga la horquilla.
// ============================================================================
static void test_nunca_vende_lo_que_quiere_comprar() {
    auto g = make_state();
    g->player_stock[0][R_FOOD] = 0;
    g->player_stock[0][R_WOOD] = 5000;    // montaña de madera...
    g->player_stock[0][R_GOLD] = 0;
    // ...pero el cuartel ya es pagable con 5000 de madera, así que no hay
    // compra pendiente y por tanto tampoco venta.
    const AiTradeV1 t = ai_find_trade(*g, 0u);
    CHECK(!t.found);
}

// ============================================================================
// 5) POR DEBAJO DEL UMBRAL NO SE TOCA NADA. Con compra pendiente pero sin
//    excedente abrumador, la IA aguanta. AI_TRADE_GLUT existe para distinguir
//    "tengo comida" de "estoy sentado sobre una montaña de comida": vender la
//    despensa por una tabla es peor que quedarse quieto.
// ============================================================================
static void test_sin_excedente_abrumador_no_vende() {
    auto g = make_state();
    g->player_stock[0][R_FOOD] = AI_TRADE_GLUT - 1;
    g->player_stock[0][R_WOOD] = 0;
    g->player_stock[0][R_GOLD] = 0;

    const AiTradeV1 t = ai_find_trade(*g, 0u);
    CHECK(!t.found);
}

// ============================================================================
// 6) SIN MERCADO NO HAY NADA. Es la guarda de siempre y se comprueba porque
//    todo lo anterior se apoya en ella: un TRADE sin mercado lo rechazaría el
//    kernel, y un comando condenado es ruido en el buzón.
// ============================================================================
static void test_sin_mercado_no_comercia() {
    auto g = make_state();
    g->player_stock[0][R_FOOD] = 10000;
    g->player_stock[0][R_WOOD] = 0;
    g->building_id[0] = 1u;               // el edificio pasa a ser el cuartel
    const AiTradeV1 t = ai_find_trade(*g, 0u);
    CHECK(!t.found);
}

int main() {
    test_vende_comida_para_pagar_la_madera();
    test_no_vende_si_no_hay_nada_que_comprar();
    test_con_oro_compra_sin_vender();
    test_nunca_vende_lo_que_quiere_comprar();
    test_sin_excedente_abrumador_no_vende();
    test_sin_mercado_no_comercia();

    if (g_fails == 0) {
        std::printf("ai_mercado OK\n");
        return 0;
    }
    std::printf("ai_mercado: %d fallo(s)\n", g_fails);
    return 1;
}
