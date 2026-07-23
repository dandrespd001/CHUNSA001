// Test del replay v2 (effective_tick, Sprint 0.3-cierre): la agenda de comandos
// deja de ser implícita. Verifica (1) que grabar→cargar→reproducir es
// bit-exacto con la agenda auto-verificada (schedule_mismatches==0), (2) que la
// config de normalización §6.2 viaja en el archivo, y (3) que corromper un
// effective_tick grabado se DETECTA (la red de seguridad no es decorativa).
// Autor: Arquitecto.
// Actualizado Sprint 1.2 (SPEC-004 §10.1/§10.3, sonnet-5): ReplayWriter graba
// SIEMPRE en v3 (REPLAY_WRITE_VERSION) desde este sprint — el nombre del
// archivo/test se conserva (cubre la agenda auto-verificada, agnóstico de si
// el layout exacto es v2 o v3 salvo donde se comprueba `.version`
// explícitamente). El valor esperado del primer effective_tick también
// cambia: la ventana de setup (§10.3) da eff=0 a un target_tick=0 ingerido en
// el PRIMER Step, sin sumar delay (antes: eff=max(0,0+delay)=1). Ver
// test_replay_v3.cpp para la cobertura específica del gap D8 (unit_id) y del
// caso "legacy_payload_loss" de un stream v2 forzado.
#include <cstdint>
#include <cstdio>

#include "chunsa/driver.hpp"

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa;

int main() {
    const char* path = "test_replay_v2.curp";

    // (1) Grabar un replay con IA (mismo flujo que el CLI 'record': la IA se
    // ejecuta y sus comandos quedan grabados — G5/ADR-019).
    DriveOpts orec{};
    orec.units = 200; orec.ticks = 400; orec.checksum_every = 1;
    orec.seed = 20260716ull; orec.with_ai = true;
    ReplayWriter rec;
    rec.begin(orec.seed, orec.units, orec.ticks, orec.checksum_every,
              orec.human_input_delay_ticks, orec.max_future_command_ticks);
    orec.rec = &rec;
    DriveOut orout{};
    CHECK(drive_fresh(orec, orout) == 0);
    CHECK(rec.finish(orout.final_checksum, path) == 0);

    // (2) Cargar y validar el formato: versión (siempre v3 al grabar), config
    // §6.2 persistida y agenda paralela a los batches.
    ReplayData data;
    CHECK(replay_load(path, data) == 0);
    CHECK(data.version == 3u);
    CHECK(data.legacy_payload_loss == 0u);  // v3: la agenda es fiel por construcción
    CHECK(data.human_input_delay_ticks == 1u);
    CHECK(data.max_future_command_ticks == 20u);
    CHECK(data.eff_ticks.size() == data.ticks);
    // El primer comando (SPAWN_DEBUG con target_tick=0 en t=0) cae en la
    // ventana de setup (SPEC-004 §10.3): effective_tick = 0 SIN sumar delay
    // (antes de Sprint 1.2 era max(0, 0+delay) = 1).
    CHECK(!data.eff_ticks.empty() && !data.eff_ticks[0].empty());
    CHECK(data.eff_ticks[0][0] == 0u);

    // (3) Reproducir con la config del propio replay: checksum idéntico, IA
    // JAMÁS ejecutada (G5), y agenda exacta (cero discrepancias).
    DriveOpts over{};
    over.units = data.units; over.ticks = data.ticks;
    over.checksum_every = data.checksum_every; over.seed = data.seed;
    over.with_ai = false;
    over.human_input_delay_ticks = data.human_input_delay_ticks;
    over.max_future_command_ticks = data.max_future_command_ticks;
    over.feed = &data;
    DriveOut ov{};
    CHECK(drive_fresh(over, ov) == 0);
    CHECK(ov.final_checksum == data.final_checksum);
    CHECK(ov.ai_executions == 0);
    CHECK(ov.schedule_mismatches == 0);

    // (4) Red de seguridad: corromper un effective_tick grabado. La
    // reproducción no crashea, pero la discrepancia de agenda se detecta —
    // fallo ruidoso en vez de divergencia silenciosa del checksum.
    ReplayData tampered = data;
    CHECK(!tampered.eff_ticks.empty() && !tampered.eff_ticks[0].empty());
    tampered.eff_ticks[0][0] += 7u;
    DriveOpts overt = over;
    overt.feed = &tampered;
    DriveOut ovt{};
    CHECK(drive_fresh(overt, ovt) == 0);
    CHECK(ovt.schedule_mismatches > 0u);

    std::remove(path);

    if (g_fails == 0) { std::printf("replay_v2: OK\n"); return 0; }
    std::printf("replay_v2: %d fallos\n", g_fails);
    return 1;
}
