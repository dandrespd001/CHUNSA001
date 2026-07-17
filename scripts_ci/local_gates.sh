#!/usr/bin/env bash
# Gates locales 0.1A: 3 lanes (gcc-nativo, clang-nativo, gcc-portable) deben
# producir golden vectors byte-idénticos. (SPEC-001 §14 G2-local; el G2
# Windows↔Linux completo corre en GitHub Actions.)
set -euo pipefail
cd "$(dirname "$0")/.."

declare -A CFG=(
  [gcc]="-DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCHUNSA_WIDE128_FORCE_PORTABLE=OFF"
  [clang]="-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCHUNSA_WIDE128_FORCE_PORTABLE=OFF"
  [portable]="-DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCHUNSA_WIDE128_FORCE_PORTABLE=ON"
)

mkdir -p build-reports
for lane in gcc clang portable; do
  echo "=== lane: $lane ==="
  cmake -B "build-$lane" -DCMAKE_BUILD_TYPE=Release ${CFG[$lane]} >/dev/null
  cmake --build "build-$lane" -j >/dev/null
  "./build-$lane/chunsa_sim_cli" golden --vectors tests/determinism/golden | tee "build-reports/golden-$lane.txt"
  ctest --test-dir "build-$lane" --output-on-failure -Q || { echo "ctest falló en $lane"; exit 1; }
done

# La línea GOLDEN incluye el nombre del backend (difiere por diseño): compara casos/fallos.
for lane in gcc clang portable; do
  sed 's/backend=[a-z0-9]*/backend=X/' "build-reports/golden-$lane.txt" > "build-reports/golden-$lane.norm"
done
if diff -q build-reports/golden-gcc.norm build-reports/golden-clang.norm >/dev/null \
   && diff -q build-reports/golden-gcc.norm build-reports/golden-portable.norm >/dev/null; then
  echo "LANES IDÉNTICAS — gate local OK"
else
  echo "DIVERGENCIA ENTRE LANES — gate FALLÓ"
  exit 1
fi
