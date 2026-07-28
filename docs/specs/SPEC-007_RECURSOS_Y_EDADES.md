# SPEC-007 — Modelo de recursos y escalera de edades

**Estado: PROPUESTA. Requiere aprobación del Director antes de implementar.**

Fecha: 2026-07-28
Origen: directriz del Director (2026-07-28) — jugabilidad tipo AoE2, recursos
por edad, recolectados **y** producidos, científicamente correctos, y
tecnologías de extracción que reabran yacimientos agotados.

---

## §1 Lo que ya existe (base de la propuesta)

No parto de cero. El kernel y los datos ya soportan:

| Pieza | Estado hoy |
|---|---|
| Épocas | `epoch` 1..15 en el esquema; `EPOCH_MAX_V1 = 7` en el slice actual |
| Capacidades | `player_caps` bitmask; las techs son **paquetes de capacidad** que gatean contenido (§12.1) |
| Gating por época | `epoch_window` en unidades y edificios; `tech.epoch <= player_epoch` |
| Periodos históricos | `playable_periods` con `start_year`/`end_year` reales |
| Recursos | **3 cableados**: `player_stock[...][3]`, literalmente `A`, `B`, `Me` |
| Depósitos | `ECO_MAX_DEPOSITS = 32` (12 en uso), campo `remaining` |

Anclas históricas ya authored: Egipto −3100/−1069 (épocas 3-4), Roma −509/+476
(época 5).

`data/epochs/` y `data/resources/` existen pero están **vacías**. Las edades y
los recursos nunca se han definido como datos: es exactamente el hueco que esta
spec llena.

---

## §2 Escalera de edades (15)

Anclada a las épocas ya usadas por los datos: Egipto 3-4, Roma 5.

| # | Edad | Ancla histórica | Recursos que introduce |
|---|---|---|---|
| 1 | Paleolítica | caza y recolección | comida, madera, piedra |
| 2 | Neolítica | agricultura, −10000 | *(granja: comida renovable)* |
| 3 | Calcolítica | cobre nativo, −4500 · **Egipto** | cobre, oro |
| 4 | Bronce | −3300 · **Egipto Reino Nuevo** | estaño · **bronce (producido)** |
| 5 | Hierro / Clásica | −1200 · **Roma** | mena de hierro · **hierro forjado (producido)** |
| 6 | Tardoantigua | 300–800 | — |
| 7 | Medieval | 800–1300 · *tope del slice v1* | carbón vegetal (producido) |
| 8 | Pólvora | 1300–1500 | salitre, azufre · **pólvora (producida)** |
| 9 | Oceánica | 1500–1700 | — |
| 10 | Manufactura | 1700–1760 | — |
| 11 | Industrial I | 1760–1840, vapor | **carbón** (energético) · **coque (producido)** |
| 12 | Industrial II | 1840–1900, acero | **acero (producido)** · electricidad |
| 13 | Petróleo | 1900–1945 | **petróleo** (energético) · derivados |
| 14 | Atómica | 1945–1980 | uranio · **aluminio (producido)** |
| 15 | Información | 1980– | silicio · tierras raras |

Las edades 6, 9 y 10 no introducen recurso nuevo a propósito: no toda edad
tiene que hacerlo, y su valor está en unidades, edificios y capacidades.

---

## §3 Modelo de recursos

### §3.1 Dos naturalezas

- **Recolectado**: sale de un depósito del mapa mediante un aldeano.
- **Producido**: sale de un edificio que consume otros recursos según una
  receta. No existe en el mapa.

La distinción es de datos, no de código: un recurso producido simplemente
carece de depósitos.

### §3.2 Cadenas de producción (científicamente correctas)

| Producido | Receta | Edad | Nota |
|---|---|---|---|
| bronce | cobre + estaño | 4 | ~9:1 histórico; se propone 4:1 por jugabilidad |
| hierro forjado | mena de hierro + carbón vegetal | 5 | reducción en horno de cuba |
| carbón vegetal | madera | 7 | insumo metalúrgico previo al coque |
| pólvora | salitre + azufre + carbón vegetal | 8 | ~75:10:15 histórico |
| coque | carbón | 11 | destilación seca; sustituye al vegetal |
| acero | hierro + coque | 12 | Bessemer |
| aluminio | bauxita + **electricidad** | 14 | Hall-Héroult: sin energía no hay aluminio |

La última fila es el motivo de que los energéticos importen: a partir de la
edad 12 hay recetas cuyo insumo es **energía**, no materia.

### §3.3 Dimensionado

