// Población por CASAS (Sprint 1.14, SPEC-004 §11.3).
//
// POR QUÉ EXISTE. Hasta hoy el tope de población era `POP_CAP_V1 = 200`, una
// constante igual para todos y disponible desde el tick 0. Eso quita del juego
// la decisión económica más básica de un RTS: **crecer cuesta**, y cada casa
// que levantas es madera que no gastas en ejército. Sin tope creciente, la
// única presión sobre el jugador es el coste de la unidad, y la expansión no
// tiene precio propio.
//
// El tipo `housing` ya existía en `building.schema.json` desde el Sprint 1.1 y
// NINGUNA parte del kernel lo miraba. Era una etiqueta sin consecuencia.
//
// CÓMO SE MIDE, y por qué así. El tope pasa a ser una FUNCIÓN PURA del estado:
//
//     tope(jugador) = suma de `population_provided` de sus edificios
//                     COMPLETOS, acotada por POP_CAP_V1
//
// Que sea derivada y no un campo nuevo tiene dos consecuencias buenas: no entra
// en el dominio del checksum (como `flow`, que también es derivada), así que
// NO hay bump de CHECKSUM_ALGO_VERSION; y no puede desincronizarse, que es el
// fallo clásico de llevar un contador incremental a mano —al morir un edificio
// a medio construir, al cancelar una cola, al reciclar un slot—.
//
// POP_CAP_V1 sobrevive como COTA DURA de seguridad, no como regla de juego:
// acota los arrays y evita que un dato absurdo (`population_provided` enorme)
// deje al kernel sin límite.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>

#include "chunsa/data_catalog.hpp"
#include "chunsa/game_state.hpp"
#include "chunsa/step.hpp"

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

namespace {

MatchConfig01A cfg_of() {
    MatchConfig01A c{};
    c.max_entities = 64;
    c.player_count = 2;
    c.human_input_delay_ticks = 0;
    c.max_future_command_ticks = 20;
    c.checksum_every_ticks = 1;
    c.map_tiles_x = 256; c.map_tiles_y = 256;
    c.seed = 20260731ull;
    c.allow_debug_stat_payload = 1;
    return c;
}

// Catálogo sintético mínimo: un edificio que da población y otro que no.
struct Fixture {
    DataCatalogV1 cat{};
    BuildingDefinitionV1 buildings[2]{};
    UnitDefinitionV1 units[1]{};
};

void build_catalog(Fixture& f) {
    // [0] casa: aporta 5 de población.
    BuildingDefinitionV1& casa = f.buildings[0];
    casa.id = 0; casa.hp = 100; casa.footprint_w = 1; casa.footprint_h = 1;
    casa.build_time_ticks = 10;
    casa.constructible = 1;
    casa.epoch_min = 1; casa.epoch_max = 15;
    casa.civ_id = INVALID_CIV_ID;
    casa.population_provided = 5;

    // [1] cuartel: NO aporta población, pero entrena.
    BuildingDefinitionV1& cuartel = f.buildings[1];
    cuartel.id = 1; cuartel.hp = 100; cuartel.footprint_w = 1; cuartel.footprint_h = 1;
    cuartel.build_time_ticks = 10;
    cuartel.constructible = 1;
    cuartel.epoch_min = 1; cuartel.epoch_max = 15;
    cuartel.civ_id = INVALID_CIV_ID;
    cuartel.population_provided = 0;
    cuartel.trains[0] = 0; cuartel.train_count = 1;

    UnitDefinitionV1& u = f.units[0];
    u.id = 0; u.unit_class = UnitClassV1::Infantry;
    u.hp = 10; u.attack = 1; u.pop_cost = 1;
    u.epoch_min = 1; u.epoch_max = 15;
    u.civ_id = INVALID_CIV_ID;
    u.build_time_ticks = 1;

    f.cat.buildings = f.buildings; f.cat.building_count = 2;
    f.cat.units = f.units;         f.cat.unit_count = 1;
}

// Coloca un edificio YA COMPLETO del tipo dado.
uint32_t put_building(GameState& g, uint8_t owner, BuildingId type, uint32_t build_progress) {
    const EntityHandle h = et_spawn(g.entities);
    const uint32_t i = h.index;
    // Sprint 1.42: la reserva de entidades puede agotarse y et_spawn devuelve
    // un handle invalido. Sin esta guarda se escribe FUERA del array, y en
    // Release GCC lo detecta con -Werror=stringop-overflow. Solo aparecia con
    // optimizacion, asi que el proyecto llevaba ciego: aqui solo se compilaba
    // RelWithDebInfo y la CI, que usa Release, lleva roja desde el 25 de julio.
    if (i >= g.entities.capacity) return i;
    zero_components(g, i);
    g.owner[i] = owner;
    g.entity_kind[i] = 1u;
    g.building_id[i] = type;
    g.build_progress[i] = static_cast<int32_t>(build_progress);
    g.hp[i] = 100; g.max_hp[i] = 100;
    return i;
}

std::unique_ptr<GameState> fresh(Fixture& f) {
    auto g = std::make_unique<GameState>();
    gs_init(*g, cfg_of());
    gs_bind_catalog(*g, f.cat);
    // gs_init deja `player_epoch` en 0 por el memset; solo lo fijan las
    // funciones `gs_init_epoch_from_catalog*`, que necesitan un catalogo con
    // civilizaciones. Este fixture es sintetico y no las tiene, asi que la
    // epoca se pone a mano. Sin esto, TRAIN_UNIT rechaza con ILLEGAL_STATE en
    // el gate de epoca (0 < epoch_min) ANTES de llegar al de poblacion, y la
    // prueba estaria midiendo otra cosa sin que se notara.
    for (uint8_t e = 0; e < 2u; ++e) g->player_epoch[e] = 1u;
    return g;
}

}  // namespace

