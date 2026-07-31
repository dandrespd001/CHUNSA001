# Auditoría del proyecto — 2026-07-30

**Autor:** Arquitecto Jefe (Claude Opus 5) · **Encargo:** el Director pidió
auditar y, con los resultados, revisar hoja de ruta, sprints, arquitectura y
planes.

Todo lo que sigue está **medido hoy contra el repo**, no recordado.

---

## 1. Lo que está sano, y conviene no tocarlo

| Medida | Valor |
|---|---|
| Suite completa | **39/39** en 265 s |
| Ciclo rápido (`-L fast`) | **36/36 en 30,2 s** — presupuesto SPEC-008: 60 s |
| Gates de determinismo | G1/G3/G4 aserverados dentro de `ctest` |
| Vectores dorados | 1074 / 0 fallos |
| Índices de catálogo cableados | **cero** — todo resuelve por `record_id` |
| Ramas vivas / worktrees | 3 / 2 |
| Artefactos del demo | `.chdb` y `.so` **sincronizados** con las fuentes |

El **determinismo** está bien defendido: gates dentro de la suite, baselines
aserverados, sin float, sin reloj. La disciplina de TDD de las últimas semanas
ha producido pruebas que **cazan de verdad** —15 fallos en rojo en el 1.9, 11 en
la fórmula de daño, 7 en los efectos de tecnología— y no decorativas.

La **ampliabilidad a varias civilizaciones**, que el Director marcó como
requisito, tiene ahora dos defensas reales: el guardián de época inicial y la
auditoría de índices que acabo de cerrar con resultado limpio.

---

## 2. Hallazgo principal: **la IA está ciega a todo lo que construimos**

Medido sobre `ai_stub.hpp` (1087 líneas):

| Sistema | Ocurrencias en la IA |
|---|---|
| `CRAFT` / recetas | **0** |
| armadura | **0** |
| efectos de tecnología | **0** |
| bronce | **0** |

**Consecuencia jugable:** el oponente **nunca fabricará bronce**, nunca
investigará pensando en armadura y nunca entenderá un contador. Toda la
profundidad de los sprints 1.9 y 1.18 es, hoy, **exclusiva del jugador humano**.

Eso no es una partida desafiante: es un adversario que juega a otro juego, más
pobre, y que el jugador superará por acumulación en cuanto entienda el sistema.

**Es el mayor problema de coherencia del proyecto ahora mismo.** No es un fallo
de código —nada está roto— sino una divergencia entre lo que el juego permite y
lo que el oponente sabe hacer, y crece con cada sprint de profundidad que
añadimos sin tocar la IA.

Anexo del mismo hallazgo: `ai_afford_epoch` cablea **tres recursos** mientras
`ai_afford` recorre los 32. Hoy es correcto —el coste de época son tres
constantes— pero es un acoplamiento que romperá en silencio el día que subir de
época cueste otra cosa.

---

## 3. Segundo hallazgo: **SPEC-008 declara presupuestos que nadie mide**

`tests/perf/` está **vacío**.

SPEC-008 fija presupuestos duros —`Step()` ≤ 2,0 ms, `state_checksum_v1`
≤ 0,2 ms, con 4 jugadores × 200 entidades, 64 depósitos y 200 granjas— y **no
existe ni una medición**. Un presupuesto que nadie comprueba es un deseo.

Se sabía que PERF-0 estaba bloqueado por no tener el hardware de referencia
(UHD 620), y eso sigue siendo cierto. Pero **no medir en la máquina actual no se
sigue de ahí**: una medición relativa que detecte regresiones vale mucho aunque
no valide el objetivo absoluto.

Riesgo concreto y ya presente: en el 1.18 dejé registrado que
`player_tech_bonus` es **O(tecnologías) por golpe**. Con 4 tecnologías es ruido.
Nadie se enterará de cuándo deja de serlo, porque nada lo mide.

---

## 4. Tercer hallazgo: la distancia entre lo especificado y lo construido

### 4.1 Recursos

**30 recursos definidos · 8 presentes en algún mapa.** Los 22 restantes no
existen en la partida por ninguna vía:

`aluminio, bauxita, bronce*, cemento, carbón vegetal, carbón, coque, pólvora,
mena de hierro, plomo, caliza, salitre, nitrógeno fijado, petróleo, derivados
del petróleo, cal viva, tierras raras, silicio, acero, azufre, uranio, hierro
forjado`

(*el bronce es **producido**, no de mapa: ése sí es alcanzable desde el 1.9.)

### 4.2 Sistemas que SPEC-007 promete y **no existen en el kernel**

Medido por ocurrencias en `step.hpp`: `upkeep` 0 · `energy` 0 · `reserve` 0 ·
`extracted` 0 · `farm` 0 · `forestry` 0.

Son **cinco sistemas completos** —energía por edad, upkeep, reserva y
recuperación de yacimientos, granjas, reforestación— especificados con detalle y
con cero líneas escritas.

### 4.3 Épocas

