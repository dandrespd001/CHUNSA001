# REPORTE — Sprint 0.4: Datos reales (data-driven)

Fecha de cierre: 2026-07-23 · Rama integrada: `arch/sprint-0.4-integration` → `main` · Arquitecto: Claude (Fable 5)

## Objetivo del sprint (PLAN_MAESTRO §4, Fase 0)
Que el juego deje de usar stats hardcodeadas: SPEC-002 escrita, schemas + compilador
completos, kernel consumiendo el catálogo binario (CHDB), demo 100% data-driven.
**CUMPLIDO.**

## Entregables y reparto real
| Pieza | Implementó | Revisó | Estado |
|---|---|---|---|
| SPEC-002 (16 secciones), 8 schemas, compilador CHDB, blob + datos Egipto/Roma, gate `data_compile` | Codex/GPT | Arquitecto + Opus (informe profundo) | ✅ sin P0; blob bit-exacto por 4 vías |
| P1-1 procedencia (evidencia vendorizada + `E_PROVENANCE`) | Sonnet | Arquitecto | ✅ 30/30 tests, blob bit-idéntico |
| Kernel data-driven: loader CHDB (`data_catalog.hpp`), SPAWN_UNIT/CITIZEN por `unit_id`, checksum v2, save v7 | Sonnet | Arquitecto + Opus (auditoría del loader) | ✅ 2 P1 cerrados (fuga con ASan, NFC parcial) |
| Fix driver: rechazo de delays > u16 (bug latente del replay v2 del Arquitecto) | Codex | Arquitecto | ✅ con test de borde |
| Adaptador Godot data-driven (catálogo en `_ready`, spawns por `unit_id`) | MiniMax (brief ultra-literal) | Arquitecto | ✅ paró y reportó en el edge case en vez de inventar |
| Reconciliación gate `data_compile` (selección de Python con deps) | Arquitecto | — | ✅ |

## Verificación final (main, post-merge `arch/sprint-0.4-integration`)
- Golden vectors **1074/1074** · G1 `45801aa21d18c8a8` (alloc_delta=0) · G3/G4 `8ebe4c097172bbb4` · G5 `309cd496d10526d6` (schedule_mismatches=0).
- **ctest 13/13** (incluye `data_compile`); build limpio con `-Werror`.
- `content_hash` del blob reproducible: `f19640cc…b759bab`.
- Demo headless: spawns desde catálogo (cav=238, art=240, citizens=120), sin `CHUNSA ERROR`.
- Los checksums cambiaron respecto al 0.3 **solo** por el checksum v2 (dominio incluye `unit_id`) — cambio intencional y documentado.

## Deuda diferida (no bloqueante, registrada para evolución del kernel)
1. Save v7 con envelope literal del SPEC-002 §9 (hoy: extensión mínima compatible).
2. Replay v3 (hoy: v2 con effective_tick persiste y basta para G5).
3. CLI `--data` en el driver (hoy: el blob se carga por el adaptador/env).
4. Revalidación de `unit_id` contra el catálogo en `gs_deserialize`.

## Lecciones de delegación
- MiniMax rinde muy bien con briefs ultra-literales (código a insertar, no diseño), incluso en godot-cpp; su disciplina de pararse en el edge case fue ejemplar.
- La verificación cruzada (Opus audita a Sonnet; Arquitecto audita a Codex) volvió a cazar defectos reales (fuga en loader, truncamiento u16).

## Siguiente
**Sprint 1.1 — Edificios y construcción** (Fase 1: vertical slice M1-INT Egipto vs Roma).
Contrato: SPEC-004 (sistemas de partida, parte construcción).
