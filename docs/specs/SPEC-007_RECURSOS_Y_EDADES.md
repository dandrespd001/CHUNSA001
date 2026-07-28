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

---

# REVISIÓN TRAS PANEL MULTIMODELO (2026-07-28)

Panel: MiniMax-M3 (investigación con `WebFetch`, 33 URLs), Gemini 3.1 Pro /
3.5 Flash / 3.1 Flash-Lite (investigación web), DeepSeek V4 Pro y Qwen 3.7 Plus
(crítica adversarial sin web). Material en `docs/research/`.

Las correcciones §8.1–§8.5 son **firmes**: son errores del documento original,
independientes de cualquier decisión pendiente. §8.6 y §8.7 quedan **abiertas**.

## §8.1 La energía NO es un índice de `player_stock` — CORRECCIÓN FIRME

Los **tres** críticos lo señalaron de forma independiente, que es la señal más
fuerte que produjo el panel.

`[V] [Total Annihilation]` «Out of energy? power-dependent structures such as
radar towers, metal extractors and laser towers will cease to function».

La energía **no se almacena por aldeano ni se agota como insumo: se gasta al
instante**. La dicotomía de §3.1 (recolectado / producido) no la captura, y la
receta de aluminio de §3.2 —que lista «electricidad» como si fuera materia—
es incorrecta tanto física como computacionalmente.

**Corrección.** Tercera naturaleza, **streaming**:

- No ocupa índice en `player_stock`.
- Se deriva cada tick: `disponible = producción − consumo`.
- Si `disponible < 0`, las recetas dependientes de energía **se paran en seco**
  (modelo TA), no se ralentizan ni encolan. Es lo único que hace que la edad 14
  (aluminio por electrólisis) se sienta distinta de la 13.

## §8.2 El recuento de recursos del documento estaba mal — CORRECCIÓN FIRME

`§3.2` usa **bauxita** en la receta del aluminio, pero `§2` nunca la introduce
como recurso ni la cuenta. El total real supera al declarado. Antes de fijar
`RESOURCE_COUNT` hay que **enumerar exhaustivamente** los recursos por edad y
contar, no estimar.

## §8.3 Contradicción interna §3.1 ↔ §5 — CORRECCIÓN FIRME

`§3.1` afirma que la distinción entre naturalezas es «de datos, no de código».
`§5` exige que la granja **regenere** `extracted` por tick, que es lógica de
simulación. Las dos cosas no pueden ser ciertas a la vez.

**Corrección.** La granja es un **depósito con regeneración**, y eso **sí** es
comportamiento en el kernel. §3.1 se reescribe: lo que es puro dato es *qué*
recursos existen y sus recetas; *cómo* se comporta un depósito (estático,
regenerativo) es código con un flag en datos.

## §8.4 `extracted` es del depósito, no del jugador — CORRECCIÓN FIRME

Hueco detectado por Qwen: el documento no dice qué ocurre con un yacimiento
agotado **al conquistarlo**, ni qué pasa si **dos jugadores** explotan el mismo.

**Corrección.** `extracted` es estado **del depósito**, compartido y global.
`recovery_pct` es **del jugador**. Consecuencias deliberadas: conquistar una
mina agotada **no la resetea** (mata el exploit), y un jugador con mejor
tecnología **sí** puede seguir extrayendo de un yacimiento que su rival dio por
muerto — que es justo la fantasía que perseguimos.

## §8.5 La recuperación no llega al 100% — CORRECCIÓN FIRME

`[I] [Qwen]` con recuperación total, la partida se vuelve infinita y desaparece
la presión por expandirse.

**Corrección.** El tope acumulado de `recovery_pct` queda **estrictamente por
debajo de 100** (propongo 90). Siempre queda reserva inaccesible: expandirse
sigue siendo necesario.

## §8.6 ABIERTA — ¿15 edades o menos?

Evidencia en contra de 15:

- `[V] [0 A.D.]` — RTS histórico como el nuestro, resuelve con **3 fases**
  (Village / Town / City).
- `[V] [Empire Earth]` — 14 épocas; la serie fue de 82% a 79% a **50%** de
  recepción. Más granularidad de edad no mejoró el juego.
- `[I] [DeepSeek]` — en partidas de 30–60 min, las últimas 8 edades **no se
  alcanzan nunca**: diseño y balance desperdiciados.
- `[I] [Qwen]` — las edades 6, 9 y 10, sin recurso nuevo, son «valles de
  progresión».

Recomendación del panel: **7–9 edades** con **3 fases narrativas** encima.

**Decisión del Director pendiente.** Es la más cara de revertir: los datos
históricos de cada civilización (`epoch_window`, `playable_periods`) se cuelgan
de esta escalera.

## §8.7 ABIERTA — Falta un mecanismo de upkeep

`[V] [Warcraft III]` «producing units over certain amounts will decrease the
amount of gold one can earn».

SPEC-007 no tiene **ningún** freno económico al late game. A partir de la edad 9
la producción se descontrola. Hay que añadir coste de mantenimiento creciente
por edad, o un equivalente. **No especificado todavía.**

## §8.8 Riesgos aceptados conscientemente

No son errores, son apuestas. Quedan registradas para no re-litigarlas:

- **Depósitos finitos van contra la corriente.** `[V]` Rise of Nations eligió 6
  recursos **infinitos** a propósito; Total Annihilation, BAR y Supreme
  Commander tampoco modelan agotamiento. El único que sí lo hace, Anno, es
  cívico-casual, no RTS. Lo mantenemos porque el agotamiento **crea decisiones**
  y es el eje de §4, pero sabiendo que el RTS competitivo lo suele rechazar.
- **Carga cognitiva.** Se mitiga con la regla de **pocos recursos activos por
  edad**: muchos a lo largo de la partida, ~4–5 simultáneos, que es el nivel de
  AoE2.
- **HUD.** `[V]` Anno necesitó **3 regiones** para sostener 30+ recursos. Con
  más de ~8 activos habrá que **agrupar por familias** en el HUD.
