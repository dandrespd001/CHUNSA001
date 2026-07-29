# SPEC-007 — Modelo de recursos y escalera de edades

**Estado: APROBADA por el Director (2026-07-28). Lista para implementar.**

Aprobada con la escalera de **15 edades**, `RESOURCE_COUNT = 32`, los recursos
permaneciendo activos a lo largo de varias edades (§9), la energía como
**streaming** y no como stock (§8.1), y el **upkeep por consumo continuo**
(§10). Las cinco correcciones firmes de §8 forman parte de lo aprobado.

Quedan como **calibración de playtest**, no como decisiones de diseño
pendientes: el ratio del bronce (§3.2), las tasas de consumo del upkeep
(§10.4) y los porcentajes de recuperación por tecnología (§4.2).

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
| 2 | Neolítica | agricultura, −10000 | *(granja: comida renovable, §15)* |
| 3 | Calcolítica | cobre nativo, −4500 · **Egipto** | cobre, oro |
| 4 | Bronce | −3300 · **Egipto Reino Nuevo** | estaño · **bronce (producido)** |
| 5 | Hierro / Clásica | −1200 · **Roma** | mena de hierro · **hierro forjado (producido)** |
| 6 | Tardoantigua | 300–800 | — |
| 7 | Medieval | 800–1300 · *tope del slice v1* | carbón vegetal (producido) · *(plantación forestal: madera renovable, §16)* |
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

---

# §9 Análisis de recursos por edad (decisión del Director: 15 edades)

Directriz (2026-07-28): **15 edades**; los recursos esenciales y los
energéticos **permanecen activos a lo largo de varias edades**; se acepta algo
más de carga a cambio de realismo.

## §9.1 Principio: los recursos no se jubilan

Un recurso **no desaparece** al llegar la edad siguiente: **cambia de uso**.
Es históricamente cierto y resuelve la directriz sin reglas artificiales.

| Recurso | Uso temprano | Uso tardío | Activo |
|---|---|---|---|
| cobre | bronce (edad 4) | cableado eléctrico (12+) | 3–15 |
| estaño | bronce (edad 4) | soldadura, hojalata (11+) | 4–15 |
| madera | construcción | carbón vegetal, celulosa, química | 1–15 |
| piedra | muros, monumentos | hormigón, áridos | 1–15 |
| oro | moneda, prestigio | contactos electrónicos (14+) | 3–15 |
| comida | subsistencia | **upkeep militar (§10)** | 1–15 |
| carbón | coque metalúrgico (11) | generación eléctrica (12+) | 11–15 |

## §9.2 Catálogo completo

**Tabla autoritativa.** Incorpora las correcciones de §19. Si algo contradice
esta tabla en otra sección, manda ésta.

**Recolectados (20)** — salen de un depósito del mapa:

| # | Recurso | Edad | Nota |
|---:|---|---:|---|
| 1 | comida | 1 | fuentes naturales **finitas** (bayas, fruta, caza); la granja (§15) es la fuente construida y regenerativa |
| 2 | madera | 1 | bosques **finitos**; plantación forestal (§16) desde la edad 7 |
| 3 | piedra | 1 | |
| 4 | arcilla | 2 | cerámica, ladrillo, refractario de horno |
| 5 | cobre | 3 | nativo primero, mena después |
| 6 | oro | 3 | |
| 7 | plomo | 3 | fundido desde el 7º milenio a.C. · proyectiles → baterías |
| 8 | **sal** | 3 | conservación de alimentos; sostiene el ejército en campaña (§10) |
| 9 | estaño | 4 | escaso: es el cuello de botella del bronce |
| 10 | mena de hierro | 5 | |
| 11 | **caliza** | 5 | **fundente**: sin ella la reducción de mena de hierro es falsa |
| 12 | salitre | 8 | pólvora · también fertilizante |
| 13 | azufre | 8 | pólvora → ácido sulfúrico |
| 14 | carbón | 11 | energético |
| 15 | **nitrógeno fijado** | 12 | Haber-Bosch (1913) · fertilizantes y explosivos modernos |
| 16 | petróleo | 13 | energético |
| 17 | bauxita | 14 | insumo del aluminio |
| 18 | uranio | 14 | energético |
| 19 | silicio | 15 | de arena |
| 20 | tierras raras | 15 | un solo recurso por el criterio de §19.4 |

**Producidos (8)** — salen de un edificio con receta, no del mapa:

| # | Producido | Receta | Edad |
|---:|---|---|---:|
| 21 | bronce | cobre + estaño | 4 |
| 22 | carbón vegetal | madera | 5 |
| 23 | hierro forjado | mena de hierro + carbón vegetal + **caliza** | 5 |
| 24 | pólvora | salitre + azufre + carbón vegetal | 8 |
| 25 | coque | carbón | 9 |
| 26 | acero | hierro forjado + coque | 12 |
| 27 | aluminio | bauxita + **electricidad** | 12 |
| 28 | derivados del petróleo | petróleo | 13 |
| 29 | **cal viva** | caliza | 8 |
| 30 | **cemento** | caliza + arcilla (requiere energía) | 13 |

**Streaming (1)** — **no** ocupa índice de stock (§8.1): **electricidad**,
desde la edad 12. No se almacena: se deriva por tick y su falta **para en seco**
las recetas dependientes.

**Total almacenado: 30** de `RESOURCE_COUNT = 32` (§20.5). Quedan **2 slots**.
El margen que §9.3 reservó ya se ha consumido en dos rondas de investigación;
cualquier adición futura debe justificar el slot o asumir la migración.

**El carbón vegetal aparece en la edad 5**, no en la 7: la reducción de mena de
hierro lo exige desde el primer día del hierro. Y el coque **no lo sustituye**
(§19.3): lo desplaza del alto horno hacia 1850, pero el carbón vegetal sigue en
las fraguas pequeñas. Coexisten.

## §9.3 `RESOURCE_COUNT = 32`

25 almacenados + margen. **32 no es arbitrario**: es exactamente el ancho de
`dropoff_mask` como `uint32_t`, así que el mask y el vector de stock quedan
alineados y no hay que volver a tocar ninguno de los dos.

## §9.4 Carga simultánea por edad

Es la métrica que importa, no el total. «Activos» = recursos que el jugador
gasta o recolecta de verdad en esa edad.

| Edad | Activos | Cuáles |
|---|---:|---|
| 1 | 3 | comida, madera, piedra |
| 2 | 4 | + arcilla |
| 3 | 6 | + cobre, oro |
| 4 | 8 | + estaño, **bronce** |
| 5 | 11 | + mena de hierro, carbón vegetal, **hierro forjado** |
| 6–7 | 11 | sin recurso nuevo: consolidación |
| 8 | 15 | + plomo, salitre, azufre, **pólvora** |
| 9–10 | 15 | sin recurso nuevo |
| 11 | 17 | + carbón, **coque** |
| 12 | 19 | + **acero**, *electricidad (streaming)* |
| 13 | 21 | + petróleo, **derivados** |
| 14 | 24 | + bauxita, uranio, **aluminio** |
| 15 | 26 | + silicio, tierras raras |

