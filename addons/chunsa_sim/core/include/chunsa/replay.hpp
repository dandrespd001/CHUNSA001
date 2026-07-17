#pragma once

// chunsa_sim_cli — replay v1: log de input crudo por tick (SPEC-001 §11.3 subset 0.1B)
// generado: minimax-m3 · revisado: Arquitecto 2026-07-17
// Nota de alcance: 0.1B graba RawCommands (la normalización del kernel es
// determinista); el stream de ScheduledCommand con effective_tick llega en 0.2.

#include "chunsa/commands.hpp"
#include "chunsa/serialize.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace chunsa {

// ============================================================================
// helpers little-endian: escritura byte a byte sobre vector<uint8_t>
// ============================================================================
namespace replay_detail {

inline void pb_u8(std::vector<uint8_t>& b, uint8_t v) {
    b.push_back(v);
}

inline void pb_u16(std::vector<uint8_t>& b, uint16_t v) {
    b.push_back(static_cast<uint8_t>(v & 0xFFu));
    b.push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
}

inline void pb_u32(std::vector<uint8_t>& b, uint32_t v) {
    b.push_back(static_cast<uint8_t>(v         & 0xFFu));
    b.push_back(static_cast<uint8_t>((v >>  8) & 0xFFu));
    b.push_back(static_cast<uint8_t>((v >> 16) & 0xFFu));
    b.push_back(static_cast<uint8_t>((v >> 24) & 0xFFu));
}

inline void pb_u64(std::vector<uint8_t>& b, uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        b.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFFu));
    }
}

inline void pb_i32(std::vector<uint8_t>& b, int32_t v) {
    pb_u32(b, static_cast<uint32_t>(v));
}

inline void pb_i64(std::vector<uint8_t>& b, int64_t v) {
    pb_u64(b, static_cast<uint64_t>(v));
}

} // namespace replay_detail

// ============================================================================
// ReplayWriter — acumula en memoria, vuelca a disco en finish()
// ============================================================================
struct ReplayWriter {
    std::vector<uint8_t> buf;   // acumulación en memoria

    // Inicia el stream: escribe magic + versión + cabecera de la simulación.
    void begin(uint64_t seed, uint32_t units, uint32_t ticks, uint16_t checksum_every) {
        buf.clear();
        // magic "CURP" en little-endian: 0x43 0x55 0x52 0x50
        replay_detail::pb_u32(buf, 0x50525543u);
        // versión del formato
        replay_detail::pb_u32(buf, 1u);
        // cabecera
        replay_detail::pb_u64(buf, seed);
        replay_detail::pb_u32(buf, units);
        replay_detail::pb_u32(buf, ticks);
        replay_detail::pb_u16(buf, checksum_every);
    }

    // Añade un batch de n RawCommands correspondientes al tick actual.
    void tick_batch(const RawCommand* cmds, uint32_t n) {
        replay_detail::pb_u32(buf, n);
        for (uint32_t i = 0; i < n; ++i) {
            const RawCommand& c = cmds[i];
            replay_detail::pb_u32(buf, c.target_tick);
            replay_detail::pb_u16(buf, c.emitter);
            replay_detail::pb_u16(buf, static_cast<uint16_t>(c.type));
            replay_detail::pb_u64(buf, c.sequence);
            replay_detail::pb_u32(buf, c.p.handle.index);
            replay_detail::pb_u32(buf, c.p.handle.generation);
            replay_detail::pb_i64(buf, c.p.x_raw);
            replay_detail::pb_i64(buf, c.p.y_raw);
            replay_detail::pb_i32(buf, c.p.speed_mtpt);
        }
    }

    // Escribe el checksum final y vuelca el buffer a 'path'.
    // Retorna 0 ok, 2 error de E/S.
    int finish(uint64_t final_checksum, const char* path) {
        // trailer
        replay_detail::pb_u64(buf, final_checksum);

        if (!path) return 2;
        FILE* fp = std::fopen(path, "wb");
        if (!fp) return 2;

        const size_t total = buf.size();
        size_t written = 0;
        if (total > 0) {
            written = std::fwrite(buf.data(), 1, total, fp);
        }
        const int closed = std::fclose(fp);
        if (closed != 0) return 2;
        if (written != total) return 2;
        return 0;
    }
};

