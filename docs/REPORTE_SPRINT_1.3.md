# REPORTE — Sprint 1.3: UI / HUD v1

Fecha de cierre: 2026-07-24 · Integrado en `main` · Arquitecto: Claude (Fable 5)

## Objetivo (PLAN_MAESTRO §4, Fase 1)
Convertir la demo técnica en algo que se **controla como un RTS**: cámara movible, minimapa,
panel de selección, grupos de control, y las acciones de partida accesibles de forma
coherente. **CUMPLIDO.**

## Entregables y reparto real
| Pieza | Implementó | Revisó | Merge |
|---|---|---|---|
| SPEC-006 Parte I (HUD v1) + decisión de teclas | Arquitecto | — | `152294f` |
| UI completa en Godot | Codex (GPT-5.6 Luna Max) | Arquitecto | `arch/sprint-1.3-integration` |

Sprint **100% frontend**: sin trabajo de kernel ni de datos (el core no se tocó).

## Lo entregado (contra las § del SPEC-006)
- **Cámara (§2)**: pan (WASD/flechas + arrastre central) con clamp al mapa, zoom con rueda
  anclado al cursor sobre `set_size`.
- **Minimapa v1 (§3)**: mapa a escala con muros, entidades por owner y rectángulo de viewport;
  clic recentra la cámara. Sin fog (deuda para cuando se exponga la visión).
- **Panel de selección + vida (§4/§7)**: `hp`/`max_hp` añadidos al `DemoSnapshot`; barras de
  vida sobre las entidades; panel con tipo/owner/estado.
- **Grupos de control (§5)**: `Ctrl+N` asigna, `N` recupera — guardando slot **y generation**,
  de modo que un slot reciclado no se re-selecciona. Los números quedaron libres para esto.
- **Producción a botones (§5)**: TRAIN/RESEARCH migrados de las teclas `1..8` a pestañas/
  botones contextuales del panel; `EPOCH_UP` a botón dedicado; `SET_RALLY` sigue en `R`+clic.
- **Marcadores de orden (§6)**: marcador breve en el destino de MOVE_TO/rally; rally
  persistente del snapshot.
- Además completó el binding mecánico de `rally_x/y/set` en el snapshot (el contrato ya los
  declaraba; no estaban copiados).

## Verificación (Arquitecto)
- **Core intacto** (`git diff` sobre `core/` vacío) → ctest del kernel sigue **16/16** por
  construcción · build del adaptador `-Werror` **0 warnings**.
- Demo headless exit 0, `buildings=4`, **0 errores**; combate progresa con los mismos números
  que antes del sprint (cav 240→238→226, art 240→228) → **determinismo del showcase intacto**;
  el escenario `build_showcase_batch` y el `step` no se tocaron.
- El SIGSEGV del headless sin `--log-file` que Codex reportó volvió a ser de su sandbox
  (`user://` inaccesible); para el Arquitecto corre normal.

## La demo ahora
`cd CHUNSA001 && ./third_party_build/Godot_v4.7.1-stable_linux.x86_64 --path demo`
- Cámara: WASD/flechas + rueda para zoom; minimapa con clic para saltar.
- Selección con arrastre; `Ctrl+1..9` guarda grupo, `1..9` lo recupera.
- Cuartel seleccionado → botones de entrenar/investigar; `E` sube de época; `B` construir;
  `R`+clic rally; clic derecho mover.

## Deuda registrada
1. **Fog of war**: minimapa/mundo sin niebla; exponer `explored`/`visible` de la visión al
   snapshot (candidato 1.4/1.5).
2. Assets finales y pulido de game-feel: Sprint 1.5.

## Siguiente
**Sprint 1.4 — IA oponente v1 + partida completa** (SPEC-005). Es el **GATE DE FASE**: una
partida de 30+ min jugable y ganable, Egipto vs Roma, humano vs IA, con G1–G5 y replay
íntegro. Vuelve a haber trabajo de kernel (la IA emite comandos por el mailbox determinista de
SPEC-001 §7) → Sonnet + Arquitecto. Pendiente OK del Director.