**El pico es 26 en la edad 15**, muy por encima de los 4 de AoE2. Se acepta
conscientemente por la directriz del Director, con dos mitigaciones:

1. **La mayoría se gasta indirectamente.** Desde la edad 12 no compras unidades
   con cobre: compras con acero, y el cobre alimenta la receta. La superficie de
   **decisión** del jugador es mucho más estrecha que el número de recursos.
2. **Agrupación por familias en el HUD**, que el panel identificó como
   obligatoria por encima de ~8 activos: subsistencia · construcción ·
   metales base · metalurgia · química · energía. Seis grupos, no 26 contadores.

## §9.5 Los energéticos no se sustituyen: se acumulan

Directriz explícita del Director. Cada energético nuevo **no jubila** al
anterior; abre un uso distinto:

| Energético | Desde | Uso propio que no cubre el anterior |
|---|---|---|
| madera / carbón vegetal | 5 | reducción metalúrgica preindustrial |
| carbón | 11 | vapor y coque |
| petróleo | 13 | motor de combustión, química |
| uranio | 14 | densidad energética para generación masiva |

En la edad 15 conviven los cuatro. Es correcto: hoy el mundo sigue quemando
carbón, petróleo y uranio a la vez.

---

# §10 Upkeep: coste de mantenimiento (cierra §8.7)

## §10.1 Elección

De los tres modelos posibles —tope de población duro (AoE2), penalización de
ingresos (Warcraft III) y consumo continuo (Total War, Anno, Supreme
Commander)— se adopta el **tercero**, por ser el único históricamente veraz y
por reutilizar el sistema de energía que §8.1 ya obliga a construir.

## §10.2 Regla

- **Unidades militares consumen comida por tick** mientras viven. Es la
  logística de intendencia, el problema central de toda campaña antigua.
- **Edificios industriales consumen energía por tick** mientras operan.

El freno del late game deja de ser una regla de videojuego y pasa a ser
ficción: no sostienes 500 unidades porque no puedes alimentarlas.

## §10.3 Qué pasa al no poder pagar

Coherente con §8.1, **sin ralentización gradual**:

- Sin energía: los edificios dependientes **se paran**.
- Sin comida: las unidades **no mueren de golpe** — la muerte por inanición
  castiga demasiado un descuido momentáneo. Pierden eficacia (moral o daño) y
  la producción de unidades nuevas se bloquea hasta recuperar superávit.

## §10.4 Consecuencia sobre la comida

La comida deja de ser un recurso de arranque para convertirse en el **eje
económico de toda la partida**: es lo que sostiene al ejército en las 15
edades. Eso justifica por sí solo las granjas de §5 y su regeneración.

**Pendiente de especificar**: tasas concretas de consumo por clase de unidad y
por edificio. Requieren playtest, no se pueden fijar desde el escritorio.

---

# §11 Secuenciación revisada (sustituye a §6)

§6 planteaba el 1.8 como un solo sprint. El panel lo señaló como la
subestimación más grave del documento:

> `[I] [DeepSeek]` La migración de 3 a 24 recursos + renombrado + costes nuevos
> no es solo un cambio técnico; requiere **diseñar y balancear los costes de
> todas las unidades, edificios y tecnologías existentes** → el diseño de
> contenido absorberá meses, no un sprint, y el juego será injugable hasta
> completarlo.
>
> `[I] [Qwen]` Sprint 1.8 toca save, checksum, catálogo, costes y HUD
> simultáneamente → riesgo de regresión catastrófica; debería dividirse en
> migración de datos y actualización de UI.

Ambos tienen razón, y el riesgo real no es el esfuerzo: es que **el juego quede
injugable a mitad de migración**. Se divide así:

| Sprint | Contenido | Criterio de "hecho" |
|---|---|---|
| **1.7** *(en curso)* | Zona aliada §23 + HUD codificación | El Director juega y recolecta |
| **1.8A** | `RESOURCE_COUNT = 32`, `dropoff_mask` a `uint32_t`, save/checksum. **Sin tocar datos**: los índices 0/1/2 siguen siendo A/B/Me y 3..31 quedan a cero | Suite verde, baselines re-registrados, **el juego sigue jugable exactamente igual** |
| **1.8B** | `data/resources/` authored: los 25 recursos con nombre, familia y edad. Renombrar A/B/Me → comida/madera/piedra. Sin recursos nuevos en el mapa | El juego sigue jugable; el HUD muestra nombres reales |
| **1.8C** | Depósitos grandes + el resto de recolectados en el mapa por edad | Partida larga sin peregrinaciones |
| **1.9** | Recetas y edificio de conversión (bronce, hierro forjado) | Se puede fabricar bronce |
| **1.10** | Energía streaming §8.1 + upkeep §10 | El late game tiene freno |
| **1.11** | Reserva/recuperación §4 + techs de extracción | Reabrir una mina agotada |
| **1.12** | Granjas §5 | Comida sostenible |

**La regla que gobierna el troceo**: al final de cada sprint el juego debe
quedar **jugable**. 1.8A es puramente estructural y no debe cambiar ni una
trayectoria observable salvo los checksums; si cambia algo más, es un error.

**Orden deliberado**: la energía (1.10) va **antes** que la recuperación (1.11)
porque el upkeep es lo que frena el late game, y sin freno no se puede calibrar
nada de lo que venga después.

---

# §12 Recetas y producción de recursos (Sprint 1.9)

## §12.1 Concepto

Un **recurso producido** no existe en el mapa: sale de un edificio que consume
otros recursos según una **receta**. Es la mecánica que hace que cobre y estaño
importen más allá de la edad 4.

**Restricción de diseño, deliberada y no negociable:** las cadenas son de **un
solo paso**. `cobre + estaño → bronce`, nunca `A → B → C → D`.

`[V] [Widelands]` demuestra que las cadenas profundas funcionan, pero convierten
el juego en gestión logística. `[I] [DeepSeek]` lo señaló como riesgo:
«se añade una capa de gestión de fábricas típica de Factorio, ajena al ritmo de
un RTS». Se acepta la crítica: profundidad 1, y quien quiera más, que juegue a
otra cosa.

## §12.2 Datos

`RecipeDefinitionV1`, tabla tipada nueva en el catálogo:

```
id                  RecipeId
output_resource     índice de recurso (0..RESOURCE_COUNT)
output_amount       >= 1
inputs[4]           pares (índice de recurso, cantidad), hasta 4
input_count         1..4
craft_time_ticks    >= 1
epoch               1..15
building_id         edificio que la ejecuta
```

