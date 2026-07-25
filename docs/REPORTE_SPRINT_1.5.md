# REPORTE — Sprint 1.5A: fog de guerra y legibilidad RTS

Fecha: 2026-07-24
Rama: `arch/sprint-1.5-fog-legibility`
Base: Sprint 1.4 Godot (`b9b8afa`) sobre kernel aceptado (`afbe66f`)

## Veredicto

**Sprint 1.5A completo.** El jugador deja de ser omnisciente: el mundo y el
minimapa distinguen territorio visible, explorado y desconocido; las entidades
enemigas sólo aparecen en su celda de visión exacta y desaparecen al perder
contacto. Las unidades propias permanecen visibles. El incremento es
presentación pura: no hay diff en `addons/chunsa_sim/core/` y el skirmish
conserva `winner=1 tick=2554`.

## Revisión del estado jugable frente a los referentes

El proyecto ya contiene el bucle RTS mínimo reconocible de Age of Empires II y
Empire Earth I–II: aldeanos/economía, construcción, producción, investigación,
ascenso de época, combate, cámara, minimapa, grupos de control, IA, victoria y
ahora exploración con fog. Además conserva ventajas arquitectónicas buscadas por
el proyecto: flow field, datos CHDB, command stream, save/replay deterministas y
render MultiMesh.

| Área | Estado después de 1.5A | Brecha relevante |
|---|---|---|
| Apertura económica | Parcial | La demo comienza con centro, cuartel y ejército; no valida una apertura humana larga. |
| Control RTS | Parcial | Selección, movimiento y grupos funcionan; faltan órdenes explícitas ATTACK/ATTACK_MOVE, patrulla y formaciones. |
| Exploración | Implementada v1 | Fog visual exacto por entidad; sin memoria de última posición ni IA limitada por visión. |
| Civilizaciones/épocas | Slice técnico | Egipto/Roma y epoch-up existen, pero la diferenciación jugable y visual es todavía mínima. |
| Tecnología/población | Parcial | Tech como paquete de capacidad; efectos de stats y `pop_used` preciso siguen pendientes. |
| Combate | Funcional v1 | RPS, aggro, moral y conquista; posición, flanqueo y formaciones quedan para combate v2. |
| Contenido/producto | Pendiente | Arte/audio final, mapas, campaña, modding, setup de skirmish y save/load desde UI. |

La comparación confirma la dirección “EE2 con QoL moderno” del corpus y evita
el anti-patrón de EE3 de simplificar civs/épocas sin identidad. También muestra
que el próximo riesgo no es otro sistema aislado, sino convertir el slice
técnico en una partida con pacing, órdenes y feedback de producto.

## Correcciones al plan general

1. Sprint 1.4 estaba implementado pero no cerrado en el Plan Maestro. Se creó
   `docs/REPORTE_SPRINT_1.4.md` y se reconciliaron sus dos contratos temporales:
   la evidencia valida una partida concluyente **antes** del límite de 30 min,
   no un playtest humano de 30+ min.
2. El Sprint 1.5 original mezclaba arte, audio, feel y PERF-1, pero depende de
   una decisión de presupuesto y de hardware mínimo no disponible. Se insertó
   1.5A para saldar primero la deuda jugable de fog; arte/audio pasa a 1.6.
3. La referencia “doc 37” del plan no existe en el corpus actual. Se sustituyó
   por `13_PIPELINE_ASSETS.md`, junto con los documentos canónicos 34 y 35.
4. Se corrigió el registro de SPECs y la doctrina de delegación para reflejar
   el estado ejecutado y el reparto actual GPT-5.6/MiniMax supervisado.

## Implementación

- `DemoSnapshot` transporta `map_w`, `map_h`, `visible[0]`, `explored[0]` y
  posiciones raw Q47.16.
- `fog_view.hpp` concentra tres helpers puros y `noexcept`: tile, entidad y
  bloque; no depende de Godot ni muta estado.
- Un MultiMesh de 32×32 bloques representa fog visual de 8×8 tiles y sólo se
  actualiza al aceptar un snapshot nuevo.
- Un único predicado filtra unidades, edificios, minimapa, hit-testing,
  selección, barras y rally. Los edificios usan cualquier celda visible de su
  footprint; no sólo el centro.
- Al reaparecer un enemigo no se interpola desde una posición previa oculta.
- El minimapa aplica el mismo fog y mantiene las entidades propias por encima
  del velo.
- CTest incorpora `fog_view`, con bounds, punteros nulos, Q47.16, precedencia
  visible/explored, propios siempre visibles, bloques mixtos y recorte en borde.

## Delegación y supervisión

- Arquitectura, alcance, revisión e integración: GPT-5.6 Sol Xhigh.
- Auditoría de arquitectura/pacing: GPT-5.6 Tierra High.
- Integración Godot: GPT-5.6 Luna Max; el Arquitecto corrigió visibilidad por
  footprint e interpolación al reaparecer.
- Helper/tests: GPT-5.6 SOL Low.
- SPEC-006 Parte II y cierre 1.4: GPT-5.6 SOL Medium.
- MiniMax-M3: se preparó y selló un job de tres archivos sin secretos. El gate
  de permisos bloqueó el egress de `vision.hpp`; no se envió código externo, el
  job se purgó y se usó el fallback SOL Low.

## Verificación independiente

```text
Godot GDExtension: Built target chunsa_godot
CTest: 21/21
Golden: incluido en CTest, verde
G1: alloc_delta=0, checksum=2defd6416796e3d8
G4: state=2681ad5f3eb161ad, cont=486c9601301ff753
G5: ai_executions=0, schedule_mismatches=0, replay_v=3,
    checksum=2681ad5f3eb161ad
```

Godot real con OpenGL/NVIDIA:

- tick 100: 16 bloques visibles, 0 explorados, 1008 desconocidos, 0 enemigos;
- tick 200: aparece el primer bloque explorado;
- tick 900: 8 enemigos entran en visión;
- tick 1800: 0 enemigos presentados al perder contacto;
- tick 2500: 8 enemigos reaparecen en el asalto final;
- fin: `CHUNSA game_over winner=1 tick=2554`.

Se revisaron capturas reales en `/tmp/chunsa-fog-gui.f600.png` y
`/tmp/chunsa-fog-contact.f600.png`. El smoke headless salió sin
`CHUNSA ERROR`; el error de textura observado al intentar capturar con renderer
dummy pertenece al mecanismo de screenshot headless, no al fog ni al run
gráfico.

## Deuda posterior

- Playtest humano de apertura económica y pacing; la demo automática dura
  ~2:08.
- Automatización de input/visual Godot en CI; hoy el contrato puro sí está en
  CTest, pero cámara/ratón/composición siguen requiriendo smoke/captura.
- Mojibake visible en literales acentuados del HUD (`Población`, `cámara`);
  corregir junto con localización/UI.
- PERF-1 en Intel UHD 620 sigue bloqueado por falta de hardware; la prueba
  gráfica de este sprint se hizo en GTX 1650.
- Siguiente decisión: presupuesto de Sprint 1.6. Si se difiere, priorizar
  ATTACK/ATTACK_MOVE y un escenario de apertura económica antes de ampliar
  contenido.
