// Tests de estado 0.1B: SHA-256 (vectores NIST FIPS 180-4), roundtrip completo
// save→load bit-exacto y caja de IA. Autor: Arquitecto.
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "chunsa/driver.hpp"

// Requerido por cli_run/driver? no: el contador vive en main del CLI. Aquí definimos el nuestro.
uint64_t g_chunsa_allocs = 0;

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

static bool sha_hex_is(const char* msg, const char* hex) {
    uint8_t d[32];
    sha256(msg, std::strlen(msg), d);
    char out[65];
    for (int i = 0; i < 32; ++i) std::snprintf(out + i * 2, 3, "%02x", d[i]);
    return std::strcmp(out, hex) == 0;
}

int main() {
    // 1) NIST FIPS 180-4
    CHECK(sha_hex_is("", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    CHECK(sha_hex_is("abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    CHECK(sha_hex_is("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
                     "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));

    // 2) Roundtrip save→load bit-exacto (con IA activa y job en vuelo natural)
    {
        MatchConfig01A cfg{};
        cfg.max_entities = 64; cfg.player_count = 2;
        cfg.human_input_delay_ticks = 1; cfg.max_future_command_ticks = 20;
        cfg.checksum_every_ticks = 1; cfg.map_tiles_x = 256; cfg.map_tiles_y = 256;
        cfg.seed = 42;
        auto* gs = new GameState(); gs_init(*gs, cfg);
        AiJobBox box; ai_box_init(box, 1);
        AiRuntimeV1 rt{0, 30};  // 60 unidades: 30 spawns de IA
        DriveOpts o{}; o.units = 60; o.ticks = 9; o.checksum_every = 1;
        o.seed = 42; o.with_ai = true;
        DriveOut out{};
        CHECK(drive(o, *gs, box, rt, out) == 0);
        CHECK(box.state == AiJobState::COMPLETED);  // dispatch fase 7, due en 11
        CHECK(save_game(*gs, box, rt, "test_state.sav") == 0);
        auto* g2 = new GameState(); AiJobBox b2{}; AiRuntimeV1 r2{};
        CHECK(load_game(*g2, b2, r2, "test_state.sav") == 0);
        CHECK(state_checksum_v1(*gs) == state_checksum_v1(*g2));
        CHECK(continuation_checksum(*gs, box, rt) == continuation_checksum(*g2, b2, r2));
        // 3) Manipulación: un byte volteado debe rechazarse por digest
        std::FILE* f = std::fopen("test_state.sav", "r+b");
        std::fseek(f, 200, SEEK_SET);
        int c = std::fgetc(f); std::fseek(f, 200, SEEK_SET); std::fputc(c ^ 0x40, f);
        std::fclose(f);
        auto* g3 = new GameState(); AiJobBox b3{}; AiRuntimeV1 r3{};
        CHECK(load_game(*g3, b3, r3, "test_state.sav") == 1);
        delete gs; delete g2; delete g3;
    }

    if (g_fails == 0) { std::printf("state: OK\n"); return 0; }
    std::printf("state: %d fallos\n", g_fails);
    return 1;
}
