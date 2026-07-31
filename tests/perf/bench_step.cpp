// Sprint 1.20 — banco de rendimiento (SPEC-008 §2.1).
//
// POR QUÉ EXISTE. La auditoría del 2026-07-30 encontró `tests/perf/` VACÍO:
// SPEC-008 fija presupuestos duros —`Step()` ≤ 2,0 ms, `state_checksum_v1`
// ≤ 0,2 ms— y **nadie los medía**. Un presupuesto que nadie comprueba es un
// deseo. Y desde entonces se le han añadido a `Step()` tres cosas que consumen:
// el sistema de proyectiles, el de órdenes, y un `player_tech_bonus` que es
// O(tecnologías) **por golpe**.
//
// QUÉ MIDE Y QUÉ NO. Sin el hardware de referencia (UHD 620, PERF-0 sigue
// bloqueado) esto **no valida el objetivo absoluto**. Lo que sí hace, y es el
// grueso del valor, es **detectar regresiones**: compara contra una referencia
// registrada y falla solo ante una desviación grande.
//
// POR QUÉ EL UMBRAL ES GENEROSO. El tiempo de pared no es determinista: depende
// de la carga de la máquina, del gobernador de frecuencia y del vecino de al
// lado. Un umbral ajustado produciría fallos aleatorios, y una prueba que falla
// sola se acaba ignorando — que es peor que no tenerla. Por eso el factor es
// amplio: caza duplicaciones de coste, no ruido.
//
// NO lleva la etiqueta `fast`: no debe entrar en el ciclo de PR ni volverlo
// inestable. Se ejecuta a propósito.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>

#include "chunsa/game_state.hpp"
#include "chunsa/step.hpp"
#include "chunsa/checksum.hpp"

using namespace chunsa;

namespace {

// Referencia medida en la máquina de desarrollo el 2026-07-31, tras el 1.19.
// Se re-registra a mano cuando un cambio justifique el coste, igual que los
// baselines de determinismo.
// MEDIDAS REALES, no estimaciones. Las primeras que existen en el proyecto.
constexpr double REF_STEP_US = 975.0;      // microsegundos por Step()
constexpr double REF_CHECKSUM_US = 530.0;  // microsegundos por checksum
// Factor de tolerancia: solo interesa cazar duplicaciones, no ruido.
constexpr double TOLERANCE = 3.0;

constexpr uint32_t N_PLAYERS = 4;
constexpr uint32_t PER_PLAYER = 200;
constexpr uint32_t TICKS = 300;

MatchConfig01A cfg_of() {
    MatchConfig01A c{};
    c.max_entities = N_PLAYERS * PER_PLAYER + 64;
    c.player_count = static_cast<uint8_t>(N_PLAYERS);
    c.human_input_delay_ticks = 0;
    c.max_future_command_ticks = 20;
    c.checksum_every_ticks = 1;
    c.map_tiles_x = 256; c.map_tiles_y = 256;
    c.seed = 20260731ull;
    c.allow_debug_stat_payload = 1;
    return c;
}

// Escenario de §2.1: 4 jugadores × 200 entidades. Las 200 granjas del enunciado
// NO existen todavía (Sprint 1.12), así que se sustituyen por los depósitos que
// sí hay; queda dicho para que nadie lea de más en el número.
void build_scenario(GameState& g) {
    gs_init(g, cfg_of());
    for (uint32_t p = 0; p < N_PLAYERS; ++p) {
        for (uint32_t k = 0; k < PER_PLAYER; ++k) {
            const EntityHandle h = et_spawn(g.entities);
            if (h.index >= g.entities.capacity) return;
            const uint32_t i = h.index;
            zero_components(g, i);
            g.owner[i] = static_cast<uint8_t>(p);
            g.entity_kind[i] = 0u;
            g.unit_class[i] = static_cast<uint8_t>(k % 3u);
            g.unit_id[i] = INVALID_UNIT_ID;
            g.hp[i] = 100; g.max_hp[i] = 100;
            g.attack[i] = 5;
            // Mitad a distancia: ejercita el sistema de PROYECTILES, que es
            // justo lo que se acaba de añadir y nadie había medido.
            g.range_mt[i] = (k % 2u == 0u) ? 3000 : 0;
            g.speed_mtpt[i] = 100;
            g.morale[i] = 100;
            const int64_t bx = 30 + static_cast<int64_t>(p) * 60;
            g.pos_x[i] = (bx + static_cast<int64_t>(k % 20u)) * FX_ONE_RAW;
            g.pos_y[i] = (30 + static_cast<int64_t>(k / 20u)) * FX_ONE_RAW;
            g.tgt_x[i] = g.pos_x[i];
            g.tgt_y[i] = g.pos_y[i];
        }
    }
}

double us_per_call(double total_us, uint32_t calls) {
    return calls > 0 ? total_us / static_cast<double>(calls) : 0.0;
}

}  // namespace

