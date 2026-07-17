#pragma once

// chunsa_sim — ring de snapshots race-free: (generation<<3)|state en UNA palabra (SPEC-001 §9.1)
// generado: minimax-m3 · revisado: Arquitecto 2026-07-17

#include <atomic>
#include <cstdint>
#include <cstring>

namespace chunsa {

// Estados codificados en los 3 bits bajos de la palabra de control (SPEC-001 §9.1).
enum : uint64_t {
    STATE_FREE      = 0,  // disponible para que el writer lo tome
    STATE_WRITING   = 1,  // writer rellenando el payload (lector lo ignora)
    STATE_PUBLISHED = 2,  // writer terminó; payload visible y estable
    STATE_READING   = 3,  // reader consumiendo (writer NUNCA lo recicla)
    STATE_MASK      = 0x7u
};

template <typename Payload>
struct SnapshotRing {
    struct Slot {
        std::atomic<uint64_t> ctrl;  // (generation << 3) | state
        Payload data;
    };

    static constexpr uint32_t N_SLOTS = 4;
    Slot slots[N_SLOTS];

    // Estado privado del writer (solo lo toca el hilo de sim).
    uint64_t next_generation;  // siguiente gen a usar (monotónica, basta con 61 bits)
    uint32_t writing_slot;     // slot actualmente en escritura

    // Inicialización single-thread: se ejecuta antes de lanzar workers. Relaxed
    // basta aquí porque std::thread() sincroniza-with el inicio del nuevo hilo,
    // así que estas escrituras son visibles al primer load del worker.
    void init() noexcept {
        for (uint32_t i = 0; i < N_SLOTS; ++i) {
            slots[i].ctrl.store(STATE_FREE, std::memory_order_relaxed);
        }
        next_generation = 1;  // 0 se reserva como "ningún publish aún"
        writing_slot = 0;
    }

    // ---------------- WRITER (hilo de sim, único) ----------------
    // Política: 1) cualquier FREE; 2) si no, PUBLISHED de menor generación.
    // Nunca reclama READING ni WRITING. Si el CAS falla, re-escanea (jamás bloquea).