Referencias no resolubles ⇒ **error de carga**, mismo criterio que el resto del
catálogo.

`BuildingDefinitionV1` gana `recipes[8]` / `recipe_count`, análogo al
`researches` que ya existe.

## §12.3 Estado

```cpp
uint32_t craft_recipe[ENTITY_HARD_CAP];    // INVALID_RECIPE_ID = ocioso
uint32_t craft_progress[ENTITY_HARD_CAP];
```

Mismo patrón que `research_tech`/`research_progress` (§12.2 de SPEC-004). No se
inventa una estructura nueva donde ya hay una que funciona.

## §12.4 Comando `CRAFT = 14`

`CommandType` es append-only: **14**, a continuación de `GATHER = 13`.

- `p.handle` = edificio propio **completo** cuya lista `recipes` contiene la
  receta.
- `p.unit_id` = `RecipeId`.

**Orden de validación** — idéntico en forma al de `RESEARCH_TECH`, y ese orden
es contractual:

1. handle vivo · 2. propio · 3. es edificio · 4. completo ·
5. receta en la lista del edificio · 6. `recipe.epoch <= player_epoch` ·
7. edificio ocioso de producción · 8. stock cubre **todos** los inputs.

Cualquier fallo ⇒ `ILLEGAL_STATE`, salvo los de handle que ya tienen su código.

**Efecto al aceptar**: deducir **todos** los inputs de golpe, fijar
`craft_recipe`/`craft_progress`.

**Al completar** (`craft_system`, misma fase que `production_system`):
`player_stock[owner][output_resource] += output_amount`, y el edificio queda
ocioso.

**Deducción por adelantado, deliberado**: si los inputs se dedujeran al
terminar, un jugador podría encolar producción que no puede pagar y el sistema
tendría que decidir qué hacer al final. Deducir al aceptar elimina esa clase
entera de casos.

## §12.5 Versionado

`SAVE_FORMAT_VERSION` +1 · `CHECKSUM_ALGO_VERSION` +1. `craft_recipe` y
`craft_progress` entran en serialización y checksum.

## §12.6 Criterios de aceptación (= lista de pruebas del sprint)

1. `CRAFT` con inputs suficientes es **aceptado** y deduce **todos** los inputs
   en el tick de aceptación.
2. `CRAFT` con **un** input insuficiente es rechazado con `ILLEGAL_STATE` y
   **no deduce nada**.
3. Al completarse, el stock del recurso de salida sube exactamente
   `output_amount`.
4. `CRAFT` sobre edificio **no completo** ⇒ `ILLEGAL_STATE`.
5. `CRAFT` de receta **no listada** en ese edificio ⇒ `ILLEGAL_STATE`.
6. `CRAFT` de receta de **época superior** ⇒ `ILLEGAL_STATE`.
7. `CRAFT` sobre edificio ya ocupado produciendo ⇒ `ILLEGAL_STATE`.
8. El **orden de rechazos** se conserva: un comando que viola varias reglas
   devuelve el código de la **primera** en el orden de §12.4.
9. Save/load y replay conservan `craft_recipe` y `craft_progress` a mitad de
   producción.
10. Mutar `craft_recipe` **cambia el checksum** (prueba de pertenencia al
    dominio).
11. Destruir el edificio a mitad de producción **no** acredita la salida y
    **no** devuelve los inputs.

---

# §13 Energía y upkeep (Sprint 1.10)

## §13.1 La energía no es stock — recordatorio

Establecido en §8.1 y confirmado por los tres críticos del panel de forma
independiente. **No ocupa índice en `player_stock`.** Se deriva cada tick.

## §13.2 Estado derivado

```cpp
int64_t energy_production[MAX_EMITTERS];   // recalculado cada tick
int64_t energy_consumption[MAX_EMITTERS];  // recalculado cada tick
```

**No se persisten**: se recalculan al inicio de la fase económica desde los
edificios vivos. Persistirlos sería duplicar la fuente de verdad y abrir la
puerta a que save y simulación discrepen.

Sí entran en el **checksum**, como comprobación de que la derivación es
determinista.

## §13.3 Regla de parada en seco

```
disponible = energy_production[p] - energy_consumption[p]
```

Si `disponible < 0`, **todos** los edificios de ese jugador que requieren
energía **se paran**: `craft_progress` no avanza. No se ralentizan, no encolan.

`[V] [Total Annihilation]` «Out of energy? power-dependent structures such as
radar towers, metal extractors and laser towers will cease to function».

**Por qué parada y no ralentización**: la ralentización proporcional exige una
división por tick sobre un valor que cambia, y eso es superficie de error de
determinismo a cambio de un matiz que el jugador no percibe. Parar es binario,
barato y legible.

**Determinismo del reparto**: cuando la energía no alcanza, se paran **todos**
los edificios dependientes, no un subconjunto. Así no hay que elegir cuáles, y
no hay reparto que pueda depender del orden.

## §13.4 Upkeep

Cierra §10.

```cpp
// Derivados, no persistidos, mismo criterio que la energía:
int64_t food_upkeep[MAX_EMITTERS];   // suma de unidades militares vivas
```

- Cada unidad de `unit_class <= 2` consume `upkeep_food` por tick (campo nuevo
  de `UnitDefinitionV1`, por defecto 0 para compatibilidad).
- El consumo se descuenta de `player_stock[p][ÍNDICE_COMIDA]`.

**Si la comida llega a 0 y el upkeep la excede** (§10.3, decisión ya tomada):

- Las unidades **no mueren**.
- `TRAIN_UNIT` se rechaza con `ILLEGAL_STATE` mientras dure el déficit.
- Las unidades militares pierden eficacia: se aplica un **penalizador de moral**
  determinista y acotado.

**El penalizador es de moral, no de daño**: la moral ya existe en el kernel
(Sprint 0.3), ya está en el checksum y ya tiene efectos de combate. Reutilizarla
evita inventar un canal nuevo.

## §13.5 Versionado

`SAVE_FORMAT_VERSION` +1 (por `upkeep_food` en el catálogo y la penalización
activa) · `CHECKSUM_ALGO_VERSION` +1.

## §13.6 Criterios de aceptación

1. Con producción ≥ consumo, un edificio con receta avanza normalmente.
2. Con producción < consumo, `craft_progress` **no avanza** en ese tick.
3. Al restaurar el superávit, la producción **continúa desde donde estaba**, no
   se reinicia.
4. La parada afecta a **todos** los edificios dependientes del jugador, nunca a
   un subconjunto.
