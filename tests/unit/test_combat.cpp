// Test de combate RPS v1 (Sprint 0.3): choque de dos bandos (caballería vs
// artillería) que se buscan por el spatial hash y se dañan según el
// triángulo Piedra-Papel-Tijera. Autor: sonnet-5 (contrato cerrado del
// Arquitecto).
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "chunsa/game_state.hpp"
#include "chunsa/step.hpp"
#include "chunsa/checksum.hpp"

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

static constexpr uint32_t N_PER_SIDE  = 60;
static constexpr uint32_t TOTAL_TICKS = 400;

// Escenario: 60 caballería (owner 0) en x∈[120,130] y∈[120,136), y 60
// artillería (owner 1) en x∈[126,136] y∈[120,136), solapando en la franja
// x∈[126,130] para que entren en rango (range_mt=1500 = 1.5 tiles).
static void run_scenario(GameState& g) {
    // Sprint 0.4: SPAWN_UNIT es data-driven por defecto; este test ejercita
    // explícitamente el camino debug legado (stats fijas en el payload), por
    // lo que debe activar allow_debug_stat_payload y marcar unit_id=INVALID
    // en cada comando (ver commands.hpp).
    MatchConfig01A cfg{512u, 2u, 1u, 20u, 20u, 256u, 256u, 7ull, 1u};
    gs_init(g, cfg);

    static RawCommand batch[2 * N_PER_SIDE];

    for (uint32_t t = 0; t < TOTAL_TICKS; ++t) {
        uint32_t n = 0;
        if (t == 0u) {
            for (uint32_t i = 0; i < N_PER_SIDE; ++i) {
                RawCommand& c = batch[n];
                std::memset(&c, 0, sizeof(RawCommand));
                c.target_tick  = 0;
                c.emitter      = 0;
                c.type         = CommandType::SPAWN_UNIT;
                c.sequence     = i + 1u;
                c.p.handle     = EntityHandle{i, 1u};
                const uint32_t tile_x = 120u + (i % 11u);          // ∈[120,130]
                const uint32_t tile_y = 120u + (i / 11u);          // ∈[120,125]
                c.p.x_raw      = static_cast<int64_t>(tile_x) * 65536 + 32768;
                c.p.y_raw      = static_cast<int64_t>(tile_y) * 65536 + 32768;
                c.p.hp         = 100;
                c.p.attack     = 20;
                c.p.range_mt   = 1500;
                c.p.unit_class = 1;  // cavalry
                c.p.unit_id    = INVALID_UNIT_ID;  // camino debug (Sprint 0.4)
                ++n;
            }
            for (uint32_t i = 0; i < N_PER_SIDE; ++i) {
                RawCommand& c = batch[n];
                std::memset(&c, 0, sizeof(RawCommand));
                c.target_tick  = 0;
                c.emitter      = 1;
                c.type         = CommandType::SPAWN_UNIT;
                c.sequence     = i + 1u;
                c.p.handle     = EntityHandle{N_PER_SIDE + i, 1u};
                const uint32_t tile_x = 126u + (i % 11u);          // ∈[126,136]
                const uint32_t tile_y = 120u + (i / 11u);          // ∈[120,125]
                c.p.x_raw      = static_cast<int64_t>(tile_x) * 65536 + 32768;
                c.p.y_raw      = static_cast<int64_t>(tile_y) * 65536 + 32768;
                c.p.hp         = 100;
                c.p.attack     = 20;
                c.p.range_mt   = 1500;
                c.p.unit_class = 2;  // artillery
                c.p.unit_id    = INVALID_UNIT_ID;  // camino debug (Sprint 0.4)
                ++n;
            }
        }
        step(g, batch, n);
    }
}

// ============================================================================
// Sprint 1.4-cierre (SPEC-004 §7.1, enmienda "aldeanos vulnerables"): un
// aldeano enemigo (unit_class==3) pasa a ser objetivo válido de combat_system
// y aggro_system, con RPS ×1.0 (10000 bp) neutro — sin OOB en la tabla 3×3.
// ============================================================================

