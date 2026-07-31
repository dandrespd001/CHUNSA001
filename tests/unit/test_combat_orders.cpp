// Sprint 1.13 — órdenes de combate (SPEC-004 §24). Criterios §24.7.
//
// Fase A de cuatro: comandos y precedencia sobre el aggro. Los proyectiles
// (§24.5, criterios 8–10) entran en su propia fase.
//
// NOTA: GameState SIEMPRE en heap — en pila segfaultea bajo ctest.

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

// Unidad de combate colocada a mano, sin pasar por SPAWN.
uint32_t put_unit(GameState& g, uint8_t owner, int64_t tx, int64_t ty,
                  int32_t hp, int32_t attack, int32_t range_mt, uint8_t cls) {
    const EntityHandle h = et_spawn(g.entities);
    const uint32_t i = h.index;
    zero_components(g, i);
    g.owner[i] = owner;
    g.entity_kind[i] = 0u;
    g.unit_class[i] = cls;
    g.unit_id[i] = INVALID_UNIT_ID;
    g.hp[i] = hp; g.max_hp[i] = hp;
    g.attack[i] = attack;
    g.range_mt[i] = range_mt;
    g.speed_mtpt[i] = 200;
    g.morale[i] = 100;
    g.pos_x[i] = tx * FX_ONE_RAW;
    g.pos_y[i] = ty * FX_ONE_RAW;
    g.tgt_x[i] = g.pos_x[i];
    g.tgt_y[i] = g.pos_y[i];
    return i;
}

RawCommand attack_cmd(const GameState& g, uint16_t emitter, uint64_t seq,
                      uint32_t self_slot, uint32_t target_slot) {
    RawCommand c{};
    std::memset(&c, 0, sizeof(c));
    c.target_tick = 0;
    c.emitter = static_cast<uint8_t>(emitter);
    c.sequence = seq;
    c.type = CommandType::ATTACK;
    c.p.handle = EntityHandle{self_slot, g.entities.generation[self_slot]};
    // El objetivo viaja en unit_id (indice) y speed_mtpt (generacion): dos
    // campos que el replay YA serializa, para no subir su formato.
    c.p.unit_id = target_slot;
    c.p.speed_mtpt = static_cast<int32_t>(g.entities.generation[target_slot]);
    return c;
}

RawCommand attack_move_cmd(const GameState& g, uint16_t emitter, uint64_t seq,
                           uint32_t self_slot, int64_t x_raw, int64_t y_raw) {
    RawCommand c{};
    std::memset(&c, 0, sizeof(c));
    c.target_tick = 0;
    c.emitter = static_cast<uint8_t>(emitter);
    c.sequence = seq;
    c.type = CommandType::ATTACK_MOVE;
    c.p.handle = EntityHandle{self_slot, g.entities.generation[self_slot]};
    c.p.x_raw = x_raw;
    c.p.y_raw = y_raw;
    return c;
}

RejectReason submit(GameState& g, const RawCommand& c) {
    RawCommand batch[1] = {c};
    step(g, batch, 1);
    const ReceiptMailbox& m = g.mailbox[c.emitter];
    if (m.count == 0) return RejectReason::MALFORMED;
    return m.ring[(m.head + m.count - 1) % MAILBOX_CAP].result;
}

std::unique_ptr<GameState> fresh() {
    auto g = std::make_unique<GameState>();
    gs_init(*g, cfg_of());
    return g;
}

}  // namespace