5. La energía de un jugador **no afecta** a los edificios de otro.
6. El upkeep descuenta comida cada tick en proporción a las unidades vivas.
7. Al morir una unidad, el upkeep baja en el tick siguiente.
8. Con déficit de comida, `TRAIN_UNIT` ⇒ `ILLEGAL_STATE`.
9. Con déficit de comida, ninguna unidad muere.
10. El penalizador de moral por hambre es determinista: dos corridas idénticas
    dan el mismo checksum.
11. Un escenario **sin** unidades militares tiene upkeep 0 y **checksum
    bit-idéntico** al de antes de este sprint, salvo el bump.

---

# §14 Reserva y recuperación (Sprint 1.11)

## §14.1 Modelo

Sustituye `remaining` (§4.2 ya lo estableció; aquí se detalla).

```cpp
struct EcoDeposit {
    int64_t x_raw, y_raw;
    uint8_t resource_idx;
    int64_t reserve_total;   // geología, del mapa, FIJA
    int64_t extracted;       // crece, nunca decrece
};
```

`extracted` es **del depósito**, compartido entre jugadores (§8.4).
`recovery_pct` es **del jugador**, derivado de `player_caps`.

```
extraible(p, d) = (reserve_total[d] * recovery_pct[p][resource]) / 100 - extracted[d]
```

Agotado **para ese jugador** cuando `extraible <= 0`.

## §14.2 Aritmética determinista

Multiplicación y división **enteras**, en ese orden: primero multiplicar por el
porcentaje, después dividir por 100. Invertirlo perdería precisión de forma
distinta según los valores.

`reserve_total` acotado para que `reserve_total * 100` no desborde `int64_t`.
Es holgadísimo, pero se comprueba en el loader: es exactamente la clase de cota
que el desbordamiento del 1.6B enseñó a no dar por supuesta.

## §14.3 Techos de recuperación

Tecnologías de extracción, como capacidades (`player_caps`), acumulativas:

| Tecnología | Época | `recovery_pct` |
|---|---:|---:|
| (base, sin tecnología) | 1 | 40 |
| Galería | 5 | 60 |
| Voladura | 11 | 75 |
| Flotación | 14 | **90** |

**Tope 90, nunca 100** (§8.5). Siempre queda reserva inaccesible, así que
expandirse sigue siendo necesario.

## §14.4 Criterios de aceptación

1. Con `recovery_pct = 40` y `reserve_total = 1000`, se extraen exactamente 400
   y el depósito queda agotado para ese jugador.
2. Investigar Galería sobre ese depósito agotado lo **reabre** con 200 más.
3. `extracted` **no** se resetea al cambiar el dueño del depósito ni al
   conquistar la zona.
4. Dos jugadores con `recovery_pct` distinto ven **distinta** disponibilidad del
   **mismo** depósito.
5. La extracción de un jugador **reduce** lo disponible para el otro
   (`extracted` es compartido).
6. Con todas las tecnologías, `recovery_pct` es 90 y **queda reserva
   inaccesible**.
7. La aritmética no desborda con `reserve_total` en su cota máxima.
8. Mutar `extracted` cambia el checksum.
9. Un escenario sin tecnologías de extracción reproduce **exactamente** la
   trayectoria de antes del sprint, salvo el bump.

---

# §15 Granjas y fuentes de comida (Sprint 1.12)

**Revisado por directriz del Director (2026-07-28):** las granjas deben poder
construirse en **número muy grande**, limitadas por el **espacio** del mapa y no
por un tope de cantidad. Los depósitos del mapa sí conservan un máximo.

## §15.1 Dos cosas distintas que antes confundí

La versión anterior de esta sección metía las granjas en `deposits[]`, que es el
array de recursos del mapa. Era un error de diseño: obligaba a subir
`ECO_MAX_DEPOSITS` a 128 y aun así dejaba las granjas compitiendo por slots con
las minas, con un techo arbitrario que el jugador notaría como «no puedo
construir más granjas» sin motivo comprensible.

Se separan en dos conceptos:

| | **Depósito de mapa** | **Granja** |
|---|---|---|
| Qué es | Yacimiento geológico o biológico | Edificio del jugador |
| Ejemplos | mina de cobre, bosque, **arbustos de bayas, plataneras, animales de caza** | granja de cereal |
| Origen | Datos del mapa, fijo | Construida en partida |
| Almacenamiento | `deposits[ECO_MAX_DEPOSITS]` | **La propia entidad edificio** |
| Límite | **Máximo duro** (dato del mapa) | **El espacio del mapa** y `ENTITY_HARD_CAP` |
| Se agota | Sí, con reserva/recuperación (§14) | No: **regenera** mientras viva |

**La comida tiene ambas fuentes**, y ésa es la gracia. Las naturales —bayas,
fruta, caza— son depósitos del mapa: abundantes al principio, finitas, y te
empujan a expandirte. La granja es la respuesta construida a ese agotamiento, y
es la que sostiene el upkeep militar de las quince edades (§10.4).

## §15.2 Las granjas no consumen slots de depósito

Una granja **es una entidad edificio** y ya ocupa un slot de entidad. No hace
falta ningún array nuevo: su almacén vive en arrays indexados por entidad, igual
que `build_progress`.

```cpp
int64_t farm_stored[ENTITY_HARD_CAP];   // comida disponible ahora mismo
```

- `BuildingDefinitionV1` gana `farm_capacity` y `farm_regen_per_tick`
  (`farm_capacity == 0` ⇒ no es granja).
- Al completarse la construcción: `farm_stored = 0`.
- Cada tick: `farm_stored = min(farm_stored + farm_regen_per_tick, farm_capacity)`.
- Un ciudadano cosecha de ella y `farm_stored` baja.
- Al destruirse el edificio, su almacén desaparece con él. No hay limpieza
  aparte porque no hay estructura aparte.

**El límite real es el espacio.** Cada granja ocupa su footprint en el mapa y no
puede solaparse con otra construcción — regla que ya existe desde el Sprint 1.1.
Un mapa de 256×256 tiles con granjas de 3×3 admite miles; el techo efectivo lo
pone `ENTITY_HARD_CAP`, compartido con el resto de entidades, que es un límite
que el jugador entiende: «tengo demasiadas cosas», no «el juego dice que no».

## §15.3 Direccionamiento del objetivo económico

`eco_assigned_deposit` era un índice en `deposits[]`. Ahora un ciudadano puede
ir a un depósito **o** a una granja, así que el objetivo pasa a ser explícito:

```cpp
uint8_t  eco_target_kind[ENTITY_HARD_CAP];   // 0=NINGUNO 1=DEPOSITO 2=GRANJA
uint32_t eco_target_index[ENTITY_HARD_CAP];  // índice en deposits[] | índice de entidad
```

**Explícito y no empaquetado en bits a propósito.** Un campo con bandera en el
bit alto ahorraría cuatro bytes por entidad y costaría legibilidad en el sistema
más delicado del kernel. No merece la pena.

