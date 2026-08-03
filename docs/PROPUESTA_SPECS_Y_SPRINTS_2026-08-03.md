# Propuesta de SPECs y Sprints — revisión del plan maestro

Arquitecto Jefe · 2026-08-03 · **pendiente de aprobación del Director**

---

## §1 Estado medido, no recordado

| | Hoy | Alcance 1.0 (ADR-012) |
|---|---|---|
| Civilizaciones | **2** | **4–6** |
| Edificios | 38 | — |
| Unidades | 7 | — |
| Tecnologías | **6** | — |
| Recursos definidos | 36 | — |
| Recursos **en algún mapa** | **12** | — |
| Épocas con contenido | **1–15** | M1–M3 |
| Pruebas | 48 | — |

Sistemas de SPEC-007 con **cero líneas** en el kernel: energía por edad,
upkeep, granjas, reforestación. (Reserva/recuperación tiene 3 ocurrencias
sueltas, no un sistema.)

---

## §2 Tres hallazgos que cambian el orden del plan

### 2.1 La partida de 15 épocas que acabamos de montar NO SE PUEDE TERMINAR

Es el hallazgo grave y sale de cruzar dos cosas que hicimos por separado.

- El mapa tiene **6000 de comida en total**, para **los dos** jugadores.
- No existen granjas. La comida **no se regenera**: `economy.hpp` y `step.hpp`
  tienen cero ocurrencias de regeneración.
- Subir de época cuesta **200 de comida**. Catorce subidas son **2800**, casi
  la mitad del mapa, sólo en avanzar.
- Un aldeano cuesta 25. Lo que queda da para unos 60 aldeanos por bando **en
  toda la partida**, sin contar ejército.

El marco de 15 épocas del Sprint 1.25 es correcto y **económicamente
inviable**: la comida se agota mucho antes de la época 15. No es un problema de
balance de números, es que **falta un sistema entero** — el de SPEC-007 §15.

Esto asciende las granjas de "sistema pendiente" a **bloqueante**, y lo asciende
nuestro propio trabajo reciente, no una opinión.

### 2.2 Hemos construido PROFUNDIDAD donde el alcance pide ANCHURA

El alcance vinculante de la 1.0 son **4–6 civilizaciones** y las épocas del
slice. Tenemos **2 civilizaciones y las 15 épocas**.

No está mal hecho —el Director lo pidió— pero conviene verlo claro: **38
edificios reparten entre dos civilizaciones lo que la 1.0 necesita repartido
entre cuatro**. Y la infraestructura que construimos para escalar (plantillas
del 1.24, guardián de jugabilidad, marco de épocas) **está probada con dos
civilizaciones y con dos nada más**. Su valor sigue siendo una hipótesis hasta
que entre una tercera.

### 2.3 El SPEC que bloquea la tercera civilización NO EXISTE

El plan maestro §3 dice, literalmente, que **SPEC-TRAYECTORIA** se escribe
«antes de integrar la 3ª civ». Estado actual: **POR ESCRIBIR**. No hay fichero.

Es el contrato de ADR-016: sucesión y legado de módulos históricos,
identificadores con espacio de nombres, reglas de era. Sin él, meter Mali o
Tawantinsuyu es improvisar la parte más difícil de deshacer.

**Además tenemos ahora la pregunta resuelta a medias**: el Sprint 1.25 ya
inventó una trayectoria de facto para Egipto e Italia —linajes continuos con
`playable_periods` por época—. Eso es material de primera para el SPEC, pero es
precedente sin contrato.

---

## §3 SPECs propuestas

### SPEC-TRAYECTORIA — **escribir YA** (bloqueante)

Contrato de ADR-016. Debe fijar:

1. Qué es un **linaje**: una civilización que atraviesa épocas cambiando de
   régimen sin cambiar de identidad. Ya hay dos implementados; el SPEC los
   describe en vez de inventarlos.
2. **Qué se hereda y qué se pierde** al cambiar de periodo. La pregunta que
   quedó abierta en SPEC-007 §22.2 y que sigue sin respuesta del Director.
3. **Fondo común contra contenido propio**: cuándo un edificio va a plantilla
   compartida y cuándo es de una civilización. El 1.24 dio el mecanismo; falta
   la regla de cuándo usarlo.
