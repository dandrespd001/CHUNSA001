# Revisión del kernel de IA + partida completa — Sprint 1.4 (implementó: Sonnet · revisó: Arquitecto + Opus)

Fecha: 2026-07-24 · GATE DE FASE 1 · Ramas K1 (`ba0a2f8`) + K2 (`39bdb66`) + K3 (integrada vía `arch/sprint-1.4-k3-integration`)

## Veredicto
**GATE DE FASE 1 SUPERADO Y ACEPTADO.** Tres piezas de kernel entregadas por Sonnet con
RESULTs ejemplares (K1: 5 desviaciones; K2: 9; K3: verificación pre/post directa). CHUNSA es
ahora un juego con una IA que juega y gana una partida completa, determinista y reproducible.

## K1 — victoria + perfil de IA (main `ba0a2f8`)
Condición de victoria `game_over`/`winner`/`participants_mask` (jugador activo = máscara
monótona + gate ≥2 activos, que evita ganadores espurios en tests preexistentes), perfil
`AiProfileV1` tipado. Auditoría Opus: SIN P0/P1/P2 (array vacío rechaza sin OOB, ASan 10/10).

## K2 — IA de 3 capas + skirmish (main `39bdb66`) — el corazón del gate
`ai_execute` real (estratégica/reactiva/táctica) emitiendo comandos por el mailbox
determinista. **Auditoría Opus: DETERMINISTA SIN P0** — verificación empírica definitiva:
checksum invariante en `-O0/-O2/-O3` (un float divergiría), ejecución byte-idéntica sobre
estado congelado. La IA gana el skirmish militar en 1236 ticks. `AI_ALGO_VERSION` 1→2. Único
P2: documentar la invariante "execute en tick de dispatch" → aplicado por el Arquitecto.

## K3 — aldeanos vulnerables + partida con economía (esta revisión)
Cierre del kernel tras la **decisión del Director** (SPEC-004 §7.1): los aldeanos pasan a ser
objetivo de combate (visión AoE2), habilitando que una partida con economía termine por
conquista. Cambio acotado y verificado por inspección + gates (no requiere auditoría Opus: no
hay parsing de entrada no confiable ni lógica de determinismo nueva — solo amplía el conjunto
de objetivos, cero float/heap/RNG añadido):
- Guards de targeting en `combat_system`/`aggro_system`: aldeano enemigo = objetivo válido
  (edificios y propios intactos). Los 3 guards de ATACANTE (`unit_class[i] > 2`) intactos → los
  aldeanos siguen sin atacar (vulnerables, no combatientes).
- `rps_mult_vs_citizen_bp` (×1.0 neutro, patrón de `rps_mult_vs_building_bp`) → sin OOB en la
  tabla 3×3.
- **Escenario `skirmish_eco`** nuevo (archivo separado; el skirmish militar de K2 queda
  append-only intacto): defensor con 3 aldeanos reales (economía autónoma) vs IA atacante.
  **Termina por conquista en el tick 1827** (defensor con 0 edificios y 0 aldeanos), tras
  recolectar 300 de recurso B — economía real. Determinista, save/continuar, replay bit-exacto.

## Verificación independiente del Arquitecto (rama de integración K3)
- Golden **1074/1074** · **G1** alloc_delta=0 `2defd6416796e3d8` · **G4** con IA
  `2681ad5f3eb161ad` — todos **idénticos a main** (los escenarios sin aldeanos-en-peligro no
  cambian).
- **Regresión**: el skirmish militar de K2 sigue **bit-idéntico** (end_tick=1236, winner=1,
  mismos checksums) → el cambio de combate solo afecta escenarios con aldeanos al alcance.
- **skirmish_eco**: end_tick=1827, winner=1, dos corridas idénticas (`state=8ea5bc60ca1a48cd`).
- **ctest 20/20** · build `-Werror` limpio.

## Deuda registrada
1. Balance del RPS contra aldeanos (×1.0 v1; ajustar cuando haya pase de balance — doc 33).
2. Efectos de stats por tech (Parte III de SPEC-004).
3. Época/civ por entidad (catálogo-ancha) — Fase 2.
4. `pop_used` aproximado (K2 desv. 7).

## Siguiente
La parte jugable en Godot (SPEC-005 §8.6): modo skirmish humano-vs-IA + pantalla de victoria/
derrota leyendo `game_over`/`winner` del snapshot — cierra el Sprint 1.4 y la Fase 1. Delegable
a Codex/Luna Max.