`ECO_NO_DEPOSIT` se conserva como centinela de `eco_target_index` cuando
`kind == NINGUNO`, para no romper el código existente.

## §15.4 Selección automática

La búsqueda de §23 (zona aliada) considera **ambos tipos**:

1. Granjas propias con `farm_stored > 0` dentro de la zona aliada.
2. Depósitos del mapa con extraíble > 0 dentro de la zona aliada.

Gana el más cercano; empate por **tipo primero** (depósito antes que granja,
para desempatar de forma estable) y luego por índice más bajo. El orden
completo es determinista y no depende de cómo estén dispuestos los arrays.

**Preferencia por granja propia cuando hay empate de distancia real**: no. Se
evita cualquier regla «inteligente» que el jugador no pueda predecir. El
criterio es distancia, y punto.

## §15.5 Consecuencia sobre `ECO_MAX_DEPOSITS`

**Ya no hace falta subirlo a 128.** Los depósitos son solo del mapa y su número
lo fija el diseñador del mapa. Se sube de 32 a **64** para dar aire a mapas
grandes con muchas fuentes de comida natural, y ahí se queda.

Esto corrige a la baja SPEC-008 §4.1 y reduce el coste de la búsqueda
económica, que era `O(ciudadanos × depósitos)`: con 64 en vez de 128 se
reduce a la mitad. El coste de las granjas se paga aparte y solo entre las
**propias**, que son muchas menos que el total de entidades.

## §15.6 Criterios de aceptación

1. Al completarse una granja, `farm_stored` empieza en 0 y **crece** cada tick.
2. `farm_stored` **nunca** supera `farm_capacity`.
3. Un ciudadano cosecha de una granja y `farm_stored` baja en consecuencia.
4. Un ciudadano cosecha de un arbusto de bayas (depósito de mapa con recurso
   comida) exactamente igual que de una mina.
5. **Se pueden construir muchas más granjas que `ECO_MAX_DEPOSITS`** — al menos
   100 — sin que ninguna falle por falta de slots.
6. Dos granjas **no pueden solaparse**; la segunda se rechaza por footprint,
   como cualquier edificio.
7. Al destruir una granja, los ciudadanos asignados pasan a `IDLE`, **no** a
   `SEEK` perpetuo.
8. Un ciudadano que transporta comida de una granja destruida **conserva la
   carga** y la entrega.
9. La selección automática elige entre granja y depósito por **distancia**, con
   desempate determinista y reproducible.
10. Save/load y replay conservan `farm_stored`, `eco_target_kind` y
    `eco_target_index`.
11. Mutar `farm_stored` **cambia el checksum**.
12. Un escenario sin granjas es **bit-idéntico** al anterior, salvo el bump.
13. Agotar todas las fuentes naturales de comida y sostener el upkeep **solo**
    con granjas es posible: es el caso que justifica la mecánica.

---

# §16 Fuentes construidas: generalización y reforestación (Sprint 1.12)

**Directriz del Director (2026-07-28):** debe poder **plantarse árboles para
reforestar** a partir de cierta edad.

## §16.1 La granja no era un caso especial: era el primero

§15 definió la granja como un edificio con almacén propio que se regenera. La
reforestación pide **exactamente el mismo mecanismo** con otro recurso y otro
ritmo. Así que §15 no se amplía con un caso nuevo: se **generaliza**.

Una **fuente construida** es un edificio que produce un recurso renovable en su
propio almacén. Los campos de §15.2 ganan un índice de recurso:

```
farm_resource_idx     índice del recurso que produce  (nuevo)
farm_capacity         almacén máximo
farm_regen_per_tick   ritmo de reposición
```

| Fuente construida | Recurso | Ritmo | Aparece |
|---|---|---|---|
| Granja | comida | rápido | edad 2 |
| **Plantación forestal** | **madera** | **lento** | **edad 7** |

Nada más cambia: mismo almacén, misma cosecha, misma búsqueda, mismas pruebas.
Cualquier renovable futuro —un vivero, un criadero— es un registro de datos, no
código nuevo.

**Por qué generalizar en vez de añadir un tipo**: dos mecanismos casi idénticos
divergen con el tiempo, y acabas arreglando el mismo bug dos veces. Este ya es
el segundo caso; habrá un tercero.

## §16.2 Por qué la edad 7

Anclaje histórico real. El manejo deliberado del bosque —el monte bajo con
turnos de corta— está documentado en la Europa medieval, y encaja con la edad 7
(Medieval, 800–1300) de §2.

Y hay una simetría que merece la pena hacer explícita, porque es el corazón de
lo que estamos construyendo: la silvicultura moderna **nació de una crisis de
agotamiento minero**. En 1713 von Carlowitz escribe *Sylvicultura oeconomica*
porque las minas de plata sajonas se quedaban sin madera para las galerías, y
al hacerlo acuña el concepto de sostenibilidad.

En CHUNSA ocurre lo mismo por mecánica pura: la madera es finita y alimenta el
carbón vegetal que reduce el hierro (§9.2). Quien deforeste su zona se queda sin
metalurgia. **La reforestación no es una mejora opcional: es la respuesta a un
agotamiento que el propio jugador provoca.**

Es el mismo patrón que §14: allí la tecnología de extracción responde al
agotamiento del yacimiento; aquí la plantación responde al del bosque. Dos
respuestas distintas al mismo problema, ninguna de ellas «expandirse».

## §16.3 Ritmo lento, y deliberadamente

`farm_regen_per_tick` de la plantación es **mucho menor** que el de la granja.
Un árbol tarda décadas en crecer y la mecánica debe reflejarlo: plantar es una
inversión a largo plazo, no un grifo.

Consecuencia de diseño buscada: **reforestar tarde no te salva**. Si dejas que
tu bosque se agote antes de plantar, la plantación no llega a tiempo para esa
partida. Hay que decidir antes de que duela, que es exactamente la decisión
interesante.

Los números concretos son **calibración de playtest**, como el resto (§ cabecera).

## §16.4 Gating por edad

No hace falta mecanismo nuevo: `epoch_window` del `BuildingDefinitionV1` ya
gatea qué se puede construir y cuándo (SPEC-004 §12.4). La plantación forestal
lleva `epoch_window: [7, 15]` y el kernel no se entera de que es especial.

**Comprobación de coherencia**: `farm_resource_idx` debe apuntar a un recurso
que exista en la edad del `epoch_window`. Un edificio de edad 7 que produzca
petróleo (edad 13) es un error de datos y el **loader debe rechazarlo**, no el
kernel en ejecución.

## §16.5 Criterios de aceptación (amplían §15.6)

14. Una plantación forestal completa produce **madera**, no comida.
15. Su `farm_regen_per_tick` es menor que el de la granja: en el mismo número de
    ticks acumula estrictamente menos.
