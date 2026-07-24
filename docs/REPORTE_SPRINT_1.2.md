# REPORTE — Sprint 1.2: Producción y tecnología

Fecha de cierre: 2026-07-24 · Todo integrado en `main` · Arquitecto: Claude (Fable 5)

## Objetivo (PLAN_MAESTRO §4, Fase 1)
El bucle económico-militar completo: los edificios producen unidades, la tecnología
avanza y las épocas suben. **CUMPLIDO.**

## Entregables y reparto real
| Pieza | Implementó | Revisó | Merge |
|---|---|---|---|
| SPEC-004 Parte II (§10–§13) | Arquitecto | — | `4bcecc7` |
| K1: replay v3 + save v9 + checksum v4 + ventana de setup | Sonnet | Arquitecto | `785408d` |
| Datos: 2 cuarteles con `trains`, 4 techs con capacidades | MiniMax | Arquitecto (promoción ADR-014) | `ddc8952` |
| K2: producción/tech/épocas (TRAIN/RALLY/RESEARCH/EPOCH_UP, save v10/checksum v5) | Sonnet | Arquitecto + **Opus (auditoría tech: SIN P0)** | `9b59965` |
| UI Godot: HUD, colas, investigación, época, cuarteles pre-colocados | Codex (GPT-5.6 Luna Max) | Arquitecto | `arch/sprint-1.2-ui-integration` |

## Hallazgos del ciclo (lo que el proceso cazó)
1. **D8 cerrada** (K1): replay v2 y la agenda del save perdían `unit_id`/`BuildingId`.
2. **Ventana de setup inalcanzable con delay≥1** (hallazgo Sprint 1.1): resuelto en K1
   (`target 0 && t 0 → eff 0`); la demo vuelve al `delay=1` de producción.
3. **`researches` vacío** (K2 desv. 6, cazado por Sonnet): RESEARCH_TECH era inusable con
   datos reales → poblé el campo en 4 edificios (Arquitecto).
4. **Adaptador dejaba `player_epoch=0`** (K2 desv. 9): habría roto la colocación de
   edificios del Sprint 1.1 → 1 línea en `_ready` (Arquitecto).
5. **2 P2 de robustez en el parsing de capacidades** (Opus): endurecidos (Arquitecto).
6. **SIGSEGV del headless sin `--log-file`** (reportado por Codex): era su sandbox
   (`user://` inaccesible), NO el adaptador — verificado corriendo sin `--log-file`, exit 0.

## Verificación final (main)
- Golden 1074/1074 · G1 `544ae6be` alloc_delta=0 · G3 `309d6ecc` · G4/G5 `86cbed58`
  (replay_v=3) · **ctest 16/16** · trayectoria pre/post bit-idéntica.
- Save v10, checksum v5 (`CHUNSA_STATE_V5`). Época inicial del slice = 3.
- Demo headless: `buildings=4` (2 centros + 2 cuarteles pre-colocados), combate progresa
  (los `MOVE_TO` siguen correctos pese al reajuste de slots), **0 errores**.
- Core intacto (la UI solo tocó `gdextension/` + `demo/`).

## La demo ahora (probar en vivo)
`cd CHUNSA001 && ./third_party_build/Godot_v4.7.1-stable_linux.x86_64 --path demo`
- HUD con recursos A/B/Me, época y población.
- Selecciona un cuartel propio → teclas `1..8` entrenan (`TRAIN_UNIT`); `T` alterna a modo
  investigación; `R`+clic fija rally; `E` sube de época (`EPOCH_UP`).
- Feedback del último comando leído del mailbox de receipts (verde=aceptado, naranja=rechazo).

## Deuda registrada (Fase 1 restante)
1. `pop_used` aproximado (K2 desv. 7). 2. Efectos de stats por tech (Parte III).
3. Época/civ por entidad (hoy catálogo-ancha) — SPEC-TRAYECTORIA / Fase 2.
4. Sin CANCEL de cola/research; sin migración de saves entre versiones.

## Siguiente
**Sprint 1.3 — UI/HUD v1** (SPEC-006: panel de selección, minimapa, control groups,
cámara) o **Sprint 1.4 — IA oponente + partida completa** (el gate de fase). Pendiente
decisión del Director sobre el orden.