// A) combat_system inflige daño ×1.0 a un aldeano enemigo en rango: el guard
//    de targeting (unit_class[j] > 2) ya NO lo excluye. attack=5 (< hp=20 del
//    aldeano) a propósito: el aldeano SOBREVIVE al golpe, así se puede leer
//    su hp exacto tras el impacto (15) sin depender de un índice post-mortem
//    (el destroy batch del mismo tick recicla y resetea el slot del que
//    muere — zero_components borra owner/unit_class/hp, inservible para
//    localizarlo después).
static void test_citizen_is_vulnerable_target() {
    auto* g = new GameState();
    MatchConfig01A cfg{64u, 2u, 1u, 20u, 20u, 256u, 256u, 11ull, 1u};
    gs_init(*g, cfg);

    RawCommand batch[2];
    // Atacante: infantería (owner 0), attack=5, rango 1.5 tiles.
    std::memset(&batch[0], 0, sizeof(RawCommand));
    batch[0].target_tick = 0; batch[0].emitter = 0; batch[0].type = CommandType::SPAWN_UNIT;
    batch[0].sequence = 1; batch[0].p.handle = EntityHandle{0u, 1u};
    batch[0].p.x_raw = 100 * 65536 + 32768; batch[0].p.y_raw = 100 * 65536 + 32768;
    batch[0].p.hp = 100; batch[0].p.attack = 5; batch[0].p.range_mt = 1500;
    batch[0].p.unit_class = 0;  // infantry
    batch[0].p.unit_id = INVALID_UNIT_ID;

    // Aldeano enemigo (owner 1) a 1 tile de distancia: dentro de rango.
    std::memset(&batch[1], 0, sizeof(RawCommand));
    batch[1].target_tick = 0; batch[1].emitter = 1; batch[1].type = CommandType::SPAWN_CITIZEN;
    batch[1].sequence = 1; batch[1].p.handle = EntityHandle{1u, 1u};
    batch[1].p.x_raw = 101 * 65536 + 32768; batch[1].p.y_raw = 100 * 65536 + 32768;
    batch[1].p.speed_mtpt = 100;  // > 0 exigido por el camino debug de SPAWN_CITIZEN
    batch[1].p.unit_id = INVALID_UNIT_ID;

    // Fases del MISMO step(): apply_command (spawn) -> movement -> sh_rebuild
    // -> combat_system, todo dentro de esta única llamada (SPEC-001 §2) — el
    // combate del tick 0 ya ve a ambas entidades recién spawneadas y
    // atk_cd==0 no bloquea el primer disparo, así que el impacto ocurre en
    // este MISMO step(), no en uno posterior.
    const StepResult r0 = step(*g, batch, 2);
    CHECK(r0.accepted == 2);

    uint32_t citizen_idx = g->entities.capacity;
    for (uint32_t i = 0; i < g->entities.capacity; ++i) {
        if (g->entities.alive[i] && g->owner[i] == 1u && g->unit_class[i] == 3u) citizen_idx = i;
    }
    CHECK(citizen_idx != g->entities.capacity);

    // RPS ×1.0 (10000 bp) neutro: dmg == attack (5) exacto, sin importar la
    // clase del atacante — ver rps_mult_vs_citizen_bp. Si el guard hubiera
    // dejado pasar unit_class==3 a rps_mult_bp (tabla 3×3), habría sido una
    // lectura fuera de rango (OOB) marcada como Fatal, no un hp exacto.
    CHECK(g->hp[citizen_idx] == 15);  // 20 - 5 = 15: exactamente ×1.0, no OOB de la tabla 3×3
    CHECK(g->entities.alive[citizen_idx]);  // sobrevivió (attack < hp)
    CHECK(g->fatal == FatalReason::NONE);  // ninguna lectura fuera de rango

    delete g;
}