16. `PLACE_BUILDING` de una plantación en **edad 6** ⇒ `ILLEGAL_STATE`; en
    edad 7 ⇒ aceptado.
17. Un ciudadano cosecha madera de una plantación igual que de un bosque del
    mapa.
18. Agotar todos los bosques del mapa y sostener el consumo de madera **solo**
    con plantaciones es posible.
19. Un catálogo con una fuente construida cuyo `farm_resource_idx` no existe en
    su `epoch_window` es **rechazado por el loader** con código de error.
20. Añadir una fuente construida nueva **por datos** —sin tocar código— funciona:
    es la prueba de que la generalización de §16.1 es real.

La prueba 20 es la que justifica todo este apartado. Si falla, no hemos
generalizado nada: hemos escrito el mismo caso especial dos veces.

---

# §17 Selección de objetivo económico: coste y optimización

## §17.1 El problema que crean §15 y §16

Antes, un ciudadano buscaba entre `deposits[]`, acotado a 32. Ahora busca entre
depósitos del mapa **y** fuentes construidas propias, y estas últimas **no
tienen tope duro**: son entidades, y el jugador puede llenar el mapa de granjas
y plantaciones.

Coste ingenuo, por tick:

```
O(ciudadanos × (depósitos + fuentes propias))
```

Con 200 ciudadanos, 64 depósitos y 300 granjas son **72 800 comprobaciones de
distancia por tick**, y crece con lo que el jugador construya. Es el único
bucle del kernel sin cota superior conocida.

## §17.2 La solución ya está inventada en este mismo proyecto

En el Sprint 1.7, la zona aliada se resolvió **precalculando una máscara por
jugador una vez por tick** en lugar de recomputarla por ciudadano. SOL lo hizo
sin que el contrato se lo pidiera y fue la decisión correcta.

**Se generaliza ese patrón**, que ya es doctrina en SPEC-008 §4.2: lo que se
consulta por entidad pero solo cambia por tick, se calcula una vez.

Al inicio de la fase económica, por jugador:

```cpp
// Objetivos elegibles del jugador p, recalculados una vez por tick.
struct EcoTarget { uint8_t kind; uint32_t index; int64_t x_raw, y_raw; };
EcoTarget eco_targets[MAX_EMITTERS][ECO_MAX_TARGETS];
uint32_t  n_eco_targets[MAX_EMITTERS];
```

Se construye recorriendo **una vez** los depósitos en zona aliada con extraíble
> 0 y las fuentes construidas propias con almacén > 0. Cada ciudadano recorre
después esa lista, ya filtrada.

Coste resultante:

```
O(edificios × depósitos)  +  O(fuentes)     [una vez por tick]
+ O(ciudadanos × elegibles)                  [selección]
```

Los elegibles son muchos menos que el total, porque la zona aliada ya descartó
lo lejano y el almacén vacío descartó lo inútil.

## §17.3 Cota dura sobre la lista

`ECO_MAX_TARGETS` acota la lista por jugador. Al llenarse, se **conservan los
más cercanos al centro de la zona aliada** y se descarta el resto.

Este descarte es **determinista y documentado**, no silencioso: mismo criterio
de SPEC-008 §3.3. Un ciudadano no dejará de trabajar por ello — si hay más
objetivos elegibles que el tope, sobran objetivos.

**Por qué una cota y no una lista dinámica**: `Step()` no asigna memoria
(regla del proyecto). Un array fijo por jugador es la única forma compatible.

## §17.4 Qué NO se hace todavía

**No se usa el hash espacial**, aunque existe (`sh_rebuild`). Sería la
optimización siguiente si la medición la exige, pero:

1. SPEC-008 §1 prohíbe optimizar sin medición previa.
2. Consultar el hash espacial introduce un orden de recorrido que hay que
   **fijar explícitamente** para no romper el determinismo, y eso es riesgo a
   cambio de una ganancia todavía no demostrada.

**Se mide en el Sprint 1.12** con el escenario de SPEC-008 §2.1 ampliado a 300
fuentes construidas. Si el presupuesto de 0,3 ms del sistema económico se
supera, entonces —y solo entonces— se aborda el hash espacial con contrato
propio.

## §17.5 Criterios de aceptación

21. La lista de objetivos se recalcula **una vez por tick**, no una por
    ciudadano. Comprobable instrumentando el contador de construcciones.
22. Con la lista llena (`ECO_MAX_TARGETS`), el descarte es **determinista**:
    dos corridas idénticas producen el mismo checksum.
23. Un ciudadano nunca queda ocioso por el descarte mientras exista al menos un
    objetivo elegible en la lista.
24. El escenario de 200 ciudadanos, 64 depósitos y 300 fuentes construidas
    **completa un tick sin fatal** y su coste queda **registrado** (no se exige
    que cumpla presupuesto todavía: se exige medirlo).

---

# §18 Reconciliación con el esquema de datos existente

**Hallazgo (2026-07-29):** al preparar el Sprint 1.8B descubrí que
`data/schemas/` **ya tenía** un modelo de recursos diseñado por delante del
kernel, y que no coincide con SPEC-007. Reconciliarlo **antes** de implementar
evita construir sobre una ambigüedad.

## §18.1 Qué había

| Pieza del esquema | Qué define | Uso real en `data/` |
|---|---|---|
| `resource` (enum) | **8** recursos: `A, B, P, W, Me, F, I, El` | solo `A`, `B`, `Me` |
| `resource_costs` | objeto con esas 8 claves | 3 claves |
| `material_costs` | array abierto de `(material_id, amount)` | **ninguno** |
| `recipes` en `building` | insumos + `output_material_id` + duración | **`recipes: []` en todos** |

O sea: dos sistemas de coste en paralelo —recursos por enum fijo y materiales
por identificador abierto— y un sistema de recetas ya declarado pero vacío.

## §18.2 Las tres decisiones

**1. El enum de ocho letras se elimina.** `Me` agrupa todos los metales, y la
directriz del Director exige **cobre, estaño y hierro por separado** (§9.2).
La abstracción es incompatible con el diseño aprobado.

Los recursos pasan a identificarse por `record_id` con espacio de nombres
(`chunsa:copper`, `chunsa:tin`), como ya hacen unidades, edificios y tecnologías.
El índice numérico del kernel lo asigna el compilador de datos al construir el
blob, **no el autor del YAML**: los datos nombran, el compilador numera.

**2. `material_costs` se fusiona con `resource_costs`.** Dos vocabularios de
coste en paralelo es exactamente la complejidad que el panel señaló. Y no hay
diferencia real: el bronce se almacena y se gasta **igual** que el cobre. Que
uno salga de una mina y otro de un horno es su **origen**, no su naturaleza.