// ============================================================================
// ReplayData — estructura rellenada por replay_load
// ============================================================================
struct ReplayData {
    uint64_t seed{0};
    uint32_t units{0};
    uint32_t ticks{0};
    uint16_t checksum_every{0};
    std::vector<std::vector<RawCommand>> batches;   // batches[t]
    uint64_t final_checksum{0};
};

// ============================================================================
// Loader con límites duros aplicados ANTES de reservar memoria
// ============================================================================
namespace replay_detail {

// Límites del formato v1 (congelados para subset 0.1B)
constexpr uint64_t MAX_FILE_SIZE  = 256ull * 1024ull * 1024ull;  // 256 MiB
constexpr uint32_t MAX_TICKS      = 10'000'000u;                // 10 M
constexpr uint32_t MAX_PER_TICK   = 4096u;
constexpr uint64_t MAX_TOTAL_CMDS = 50'000'000ull;              // 50 M

} // namespace replay_detail

inline int replay_load(const char* path, ReplayData& out) {
    out = ReplayData{};

    if (!path) return 2;

    // --- cargar archivo completo a memoria --------------------------------
    FILE* fp = std::fopen(path, "rb");
    if (!fp) return 2;

    if (std::fseek(fp, 0, SEEK_END) != 0)   { std::fclose(fp); return 2; }
    const long sz_signed = std::ftell(fp);
    if (sz_signed < 0)                      { std::fclose(fp); return 2; }
    if (std::fseek(fp, 0, SEEK_SET) != 0)   { std::fclose(fp); return 2; }

    const uint64_t sz = static_cast<uint64_t>(sz_signed);

    // límite 256 MiB ANTES de reservar el buffer imagen
    if (sz > replay_detail::MAX_FILE_SIZE)  { std::fclose(fp); return 1; }

    std::vector<uint8_t> file;
    if (sz > 0) {
        file.resize(static_cast<size_t>(sz));
        const size_t r = std::fread(file.data(), 1, static_cast<size_t>(sz), fp);
        std::fclose(fp);
        if (r != static_cast<size_t>(sz)) return 2;
    } else {
        std::fclose(fp);
    }

    ByteReader rdr{file.data(), file.size(), 0, false};

    // --- cabecera ---------------------------------------------------------
    const uint32_t magic = rdr.u32();
    if (rdr.fail) return 1;
    if (magic != 0x50525543u) return 1;

    const uint32_t version = rdr.u32();
    if (rdr.fail) return 1;
    if (version != 1u) return 1;

    out.seed           = rdr.u64();
    out.units          = rdr.u32();
    out.ticks          = rdr.u32();
    out.checksum_every = rdr.u16();
    if (rdr.fail) return 1;

    // límite de ticks ANTES de reservar el vector de batches
    if (out.ticks > replay_detail::MAX_TICKS) return 1;
    out.batches.clear();
    out.batches.resize(out.ticks);

    // --- cuerpo: un bloque por tick ---------------------------------------
    uint64_t total_cmds = 0;

    for (uint32_t t = 0; t < out.ticks; ++t) {
        const uint32_t n = rdr.u32();
        if (rdr.fail) return 1;
        if (n > replay_detail::MAX_PER_TICK) return 1;

        std::vector<RawCommand> batch;
        batch.resize(n);
        for (uint32_t i = 0; i < n; ++i) {
            RawCommand& c = batch[i];
            c.target_tick  = rdr.u32();
            c.emitter      = rdr.u16();
            const uint16_t ty = rdr.u16();
            c.type         = static_cast<CommandType>(ty);
            c.sequence     = rdr.u64();
            c.p.handle.index      = rdr.u32();
            c.p.handle.generation = rdr.u32();
            c.p.x_raw      = rdr.i64();
            c.p.y_raw      = rdr.i64();
            c.p.speed_mtpt = rdr.i32();
        }
        if (rdr.fail) return 1;

        total_cmds += static_cast<uint64_t>(n);
        if (total_cmds > replay_detail::MAX_TOTAL_CMDS) return 1;

        out.batches[t] = std::move(batch);
    }

    // --- trailer: checksum final ------------------------------------------
    out.final_checksum = rdr.u64();
    if (rdr.fail) return 1;

    return 0;
}

} // namespace chunsa