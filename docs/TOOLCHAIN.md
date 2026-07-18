# TOOLCHAIN — pins reproducibles (L0-TOOLCHAIN, SPEC-001 §1.1.2 de la base)

**Acto de pin: 2026-07-16.** Upgrades solo entre fases, vía ADR; todo upgrade re-corre gates y golden vectors.

## Local (máquina de desarrollo, CachyOS Linux x86-64)

| Componente | Versión fijada |
|---|---|
| GCC | 16.1.1 20260625 |
| Clang | 22.1.8 |
| CMake | 4.4.0 |
| Git | 2.55.0 |
| C++ | **C++20**, extensiones OFF |
| Flags | `-O2` release · `-Wall -Wextra -Wshadow -Werror` · **fast-math PROHIBIDO** (no hay FP en el kernel; prohibido igualmente por contrato) |

## Lanes de verificación numérica

- `nativo` — `__int128` (GCC/Clang) / intrinsics (MSVC x64).
- `CHUNSA_WIDE128_FORCE_PORTABLE=ON` — backend portable de referencia; **obligatorio en CI** aunque el host tenga `__int128`.
- Gate local (sin runner Windows): golden vectors **byte-idénticos** entre GCC-nativo, Clang-nativo y GCC-portable.

## Windows (CI, pendiente de primer run en GitHub Actions)

- MSVC ≥ VS 2019 x64 (`windows-latest`). **Verificación obligatoria** de `_mul128/_umul128/_div128/_udiv128` (Anexo B.1 de SPEC-001): si faltara, el backend portable pasa a ser el default en Windows.

## Componentes de fases futuras (registrados, no usados en 0.1A)

| Componente | Pin |
|---|---|
| Godot | 4.7.1-stable, commit `a13da4feb` — sha256 del editor y export templates: registrar al descargar (0.2, adaptador GDExtension) |
| godot-cpp | commit exacto + `extension_api.json` versionado en repo: pendiente (0.2) |
| Steam Linux Runtime | target `steamrt4`; `sdk_image_digest`: pendiente de pin al preparar builds de distribución |
| zstd | `v1.5.7`, commit `f8745da6ff1ad1e7bab384bd1f9d742439278e99` — `zstd.c` sha256 `208e110a1f052d007242d4eef6ed20a03ab1dc6e13eb2c4ff8d158112120ba8e`, `zstd.h` sha256 `9b4bc8245565c98ccfc61c07749928b57e7c0f6fddb0530c4f6aa1971893d88b` (single-file vía `build/single_file_libs/create_single_file_library.sh`; usado por saves: 0.1B) |
| xxHash (XXH3_64) | pendiente de vendor (saves: 0.1B) |

## Definición de "reproducible" (base §1.2)

1. **Build hermético** — este archivo + CI fijan inputs/toolchain (contenedor de build: pendiente, se registra aquí al crearse).
2. **Reproducibilidad de simulación** — mismos inputs → mismos checksums (gates G1/G2).
3. Artefacto byte-reproducible: objetivo posterior, no requisito.
