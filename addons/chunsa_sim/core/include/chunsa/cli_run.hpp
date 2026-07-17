#pragma once

// chunsa_sim_cli — escenario synthetic_movement_v1@1 y reporte (SPEC-001 §12-13)
// generado: minimax-m3 · revisado: Arquitecto 2026-07-17

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "chunsa/game_state.hpp"
#include "chunsa/rng.hpp"
#include "chunsa/step.hpp"

// Definido en main.cpp (contador global de allocations; parche del Arquitecto 2026-07-17).
extern uint64_t g_chunsa_allocs;

namespace chunsa {

// Resumen agregado de una corrida del escenario (telemetría + estado final del kernel).
struct RunReport {
    uint64_t    final_checksum{0};
    uint32_t    checksums_seen{0};
    uint64_t    accepted{0};
    uint64_t    rejected{0};
    uint64_t    p50_ns{0};
    uint64_t    p95_ns{0};
    uint64_t    p99_ns{0};
    uint64_t    max_ns{0};
    FatalReason fatal{FatalReason::NONE};
    uint64_t    alloc_delta{~0ull};  // allocations DENTRO del bucle de ticks (gate: 0)
};

namespace detail {

// Percentil por método nearest-rank sobre un vector YA ORDENADO ascendentemente.
// Fórmula congelada del escenario: idx = (pct * n + 99) / 100 - 1, clampeado a [0, n-1].
// pct se recibe como int pero solo se invocará con 50, 95 o 99.
inline uint64_t nearest_rank_ns(const std::vector<uint64_t>& sorted, int pct) noexcept {
    const std::size_t n = sorted.size();
    if (n == 0) {
        return 0;
    }
    const std::size_t numerator = static_cast<std::size_t>(pct) * n + 99;
    std::size_t idx = numerator / 100;
    if (idx == 0) {
        // Caso borde n == 1 (o pct == 0): devolvemos el único elemento.
        return sorted[0];
    }
    --idx;
    if (idx >= n) {
        idx = n - 1; // clamp defensivo (la fórmula no debería salirse para pct <= 99).
    }
    return sorted[idx];
}

} // namespace detail

// Ejecuta el escenario synthetic_movement_v1@1 con telemetría local (steady_clock).
// Retornos:
//   0  -> corrida exitosa (fatal == NONE al terminar)
//   2  -> config inválida (config_validate rechazó la MatchConfig01A)
//   3  -> el kernel reportó FatalReason != NONE durante o al final del bucle
inline int run_synthetic(uint32_t units, uint32_t ticks, uint16_t checksum_every,
                         uint64_t seed, RunReport& out) {
    // (1) Construir y validar la configuración del partido (SPEC-001 §12).
    MatchConfig01A cfg{};
    cfg.max_entities             = units;
    cfg.player_count             = 1;
    cfg.human_input_delay_ticks  = 1;
    cfg.max_future_command_ticks = 20;
    cfg.checksum_every_ticks     = checksum_every;
    cfg.map_tiles_x              = 256;
    cfg.map_tiles_y              = 256;
    cfg.seed                     = seed;

    if (!config_validate(cfg)) {
        return 2;
    }

    // (2) GameState pesa ~10MB; se asigna UNA sola vez con new y se libera con delete.
    GameState* gs = new GameState();
    gs_init(*gs, cfg);

    // (3) Pre-reservar TODOS los buffers: cero asignaciones dentro del bucle caliente.
    std::vector<RawCommand> batch;
    batch.resize(units);
    std::vector<uint64_t> samples;
    samples.resize(ticks);

    uint64_t sequence_counter = 0; // primer sequence emitido será 1 (estrictamente creciente).
    uint64_t last_checksum    = 0;
    uint32_t checksums_seen   = 0;
    uint64_t accepted_total   = 0;
    uint64_t rejected_total   = 0;

    // dummy: FatalReason local para detectar parámetros inválidos de rng_range.
    // Bajo el escenario congelado NUNCA debe tomar un valor distinto de NONE.
    FatalReason dummy = FatalReason::NONE;

    const uint64_t alloc_before = ::g_chunsa_allocs;  // tras la prealocación, antes del bucle

    constexpr uint32_t BENCH_STREAM = static_cast<uint32_t>(RngStream::BENCH);
    // 1.0 raw = 65536 (FX_ONE_RAW); el escenario fija coords en [1.0, 255.0] raw.
    constexpr uint32_t COORD_LO = static_cast<uint32_t>(FX_ONE_RAW);
    constexpr uint32_t COORD_HI = 255U * static_cast<uint32_t>(FX_ONE_RAW);

    for (uint32_t t = 0; t < ticks; ++t) {
        uint32_t n = 0;

        if (t == 0U) {
            // Tick 0: SPAWN_DEBUG, uno por unidad, todos con target_tick = 0.
            for (uint32_t i = 0; i < units; ++i) {
                ++sequence_counter;
                RawCommand& cmd = batch[n];
                std::memset(&cmd, 0, sizeof(RawCommand));
                cmd.target_tick     = 0;
                cmd.emitter         = 0;
                cmd.type            = CommandType::SPAWN_DEBUG;
                cmd.sequence        = sequence_counter;
                cmd.p.handle        = EntityHandle{i, 1U};
                cmd.p.x_raw         = static_cast<int64_t>(
                    rng_range(seed, BENCH_STREAM, 0U, i, 1U, COORD_LO, COORD_HI, dummy));
                cmd.p.y_raw         = static_cast<int64_t>(
                    rng_range(seed, BENCH_STREAM, 0U, i, 2U, COORD_LO, COORD_HI, dummy));
                cmd.p.speed_mtpt    = static_cast<int32_t>(
                    rng_range(seed, BENCH_STREAM, 0U, i, 3U, 50U, 501U, dummy));
                ++n;
            }
        } else if ((t % 200U) == 100U) {
            // Retarget periódico: MOVE_TO para todas las unidades vivas.
            // El fixture nunca destruye nada, así que la generación siempre es 1.
            for (uint32_t i = 0; i < units; ++i) {
                ++sequence_counter;
                RawCommand& cmd = batch[n];
                std::memset(&cmd, 0, sizeof(RawCommand));
                cmd.target_tick     = t;
                cmd.emitter         = 0;
                cmd.type            = CommandType::MOVE_TO;
                cmd.sequence        = sequence_counter;
                cmd.p.handle        = EntityHandle{i, 1U};
                cmd.p.x_raw         = static_cast<int64_t>(
                    rng_range(seed, BENCH_STREAM, t, i, 4U, COORD_LO, COORD_HI, dummy));
                cmd.p.y_raw         = static_cast<int64_t>(
                    rng_range(seed, BENCH_STREAM, t, i, 5U, COORD_LO, COORD_HI, dummy));
                ++n;
            }
        }
        // Resto de ticks: lote vacío (n = 0).

        // Telemetría LOCAL con steady_clock; no modifica el estado del juego.
        const auto t0 = std::chrono::steady_clock::now();
        const StepResult res = step(*gs, batch.data(), n);
        const auto t1 = std::chrono::steady_clock::now();

        const auto elapsed_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
        samples[t] = static_cast<uint64_t>(elapsed_ns);

        if (res.checksum_computed) {
            last_checksum = res.checksum;
            ++checksums_seen;
        }
        accepted_total += res.accepted;
        rejected_total += res.rejected;
    }

    const uint64_t alloc_delta = ::g_chunsa_allocs - alloc_before;

    // (4) Snapshot del estado final antes de liberar memoria.
    const FatalReason final_fatal = gs->fatal;

    // (5) Ordenar muestras (in-place) y calcular percentiles nearest-rank.
    std::sort(samples.begin(), samples.end());
    const uint64_t p50  = detail::nearest_rank_ns(samples, 50);
    const uint64_t p95  = detail::nearest_rank_ns(samples, 95);
    const uint64_t p99  = detail::nearest_rank_ns(samples, 99);
    const uint64_t maxv = samples.empty() ? 0ULL : samples.back();

    delete gs;

    out.final_checksum = last_checksum;
    out.checksums_seen = checksums_seen;
    out.accepted       = accepted_total;
    out.rejected       = rejected_total;
    out.p50_ns         = p50;
    out.p95_ns         = p95;
    out.p99_ns         = p99;
    out.max_ns         = maxv;
    out.fatal          = final_fatal;
    out.alloc_delta    = alloc_delta;

    if (final_fatal != FatalReason::NONE) {
        return 3;
    }
    return 0;
}

// Serializa RunReport como JSON en out_path (o en stdout si out_path == nullptr).
// Retornos: 0 en éxito, 2 si no se pudo abrir el archivo de salida.
inline int write_report_json(const RunReport& r, uint32_t units, uint32_t ticks,
                             uint16_t checksum_every, uint64_t seed, const char* out_path) {
    std::FILE* fp = nullptr;
    const bool to_stdout = (out_path == nullptr);
    if (to_stdout) {
        fp = stdout;
    } else {
        fp = std::fopen(out_path, "wb");
        if (fp == nullptr) {
            return 2;
        }
    }

    std::fprintf(fp,
        "{\"schema\":\"chunsa_perf0_v1\",\"scenario_id\":\"synthetic_movement_v1@1\","
        "\"units\":%u,\"ticks\":%u,\"seed\":%llu,\"checksum_every\":%u,"
        "\"backend\":\"%s\",\"renderer\":\"headless\","
        "\"final_checksum\":\"%016llx\",\"checksums_seen\":%u,"
        "\"accepted\":%llu,\"rejected\":%llu,"
        "\"alloc_delta\":%llu,"
        "\"tick_ns\":{\"p50\":%llu,\"p95\":%llu,\"p99\":%llu,\"max\":%llu},"
        "\"percentile_method\":\"nearest-rank\",\"fatal\":\"%s\"}\n",
        static_cast<unsigned>(units),
        static_cast<unsigned>(ticks),
        static_cast<unsigned long long>(seed),
        static_cast<unsigned>(checksum_every),
        CHUNSA_WIDE128_BACKEND_NAME,
        static_cast<unsigned long long>(r.final_checksum),
        static_cast<unsigned>(r.checksums_seen),
        static_cast<unsigned long long>(r.accepted),
        static_cast<unsigned long long>(r.rejected),
        static_cast<unsigned long long>(r.alloc_delta),
        static_cast<unsigned long long>(r.p50_ns),
        static_cast<unsigned long long>(r.p95_ns),
        static_cast<unsigned long long>(r.p99_ns),
        static_cast<unsigned long long>(r.max_ns),
        fatal_reason_name(r.fatal)
    );

    if (!to_stdout) {
        std::fclose(fp);
    }
    return 0;
}

} // namespace chunsa