// B) El aldeano sigue sin atacar/perseguir: el guard de ATACANTE
//    (unit_class[i] > 2) permanece intacto. Manipulación directa de
//    GameState (white-box) para dar attack>0 a un aldeano — SPAWN_CITIZEN
//    real jamás produce esto (attack=0 forzado); esto verifica el guard, no
//    un camino de comandos alcanzable en producción.
static void test_citizen_attacker_guard_intact() {
    auto* g = new GameState();
    MatchConfig01A cfg{64u, 2u, 1u, 20u, 20u, 256u, 256u, 13ull, 1u};
    gs_init(*g, cfg);

    // Separación de 4 tiles: > rango de arma del enemigo (1.5 tiles), así
    // que el enemigo NUNCA alcanza al aldeano — el aldeano sobrevive intacto
    // hasta la manipulación de abajo. El enemigo nace con speed_mtpt=0 (no
    // seteado en el payload): jamás puede acercarse aunque aggro_system lo
    // retargete hacia el aldeano (ahora objetivo válido), así que tampoco lo
    // alcanza por movimiento durante el bucle. El guard de ATACANTE del
    // aldeano (`unit_class[i] > 2`, primera línea de combat_system) hace
    // `continue` ANTES de cualquier búsqueda espacial, así que el resultado
    // no depende de si el enemigo cae dentro del vecindario del spatial hash.
    RawCommand batch[2];
    std::memset(&batch[0], 0, sizeof(RawCommand));
    batch[0].target_tick = 0; batch[0].emitter = 0; batch[0].type = CommandType::SPAWN_CITIZEN;
    batch[0].sequence = 1; batch[0].p.handle = EntityHandle{0u, 1u};
    batch[0].p.x_raw = 100 * 65536 + 32768; batch[0].p.y_raw = 100 * 65536 + 32768;
    batch[0].p.speed_mtpt = 100;  // > 0 exigido por el camino debug (se congela después)
    batch[0].p.unit_id = INVALID_UNIT_ID;

    std::memset(&batch[1], 0, sizeof(RawCommand));
    batch[1].target_tick = 0; batch[1].emitter = 1; batch[1].type = CommandType::SPAWN_UNIT;
    batch[1].sequence = 1; batch[1].p.handle = EntityHandle{1u, 1u};
    batch[1].p.x_raw = 104 * 65536 + 32768; batch[1].p.y_raw = 100 * 65536 + 32768;
    batch[1].p.hp = 100; batch[1].p.attack = 20; batch[1].p.range_mt = 1500;
    batch[1].p.unit_class = 0;
    batch[1].p.unit_id = INVALID_UNIT_ID;

    const StepResult r0 = step(*g, batch, 2);
    CHECK(r0.accepted == 2);

    uint32_t citizen_idx = g->entities.capacity, enemy_idx = g->entities.capacity;
    for (uint32_t i = 0; i < g->entities.capacity; ++i) {
        if (!g->entities.alive[i]) continue;
        if (g->owner[i] == 0u) citizen_idx = i;
        if (g->owner[i] == 1u) enemy_idx = i;
    }
    CHECK(citizen_idx != g->entities.capacity && enemy_idx != g->entities.capacity);
    CHECK(g->hp[citizen_idx] == 20);  // a 4 tiles, fuera de rango del enemigo: sobrevive intacto

    // White-box: fuerza attack>0 y rango amplio en el aldeano (inalcanzable
    // vía comandos reales — SPAWN_CITIZEN fuerza attack=0) para probar el
    // guard de ATACANTE en aislamiento; congela su posición (speed_mtpt=0)
    // para que economy_system no lo desplace durante el bucle.
    g->attack[citizen_idx] = 50;
    g->range_mt[citizen_idx] = 6000;  // 6 tiles: cubre de sobra los 4 de separación
    g->speed_mtpt[citizen_idx] = 0;
    const int32_t enemy_hp_before = g->hp[enemy_idx];
    const int64_t citizen_pos_x = g->pos_x[citizen_idx];
    const int64_t citizen_pos_y = g->pos_y[citizen_idx];
    // tgt_x/tgt_y del aldeano: aggro_system SOLO los escribiría si el
    // aldeano fuera el "mover" (i) — el guard de ATACANTE se lo impide
    // (unit_class[i] > 2), así que deben quedarse en el valor de spawn PARA
    // SIEMPRE, sin importar hacia dónde economy_system mueva pos_x/pos_y
    // (economía escribe pos_x/pos_y directamente, nunca tgt_x/tgt_y — por
    // eso se compara contra el valor de spawn, no contra pos_x/pos_y).
    const int64_t citizen_tgt_x = g->tgt_x[citizen_idx];
    const int64_t citizen_tgt_y = g->tgt_y[citizen_idx];

    for (int t = 0; t < 5; ++t) step(*g, nullptr, 0);

    CHECK(g->hp[enemy_idx] == enemy_hp_before);  // el aldeano NUNCA disparó (guard de atacante)
    CHECK(g->pos_x[citizen_idx] == citizen_pos_x);  // ni se movió (speed=0: aísla el resultado)
    CHECK(g->pos_y[citizen_idx] == citizen_pos_y);
    CHECK(g->tgt_x[citizen_idx] == citizen_tgt_x);  // ni persiguió (aggro nunca lo mueve como "i")
    CHECK(g->tgt_y[citizen_idx] == citizen_tgt_y);

    delete g;
}

