# Reporte Sprint 1.6A — combate a distancia y movilidad

**Fecha:** 2026-07-24
**Estado:** ACEPTADO
**Rama:** `arch/sprint-1.6-mechanics`

## Resultado

El ranged hitscan v1 ya respeta el alcance geométrico real aunque el blanco
esté a varias celdas del spatial hash. La persecución automática termina en un
punto de standoff dentro del alcance, la cadencia fija tiene exactamente diez
ticks entre impactos y las velocidades distintas quedan verificadas desde los
datos y visibles en el HUD.

Este sprint no declara el juego completo. Cierra el primer gate mecánico del
replan; apertura económica, población/construcción/producción completas y
proyectiles/órdenes de ataque quedan en 1.6B, 1.6C y 1.7.

## Cambios aceptados

- `combat_system` calcula el rectángulo de celdas intersectado por
  `range_mt` y conserva el filtro circular exacto por distancia al cuadrado.
  La ballista de cuatro tiles ya no pierde blancos por el antiguo límite 3×3.
- `ATK_COOLDOWN_TICKS=10` significa ahora impacto en `t` y siguiente impacto
  permitido en `t+10`; se eliminó el desfase efectivo de once ticks.
- El auto-aggro calcula una sola vez un destino de standoff a un mili-tile
  dentro del alcance. No cancela un `MOVE_TO` activo y no reconsulta el anillo
  de aggro cada tick durante la aproximación.
- `DemoSnapshot` expone ataque, alcance y velocidad. Una unidad de combate
  seleccionada muestra los tres valores en el panel de Godot.
- La cobertura de combate añade frontera exacta, largo alcance entre celdas,
  cadencia, standoff y velocidades proporcionales.
- La cobertura económica añade desempate de depósitos, índice inválido,
  agotamiento/reasignación, límites de extracción/carga y dropoff exacto.
- El test del catálogo congela las diferencias reales del slice:
  legionary `range=100/speed=50`, ballista `4000/35` y chariot `800/70`.
- El Plan Maestro difiere arte/audio hasta el cierre mecánico 1.7 y divide el
  trabajo restante en 1.6B, 1.6C y 1.7.

## Decisiones de arquitectura

1. El daño a distancia sigue siendo hitscan autoritativo. Proyectiles, tiempo
   de vuelo y comandos `ATTACK`/`ATTACK_MOVE` requieren nuevos estados,
   serialización y replay; se ejecutarán juntos en 1.7.
2. La cadencia por arma también se mueve a esa misma migración. En 1.6A queda
   una cadencia global explícita y exacta, evitando dos cambios consecutivos
   del contrato de datos/estado.
3. El primer prototipo de standoff avanzaba un paso y re-adquiría cada tick.
   Era correcto en comportamiento, pero multiplicaba las consultas de aggro y
   degradaba el skirmish largo. La revisión del Arquitecto lo sustituyó por un
   destino geométrico único; conserva la mecánica y recupera el coste esperado.
4. No se añadieron campos a `GameState` ni se cambió la versión de save/replay.

## Delegación real

- Arquitectura, reparto, revisión, corrección de rendimiento e integración:
  GPT-5.6 Sol Xhigh.
- Auditoría de kernel e implementación inicial de alcance/cadencia/standoff:
  GPT-5.6 Tierra High.
- Auditoría e integración de telemetría Godot: GPT-5.6 Luna Max.
- Auditoría de economía/roadmap y documentación: GPT-5.6 Sol Medium.
- Inventario y pruebas focales: GPT-5.6 Sol Low.
- GPT-5.3 Codex-Spark se añadió como perfil condicional, pero el runtime de
  esta sesión no lo expuso; no se le atribuye trabajo.
- MiniMax-M3: se selló un paquete mínimo con `economy.hpp` y
  `test_economy.cpp`. La plataforma rechazó el egress por faltar aprobación
  específica del payload; no se envió código, el job se purgó y el Arquitecto
  implementó la cobertura localmente.

## Verificación independiente

```text
Build GCC: OK
Build portable: OK
Godot GDExtension: Built target chunsa_godot
CTest GCC: 21/21, 36.98 s
CTest portable: 21/21, 6.87 s
Golden: 1074/1074 en ambos backends
```

Gates:

```text
G1: alloc_delta=0, checksum=2defd6416796e3d8
G3: state=794d43a2dd8333a8, cont=8b3a30f0b0eb11f6
G4: state=2681ad5f3eb161ad, cont=486c9601301ff753,
    ai_executions=30
G5: ai_executions=0, schedule_mismatches=0, replay_v=3,
    checksum=2681ad5f3eb161ad
```

Escenario económico bajo CTest:

```text
winner=1, end_tick=1824
state=5c8be20083cf490e
continuation=bc8fb792367ea053
```

Godot 4.7.1 headless con el binario regenerado:

```text
catálogo cargado
combate, fog, economía e IA activos
CHUNSA game_over winner=1 tick=2522
```

La variación frente al tick 2554 del Sprint 1.5 es esperada: la cadencia ya no
tiene un tick extra y la artillería conserva distancia. Ganador, terminación y
determinismo permanecen estables.

## Deuda que bloquea “juego completo”

- 1.6B: recursos definidos por mapa, orden explícita de recolección, centros
  que produzcan aldeanos, civilización y época inicial por jugador.
- 1.6C: población por edificios, costes data-driven, cancelación/reembolso,
  construcción y producción sostenibles desde una apertura sin ejército.
- 1.7: proyectiles deterministas, cadencia/velocidad de arma data-driven y
  órdenes `ATTACK`/`ATTACK_MOVE`.
- Después del cierre 1.7: arte, audio y feel.
