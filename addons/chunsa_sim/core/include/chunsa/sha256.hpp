#pragma once

// chunsa_sim_core — SHA-256 (FIPS 180-4) para save_integrity_hash (SPEC-001 §11.2)
// generado: minimax-m3 · revisado: Arquitecto 2026-07-17

#include <cstdint>
#include <cstddef>

namespace chunsa {

// ============================================================================
// Constantes del estándar FIPS 180-4 (constexpr — sin tablas generadas en runtime).
// ============================================================================

// K[64]: 32 primeros bits de las raíces cúbicas de los 64 primeros primos
//        (FIPS 180-4 §4.2.2, Tabla 4).
inline constexpr uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

// H0_init[8]: 32 primeros bits de las raíces cuadradas de los 8 primeros primos
//             (FIPS 180-4 §5.3.3).
inline constexpr uint32_t H0_init[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

// ----------------------------------------------------------------------------
// Primitivas (constexpr, header-only).
// ----------------------------------------------------------------------------

// Rotación circular a la derecha sobre uint32_t (FIPS 180-4 §3.2.3).
inline constexpr uint32_t rotr32(uint32_t x, unsigned n) noexcept {
    return (x >> n) | (x << (32u - n));
}

// ============================================================================
// Contexto de hash incremental (streaming API). Cumple el contrato:
//   void init() noexcept;
//   void update(const void* data, size_t len) noexcept;
//   void final(uint8_t out[32]) noexcept;
// Tras final(), el estado queda inválido (no reutilizar el mismo objeto).
// ============================================================================

struct Sha256 {
    uint32_t state_[8];     // H0..H7 (acumuladores SHA-256 entre bloques)
    uint8_t  buffer_[64];   // bytes pendientes para completar un bloque
    size_t   buffer_len_;   // 0..63 — bytes válidos en buffer_
    uint64_t bit_count_;    // total de bits procesados por update()

    // Inicializa el contexto con los H0..H7 del estándar. Idempotente.
    void init() noexcept {
        for (int i = 0; i < 8; ++i) state_[i] = H0_init[i];
        buffer_len_ = 0;
        bit_count_  = 0;
    }

    // Absorbe `len` bytes desde `data`. Acepta cualquier alineación y
    // cualquier tamaño (internamente parte en bloques de 64 bytes).
    void update(const void* data, size_t len) noexcept {
        const uint8_t* p = static_cast<const uint8_t*>(data);
        bit_count_ += static_cast<uint64_t>(len) * 8u;

        // (1) Vaciar el buffer parcial si la entrada entrante lo completa.
        if (buffer_len_ > 0) {
            const size_t need = 64u - buffer_len_;
            const size_t take = (len < need) ? len : need;
            for (size_t i = 0; i < take; ++i) {
                buffer_[buffer_len_ + i] = p[i];
            }
            buffer_len_ += take;
            p   += take;
            len -= take;
            if (buffer_len_ == 64u) {
                process_block(buffer_);
                buffer_len_ = 0u;
            }
        }

        // (2) Procesar bloques completos directamente desde la entrada.
        while (len >= 64u) {
            process_block(p);
            p   += 64u;
            len -= 64u;
        }

        // (3) Guardar el resto (0..63 bytes) en el buffer parcial.
        for (size_t i = 0; i < len; ++i) {
            buffer_[buffer_len_ + i] = p[i];
        }
        buffer_len_ += len;
    }

    // Aplica el padding canónico, vacía el hash final en `out` y deja
    // el contexto en estado inválido (no llamar update otra vez).
    void final(uint8_t out[32]) noexcept {
        const uint64_t bits = bit_count_;

        // Padding FIPS 180-4 §5.1.1:
        //   1) append bit '1'    -> byte 0x80
        //   2) append bits '0'   hasta longitud ≡ 448 (mod 512)
        //   3) append longitud original en bits, big-endian 64-bit
        buffer_[buffer_len_++] = 0x80u;

        // Si los 8 bytes de longitud no caben en este bloque,
        // se procesa éste y se abre uno nuevo sólo para la longitud.
        if (buffer_len_ > 56u) {
            for (size_t i = buffer_len_; i < 64u; ++i) buffer_[i] = 0u;
            process_block(buffer_);
            buffer_len_ = 0u;
        }
        for (size_t i = buffer_len_; i < 56u; ++i) buffer_[i] = 0u;

        // Longitud en bits, escrita en big-endian en los bytes [56..63].
        uint64_t b = bits;
        for (int i = 7; i >= 0; --i) {
            buffer_[56 + i] = static_cast<uint8_t>(b & 0xffu);
            b >>= 8;
        }
        process_block(buffer_);
        buffer_len_ = 0u;

        // Volcar H0..H7 como 32 bytes big-endian en out[0..31].
        for (int i = 0; i < 8; ++i) {
            const uint32_t w = state_[i];
            out[i * 4 + 0] = static_cast<uint8_t>((w >> 24) & 0xffu);
            out[i * 4 + 1] = static_cast<uint8_t>((w >> 16) & 0xffu);
            out[i * 4 + 2] = static_cast<uint8_t>((w >>  8) & 0xffu);
            out[i * 4 + 3] = static_cast<uint8_t>( w        & 0xffu);
        }

        // Invalidar el contexto: tras final() el objeto deja de ser válido.
        for (int    i = 0; i < 8;  ++i) state_[i] = 0u;
        for (size_t i = 0; i < 64; ++i) buffer_[i] = 0u;
        bit_count_ = 0u;
    }

private:
    // Procesa un único bloque de 64 bytes (lectura big-endian) y
    // acumula el resultado sobre state_ in-place.
    void process_block(const uint8_t* block) noexcept {
        uint32_t W[64];

        // W[0..15]: reinterpretar el bloque como 16 palabras big-endian.
        for (int i = 0; i < 16; ++i) {
            W[i] = (static_cast<uint32_t>(block[i * 4 + 0]) << 24) |
                   (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(block[i * 4 + 2]) <<  8) |
                   (static_cast<uint32_t>(block[i * 4 + 3])      );
        }

        // W[16..63]: expansión de mensaje (FIPS 180-4 §6.2.2 paso 1).
        for (int i = 16; i < 64; ++i) {
            const uint32_t s0 = rotr32(W[i - 15],  7) ^ rotr32(W[i - 15], 18) ^ (W[i - 15] >>  3);
            const uint32_t s1 = rotr32(W[i -  2], 17) ^ rotr32(W[i -  2], 19) ^ (W[i -  2] >> 10);
            W[i] = s1 + W[i - 7] + s0 + W[i - 16];
        }

        // Cargar working variables (FIPS 180-4 §6.2.2 paso 2).
        uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
        uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];

        // Bucle de compresión: 64 rondas (FIPS 180-4 §6.2.2 paso 3).
        for (int t = 0; t < 64; ++t) {
            const uint32_t bigS1 = rotr32(e,  6) ^ rotr32(e, 11) ^ rotr32(e, 25);
            const uint32_t ch    = (e & f) ^ ((~e) & g);
            const uint32_t T1    = h + bigS1 + ch + K[t] + W[t];
            const uint32_t bigS0 = rotr32(a,  2) ^ rotr32(a, 13) ^ rotr32(a, 22);
            const uint32_t maj   = (a & b) ^ (a & c) ^ (b & c);
            const uint32_t T2    = bigS0 + maj;

            h = g;
            g = f;
            f = e;
            e = d + T1;
            d = c;
            c = b;
            b = a;
            a = T1 + T2;
        }

        // Acumular en H (FIPS 180-4 §6.2.2 paso 4).
        state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
        state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
    }
};

// One-shot: encapsula init/update/final para un único bloque de datos.
// Equivalente a un único ctx.update(data, len) sobre un Sha256 recién inicializado.
inline void sha256(const void* data, size_t len, uint8_t out[32]) noexcept {
    Sha256 ctx;
    ctx.init();
    ctx.update(data, len);
    ctx.final(out);
}

} // namespace chunsa

// ============================================================================
// AUTOVERIFICACIÓN — vectores canónicos FIPS 180-4 Apéndice B / NIST CAVP.
// El Arquitecto puede añadirlos a un test unitario estático comparando
// byte a byte la salida de chunsa::sha256(...) con los siguientes hashes:
//
//   SHA256("")    = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
//   SHA256("abc") = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
//   SHA256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")
//                 = 248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1
//
// Cobertura del test mínimo del Arquitecto:
//   1) Cadena vacía  -> verifica inicialización y padding con un único bloque.
//   2) "abc" (3 B)   -> verifica padding dentro del mismo bloque (length 24 bits).
//   3) Cadena de 56 B (448 bits) -> caso límite de padding: la longitud cae
//      exactamente en el slot de 8B y se procesa un segundo bloque vacío.
// ============================================================================