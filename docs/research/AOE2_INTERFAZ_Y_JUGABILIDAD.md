# AoE2 — interfaz y jugabilidad, verificado

**Autor:** Arquitecto (Claude Opus 5) · 2026-07-30
**Motivo:** el Director pidió «que sea como AoE2, revisa cómo es la interfaz y
la jugabilidad». La delegación previa
(`docs/research/UI_COSTES_AOE2.md`) volvió casi entera como **NO VERIFICADO**
porque Fandom, Liquipedia y Reddit devuelven 402/403. Aquí solo va lo que he
podido **confirmar contra una fuente accesible**, y se dice de dónde sale.

---

## 1. Disposición de la pantalla

- **Barra superior**: **recursos a la izquierda**, **edad actual en el centro**,
  chat y diplomacia a la derecha.
- **Barra inferior**: **barra de comandos e información**.

> Fuente: Age of Empires Series Wiki, *User interface* (vía resultados de
> búsqueda; la página directa devuelve 402).

**Consecuencia para nosotros**: nuestro HUD ya pone los recursos arriba a la
izquierda, así que **eso ya coincide**. Lo que no coincide es que el catálogo de
construcción esté flotando arriba a la derecha en vez de en una barra inferior.

**Corrección de mi SPEC**: escribí «el panel va abajo a la derecha». Es
**inexacto** — la barra de comandos ocupa la parte **inferior**, y el minimapa
va en ella. Corregido en SPEC-006 §24.

## 2. La rejilla de comandos — dato duro

**Tres filas × cinco columnas**, con las teclas colocadas como en el teclado:

```
Q  W  E  R  T
A  S  D  F  G
Z  X  C  V  B
```

> «the hotkey designated for the action located at the top-left corner is 'Q',
> while the hotkey for the subsequent action to its right is 'W', and so forth»
> — DiamondLobby, *Age of Empires 2 Hotkey Guide*.

**Convención de contenido**: las unidades y estructuras principales van en la
**fila de arriba** (Q–T); las mejoras y tecnologías, en las filas media y baja.

**Mi 5×3 del SPEC era correcto**, pero por suerte: lo había marcado como
decisión mía por falta de fuente. Ahora está verificado.

## 3. Menús de construcción por páginas

Un aldeano no ve todos los edificios de golpe: hay **páginas**.

- **Q** → menú de edificios **económicos** (casas, molinos, campamentos
  madereros, granjas).
- **W** → menú de edificios **militares** (cuartel, torres, murallas).
- **V** → «More Buildings», más edificios.

> Fuentes: DiamondLobby (íd.) y AgeOfNotes, *Complete Hotkeys Guide*.

**Consecuencia para nosotros**: con 15 edades vamos a tener muchos más
edificios que AoE2. **La paginación no es un adorno, es la solución al problema
que vamos a tener.** Entra en el diseño.

## 4. Cómo se ve el coste

Confirmado como patrón, no como pixel: **tooltip al pasar el ratón**, con
**icono de recurso + número entero**, y junto a él el tiempo y la tecla rápida.
Lo impagable **atenúa el botón** y pone el coste en rojo; el botón **no
desaparece ni se desactiva**.

Los detalles exactos de DE (opacidad, color, tipografía) siguen
**NO VERIFICADOS** y no merece la pena perseguirlos: son cosméticos.

## 5. Jugabilidad — y qué tenemos ya

| Mecánica de AoE2 | Verificado | En CHUNSA hoy |
|---|---|---|
| 4 recursos: comida, madera, oro, piedra | Wikipedia | **30 recursos**, deliberadamente distinto |
| Comida de caza, arbustos, granjas, pesca | Wikipedia | Depósitos de comida; granjas en el 1.12 |
| **Los aldeanos depositan en un edificio**: centro, molino, campamento | Wikipedia | **YA**: `find_building_dropoff` elige el **más cercano**, con desempate por índice bajo |
| **La distancia al depósito importa** → colocar campamentos junto al recurso es habilidad | Wiki AoE2 vía búsqueda | **YA**, por la misma función |
| ~8 aldeanos por campamento antes de saturar | Wiki AoE2 vía búsqueda | **NO** modelado |
| Población por casas, de 25 en 25 hasta 200 | Wikipedia | **NO**: `POP_CAP_V1 = 200` fijo, sin casas |
| Avanzar de edad exige **edificios de la edad actual** + recursos | Wikipedia | **YA**: `≥ 2 edificios completos` de la época actual + coste + tiempo mínimo |
| Piedra-papel-tijera entre tipos de unidad | Wikipedia | Parcial: hay `bonus_vs_bp`; el combate con órdenes llega en el 1.13 |
| Mejoras de herrería (ataque/armadura) | Wikipedia | Tecnologías existen; sin familia de herrería |
| Mercado con precios que fluctúan | Wikipedia | **NO** |
| Botón de aldeano ocioso, campana | Wikipedia | **NO** |

### 5.1 Lo que más se parece ya

**El bucle económico de AoE2 está montado**: recolectar → volver al depósito
más cercano → el depósito lo decide la distancia. Es la mecánica que hace que
colocar bien los edificios sea una habilidad, y **ya la tenemos**.

**El avance de edad también**: AoE2 pide edificios de tu edad actual antes de
dejarte subir; nuestro kernel pide **dos edificios completos** cuya ventana de
época incluya la actual, más coste y un tiempo mínimo.

### 5.2 Lo que más se aleja, por orden de impacto

1. **Población fija en 200 sin casas.** En AoE2 construir casas es una tarea
   constante que compite por madera y castiga el descuido. Nosotros regalamos
   200 de tope. Es una de las decisiones que más cambia el ritmo.
2. **Sin botón de aldeano ocioso.** En AoE2 es de las ayudas más usadas, y
   nuestro HUD **ya cuenta los ociosos** — falta poder saltar a ellos.
3. **Sin saturación por campamento.** Hoy nada desincentiva amontonar aldeanos.
4. **Sin mercado.** Sin él, un recurso que se acaba es un callejón sin salida.
   Con 30 recursos esto pesa más que en AoE2, no menos.

---

## 6. Fuentes que respondieron

- https://en.wikipedia.org/wiki/Age_of_Empires_II — recursos, edades, centro
  urbano, población 25–200, contadores, edificios militares.
- https://diamondlobby.com/age-of-empires-2/hotkey-guide-aoe2/ — rejilla
  QWERT/ASDFG/ZXCVB y páginas de construcción.
- https://ageofnotes.com/tutorials/complete-hotkeys-guide-for-age-of-empires-2-definitive/
  — teclas de ciclo y presets.
- Wiki de la serie (vía búsqueda; la página directa da 402) — barra superior,
  barra inferior, campamentos como punto de entrega y distancia.

**Devolvieron 402/403**: `ageofempires.fandom.com`, `liquipedia.net`,
`reddit.com`. No se ha inventado ninguna cita para rellenar.
