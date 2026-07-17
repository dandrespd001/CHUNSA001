// chunsa_sim_cli — SPEC-001 §13. Subcomando `golden` (paso 2 del sprint);
// `run`/`bench` llegan en el paso 6; `savetest`/`verify` en 0.1B.
// Autor: Arquitecto. (El CLI no es ruta de simulación: STL/iostream permitidos.)
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "chunsa/fatal.hpp"
#include "chunsa/wide128.hpp"
#include "chunsa/fixed64.hpp"
#include "chunsa/vec2fx.hpp"

namespace {

struct GoldenStats {
    long total = 0;
    long fails = 0;
};

std::vector<std::string> split_csv(const std::string& line) {
    std::vector<std::string> out;
    std::stringstream ss(line);
    std::string item;
    while (std::getline(ss, item, ',')) out.push_back(item);
    return out;
}

void report_fail(GoldenStats& st, const std::string& line, int64_t got,
                 chunsa::FatalReason got_fatal) {
    ++st.fails;
    if (st.fails <= 10) {
        std::cerr << "FALLO: " << line << "  → got=" << got
                  << " fatal=" << chunsa::fatal_reason_name(got_fatal) << "\n";
    }
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
        const std::string& expect_fatal = c[4];
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
        if (got != expect || expect_fatal != chunsa::fatal_reason_name(f)) {
            report_fail(st, line, got, f);
        }
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
        if (got.x.raw != ex || got.y.raw != ey || f != chunsa::FatalReason::NONE) {
            report_fail(st, line, got.x.raw, f);
        }
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

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty()) {
        std::cerr << "uso: chunsa_sim_cli {golden --vectors <dir> | run | bench | savetest | verify}\n";
        return 2;
    }
    const std::string& cmd = args[0];
    if (cmd == "golden") {
        std::string dir;
        for (size_t i = 1; i + 1 < args.size() + 1; ++i) {
            if (args[i] == "--vectors" && i + 1 < args.size()) dir = args[i + 1];
        }
        if (dir.empty()) { std::cerr << "golden: falta --vectors <dir>\n"; return 2; }
        return cmd_golden(dir);
    }
    if (cmd == "run" || cmd == "bench") {
        std::cerr << cmd << ": pendiente del paso 6 del Sprint 0.1A (SPEC-001 §13)\n";
        return 2;
    }
    if (cmd == "savetest" || cmd == "verify") {
        std::cerr << cmd << ": alcance 0.1B (SPEC-001 §13)\n";
        return 2;
    }
    std::cerr << "subcomando desconocido: " << cmd << "\n";
    return 2;
}