**15 épocas declaradas · contenido real para las épocas 3–5.** Las civilizaciones
tienen ventanas de 3–4 (Egipto) y 5–5 (Roma).

**Esto no es necesariamente malo** —el alcance de la 1.0 (ADR-012) son 4–6
civilizaciones y las épocas del slice, no las 15— pero conviene decirlo en voz
alta: **el SPEC-007 describe un juego mucho mayor que el que estamos
construyendo para la 1.0**, y esa diferencia debería estar escrita en el plan
para que nadie la descubra tarde.

---

## 5. Cuarto hallazgo: la Fase 1 sigue sin cerrar, y bloquea

El **cierre mecánico de Fase 1** es el Sprint **1.13**: `ATTACK`, `ATTACK_MOVE`
y proyectiles con viaje. **Sigue abierto desde la desviación de numeración del
2026-07-28.**

Mientras siga abierto:

- **Arte y audio siguen bloqueados** por decisión del Director. Llevan
  bloqueados varias semanas.
- El jugador **no puede ordenar un ataque**: el combate solo ocurre por
  proximidad automática. Es la carencia más visible jugando, por encima de
  cualquier recurso que falte.

Y hay una ironía que conviene ver: acabamos de dar al combate un **modelo de
daño con armadura, tipos y contadores**, y el jugador todavía **no puede decirle
a una unidad a quién atacar**.

---

## 6. Quinto hallazgo: el adaptador, deuda a medio pagar

- `chunsa_sim_node.cpp` son **4232 líneas** en un solo fichero.
- La política pura **sí** está probada: `fog_view`, `outcome_view`,
  `affordability_view`, `command_panel_view` — cuatro cabeceras con prueba.
- El **dibujado y el enrutado de entrada no tienen prueba ninguna**, y ahí es
  donde han aparecido los últimos fallos: el panel tapando la barra, el modo
  construcción sin salida, el texto cortado.
- **No hay guardián del `.so`**, solo del `.chdb`. Hoy están sincronizados, pero
  el `.so` ya ha estado obsoleto dos veces con síntomas que costaron sesiones
  enteras de pruebas del Director.

---

## 7. Estado del plan como documento

El orden en `PLAN_MAESTRO.md` **no refleja el orden real de ejecución**:

`1.8A 1.8B 1.8C 1.8D 1.8F 1.8E 1.8H 1.8G 1.9 1.9B 1.10 1.11 1.12 1.13 1.18 1.14 1.15 1.16 1.17`

El 1.8F aparece antes del 1.8E, el 1.8G después del 1.8H que lo absorbió, y el
1.18 en medio de la serie 1.1x. Además **1.9, 1.18 y 1.8H están hechos y el plan
no lo dice**. Un plan que hay que interpretar deja de ser fuente de verdad.

---

## 8. Qué hacer, y por qué en ese orden

### Inmediato

**1. Sprint 1.13 — órdenes de combate.** Cierra la Fase 1, desbloquea arte y
audio, y es la carencia más visible jugando. Además es *ahora* cuando más
sentido tiene: el modelo de daño ya merece que se le den órdenes.

**2. Sprint nuevo — «la IA se pone al día».** Que fabrique, que entienda
armadura y contadores al elegir qué entrenar. Sin esto, cada sprint de
profundidad ensancha la brecha entre lo que el juego permite y lo que el
oponente sabe. Debería ir **inmediatamente después** del 1.13, para que la IA
aprenda a la vez a atacar y a usar lo que tiene.

**3. Sprint nuevo, pequeño — medición de rendimiento.** Un banco que mida
`Step()` y el checksum en el escenario de SPEC-008 y falle si se dispara
respecto a una referencia registrada. No valida el objetivo absoluto sin el
hardware, pero **detecta regresiones**, que es el 80 % del valor.

### Después

**4. 1.14 población por casas** — la divergencia con AoE2 que más cambia el
ritmo. **5. 1.12 granjas** — cierra el bucle económico sostenible.
**6. 1.10/1.11** energía y reserva. **7. 1.9B** carbón y hierro forjado.

### Mantenimiento del plan

Reordenar `PLAN_MAESTRO.md` al orden real, marcar lo hecho, y **escribir
explícitamente que SPEC-007 describe un juego mayor que la 1.0**, con qué parte
entra en el alcance y qué parte es horizonte.

---

## 9. Lo que NO recomiendo hacer todavía

- **Más recursos en el mapa.** Con 22 sin uso, añadir presencia física a más
  solo aumenta el ruido. Primero que existan los sistemas que los consumen.
- **Más épocas de contenido.** Con 3–5 pobladas y la Fase 1 sin cerrar, ampliar
  a lo ancho antes de cerrar a lo hondo es la forma clásica de no terminar nunca.
- **Refactorizar el adaptador en ficheros.** Duele mirarlo, pero 4232 líneas que
  funcionan y están bajo control valen más que una reorganización sin pruebas
  que la respalden. Primero pruebas de dibujado, después la partición.