4. **Espacios de nombres** e identificadores, para que la 3ª y 4ª civilización
   no colisionen ni obliguen a renumerar.

Sin esto no entra la tercera civilización. Con esto, entra en semanas y no en
meses, porque las plantillas ya existen.

### SPEC-010 (nueva) — Sostenibilidad económica

Hoy no hay SPEC que responda a «¿de dónde sale la comida en el minuto 60?».
SPEC-007 lo menciona en §15/§16 pero como catálogo de sistemas, no como
contrato. Debe fijar:

- **Granjas**: fuente construida y renovable, con coste de mantenimiento.
- **Reforestación**: lo mismo para la madera, desde la época 7.
- **Reserva y recuperación de yacimientos**: por qué un yacimiento agotado no
  es el fin, y qué tecnología lo recupera (el corpus ya tiene la flotación de
  1900–1916 documentada y sin usar).
- **Mercado**: con 36 recursos, quedarse sin uno es más probable, no menos.

Es el sprint 1.17 del plan viejo, ascendido a SPEC porque son cuatro sistemas
que se sostienen entre sí y decidirlos por separado produce incoherencias.

### SPEC-003 (assets) — **mantener diferida**, con motivo nuevo

Su condición de entrada era el cierre mecánico de Fase 1, que **ya se cumplió**
con el 1.13. Aun así recomiendo no abrirla: poner arte a 2 civilizaciones
cuando el alcance pide 4–6 significa **rehacer el trabajo de arte** al entrar
las otras dos. El arte va después de la anchura, no antes.

---

## §4 Sprints propuestos, en orden

| # | Nombre | Por qué ahí | DoD |
|---|---|---|---|
| **1.28** | **Granjas y comida renovable** | Desbloquea la partida larga que el 1.25 hizo posible y el mapa hace imposible. Es el único que arregla un juego roto, no uno mejorable | Una partida puede llegar a la época 15 sin quedarse sin comida · la granja tiene coste y mantenimiento · determinista |
| **1.29** | **SPEC-TRAYECTORIA** (documento) | Bloquea la 3ª civ. Se escribe describiendo lo que el 1.25 ya construyó, no inventando | Aprobado por el Director · responde las 3 preguntas de §22.2 |
| **1.30** | **Tercera civilización** (Mali o Tawantinsuyu) | Es la prueba de fuego de todo lo construido en 1.22–1.27. Si las plantillas valen, se ve aquí | Jugable de la 1 a la 15 · guardián en verde sin tocar el kernel · **medir cuánto contenido fue compartido y cuánto propio** |
| **1.31** | **Saturación por depósito** | Del plan viejo (1.16). Barato y cambia el juego: amontonar deja de ser óptimo | Rendimiento decreciente por depósito · se nota jugando |
| **1.32** | **Mercado** | Del plan viejo (1.17). Va DESPUÉS de granjas y 3ª civ porque su valor crece con el número de recursos accesibles | Trueque con precio móvil · sin float · no genera recursos de la nada |

---

## §5 Lo que NO propongo, y por qué

- **Más épocas ni más contenido para Egipto/Roma.** Ya tienen más profundidad
  que la que la 1.0 pide. Lo siguiente que aporta es la tercera civilización.
- **Las 3 celdas de andamiaje** (Egipto época 10, época 15 de ambos). Son tres
  huecos declarados y honestos; ascenderlos aporta menos que cualquier sprint
  de esta lista.
- **Ajustar la rampa de épocas** (6000 ticks acumulativos). Depende de las
  granjas: hoy la rampa no es el cuello de botella, la comida sí. Medir después.
- **Netcode.** Fuera de alcance por ADR-021 y sigue estándolo.

---

## §6 La decisión que necesito de ti

El orden de arriba supone que **arreglar la partida larga va antes que ampliar
el elenco**. Si prefieres la tercera civilización primero, es defendible —da
variedad antes— pero entonces la partida seguirá sin poder terminarse, y
tendremos tres civilizaciones incapaces de llegar a la época 15 en vez de dos.

Mi recomendación es 1.28 primero. Es el único sprint de la lista que arregla
algo **roto** en lugar de añadir algo nuevo.
