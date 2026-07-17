// chunsa_sim_cli — SPEC-001 §13. golden + run + bench (0.1A); savetest/verify en 0.1B.
// Autor: Arquitecto (golden/main) · escenario en chunsa/cli_run.hpp (minimax-m3, revisado).
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Contador global de allocations — gate cero-alloc en tick estable (SPEC-001 §1.2).
// pragma: GCC16 no reconoce el par new/delete REEMPLAZADO y acusa mismatch con free().
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
#endif
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4595 28251 4559)
#endif
#if defined(__GNUC__) && !defined(__clang__) || defined(__clang__)
// (bloque GCC/Clang continúa abajo)
uint64_t g_chunsa_allocs = 0;
void* operator new(std::size_t n) { ++g_chunsa_allocs; void* p = std::malloc(n); if (!p) std::abort(); return p; }
void* operator new[](std::size_t n) { ++g_chunsa_allocs; void* p = std::malloc(n); if (!p) std::abort(); return p; }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
#pragma GCC diagnostic pop
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "chunsa/fatal.hpp"
#include "chunsa/wide128.hpp"
#include "chunsa/fixed64.hpp"
#include "chunsa/vec2fx.hpp"
#include "chunsa/cli_run.hpp"
#include "chunsa/driver.hpp"

namespace {

struct GoldenStats { long total = 0; long fails = 0; };

std::vector<std::string> split_csv(const std::string& line) {
    std::vector<std::string> out; std::stringstream ss(line); std::string item;
    while (std::getline(ss, item, ',')) out.push_back(item);
    return out;
}

void report_fail(GoldenStats& st, const std::string& line, int64_t got, chunsa::FatalReason f) {
    ++st.fails;
    if (st.fails <= 10)
        std::cerr << "FALLO: " << line << "  → got=" << got
                  << " fatal=" << chunsa::fatal_reason_name(f) << "\n";
}

bool run_fixed64_file(const std::string& path, GoldenStats& st) {
    std::ifstream in(path);
    if (!in) { std::cerr << "no se pudo abrir " << path << "\n"; return false; }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto c = split_csv(line);
        if (c.size() != 5) { std::cerr << "línea malformada: " << line << "\n"; return false; }
        const std::string& op = c[0];
        ++st.total;
        chunsa::FatalReason f = chunsa::FatalReason::NONE;
        int64_t got = 0;
        if (op == "isqrt") {
            uint64_t a = std::strtoull(c[1].c_str(), nullptr, 10);
            uint64_t r = chunsa::isqrt_u64(a);
            uint64_t expect = std::strtoull(c[3].c_str(), nullptr, 10);
            if (r != expect) report_fail(st, line, static_cast<int64_t>(r), f);
            continue;
        }
        int64_t a = std::strtoll(c[1].c_str(), nullptr, 10);
        int64_t b = std::strtoll(c[2].c_str(), nullptr, 10);
        int64_t expect = std::strtoll(c[3].c_str(), nullptr, 10);
        if (op == "add")        got = chunsa::fx_add({a}, {b}, f).raw;
        else if (op == "sub")   got = chunsa::fx_sub({a}, {b}, f).raw;
        else if (op == "neg")   got = chunsa::fx_neg({a}, f).raw;
        else if (op == "mul")   got = chunsa::fx_mul({a}, {b}, f).raw;
        else if (op == "div")   got = chunsa::fx_div({a}, {b}, f).raw;
        else if (op == "trunc") got = chunsa::fx_trunc({a});
        else { std::cerr << "op desconocida: " << op << "\n"; return false; }
        if (got != expect || c[4] != chunsa::fatal_reason_name(f)) report_fail(st, line, got, f);
    }
    return true;
}

bool run_normalize_file(const std::string& path, GoldenStats& st) {
    std::ifstream in(path);
    if (!in) { std::cerr << "no se pudo abrir " << path << "\n"; return false; }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto c = split_csv(line);
        if (c.size() != 4) { std::cerr << "línea malformada: " << line << "\n"; return false; }
        ++st.total;
        chunsa::FatalReason f = chunsa::FatalReason::NONE;
        chunsa::Vec2Fx d{{std::strtoll(c[0].c_str(), nullptr, 10)},
                         {std::strtoll(c[1].c_str(), nullptr, 10)}};
        chunsa::Vec2Fx got = chunsa::normalize_v1(d, f);
        int64_t ex = std::strtoll(c[2].c_str(), nullptr, 10);
        int64_t ey = std::strtoll(c[3].c_str(), nullptr, 10);
        if (got.x.raw != ex || got.y.raw != ey || f != chunsa::FatalReason::NONE)
            report_fail(st, line, got.x.raw, f);
    }
    return true;
}

