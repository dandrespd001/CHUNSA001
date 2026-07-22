# REPORTE DE CIERRE — Sprint 0.3 (2026-07-22)

**Estado: CERRADO** (con una deuda menor documentada: verificación factual de la 4ª ficha, bloqueada por cuota externa — §Deuda). El Sprint 0.3 dio a CHUNSA su **capa de combate y economía completas** y el **primer control del jugador**, y cerró la última deuda de robustez del núcleo determinista (replay auto-verificado). Repo: `github.com/dandrespd001/CHUNSA001`, main en `ad6c306`.

## Qué es CHUNSA tras el Sprint 0.3

De una demo de navegación (0.2) a un **campo de batalla jugable**: dos ejércitos con triángulo RPS que se persiguen y se aniquilan, moral y pánico, ciudadanos que recolectan, y el jugador seleccionando y ordenando sus unidades con el ratón — todo determinista, guardable y reproducible bit a bit.

## Entregables vs plan

| Ítem (roadmap 0.3 + 0.3-cierre) | Resultado | Autor | Commit |
|---|---|---|---|
| Combate RPS v1 | ✅ triángulo cav/art/inf en basis points, búsqueda por spatial hash, daño determinista, muerte→destroy | Sonnet 5 + Arq. | `a4350d6` |
| Moral y pánico v1 | ✅ conteo aliados/enemigos, histéresis PANIC/RALLY, huida; endurecimiento SPAWN_UNIT speed | Sonnet 5 + Arq. | `a6287a9` |
| Economía mínima v1 (A/B/Me) | ✅ ciudadanos SEEK/HARVEST/RETURN, 6 depósitos, dropoff por jugador; `economy.hpp` autocontenido | MiniMax M3 + Arq. | `7912752` |
| Demo showcase (combate+moral+economía visibles) | ✅ 240 cav vs 240 art + ciudadanos; coloreado por bando/clase/pánico | Kimi K3 + Arq. | `ea5e3c8` |
| **Selección y órdenes del jugador con clic** | ✅ clic/arrastre selecciona caballería propia (verde), clic derecho ordena MOVE_TO; mutex input→sim | Kimi K3 + Arq. | `9162f34` |
| **Aggro / persecución v1** | ✅ el combate llega a resolución (antes se estancaba); unidad ociosa persigue enemigo en 10 tiles | Arquitecto | `a1c52b7` |
| **Replay v2 con `effective_tick`** | ✅ agenda auto-verificada; config §6.2 persistida; `schedule_mismatches` | Arquitecto | `ad6c306` |
| **Enmienda ADR-021** (MP = solo preparación) | ✅ aplicada a `SPEC_ARQUITECTURA_BASE.md` §3.9 / registro ADRs / §5.1 | Arquitecto | (doc fuera de repo) |
| PERF-0 físico | 🔶 **documentado como bloqueado** (sin hardware de referencia UHD 620) — ADR-011 sigue TARGET | Arquitecto | — |
| Verificación factual de fichas del slice | 🔶 **3/4 hechas** (Egipto/Roma/Tawantinsuyu); Mali pendiente por cuota | claude-minimax + Arq. | (staging) |

## Determinismo (invariante sagrado, intacto)

Todos los gates verdes tras cada merge, con checksums evolucionando por el estado nuevo (combate/moral/economía): **golden 1074/1074** · **G1** bit-exacto (`e35e928300ff995d`, alloc_delta=0) · **G3/G4/G5** · **ctest 11/11** (combat, morale, economy, aggro, replay_v2 nuevos). Ninguna delegación rompió el determinismo. El **replay v2** endurece G5: la agenda de comandos, antes implícita y frágil (dependía de constantes hardcodeadas y no persistidas en el reproductor), ahora es explícita, persistida y auto-verificada — una corrupción o un cambio de la regla de normalización falla ruidosamente (`schedule_mismatches`) en vez de divergir en silencio. Confirmado con una prueba de mutación de bytes.

## Hitos técnicos del sprint