    Payload* begin_write() noexcept {
        // Con 1 writer + 1 reader + 4 slots siempre hay candidato
        // (1 WRITING + ≤2 READING + ≥1 FREE|PUBLISHED). MAX_ATTEMPTS es un
        // techo defensivo para detectar invariantes rotos en instrumentación.
        constexpr uint32_t MAX_ATTEMPTS = 1u << 20;

        for (uint32_t a = 0; a < MAX_ATTEMPTS; ++a) {
            // --- Pase 1: cualquier FREE ---
            for (uint32_t i = 0; i < N_SLOTS; ++i) {
                // Acquire en el scan: si viéramos el estado antiguo, también
                // veríamos (si hiciese falta) cualquier dato previo del slot.
                // Para FREE no hay dato, pero el orden es explícito y barato.
                uint64_t cur = slots[i].ctrl.load(std::memory_order_acquire);
                if ((cur & STATE_MASK) != STATE_FREE) continue;

                uint64_t gen     = next_generation;
                uint64_t desired = (gen << 3) | STATE_WRITING;
                // CAS acq_rel:
                //   - éxito/acquire: sincroniza con el store release del reader
                //     que dejó este slot en FREE; garantiza que cualquier
                //     lectura nuestra anterior está ordenada antes del nuevo
                //     estado del slot.
                //   - éxito/release: publica que el slot está en WRITING, de
                //     modo que un reader que escanee no lo confunda con FREE.
                //   - fallo/acquire: re-leemos con acquire (mismo orden que un
                //     load independiente) y reintentamos.
                if (slots[i].ctrl.compare_exchange_weak(
                        cur, desired,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire)) {
                    writing_slot     = i;
                    next_generation  = gen + 1;  // consumimos la gen (writer only)
                    return &slots[i].data;
                }
                // CAS perdido: el estado cambió bajo nosotros. Re-escaneamos.
            }

            // --- Pase 2: PUBLISHED de menor generación (jamás READING/WRITING) ---
            uint32_t victim     = 0;
            uint64_t victim_gen = UINT64_MAX;
            for (uint32_t i = 0; i < N_SLOTS; ++i) {
                uint64_t cur = slots[i].ctrl.load(std::memory_order_acquire);
                if ((cur & STATE_MASK) != STATE_PUBLISHED) continue;
                uint64_t g = cur >> 3;
                if (g < victim_gen) {
                    victim_gen = g;
                    victim     = i;
                }
            }
            if (victim_gen == UINT64_MAX) {
                // Invariante 1W+1R+4: solo ocurre transitoriamente mientras el
                // reader abandona un READING a FREE. Cedemos CPU y re-escaneamos.
                continue;
            }

            uint64_t cur = slots[victim].ctrl.load(std::memory_order_acquire);
            if ((cur & STATE_MASK) != STATE_PUBLISHED) continue;  // cambió bajo nosotros
            uint64_t gen     = next_generation;
            uint64_t desired = (gen << 3) | STATE_WRITING;
            if (slots[victim].ctrl.compare_exchange_weak(
                    cur, desired,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                writing_slot    = victim;
                next_generation = gen + 1;
                return &slots[victim].data;
            }
            // CAS perdido: el reader se nos adelantó (PUBLISHED→READING). Re-scan.
        }
        return nullptr;  // defensivo: invariante roto
    }

    void publish() noexcept {
        const uint32_t i = writing_slot;
        // Solo el writer toca este slot entre begin_write y publish; la gen
        // está en nuestro flujo de instrucciones, así que relaxed basta para
        // re-leerla. La sincronización con el reader la da el store release
        // de abajo.
        uint64_t cur = slots[i].ctrl.load(std::memory_order_relaxed);
        uint64_t gen = cur >> 3;
        uint64_t desired = (gen << 3) | STATE_PUBLISHED;
        // Release: todas las escrituras al payload (previas en este hilo)
        // son visibles para cualquier reader que haga acquire sobre este
        // mismo ctrl y vea PUBLISHED. Es el "publish" del snapshot.
        slots[i].ctrl.store(desired, std::memory_order_release);
    }

    // ---------------- READER (hilo de presentación, único) ----------------
    // Precondición: hay exactamente UN lector. El lector puede retener
    // simultáneamente hasta 2 slots (prev/current) sin bloquear al writer:
    // el writer jamás reclama READING.

    struct ReadHandle {
        Payload*  data;       // válido solo si valid==true
        uint32_t  slot;       // índice físico (necesario para release)
        uint64_t  generation; // congelada; útil para ABA/diagnóstico
        bool      valid;
    };

    // Devuelve el PUBLISHED de mayor generación, transicionándolo a READING.
    // Si no hay PUBLISHED, devuelve valid=false (NO es error: el writer aún
    // no ha publicado nada, o todos están siendo consumidos).
    ReadHandle acquire_latest() noexcept {
        constexpr uint32_t MAX_ATTEMPTS = 1u << 20;
        ReadHandle h{nullptr, 0, 0, false};

        for (uint32_t a = 0; a < MAX_ATTEMPTS; ++a) {
            // 1) Escaneo: el PUBLISHED con mayor generación.
            //    Acquire: si vemos PUBLISHED, debemos ver el payload liberado
            //    por el writer con su store release inmediatamente anterior.
            uint32_t best     = 0;
            uint64_t best_gen = 0;
            bool found = false;
            for (uint32_t i = 0; i < N_SLOTS; ++i) {
                uint64_t cur = slots[i].ctrl.load(std::memory_order_acquire);
                if ((cur & STATE_MASK) != STATE_PUBLISHED) continue;
                uint64_t g = cur >> 3;
                if (!found || g > best_gen) {
                    best     = i;
                    best_gen = g;
                    found    = true;
                }
            }
            if (!found) return h;  // aún sin publicar nada

            // 2) Apropiación atómica: PUBLISHED(g) → READING(g) con la MISMA g.
            //    La g en expected elimina el ABA: si el writer recicló este
            //    slot (g' > g, o el estado ya no es PUBLISHED), el CAS falla
            //    y re-escaneamos para tomar la nueva mejor.
            //
            //    acquire en éxito: sincroniza con el store release del publish
            //    del writer → garantiza visibilidad del payload.
            //    acquire en fallo: re-leemos con el mismo orden que un load
            //    independiente.
            uint64_t expected = (best_gen << 3) | STATE_PUBLISHED;
            uint64_t desired  = (best_gen << 3) | STATE_READING;
            if (slots[best].ctrl.compare_exchange_weak(
                    expected, desired,
                    std::memory_order_acquire,
                    std::memory_order_acquire)) {
                h.data       = &slots[best].data;
                h.slot       = best;
                h.generation = best_gen;
                h.valid      = true;
                return h;
            }
            // CAS perdido: otro agente movió el estado. Re-escaneamos.
        }
        return h;  // defensivo
    }

    // READING → FREE. Release para que nuestras lecturas del payload (previas)
    // queden ordenadas antes de que el writer reuse el slot (su CAS acquire/
    // acq_rel en begin_write emparejará con este release).
    void release(ReadHandle& h) noexcept {
        if (!h.valid) return;
        uint64_t desired = (h.generation << 3) | STATE_FREE;
        slots[h.slot].ctrl.store(desired, std::memory_order_release);
        h.valid = false;
        h.data  = nullptr;
    }
};

} // namespace chunsa

// =====================================================================
// TEST DE ESTRÉS (compilar con -DCHUNSA_RING_TEST_MAIN)
// =====================================================================
#ifdef CHUNSA_RING_TEST_MAIN

#include <cstdio>
#include <thread>

struct TestPayload {
    uint64_t gen_echo;
    uint64_t pattern[8];
};

int main() {
    chunsa::SnapshotRing<TestPayload> ring{};
    ring.init();

    constexpr uint64_t N_PUB = 1'000'000;

    std::atomic<bool> writer_done{false};
    uint64_t ok_count  = 0;
    uint64_t bad_count = 0;
    bool     writer_oof = false;  // begin_write devolvió nullptr (invariante roto)

    std::thread writer([&]() {
        for (uint64_t g = 1; g <= N_PUB; ++g) {
            TestPayload* p = ring.begin_write();
            if (!p) { writer_oof = true; return; }
            p->gen_echo = g;
            for (int i = 0; i < 8; ++i) {
                p->pattern[i] = g * 31 + static_cast<uint64_t>(i);
            }
            ring.publish();
        }
        writer_done.store(true, std::memory_order_release);
    });

    std::thread reader([&]() {
        // Retenemos hasta 2 slots simultáneamente (prev/current) para
        // ejercitar la API. prev se libera justo después de adquirir el
        // siguiente h, de modo que durante la siguiente adquisición
        // mantenemos 2 slots vivos.
        chunsa::SnapshotRing<TestPayload>::ReadHandle prev{};

        auto process = [&](const chunsa::SnapshotRing<TestPayload>::ReadHandle& hh) {
            const uint64_t g = hh.data->gen_echo;
            bool coherent = true;
            for (int j = 0; j < 8; ++j) {
                if (hh.data->pattern[j] != g * 31 + static_cast<uint64_t>(j)) {
                    coherent = false;
                    break;
                }
            }
            if (coherent) ++ok_count; else ++bad_count;
        };

        for (uint64_t i = 0; i < N_PUB * 4; ++i) {  // margen: oversample
            auto h = ring.acquire_latest();
            if (!h.valid) {
                // Sin PUBLISHED: si el writer terminó, drena lo que quede y sal.
                if (writer_done.load(std::memory_order_acquire)) {
                    for (int d = 0; d < 16; ++d) {
                        auto hd = ring.acquire_latest();
                        if (!hd.valid) break;
                        process(hd);
                        if (prev.valid) ring.release(prev);
                        prev = hd;
                    }
                    break;
                }
                std::this_thread::yield();
                continue;
            }
            process(h);
            if (prev.valid) ring.release(prev);
            prev = h;
        }
        if (prev.valid) ring.release(prev);
    });

    writer.join();
    reader.join();

    std::printf("OK=%llu  BAD=%llu  writer_oof=%d\n",
                static_cast<unsigned long long>(ok_count),
                static_cast<unsigned long long>(bad_count),
                writer_oof ? 1 : 0);

    return (writer_oof || bad_count > 0) ? 1 : 0;
}

#endif // CHUNSA_RING_TEST_MAIN