int cmd_golden(const std::string& dir) {
    GoldenStats st;
    if (!run_fixed64_file(dir + "/fixed64_v1.csv", st)) return 2;
    if (!run_normalize_file(dir + "/normalize_v1.csv", st)) return 2;
    std::cout << "GOLDEN backend=" << CHUNSA_WIDE128_BACKEND_NAME << " casos=" << st.total
              << " fallos=" << st.fails << (st.fails == 0 ? "  [OK]" : "  [FAIL]") << "\n";
    return st.fails == 0 ? 0 : 1;
}

uint64_t opt_u64(const std::vector<std::string>& a, const std::string& k, uint64_t d) {
    for (size_t i = 0; i + 1 < a.size(); ++i)
        if (a[i] == k) return std::strtoull(a[i + 1].c_str(), nullptr, 10);
    return d;
}

const char* opt_str(const std::vector<std::string>& a, const std::string& k) {
    for (size_t i = 0; i + 1 < a.size(); ++i)
        if (a[i] == k) return a[i + 1].c_str();
    return nullptr;
}

bool has_flag(const std::vector<std::string>& a, const std::string& k) {
    for (const auto& s : a) if (s == k) return true;
    return false;
}

int cmd_run(const std::vector<std::string>& args, bool is_bench) {
    if (has_flag(args, "--selftest-g1")) {
        // G1 rápido de CI: 2 corridas idénticas, checksum bit-exacto.
        chunsa::RunReport r1{}, r2{};
        const int c1 = chunsa::run_synthetic(600, 2000, 1, 20260716ull, r1);
        const int c2 = chunsa::run_synthetic(600, 2000, 1, 20260716ull, r2);
        const bool ok = (c1 == 0 && c2 == 0 && r1.final_checksum == r2.final_checksum
                         && r1.alloc_delta == 0 && r2.alloc_delta == 0);
        std::cout << "G1 selftest: alloc_delta=" << r1.alloc_delta << " " << (ok ? "OK" : "FAIL")
                  << " checksum=" << std::hex << r1.final_checksum << std::dec << "\n";
        return ok ? 0 : 1;
    }
    const uint32_t units = static_cast<uint32_t>(opt_u64(args, "--units", 600));
    const uint32_t ticks = static_cast<uint32_t>(opt_u64(args, "--ticks", is_bench ? 1000 : 2000));
    const uint16_t ck = static_cast<uint16_t>(opt_u64(args, "--checksum-every", 20));
    const uint64_t seed = opt_u64(args, "--seed", 20260716ull);
    chunsa::RunReport rep{};
    const int code = chunsa::run_synthetic(units, ticks, ck, seed, rep);
    if (code == 2) { std::cerr << "config inválida\n"; return 2; }
    chunsa::write_report_json(rep, units, ticks, ck, seed, opt_str(args, "--out"));
    return code;
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty()) {
        std::cerr << "uso: chunsa_sim_cli {golden --vectors <dir> | run [--units N --ticks T "
                     "--checksum-every K --seed S --out F | --selftest-g1] | bench | savetest | verify}\n";
        return 2;
    }
    const std::string& cmd = args[0];
    if (cmd == "golden") {
        const char* dir = opt_str(args, "--vectors");
        if (!dir) { std::cerr << "golden: falta --vectors <dir>\n"; return 2; }
        return cmd_golden(dir);
    }
    if (cmd == "run") return cmd_run(args, false);
    if (cmd == "bench") return cmd_run(args, true);
    if (cmd == "savetest") {
        // G3 (sin --ai) / G4 (con --ai): corrida A guarda en N y sigue a M;
        // corrida B carga el save y sigue a M. Ambas deben ser bit-exactas.
        chunsa::DriveOpts a{};
        a.units = static_cast<uint32_t>(opt_u64(args, "--units", 200));
        a.ticks = static_cast<uint32_t>(opt_u64(args, "--resume-to", 400));
        a.checksum_every = 1;
        a.seed = opt_u64(args, "--seed", 20260716ull);
        a.with_ai = has_flag(args, "--ai");
        a.save_at = static_cast<uint32_t>(opt_u64(args, "--save-at", 200));
        const char* sp = opt_str(args, "--save-file");
        a.save_path = sp ? sp : "savetest.sav";
        a.hold_dispatched_until_save = has_flag(args, "--hold-dispatched");
        chunsa::DriveOut oa{};
        if (chunsa::drive_fresh(a, oa) != 0 || oa.save_result != 0) {
            std::cerr << "savetest: corrida A falló\n";
            return 2;
        }
        auto* gs = new chunsa::GameState();
        chunsa::AiJobBox box{};
        chunsa::AiRuntimeV1 rt{};
        if (chunsa::load_game(*gs, box, rt, a.save_path) != 0) {
            std::cerr << "savetest: load falló\n";
            delete gs;
            return 1;
        }
        chunsa::DriveOpts b = a;
        b.save_path = nullptr;
        chunsa::DriveOut ob{};
        const int cb = chunsa::drive(b, *gs, box, rt, ob);
        delete gs;
        const bool ok = (cb == 0 && oa.final_checksum == ob.final_checksum
                         && oa.continuation_checksum == ob.continuation_checksum);
        std::cout << (a.with_ai ? "G4" : "G3") << " savetest(save@" << a.save_at
                  << (a.hold_dispatched_until_save ? ",hold" : "") << "): "
                  << (ok ? "OK" : "FAIL") << " state=" << std::hex << oa.final_checksum
                  << " cont=" << oa.continuation_checksum << std::dec << "\n";
        return ok ? 0 : 1;
    }
    if (cmd == "record") {
        chunsa::DriveOpts o{};
        o.units = static_cast<uint32_t>(opt_u64(args, "--units", 200));
        o.ticks = static_cast<uint32_t>(opt_u64(args, "--ticks", 400));
        o.checksum_every = 1;
        o.seed = opt_u64(args, "--seed", 20260716ull);
        o.with_ai = true;   // el replay debe contener comandos de IA (G5)
        chunsa::ReplayWriter rec;
        rec.begin(o.seed, o.units, o.ticks, o.checksum_every);
        o.rec = &rec;
        chunsa::DriveOut out{};
        if (chunsa::drive_fresh(o, out) != 0) return 3;
        const char* path = opt_str(args, "--out");
        if (!path) path = "test.curp";
        if (rec.finish(out.final_checksum, path) != 0) return 2;
        std::cout << "record: " << o.ticks << " ticks → " << path << " checksum="
                  << std::hex << out.final_checksum << std::dec << "\n";
        return 0;
    }
    if (cmd == "verify") {
        const char* path = opt_str(args, "--replay");
        if (!path) { std::cerr << "verify: falta --replay\n"; return 2; }
        chunsa::ReplayData data;
        const int lc = chunsa::replay_load(path, data);
        if (lc != 0) { std::cerr << "verify: replay malformado/E-S (" << lc << ")\n"; return lc; }
        chunsa::DriveOpts o{};
        o.units = data.units; o.ticks = data.ticks;
        o.checksum_every = data.checksum_every; o.seed = data.seed;
        o.with_ai = false;          // G5: la IA JAMÁS se ejecuta al reproducir
        o.feed = &data;
        chunsa::DriveOut out{};
        const int c = chunsa::drive_fresh(o, out);
        const bool ok = (c == 0 && out.final_checksum == data.final_checksum
                         && out.ai_executions == 0);
        std::cout << "G5 verify: " << (ok ? "OK" : "FAIL")
                  << " ai_executions=" << out.ai_executions
                  << " checksum=" << std::hex << out.final_checksum << std::dec << "\n";
        return ok ? 0 : 1;
    }
    if (cmd == "loadtest") {
        // Harness de fuzzing: cargar un save arbitrario JAMÁS debe crashear.
        const char* path = args.size() > 1 ? args[1].c_str() : nullptr;
        if (!path) { std::cerr << "loadtest: falta <archivo>\n"; return 2; }
        auto* gs = new chunsa::GameState();
        chunsa::AiJobBox box{};
        chunsa::AiRuntimeV1 rt{};
        const int c = chunsa::load_game(*gs, box, rt, path);
        delete gs;
        std::cout << "loadtest: " << c << "\n";
        return c;
    }
    std::cerr << "subcomando desconocido: " << cmd << "\n";
    return 2;
}
