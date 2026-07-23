# REPORTE — Sprint 1.1: Edificios y construcción

Fecha de cierre: 2026-07-23 · Todo integrado en `main` (`9eb6c56`) · Arquitecto: Claude (Fable 5)

## Objetivo (PLAN_MAESTRO §4, Fase 1)
Edificios como entidades del kernel: colocación, construcción por ciudadanos, colisión
en navegación, destrucción por combate, dropoff real como edificio, y su capa visual.
**CUMPLIDO** — todo en un día, con el ciclo completo contrato → implementación paralela
→ revisión cruzada → integración.

## Entregables y reparto real
| Pieza | Implementó | Revisó | Merge |
|---|---|---|---|
| SPEC-004 Parte I (contrato construcción) + enmienda §4.1.2 "exención de escenario" | Arquitecto | — | `60dedd8` |
| Datos: 4 YAML de edificios (šnwt, horreum, 2 centros), civs, blob building=4 | MiniMax | Arquitecto (promoción ADR-014) | `d3378a2` |
| Kernel: tabla tipada, PLACE_BUILDING=7/ASSIGN_BUILD=8, construction_system, dropoff-edificio con fallback legacy, combate vs edificios, save v8/checksum v3 | Sonnet | Arquitecto + **Opus (auditoría parsing: SIN P0, 200k blobs ASan)** | `9997d95` |
| UI Godot: render con footprint/progreso, ghost de colocación (B/N), asignación de constructores, centros por comandos | Codex (GPT-5.6 Luna Max, vía `codex exec`) | Arquitecto | `edeba5e` |

## Hallazgos del ciclo de revisión (lo que el proceso cazó)
1. **Brief↔schema** (cazado por MiniMax al pararse): centros construibles con coste 0
   prohibidos por el schema — resuelto con centros pre-colocados (`constructible:false`)
   + exención de escenario en tick 0 (enmienda SPEC-004).
2. **Stock negativo latente** (cazado por el Arquitecto en revisión): la exención omitía
   el chequeo de costes pero no la deducción — corregido + test.
3. **Exención inalcanzable con delay≥1** (cazado por Codex): `eff = max(0, t+delay) ≥ 1`
   hace imposible el tick 0 en producción — la demo usa `delay=0` (documentado); el
   refinamiento del contrato de setup queda para 1.2.
4. **D8 de Sonnet**: replay v2 no serializa `unit_id`/`BuildingId` → replays reales con
   edificios divergirían. **Deuda ALTA → replay v3 en Sprint 1.2.**

## Verificación final (main)
- Golden 1074/1074 · G1 `1b2dc94a0dd12439` alloc_delta=0 · G3 `2a457d7d` · G4/G5
  `795c204b` schedule_mismatches=0 · **ctest 14/14** · trayectoria pre/post bit-idéntica
  (dumps de movimiento y economía).
- Demo headless larga: `buildings=2` desde tick 0, ciudadanos entregan al centro
  (`stock_A=500`, depósito agotado), combate RPS progresando, **0 errores**.
- `SAVE_FORMAT_VERSION=8`, dominio checksum `CHUNSA_STATE_V3` (bump intencional).

## Deuda registrada
1. **Replay v3** (ALTA — D8). 2. Contrato de setup delay-independiente (hallazgo 3).
3. Migración de saves v7→v8. 4. Restauración de cost_grid a valor original (terreno
variable futuro). 5. Fuentes académicas para fichas de edificios (D2 de datos).

## Siguiente
**Sprint 1.2 — Producción y tecnología** (colas de entrenamiento con costes, tech tree
slice M1, epoch-up ADR-015, población/límites) — con replay v3 como pieza temprana.
Pendiente OK del Director.