`player_stock[MAX_EMITTERS][RESOURCE_COUNT]` con **`RESOURCE_COUNT = 24`**
fijado de una vez. Motivo: cada ampliación del vector obliga a subir
`SAVE_FORMAT_VERSION` y `CHECKSUM_ALGO_VERSION` e invalida todos los baselines.
Es mucho más barato reservar el espacio ahora y dejar slots a cero que repetir
esa migración en cada edad.

Los índices se declaran **en datos** (`data/resources/`), no en el código.
El kernel solo conoce `RESOURCE_COUNT` y trata los índices como opacos.

`dropoff_mask` pasa de `uint8_t` a `uint32_t` (24 bits en uso).

---

## §4 Depósitos: finitos, pero reabribles por tecnología

Es la mecánica distintiva que pide el Director, y es la que más me gusta de
esta propuesta porque es **realista y crea decisiones**.

### §4.1 El problema con el modelo actual

Hoy un depósito tiene `remaining`, baja hasta 0 y muere para siempre. En la
realidad un yacimiento no se "acaba": se agota **para la tecnología
disponible**. Mejor tecnología reabre minas dadas por muertas.

### §4.2 Modelo propuesto

Cada depósito pasa de un campo a dos:

```
reserve_total   cantidad geológica total del yacimiento (fija, del mapa)
extracted       cuánto se ha sacado ya (crece, nunca decrece)
```

Y el jugador tiene un **factor de recuperación** por recurso, derivado de sus
capacidades de extracción:

```
extraíble_ahora = (reserve_total * recovery_pct[owner][resource]) / 100 - extracted
```

Un depósito está agotado **para ese jugador** cuando `extraíble_ahora <= 0`.
Investigar una tecnología de extracción sube `recovery_pct` y **reabre
automáticamente** todos los yacimientos de ese recurso.

Ejemplo: minería a cielo abierto 40% → galería 60% → voladura 75% → flotación
90%. Un depósito de 5000 da 2000 en la edad 5, y otros 1000 al llegar a
galería, sin tocar el mapa.

### §4.3 Por qué es determinista

Todo es aritmética entera: multiplicación y división enteras sobre
`reserve_total` (dato del mapa) y `recovery_pct` (derivado del bitmask de
capacidades). Sin float, sin estado oculto. `recovery_pct` **no** es estado
nuevo persistido: se **deriva** de `player_caps`, que ya está en el save y en
el checksum.

### §4.4 Consecuencia de diseño

Cambia el ritmo de la partida en la dirección que pide el Director: expandirse
deja de ser la única respuesta al agotamiento; **investigar** es la otra. Y
premia volver a territorio propio ya explotado en vez de empujar siempre hacia
fuera.

---

## §5 Comida renovable (granjas)

- La granja es un **edificio** que, al completarse, crea un depósito propio de
  comida asociado a su posición.
- Ese depósito tiene `reserve_total` alto y **se regenera**: `extracted`
  decrece a ritmo fijo por tick mientras la granja siga viva.
- Se trabaja con el mismo `GATHER` y la misma máquina económica: para el
  aldeano es un depósito normal.
- Al destruirse la granja, su depósito desaparece.

Coste: los depósitos dejan de ser puramente estáticos del mapa. `ECO_MAX_DEPOSITS`
deberá subir bastante (una granja por depósito) — propongo **128**.

---

## §6 Impacto y secuenciación

Esto **no** es un sprint. Propongo trocearlo así, cada trozo jugable:

| Sprint | Contenido | Riesgo |
|---|---|---|
| **1.7** *(en curso)* | Zona aliada (§23) + HUD. **Desbloquea la partida actual** | bajo |
| **1.8** | `RESOURCE_COUNT=24`, recursos como datos, renombrar A/B/Me a comida/madera/piedra, depósitos grandes. Sin recetas | medio: toca save, checksum, catálogo, todos los costes y el HUD |
| **1.9** | Recetas de producción + edificio de conversión (bronce, hierro forjado) | medio |
| **1.10** | Reserva/recuperación (§4) + techs de extracción | medio |
| **1.11** | Granjas (§5) | medio |

El 1.8 es el más caro y el menos vistoso: es migración de datos pura. Es la
lane de MiniMax.

---

## §7 Decisiones abiertas

1. **Escalera de edades**: ¿te encaja el reparto de §2, o quieres mover anclas?
   Es la decisión más difícil de cambiar después, porque los datos históricos
   de cada civilización se cuelgan de ella.
2. **`RESOURCE_COUNT = 24`**: reservar de más cuesta memoria y ancho de
   checksum; quedarse corto cuesta una migración completa. 24 es mi
   recomendación.
3. **Ratio del bronce**: histórico ~9:1 cobre:estaño. Propongo 4:1 para que el
   estaño sea escaso pero no frustrante. ¿Fidelidad o jugabilidad?