int main() {
    Fixture f{};
    build_catalog(f);

    // 1) SIN casas el tope es CERO. Es la diferencia con el mundo anterior:
    //    antes se arrancaba con 200 de regalo.
    {
        auto g = fresh(f);
        CHECK(player_pop_cap(*g, 0) == 0);
    }

    // 2) Una casa COMPLETA aporta sus 5. Dos, 10: la suma es lineal y no
    //    depende del orden en que se levantaron.
    {
        auto g = fresh(f);
        put_building(*g, 0, 0, 10);
        CHECK(player_pop_cap(*g, 0) == 5);
        put_building(*g, 0, 0, 10);
        CHECK(player_pop_cap(*g, 0) == 10);
    }

    // 3) Una casa A MEDIO CONSTRUIR no aporta NADA. Si aportara, se podría
    //    entrenar contra población que aún no existe, y bastaría cancelar la
    //    obra para quedarse por encima del tope.
    {
        auto g = fresh(f);
        put_building(*g, 0, 0, 3);   // build_time_ticks = 10
        CHECK(player_pop_cap(*g, 0) == 0);
    }

    // 4) Las casas del ENEMIGO no cuentan.
    {
        auto g = fresh(f);
        put_building(*g, 1, 0, 10);
        CHECK(player_pop_cap(*g, 0) == 0);
        CHECK(player_pop_cap(*g, 1) == 5);
    }

    // 5) Un edificio que no es vivienda no aporta.
    {
        auto g = fresh(f);
        put_building(*g, 0, 1, 10);
        CHECK(player_pop_cap(*g, 0) == 0);
    }

    // 6) La COTA DURA sigue mandando: aunque los datos declaren mucho más,
    //    el tope nunca supera POP_CAP_V1. Protege al kernel de un dato absurdo.
    {
        auto g = fresh(f);
        for (uint32_t k = 0; k < 50u; ++k) put_building(*g, 0, 0, 10);  // 250 nominales
        CHECK(player_pop_cap(*g, 0) == static_cast<int32_t>(POP_CAP_V1));
    }

    // 7) LO QUE DE VERDAD IMPORTA: entrenar se RECHAZA al llegar al tope, y se
    //    acepta en cuanto hay una casa más. Sin esto lo anterior es aritmética
    //    sin consecuencia.
    {
        auto g = fresh(f);
        const uint32_t cuartel = put_building(*g, 0, 1, 10);
        put_building(*g, 0, 0, 10);   // 1 casa => tope 5

        RawCommand cmd{};
        std::memset(&cmd, 0, sizeof(cmd));
        cmd.target_tick = 0; cmd.emitter = 0; cmd.type = CommandType::TRAIN_UNIT;
        cmd.p.handle = EntityHandle{cuartel, g->entities.generation[cuartel]};
        cmd.p.unit_id = 0;

        // Cinco aceptadas: exactamente el tope.
        for (uint32_t k = 0; k < 5u; ++k) {
            cmd.sequence = k + 1;
            RawCommand batch[1] = {cmd};
            step(*g, batch, 1);
            const ReceiptMailbox& m = g->mailbox[0];
            CHECK(m.ring[(m.head + m.count - 1) % MAILBOX_CAP].result == RejectReason::ACCEPTED);
            // La cola del cuartel se vacia sola (build_time_ticks=1), pero
            // pop_used NO baja: la unidad existe.
        }
        CHECK(g->pop_used[0] == 5);

        // La sexta NO cabe.
        cmd.sequence = 100;
        {
            RawCommand batch[1] = {cmd};
            step(*g, batch, 1);
            const ReceiptMailbox& m = g->mailbox[0];
            CHECK(m.ring[(m.head + m.count - 1) % MAILBOX_CAP].result == RejectReason::ILLEGAL_STATE);
        }

        // Una casa mas y vuelve a caber. Esta es la decision economica que el
        // sprint devuelve al juego.
        put_building(*g, 0, 0, 10);
        CHECK(player_pop_cap(*g, 0) == 10);
        cmd.sequence = 101;
        {
            RawCommand batch[1] = {cmd};
            step(*g, batch, 1);
            const ReceiptMailbox& m = g->mailbox[0];
            CHECK(m.ring[(m.head + m.count - 1) % MAILBOX_CAP].result == RejectReason::ACCEPTED);
        }
    }

    // 8) El tope es DERIVADO, no estado: NO debe entrar en el checksum por su
    //    cuenta. Lo que cambia el checksum es el edificio que lo produce, que
    //    ya estaba en el dominio. Se comprueba que colocar una casa mueve el
    //    checksum (via building_id/build_progress) — es decir, que la
    //    informacion necesaria para recalcular el tope SI viaja en el estado.
    {
        auto g = fresh(f);
        const uint64_t antes = state_checksum_v1(*g);
        put_building(*g, 0, 0, 10);
        CHECK(antes != state_checksum_v1(*g));
    }

    if (g_fails == 0) {
        std::printf("population_housing OK\n");
        return 0;
    }
    std::printf("population_housing: %d fallo(s)\n", g_fails);
    return 1;
}