Queda un solo vector de coste sobre `RESOURCE_COUNT`. Coste de migración: cero,
porque `material_costs` no se usa en ningún fichero.

**3. `El` (electricidad) sale del enum de stock.** Contradice frontalmente §8.1:
la energía **no se almacena**, se deriva por tick y provoca parada en seco. Un
`El: 50` en un coste no significa nada.

La dependencia energética de una receta se expresa con un campo propio
—`requires_energy`— no con una cantidad de un recurso inexistente.

## §18.3 Lo que SÍ se conserva

**`recipes` en `BuildingDefinitionV1` se queda tal cual está diseñado.** Su
forma —insumos, salida, `duration_ticks`, en el edificio que las ejecuta— es
casi idéntica a §12.2, y lleva ahí desde el Sprint 0.4. No se reinventa: se
completa.

Cambios mínimos: `output_material_id` → `output_resource_id`, y
`input_material_costs` desaparece al fusionarse en §18.2.

**Esto adelanta trabajo del Sprint 1.9**: la mitad del contrato de recetas ya
existía en el esquema y nadie lo sabía.

## §18.4 Por qué esto era importante encontrarlo

Sin esta reconciliación, el Sprint 1.8B habría añadido un tercer vocabulario de
recursos junto a los dos que ya había. El problema no habría aparecido hasta
el 1.9, cuando las recetas tuvieran que decidir si su salida es un «material» o
un «recurso», con datos ya escritos en ambos.

Es la clase de deuda que sale barata hoy y carísima en tres sprints.

## §18.5 Criterios de aceptación (Sprint 1.8B)

25. `data/schemas/common.schema.json` **no** contiene el enum de 8 letras.
26. Un recurso se declara en `data/resources/` con `record_id` namespaced.
27. El compilador asigna los índices numéricos de forma **determinista y
    reproducible**: dos compilaciones del mismo YAML dan el mismo blob.
28. Un coste referencia un recurso **por id**; un id inexistente es **error de
    carga**, no un índice basura.
29. `material_costs` ya no existe en ningún esquema.
30. `El` ya no aparece como recurso almacenable en ningún sitio.
31. Los tres recursos actuales se renombran a `chunsa:food`, `chunsa:wood` y
    `chunsa:stone` **conservando los índices 0, 1 y 2**, para que las
    trayectorias no cambien.

---

# §19 Correcciones tras la investigación de materiales (2026-07-29)

Panel: MiniMax-M3 con `WebFetch`. Material en
`docs/research/panel/20260729-1000/`. Las tres rutas Gemini devolvieron
respuestas genéricas y corruptas; **el valor de esta ronda vino entero de
MiniMax**.

## §19.1 Errores de fecha en §9.2 — corregidos

Los seis son `[V]` con fuente citada. Mi lista original tenía anacronismos que
un jugador con formación técnica habría notado.

| Recurso | Tenía | Pasa a | Motivo |
|---|---:|---:|---|
| **plomo** | 8 | **3** | Se funde desde el **7º milenio a.C.** Ponerlo en la edad de la pólvora era un anacronismo de 6000 años |
| **coque** | 11 | **9** | Abraham Darby I lo industrializa en **1709** |
| **aluminio** | 14 | **12** | Hall-Héroult es de **1886**: pertenece a la electrificación, no a la era atómica |

## §19.2 Materiales que faltaban — añadidos

| Recurso | Edad | Por qué es indispensable |
|---|---:|---|
| **caliza** (fundente) | 5 | La reducción de mena de hierro **necesita** fundente para arrastrar la ganga como escoria. Sin ella, la receta de hierro forjado es falsa |
| **sal** | 3 | Conservación de alimentos. Un ejército en campaña sin sal es históricamente absurdo, y conecta con el upkeep de §10 |
| **nitrógeno fijado** | 12 | Haber-Bosch, **1913**. Sin él, los fertilizantes y explosivos del s. XX no se sostienen |

`[V]` «limestone (or dolomite), to remove the accompanying rock gangue as slag».
`[V]` La sal es «the best-known food preservative, especially for meat, for many
thousands of years».

**Recuento nuevo**: 20 recolectados + 8 producidos = **28 almacenados**. Sigue
por debajo de `RESOURCE_COUNT = 32`, que era el margen que §9.3 reservó
precisamente para esto.

## §19.3 El coque NO sustituye al carbón vegetal

Corrige §9.5 y §2. `[V]` El coque desplaza al carbón vegetal en el **alto
horno** hacia 1850, pero el carbón vegetal **sigue usándose** en fraguas
pequeñas mucho después. Coexisten.

Encaja con la directriz del Director de que los energéticos se acumulan en vez
de sustituirse: aquí la historia le da la razón.

## §19.4 El criterio de fusión — la aportación más valiosa

Yo pregunté qué se puede fusionar. La respuesta útil no fue la lista sino
**la regla**:

> **Misma cadena aguas arriba + misma decisión aguas abajo = fusionable.**

Dicho de otro modo: si dos materiales salen del mismo yacimiento **y** el
jugador no toma decisiones distintas con ellos, son un solo recurso. Si toma
decisiones distintas, son dos aunque salgan del mismo sitio.

Aplicación:

| Caso | Veredicto | Por qué |
|---|---|---|
| Tierras raras (17 elementos) | **fusionar** | No hay decisión de «¿extraigo neodimio o europio?» |
| Derivados del petróleo (plástico, combustible, lubricante) | **fusionar** | Misma refinería, mismo uso desde la perspectiva del jugador |
| Salitre y azufre | **NO** | Decisiones distintas: la salitre además es fertilizante |
| Madera y carbón vegetal | **NO** | El jugador **elige**: quemar madera para carbón o usarla para construir |
| Cobre y oro | **NO** | Cadenas y rareza distintas; el jugador espera verlos aparte |

**Esta regla queda como criterio permanente** para cualquier duda futura sobre
si algo merece ser un recurso propio. Es preferible a decidir caso por caso.

## §19.5 Umbral de saturación: sin dato duro

`[?]` MiniMax buscó y **no encontró** ningún estudio cuantitativo del máximo de
recursos simultáneos que tolera un jugador de RTS. Lo dijo en vez de
inventarlo, que es la conducta correcta.

Lo que sí hay son referencias: AoE2 funciona con **4**, Rise of Nations con
**6**, y Anno 1800 sostiene **~25 bienes** en fase final con éxito comercial.
El techo real está entre 8 y 25 y **depende de la agrupación, no del conteo
bruto**.

Nuestro pico de 26 activos (§9.4) queda en el límite superior conocido. Eso
**refuerza** que la agrupación por familias del HUD (SPEC-006 Parte III) no es
cosmética: es lo que decide si el diseño es jugable.