int main() {
    // 1) ATTACK sobre enemigo vivo es ACEPTADO y la unidad se ACERCA.
    {
        auto g = fresh();
        const uint32_t me = put_unit(*g, 0, 10, 10, 100, 10, 0, 0);
        const uint32_t foe = put_unit(*g, 1, 40, 10, 100, 10, 0, 0);
        const int64_t d0 = g->pos_x[foe] - g->pos_x[me];
        CHECK(submit(*g, attack_cmd(*g, 0, 1, me, foe)) == RejectReason::ACCEPTED);
        CHECK(g->order_mode[me] == ORDER_MODE_ATTACK);
        CHECK(g->attack_target[me].index == foe);
        for (uint32_t t = 0; t < 30u; ++t) step(*g, nullptr, 0);
        const int64_t d1 = g->pos_x[foe] - g->pos_x[me];
        CHECK(d1 < d0);   // se acerco
    }

    // 2) ATTACK sobre unidad PROPIA -> NOT_OWNER. El jugador no ordena
    //    fratricidio por accidente.
    {
        auto g = fresh();
        const uint32_t me = put_unit(*g, 0, 10, 10, 100, 10, 0, 0);
        const uint32_t mate = put_unit(*g, 0, 12, 10, 100, 10, 0, 0);
        CHECK(submit(*g, attack_cmd(*g, 0, 1, me, mate)) == RejectReason::NOT_OWNER);
        CHECK(g->order_mode[me] == ORDER_MODE_NONE);
    }

    // 3) ATTACK sobre entidad MUERTA -> INVALID_ENTITY.
    {
        auto g = fresh();
        const uint32_t me = put_unit(*g, 0, 10, 10, 100, 10, 0, 0);
        const uint32_t foe = put_unit(*g, 1, 40, 10, 100, 10, 0, 0);
        auto cmd = attack_cmd(*g, 0, 1, me, foe);
        et_mark_dead(g->entities, foe);
        CHECK(submit(*g, cmd) == RejectReason::INVALID_ENTITY);
    }

    // 4) Con ATTACK activo, el AGGRO NO cambia el objetivo aunque pase otro
    //    enemigo MAS CERCA. Sin esta precedencia la orden seria una sugerencia
    //    que el aggro pisa, y el jugador lo notaria como desobediencia.
    {
        auto g = fresh();
        const uint32_t me = put_unit(*g, 0, 10, 10, 100, 10, 0, 0);
        const uint32_t lejos = put_unit(*g, 1, 40, 10, 100, 10, 0, 0);
        put_unit(*g, 1, 11, 10, 100, 10, 0, 0);   // enemigo PEGADO
        CHECK(submit(*g, attack_cmd(*g, 0, 1, me, lejos)) == RejectReason::ACCEPTED);
        for (uint32_t t = 0; t < 10u; ++t) step(*g, nullptr, 0);
        CHECK(g->attack_target[me].index == lejos);
        CHECK(g->order_mode[me] == ORDER_MODE_ATTACK);
    }

    // 5) Al MORIR el objetivo, la unidad vuelve a NINGUNA y el aggro manda.
    {
        auto g = fresh();
        const uint32_t me = put_unit(*g, 0, 10, 10, 100, 10, 0, 0);
        const uint32_t foe = put_unit(*g, 1, 40, 10, 100, 10, 0, 0);
        CHECK(submit(*g, attack_cmd(*g, 0, 1, me, foe)) == RejectReason::ACCEPTED);
        et_mark_dead(g->entities, foe);
        step(*g, nullptr, 0);
        CHECK(g->order_mode[me] == ORDER_MODE_NONE);
    }

    // 6) ATTACK_MOVE avanza hacia el destino.
    {
        auto g = fresh();
        const uint32_t me = put_unit(*g, 0, 10, 10, 100, 10, 0, 0);
        const int64_t dest = 60 * FX_ONE_RAW;
        CHECK(submit(*g, attack_move_cmd(*g, 0, 1, me, dest, 10 * FX_ONE_RAW))
              == RejectReason::ACCEPTED);
        CHECK(g->order_mode[me] == ORDER_MODE_ATTACK_MOVE);
        const int64_t x0 = g->pos_x[me];
        for (uint32_t t = 0; t < 20u; ++t) step(*g, nullptr, 0);
        CHECK(g->pos_x[me] > x0);
    }

    // 7) ATTACK_MOVE con destino FUERA DE COTA -> MALFORMED.
    {
        auto g = fresh();
        const uint32_t me = put_unit(*g, 0, 10, 10, 100, 10, 0, 0);
        CHECK(submit(*g, attack_move_cmd(*g, 0, 1, me, -5 * FX_ONE_RAW, 10 * FX_ONE_RAW))
              == RejectReason::MALFORMED);
    }

    // 13) Mutar order_mode CAMBIA el checksum: pertenencia al dominio.
    {
        auto g = fresh();
        const uint32_t me = put_unit(*g, 0, 10, 10, 100, 10, 0, 0);
        const uint64_t before = state_checksum_v1(*g);
        g->order_mode[me] = ORDER_MODE_ATTACK;
        const uint64_t after = state_checksum_v1(*g);
        CHECK(before != after);
        g->attack_target[me] = EntityHandle{7u, 1u};
        CHECK(after != state_checksum_v1(*g));
    }

    if (g_fails == 0) {
        std::printf("combat_orders OK\n");
        return 0;
    }
    std::printf("combat_orders: %d fallo(s)\n", g_fails);
    return 1;
}
