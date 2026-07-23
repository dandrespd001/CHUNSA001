# BRIEF — Kernel de edificios y construcción (Sonnet · Sprint 1.1)

Implementa **SPEC-004 Parte I** (`docs/specs/SPEC-004_SISTEMAS_PARTIDA.md`) en el kernel.
Es una tarea con juicio: el contrato define QUÉ; tú decides el CÓMO respetando las
convenciones existentes del kernel (SoA, iteración ascendente, cero float/heap en Step,
`-Werror`). Lee el SPEC completo antes de tocar nada.

## Rama y alcance
- Rama `sonnet/buildings-1.1` desde `main` (HEAD actual). Jamás toques `main` directamente.
- Archivos esperados: `data_catalog.hpp` (tabla tipada §2), `game_state.hpp` (§3),
  `commands.hpp` (§4, enum append-only), `step.hpp` (fases §5–§7), `serialize.hpp`/
  `checksum.hpp`/`save_io.hpp` (§8), tests nuevos en `tests/unit/`, regen de golden.
  El módulo `economy.hpp` sigue **autocontenido** (§6: el punto de dropoff resuelto se le
  pasa desde el wiring, no le metas GameState).

## Orden de trabajo recomendado
1. Catálogo: `BuildingDefinitionV1` + parsing tipado + `catalog_find_building` + tests de
   rechazo (§2). El formato del blob NO cambia — solo qué parsea el loader. Para los tests
   usa/extiende el fixture existente de `test_data_blob` añadiendo 1–2 records building
   mínimos (si el fixture se genera con el compilador Python, añade los YAML al fixture,
   no toques los datos reales de `data/` — de esos se ocupa otra tarea en paralelo).
2. Estado + comandos (§3–§4) con sus receipts exactos (el orden de validación de §4.1 es
   contrato, testéalo).
3. Fase constructora (§5) + dropoff-edificio con fallback legacy (§6) + combate (§7).
4. Save v8 + dominio de checksum + regen golden (§8) — usa el procedimiento del checksum
   v2 del sprint pasado como plantilla.
5. Gates §9 completos.

## Reglas duras
- Append-only en enums y formatos; nada de renumerar ni reordenar campos existentes.
- El fallback legacy de dropoff (§6) debe dejar la TRAYECTORIA de los escenarios golden
  previos bit-idéntica (verifícalo con dump pre/post, no solo checksum — gate §9.1).
- Cero asignaciones/floats dentro de Step (el CI lo cuenta).
- ⚠️ Térmica: builds `nice -n 19 ... -j2`, un build o test suite a la vez.
- Si el SPEC tiene un hueco o contradice el código existente: elige lo conservador,
  documenta la desviación en el RESULT — no "mejores" el contrato por tu cuenta.

## Entrega
Commits atómicos en la rama + `docs/briefs/SONNET_KERNEL_EDIFICIOS_SPRINT_1.1_RESULT.md`:
qué hiciste, desviaciones del SPEC (numeradas contra sus §), salida de gates (golden,
G1/G3/G4/G5, ctest N/N), checksums nuevos. NO merges tú; el Arquitecto revisa e integra.