Técnica de Rise of Nations digna de copiar: `[V]` la mayoría de unidades cuesta
solo **2 recursos**. Aunque existan 28, que cada decisión concreta implique
pocos es lo que mantiene la carga baja.

## §19.6 Lo que se rechaza de la investigación

**Fusionar plomo con estaño en «metales blandos»**: no. El plomo tiene un uso
tardío propio —baterías, blindaje— y la fusión rompería la regla de §19.4, que
exige *misma decisión aguas abajo*. Se mueve a la edad 3 y se queda solo.

**Añadir forraje/leguminosas** como precursor del nitrógeno: no en v1. Es
correcto históricamente pero añade un recurso para suavizar una transición que
ocurre en la edad 12, y no justifica su coste de carga.

---

# §20 Fuerza motriz por edad — revisión de §13 (2026-07-29)

**Directriz del Director:** antes de la electricidad, la energía debe venir de
otras fuentes. Investigación en `docs/research/RESULTADO_ENERGIA_minimax.md` y
`panel/energia-1015/`.

## §20.1 El anacronismo que se corrige

§13 hacía que toda receta con dependencia energética consumiera **electricidad**.
Un molino del siglo XIII no se enchufa. La fuerza motriz tiene su propia
sucesión y cada etapa mueve cosas distintas.

## §20.2 El hallazgo que cambia la mecánica: la energía es LOCAL

`[I]` «La energía hidráulica o de vapor clásica **no se transportaba a
distancia**; el taller debía estar físicamente junto a la fuente».
`[V]` La transmisión mecánica a distancia llega hacia **1850** (sistema
telodinámico de los hermanos Hirn).
`[V]` El transporte real y generalizado llega con la electricidad: **Pearl
Street 1882** (Edison, DC, ~1,5 km) y **AC con transformadores, Westinghouse
1886**.

Por tanto **el contador global de energía de §13 es incorrecto** salvo en las
últimas edades. Y la transición **no es binaria: es gradual**.

## §20.3 Modelo: una sola magnitud, con RADIO que crece por edad

Se rechaza la alternativa de «tipos de fuerza motriz» como enum. La
observación decisiva de la investigación:

> «(B) *fuerza motriz por tipo* **no es más fiel por ser un enum**: gana si el
> jugador lee "construyo junto al río / junto a la caldera / enchufado a la
> red" como decisión legible. **(B) sin adyacencia es solo cosmético.**»

La fidelidad **no viene de tener tipos**: viene de que la adyacencia **cree
decisiones de emplazamiento**. Así que una sola magnitud con un **radio de
alcance** parametrizado por edad es a la vez más simple y más fiel.

```cpp
// Radio de alcance de la fuerza motriz, por época. 0 = solo el propio edificio.
inline constexpr int64_t ENERGY_RADIUS_RAW_BY_EPOCH[16] = { ... };
```

| Edad | Fuente | Combustible | Alcance | Consecuencia de juego |
|---|---|---|---|---|
| 1–3 | músculo humano y animal | comida | **el propio edificio** | sin mecánica: los edificios funcionan |
| 4–8 | rueda hidráulica · molino de viento | ninguno (corriente, viento) | **adyacente a la fuente** | **hay que construir junto al río** |
| 9–11 | vapor: caldera y eje de transmisión | carbón, coque | **manzana industrial** | **el vapor libera del río**; ahora hay que llevar carbón |
| 12 | vapor de alta presión · electricidad DC | carbón | **radio corto** (~Pearl Street) | primeras redes; el aluminio (1886) exige electricidad |
| 13–15 | electricidad AC con transformadores | hidro, turbina de vapor, petróleo, uranio | **regional / mapa** | la energía deja de condicionar el emplazamiento |

**La máquina de vapor no es "más potencia": es libertad de emplazamiento.** Es
lo que fue históricamente, y es la mejor razón de juego para investigarla.

## §20.4 Implementación

Mismo patrón que la zona aliada (§23 de SPEC-004) y que la selección económica
(§17): **máscara precalculada una vez por tick**, no una consulta por edificio.

Un edificio con dependencia energética produce **solo si** existe una fuente
propia y activa dentro de `ENERGY_RADIUS_RAW_BY_EPOCH[player_epoch]`. Si no,
**se para en seco** — la regla de §13.3 se conserva; lo que cambia es **quién
cuenta como fuente alcanzable**.

Las fuentes de las edades 4–8 exigen además **terreno**: una rueda hidráulica
necesita río. Eso se valida en `PLACE_BUILDING` como el footprint, no en
tiempo de ejecución.

`ENERGY_RADIUS_RAW_BY_EPOCH` es **calibración de playtest**, no dato duro.

## §20.5 Revisión de los producidos

| Producido | Decisión | Motivo |
|---|---|---|
| **cal viva** | **AÑADIR** (e8) | Horno de cal junto al alto horno; alternativa a la caliza cruda |
| **cemento** | **AÑADIR** (e13) | Portland 1824, horno rotatorio 1885; sin vapor no hay cemento moderno |
| acero | **reformular** (e12) | Bessemer **exige vapor**: pasa a depender de energía |
| aluminio | mantener (e12) | Hall-Héroult 1886 cae en la ventana 1840–1900 de §2 |
| bronce, carbón vegetal, hierro forjado, pólvora, coque, derivados | mantener | — |
| **papel** | **NO** | No desbloquea ningún proceso posterior |
| **vidrio** | **NO** | Sin brecha industrial clara |
| **ladrillo** | **NO** | Se abstrae en «construcción» |
| **ácido sulfúrico** | **NO** | Solo tendría sentido con una cadena de fertilizantes que no modelamos |

Los cuatro rechazos comparten criterio: **son materiales reales que no crean
ninguna decisión**. Aplicando la regla de §19.4, un paso más no es un recurso.

**Recuento nuevo: 20 recolectados + 10 producidos = 30 de 32.** Quedan **dos
slots**. Cualquier adición futura debe justificar por qué merece uno de los dos,
o ampliar `RESOURCE_COUNT` con el coste de migración que eso implica.

## §20.6 Criterios de aceptación (amplían §13.6)

12. Un edificio con dependencia energética **sin fuente en radio** no avanza su
    receta.
13. El mismo edificio, con una fuente dentro del radio, **sí** avanza.
14. El radio **crece con la época**: una configuración que no produce en la
    edad 8 sí produce en la 12 sin mover nada.
15. Una rueda hidráulica **no se puede colocar lejos del río**; el rechazo es
    de `PLACE_BUILDING`, no un fallo silencioso en ejecución.
16. Una caldera de vapor **sin carbón** deja de ser fuente válida.
17. La máscara de alcance se calcula **una vez por tick**, no por edificio.
18. Un escenario anterior a la edad 4 es **bit-idéntico**: sin mecánica de
    energía no hay cambio de trayectoria.