int main() {
    auto g = std::make_unique<GameState>();
    build_scenario(*g);

    uint32_t alive = 0;
    for (uint32_t i = 0; i < g->entities.capacity; ++i) {
        if (g->entities.alive[i]) ++alive;
    }
    std::printf("PERF escenario: %u entidades vivas, %u jugadores, %u ticks\n",
                alive, N_PLAYERS, TICKS);

    // --- Step() ---
    const auto t0 = std::chrono::steady_clock::now();
    for (uint32_t t = 0; t < TICKS; ++t) step(*g, nullptr, 0);
    const auto t1 = std::chrono::steady_clock::now();
    const double step_us = us_per_call(
        std::chrono::duration<double, std::micro>(t1 - t0).count(), TICKS);

    // --- checksum ---
    const auto c0 = std::chrono::steady_clock::now();
    uint64_t sink = 0;
    for (uint32_t t = 0; t < TICKS; ++t) sink ^= state_checksum_v1(*g);
    const auto c1 = std::chrono::steady_clock::now();
    const double ck_us = us_per_call(
        std::chrono::duration<double, std::micro>(c1 - c0).count(), TICKS);

    std::printf("PERF Step()          %8.1f us/tick  (referencia %.1f, presupuesto 2000)\n",
                step_us, REF_STEP_US);
    std::printf("PERF checksum_v1     %8.1f us/tick  (referencia %.1f, presupuesto 200)\n",
                ck_us, REF_CHECKSUM_US);
    std::printf("PERF (sink=%llu)\n", static_cast<unsigned long long>(sink));

    int fails = 0;
    if (step_us > REF_STEP_US * TOLERANCE) {
        std::printf("REGRESION: Step() %.1f us supera %.1f (referencia x%.1f)\n",
                    step_us, REF_STEP_US * TOLERANCE, TOLERANCE);
        ++fails;
    }
    if (ck_us > REF_CHECKSUM_US * TOLERANCE) {
        std::printf("REGRESION: checksum %.1f us supera %.1f (referencia x%.1f)\n",
                    ck_us, REF_CHECKSUM_US * TOLERANCE, TOLERANCE);
        ++fails;
    }
    // El presupuesto ABSOLUTO de SPEC-008 se INFORMA pero NO hace fallar: sin
    // el hardware de referencia (PERF-0 sigue bloqueado) no se puede validar un
    // objetivo absoluto, y hacer fallar por algo que no se puede validar
    // convierte la prueba en ruido que se acaba ignorando.
    //
    // Lo que sí se hace es dejarlo VISIBLE en cada ejecución, con el número.
    if (step_us > 2000.0) {
        std::printf("AVISO: Step() supera el presupuesto de 2,0 ms de SPEC-008 §2.1\n");
    } else {
        std::printf("PERF Step() dentro del presupuesto (margen x%.1f)\n", 2000.0 / step_us);
    }
    if (ck_us > 200.0) {
        std::printf("AVISO: checksum SUPERA el presupuesto de 0,2 ms de SPEC-008 §2.1 "
                    "(x%.1f) — causa medida: hashea cost_grid BYTE A BYTE, 65536 "
                    "llamadas por tick. Sprint 1.21.\n", ck_us / 200.0);
    }

    if (fails == 0) {
        std::printf("bench_step OK\n");
        return 0;
    }
    return 1;
}
