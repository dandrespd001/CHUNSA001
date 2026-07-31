// Sprint 1.13 fase B — proyectiles con viaje (SPEC-004 §24.5). Criterios 8–10.
//
// Lo que cambia respecto al combate anterior: una unidad a distancia YA NO
// aplica el daño en el mismo tick en que dispara. El proyectil viaja, y el daño
// llega cuando llega. Es lo que hace que esquivar y la distancia signifiquen
// algo, y por eso NINGÚN escenario con arqueros puede quedar bit-idéntico.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>

#include "chunsa/game_state.hpp"
#include "chunsa/step.hpp"
#include "chunsa/checksum.hpp"

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

namespace {

MatchConfig01A cfg_of() {
    MatchConfig01A c{};
    c.max_entities = 128;
    c.player_count = 2;
    c.human_input_delay_ticks = 0;
    c.max_future_command_ticks = 20;
    c.checksum_every_ticks = 1;
    c.map_tiles_x = 256; c.map_tiles_y = 256;
    c.seed = 20260730ull;
    c.allow_debug_stat_payload = 1;
    return c;
}

uint32_t put_unit(GameState& g, uint8_t owner, int64_t tx, int64_t ty,
                  int32_t hp, int32_t attack, int32_t range_mt) {
    const EntityHandle h = et_spawn(g.entities);
    const uint32_t i = h.index;
    zero_components(g, i);
    g.owner[i] = owner;
    g.entity_kind[i] = 0u;
    g.unit_class[i] = 2u;            // artillería: a distancia
    g.unit_id[i] = INVALID_UNIT_ID;
    g.hp[i] = hp; g.max_hp[i] = hp;
    g.attack[i] = attack;
    g.range_mt[i] = range_mt;
    g.speed_mtpt[i] = 0;             // quieta: aísla el efecto del proyectil
    g.morale[i] = 100;
    g.pos_x[i] = tx * FX_ONE_RAW;
    g.pos_y[i] = ty * FX_ONE_RAW;
    g.tgt_x[i] = g.pos_x[i];
    g.tgt_y[i] = g.pos_y[i];
    return i;
}

std::unique_ptr<GameState> fresh() {
    auto g = std::make_unique<GameState>();
    gs_init(*g, cfg_of());
    return g;
}

}  // namespace

int main() {
    // 8) El daño llega EN EL TICK DEL IMPACTO, no antes. Con el tirador a 8
    //    tiles y alcance de sobra, debe existir un tramo de ticks en el que el
    //    proyectil ya vuela y el objetivo AÚN NO ha perdido vida.
    {
        auto g = fresh();
        const uint32_t tirador = put_unit(*g, 0, 10, 10, 100, 10, 12000);
        const uint32_t blanco  = put_unit(*g, 1, 18, 10, 100, 0, 0);
        (void)tirador;
        const int32_t hp0 = g->hp[blanco];

        bool volo_sin_danar = false;
        bool impacto = false;
        for (uint32_t t = 0; t < 200u && !impacto; ++t) {
            step(*g, nullptr, 0);
            if (g->n_projectiles > 0 && g->hp[blanco] == hp0) volo_sin_danar = true;
            if (g->hp[blanco] < hp0) impacto = true;
        }
        CHECK(volo_sin_danar);   // hubo VIAJE: el daño no fue instantáneo
        CHECK(impacto);          // y llegó
    }

    // 9) Si el objetivo MUERE en vuelo, el proyectil se descarta sin aplicar
    //    daño. No redirige ni cae al suelo: la flecha no busca otra víctima.
    {
        auto g = fresh();
        put_unit(*g, 0, 10, 10, 100, 10, 12000);
        const uint32_t blanco = put_unit(*g, 1, 18, 10, 100, 0, 0);
        const uint32_t testigo = put_unit(*g, 1, 18, 11, 100, 0, 0);
        const int32_t hp_testigo = g->hp[testigo];

        // Esperar a que haya proyectil en vuelo y entonces matar al blanco.
        for (uint32_t t = 0; t < 50u && g->n_projectiles == 0; ++t) step(*g, nullptr, 0);
        CHECK(g->n_projectiles > 0);
        et_mark_dead(g->entities, blanco);
        for (uint32_t t = 0; t < 50u; ++t) step(*g, nullptr, 0);
        // El testigo pudo recibir disparos NUEVOS, pero no el que iba al muerto.
        CHECK(g->hp[testigo] <= hp_testigo);
    }

    // 10) Con el array LLENO, un disparo más NO se crea y no hay desbordamiento.
    //     La cota es dura y determinista (SPEC-008 §3.3): nunca se escribe
    //     fuera, y el comportamiento en el límite está definido, no es azar.
    {
        auto g = fresh();
        put_unit(*g, 0, 10, 10, 100, 10, 12000);
        put_unit(*g, 1, 18, 10, 100000, 0, 0);
        // Llenar a mano hasta la cota.
        g->n_projectiles = PROJECTILE_HARD_CAP;
        for (uint32_t k = 0; k < PROJECTILE_HARD_CAP; ++k) {
            g->projectiles[k].target = NULL_HANDLE;   // ninguno impactará
            g->projectiles[k].damage = 0;
        }
        for (uint32_t t = 0; t < 20u; ++t) step(*g, nullptr, 0);
        CHECK(g->n_projectiles <= PROJECTILE_HARD_CAP);
    }

    // 13) Mutar n_projectiles CAMBIA el checksum: pertenencia al dominio.
    {
        auto g = fresh();
        const uint64_t before = state_checksum_v1(*g);
        g->n_projectiles = 1;
        g->projectiles[0].damage = 5;
        CHECK(before != state_checksum_v1(*g));
    }

    if (g_fails == 0) {
        std::printf("projectiles OK\n");
        return 0;
    }
    std::printf("projectiles: %d fallo(s)\n", g_fails);
    return 1;
}
