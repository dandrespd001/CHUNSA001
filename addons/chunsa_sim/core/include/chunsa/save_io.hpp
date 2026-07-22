#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "chunsa/game_state.hpp"
#include "chunsa/serialize.hpp"
#include "chunsa/sha256.hpp"
#include "chunsa/ai_stub.hpp"

// chunsa — save_io: envelope de save con orden de carga SEGURO (SPEC-001 §11.2).
// Autor: Arquitecto (asiento de seguridad; no delegado).
// Orden inviolable al cargar: caps duros → envelope fijo → aritmética checked →
// digest sobre header+payload → versiones → recién entonces parsear estado.
// Nota 0.1B: compression=0 (none); zstd llega en 0.2 (desviación documentada en
// el reporte del sprint). El digest es INTEGRIDAD, no autenticación (§11.2).
// Este header es de capa CLI/adaptador (usa FILE/vector); jamás se incluye
// desde código de Step().

namespace chunsa {

inline constexpr uint32_t SAVE_MAGIC = 0x4E554843u;  // "CHUN" LE
inline constexpr uint32_t SAVE_FORMAT_VERSION = 6;  // v6: +economía (deposits/stock/citizens)
inline constexpr uint32_t SAVE_PROTOCOL_VERSION = 1;
inline constexpr uint32_t SAVE_KERNEL_VERSION = 1;
inline constexpr uint32_t SAVE_DATA_SCHEMA_VERSION = 0;   // sin blob de datos en 0.1B
inline constexpr size_t SAVE_HARD_MAX_FILE = 64u * 1024u * 1024u;
inline constexpr size_t SAVE_ENVELOPE_SIZE = 64;          // fijo, sin alocación al leerlo
inline constexpr size_t SAVE_HEADER_SIZE = 28;            // 7 × u32 (versiones + tick)
inline constexpr char SAVE_DOMAIN[] = "CHUNSA_SAVE_V1";   // 14 chars, sin NUL

namespace detail {

inline void put_u32(uint8_t* p, uint32_t v) noexcept {
    p[0] = uint8_t(v); p[1] = uint8_t(v >> 8); p[2] = uint8_t(v >> 16); p[3] = uint8_t(v >> 24);
}
inline uint32_t get_u32(const uint8_t* p) noexcept {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

}  // namespace detail

// Guarda estado + caja de IA. Retorna 0 ok, 2 E/S, 3 overflow interno.
inline int save_game(const GameState& g, const AiJobBox& box, const AiRuntimeV1& rt,
                     const char* path) {
    // PAYLOAD = [gs_size u32][gs_bytes][ai_bytes]  (framing del save_io)
    std::vector<uint8_t> gs_buf(GS_SERIALIZE_MAX);
    const size_t gs_len = gs_serialize(g, gs_buf.data(), gs_buf.size());
    if (gs_len == 0) return 3;

    uint8_t ai_buf[8192];
    ByteWriter aw{ai_buf, sizeof(ai_buf), 0, false};
    ai_serialize(box, rt, aw);
    if (aw.overflow) return 3;

    std::vector<uint8_t> payload(4 + gs_len + aw.len);
    detail::put_u32(payload.data(), static_cast<uint32_t>(gs_len));
    std::memcpy(payload.data() + 4, gs_buf.data(), gs_len);
    std::memcpy(payload.data() + 4 + gs_len, ai_buf, aw.len);

    // HEADER canónico (7 × u32 LE).
    uint8_t header[SAVE_HEADER_SIZE];
    detail::put_u32(header + 0, SAVE_PROTOCOL_VERSION);
    detail::put_u32(header + 4, SAVE_KERNEL_VERSION);
    detail::put_u32(header + 8, SAVE_DATA_SCHEMA_VERSION);
    detail::put_u32(header + 12, RNG_ALGO_VERSION);
    detail::put_u32(header + 16, CHECKSUM_ALGO_VERSION);
    detail::put_u32(header + 20, AI_ALGO_VERSION);
    detail::put_u32(header + 24, g.tick);

    // DIGEST = SHA256(domain || header || payload) — cubre header Y payload (§11.2).
    uint8_t digest[32];
    {
        Sha256 h;
        h.init();
        h.update(SAVE_DOMAIN, sizeof(SAVE_DOMAIN) - 1);
        h.update(header, sizeof(header));
        h.update(payload.data(), payload.size());
        h.final(digest);
    }

    uint8_t env[SAVE_ENVELOPE_SIZE] = {};
    detail::put_u32(env + 0, SAVE_MAGIC);
    detail::put_u32(env + 4, SAVE_FORMAT_VERSION);
    detail::put_u32(env + 8, static_cast<uint32_t>(sizeof(header)));
    detail::put_u32(env + 12, static_cast<uint32_t>(payload.size()));
    detail::put_u32(env + 16, 0);  // compression = none (0.1B)
    std::memcpy(env + 32, digest, 32);

    std::FILE* f = std::fopen(path, "wb");
    if (!f) return 2;
    const bool ok = std::fwrite(env, 1, sizeof(env), f) == sizeof(env)
                 && std::fwrite(header, 1, sizeof(header), f) == sizeof(header)
                 && std::fwrite(payload.data(), 1, payload.size(), f) == payload.size();
    std::fclose(f);
    return ok ? 0 : 2;
}

// Carga con el orden seguro. Retorna 0 ok, 1 malformado/integridad/versión, 2 E/S.
// Ante cualquier retorno != 0, el GameState queda NO utilizable (descartar).
inline int load_game(GameState& g, AiJobBox& box, AiRuntimeV1& rt, const char* path) {
    std::FILE* f = std::fopen(path, "rb");
    if (!f) return 2;
    // (1) Tamaño contra el cap duro ANTES de leer el cuerpo.
    std::fseek(f, 0, SEEK_END);
    const long fsz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (fsz < long(SAVE_ENVELOPE_SIZE) || size_t(fsz) > SAVE_HARD_MAX_FILE) {
        std::fclose(f);
        return 1;
    }
    // (2) Envelope fijo, sin alocación dependiente de datos no validados.
    uint8_t env[SAVE_ENVELOPE_SIZE];
    if (std::fread(env, 1, sizeof(env), f) != sizeof(env)) { std::fclose(f); return 2; }
    if (detail::get_u32(env + 0) != SAVE_MAGIC) { std::fclose(f); return 1; }
    if (detail::get_u32(env + 4) != SAVE_FORMAT_VERSION) { std::fclose(f); return 1; }
    const uint32_t header_size = detail::get_u32(env + 8);
    const uint32_t payload_size = detail::get_u32(env + 12);
    const uint32_t compression = detail::get_u32(env + 16);
    if (compression != 0) { std::fclose(f); return 1; }
    if (header_size != SAVE_HEADER_SIZE) { std::fclose(f); return 1; }
    // (3) Aritmética checked de tamaños contra el archivo real.
    const uint64_t expected = uint64_t(SAVE_ENVELOPE_SIZE) + header_size + payload_size;
    if (expected != uint64_t(fsz)) { std::fclose(f); return 1; }
    // (4) Leer header+payload (acotados por los caps ya validados).
    std::vector<uint8_t> rest(size_t(header_size) + payload_size);
    if (std::fread(rest.data(), 1, rest.size(), f) != rest.size()) { std::fclose(f); return 2; }
    std::fclose(f);
    // (5) DIGEST ANTES DE PARSEAR NADA.
    uint8_t digest[32];
    {
        Sha256 h;
        h.init();
        h.update(SAVE_DOMAIN, sizeof(SAVE_DOMAIN) - 1);
        h.update(rest.data(), rest.size());
        h.final(digest);
    }
    if (std::memcmp(digest, env + 32, 32) != 0) return 1;
    // (6) Versiones exactas.
    const uint8_t* hd = rest.data();
    if (detail::get_u32(hd + 0) != SAVE_PROTOCOL_VERSION) return 1;
    if (detail::get_u32(hd + 4) != SAVE_KERNEL_VERSION) return 1;
    if (detail::get_u32(hd + 8) != SAVE_DATA_SCHEMA_VERSION) return 1;
    if (detail::get_u32(hd + 12) != RNG_ALGO_VERSION) return 1;
    if (detail::get_u32(hd + 16) != CHECKSUM_ALGO_VERSION) return 1;
    if (detail::get_u32(hd + 20) != AI_ALGO_VERSION) return 1;
    const uint32_t saved_tick = detail::get_u32(hd + 24);
    // (7) Parseo del estado (bounded por diseño del serializador).
    const uint8_t* pl = rest.data() + header_size;
    if (payload_size < 4) return 1;
    const uint32_t gs_len = detail::get_u32(pl);
    if (uint64_t(4) + gs_len > payload_size) return 1;
    if (!gs_deserialize(g, pl + 4, gs_len)) return 1;
    if (g.tick != saved_tick) return 1;  // consistencia header↔STATE (§11.2)
    ByteReader ar{pl + 4 + gs_len, payload_size - 4 - gs_len, 0, false};
    if (!ai_deserialize(box, rt, ar)) return 1;
    return 0;
}

}  // namespace chunsa