1. **El combate se decide de verdad** (aggro): el Director detectó en vivo que los supervivientes quedaban inertes fuera de rango; `aggro_system` (tras combate, antes de moral) hace que las unidades ociosas persigan, y el combate pasa de estancarse (239 vs 227) a resolverse (221 vs 0). Reutiliza `tgt` → sin estado nuevo, save v6 intacto, y las órdenes del jugador siempre tienen prioridad (una unidad con orden en curso no es "ociosa").
2. **Interactividad**: el jugador controla su ejército con el ratón, con la mano segura sim/render (mutex solo en la cola de comandos, el estado de selección vive entero en el hilo principal). Probado en vivo por el Director ("similar a Age of Empires").
3. **Robustez del replay** (0.3-cierre): descrita arriba.

## Verificación factual de las fichas (ADR-014) — resultado parcial

`claude-minimax` (con web) verificó 3 de las 4 fichas del slice, afirmación por afirmación, con fuentes reales y honestidad («sin fuente localizada» cuando no halló). Informes en `game_data/research/verificacion/`:

| Ficha | Veredicto | Hallazgo grave |
|---|---|---|
| Egipto (36a) | Requiere revisión mayor (20 erróneas/100) | Transliteraciones inventadas, calendario invertido (cosecha=Shemu, no Peret), «oro de Ofir» anacrónico |
| Roma (36b) | Apta con correcciones (3 erróneas/47) | Revuelta panonia mal clasificada como de esclavos |
| Tawantinsuyu (36d) | Requiere revisión mayor (8 erróneas/55) | **Referencia bibliográfica inventada** por M3 (cazada) |
| Mali (36c) | **PENDIENTE** (cuota agotada) | — |

**Veredicto del Arquitecto (ADR-014)**: ninguna de las 3 verificadas se promueve a canónica tal cual; la corrección (aplicar enmiendas + sanear bibliografía) se agenda en el **Sprint 0.4**, donde las fichas se convierten en datos (schema `civ`/`unit`). El valor confirmado de la verificación: cazó invenciones del generador que habrían contaminado el corpus.

## Deuda documentada (bloqueos externos, no de trabajo)

1. **Verificación de Mali (36c)**: bloqueada por agotamiento de cuota de MiniMax (reset ~2026-07-22 15:11). Re-delegación programada; brief acotado en `game_data/research/BRIEF_VERIFICACION_MALI.md`. Al recibir `VERIF_36c.md` se actualiza `VERIF_RESUMEN.md` y este reporte.
2. **PERF-0 físico**: sin acceso al tier mínimo de referencia (UHD 620). La máquina dev (i7-10700T + UHD 630) es más potente y daría números optimistas. ADR-011 permanece TARGET; el bench sintético del kernel (p95=265µs/tick vs 50ms de presupuesto) confirma holgura en la simulación pura (no en render). Conseguir hardware de referencia antes de Fase 2.

## Reparto de agentes (validado)

- **Sonnet 5**: combate y moral — kernel con juicio; en moral detectó honestamente una incoherencia entre dos contratos del Arquitecto.
- **Kimi K3**: showcase y control del jugador — frontend/input, su nicho; cuota agotada 2× a media tarea (rescatada por el Arquitecto). **Kimi sin cuota el resto del mes** (2026-07): la UI/HUD de Fase 1 deberá reprogramarse o reasignarse.
- **MiniMax M3 / claude-minimax**: economía (módulo autocontenido) y verificación factual con web — volumen y datos; cuota mensual también limitada (bloqueó Mali).
- **Arquitecto**: aggro, replay v2, contratos, revisión, integración, decisiones (ADR-021). Doctrina en `docs/DELEGACION_MODELOS.md`.

## Siguiente

**Sprint 0.4 — datos reales** (ver `docs/PLAN_MAESTRO.md` §4): escribir SPEC-002 (schemas), completar `chunsa_data_compiler`, y que el kernel consuma stats desde blobs (`SPAWN_UNIT` por `unit_id`) — fin del hardcodeo. Las fichas corregidas del slice alimentan el primer fixture data-driven. Nota de recursos: con Kimi sin cuota este mes, el volumen recae en MiniMax (spec cerrada) y Sonnet (integración kernel).