// C) aggro_system re-adquiere un aldeano enemigo fuera de rango de arma pero
//    dentro de AGGRO_RANGE_MT: tgt_x/tgt_y del atacante se fija hacia él.
// Nota de diseño del test: el atacante nace OCIOSO (pos==tgt) y el aldeano
// cae dentro de AGGRO_RADIUS_CELLS desde el primer tick, así que aggro_system
// ya dispara DENTRO del mismo step() del spawn — antes de que economy_system
// (fase posterior del MISMO tick) mueva al aldeano. Por eso la comparación es
// contra la posición LITERAL de spawn del aldeano (capturada de la propia
// orden), no contra una lectura de pos_x/pos_y hecha después: para cuando
// step() retorna, economía ya pudo haber desplazado al aldeano un paso.
static void test_aggro_targets_citizen() {
    auto* g = new GameState();
    MatchConfig01A cfg{64u, 2u, 1u, 20u, 20u, 256u, 256u, 17ull, 1u};
    gs_init(*g, cfg);

    const int64_t citizen_x_raw = 105 * 65536 + 32768;
    const int64_t citizen_y_raw = 100 * 65536 + 32768;

    RawCommand batch[2];
    // Atacante ocioso (pos==tgt tras spawn), rango de arma 1 tile — el
    // aldeano está a 5 tiles: fuera de arma, dentro de aggro (10 tiles).
    std::memset(&batch[0], 0, sizeof(RawCommand));
    batch[0].target_tick = 0; batch[0].emitter = 0; batch[0].type = CommandType::SPAWN_UNIT;
    batch[0].sequence = 1; batch[0].p.handle = EntityHandle{0u, 1u};
    batch[0].p.x_raw = 100 * 65536 + 32768; batch[0].p.y_raw = 100 * 65536 + 32768;
    batch[0].p.hp = 100; batch[0].p.attack = 20; batch[0].p.range_mt = 1000;
    batch[0].p.unit_class = 0;
    batch[0].p.unit_id = INVALID_UNIT_ID;

    std::memset(&batch[1], 0, sizeof(RawCommand));
    batch[1].target_tick = 0; batch[1].emitter = 1; batch[1].type = CommandType::SPAWN_CITIZEN;
    batch[1].sequence = 1; batch[1].p.handle = EntityHandle{1u, 1u};
    batch[1].p.x_raw = citizen_x_raw; batch[1].p.y_raw = citizen_y_raw;
    batch[1].p.speed_mtpt = 100;
    batch[1].p.unit_id = INVALID_UNIT_ID;

    const StepResult r0 = step(*g, batch, 2);
    CHECK(r0.accepted == 2);

    uint32_t atk_idx = g->entities.capacity, citizen_idx = g->entities.capacity;
    for (uint32_t i = 0; i < g->entities.capacity; ++i) {
        if (!g->entities.alive[i]) continue;
        if (g->owner[i] == 0u) atk_idx = i;
        if (g->owner[i] == 1u) citizen_idx = i;
    }
    CHECK(atk_idx != g->entities.capacity && citizen_idx != g->entities.capacity);

    // aggro_system ya corrió DENTRO de r0 (el atacante nace ocioso y el
    // aldeano ya está en su radio de adquisición desde el spawn): el tgt del
    // atacante apunta a la posición de spawn del aldeano.
    CHECK(g->tgt_x[atk_idx] == citizen_x_raw);
    CHECK(g->tgt_y[atk_idx] == citizen_y_raw);

    delete g;
}

int main() {
    auto* g1 = new GameState();
    run_scenario(*g1);

    CHECK(g1->fatal == FatalReason::NONE);

    uint32_t alive0 = 0, alive1 = 0;
    for (uint32_t i = 0; i < g1->entities.capacity; ++i) {
        if (!g1->entities.alive[i]) continue;
        if (g1->owner[i] == 0u) ++alive0;
        else if (g1->owner[i] == 1u) ++alive1;
    }
    const uint32_t total_alive = alive0 + alive1;

    CHECK(alive0 > alive1);                       // ventaja RPS de la caballería
    CHECK(total_alive < 2u * N_PER_SIDE);          // el combate ocurrió

    const uint64_t checksum1 = state_checksum_v1(*g1);
    delete g1;

    // Determinismo: segunda corrida fresca idéntica → mismo checksum final.
    auto* g2 = new GameState();
    run_scenario(*g2);
    const uint64_t checksum2 = state_checksum_v1(*g2);
    delete g2;

    CHECK(checksum1 == checksum2);

    std::printf("combat: owner0=%u owner1=%u checksum=%llx\n",
                alive0, alive1, (unsigned long long)checksum1);

    // Sprint 1.4-cierre (SPEC-004 §7.1): aldeanos vulnerables en combate.
    test_citizen_is_vulnerable_target();
    test_citizen_attacker_guard_intact();
    test_aggro_targets_citizen();

    if (g_fails == 0) { std::printf("combat: OK\n"); return 0; }
    std::printf("combat: %d fallos\n", g_fails);
    return 1;
}
