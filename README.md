# AEON001 — kernel de simulación determinista (Sprint 0.1A)

RTS histórico (codename AEON): 15 épocas, progresión realista, tecnologías fundamentadas en ciencia real, eventos históricos. Este repositorio contiene el **kernel de simulación determinista** (C++20 puro, sin Godot) y crecerá por sprints según los documentos rectores.

**Documentos vinculantes** (viven junto al repo, en el directorio del proyecto):
- `SPEC-001_NUCLEO_SIMULACION_DETERMINISTA.md` v1.1 — APPROVED_FOR_EXECUTION (contrato de este código).
- `SPEC_ARQUITECTURA_BASE.md` v1.1.1 — arquitectura general.
- `Investigación/INDICE_MAESTRO.md` — GDD completo (leer siempre vía índice).

## Compilar y verificar (Linux)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
./build/aeon_sim_cli golden --vectors tests/determinism/golden   # matemática determinista
ctest --test-dir build --output-on-failure                        # propiedades (simetría de signo, deriva radial)
bash scripts_ci/local_gates.sh                                    # las 3 lanes (gcc/clang/portable) + comparación
```

## Estructura (base §2.1)

`addons/aeon_sim/core/` kernel C++ puro (sin tipos Godot) · `addons/aeon_sim/cli/` `aeon_sim_cli` · `tests/determinism/golden/` vectores de referencia generados por `tools/golden_gen/` · `data/` (YAML canónico, se llena desde 0.1B) · `docs/TOOLCHAIN.md` pins · `docs/adr/` decisiones.

## Procedencia

Parte del código es generado por MiniMax-M3 bajo specs cerradas y **revisado por el Arquitecto** antes de integrarse; cada archivo lo declara en su cabecera. La matemática está verificada por golden vectors independientes (generados por implementación de referencia en Python con enteros exactos).
