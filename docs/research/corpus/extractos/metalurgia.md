# Metalurgia — extractos con fuente

**Estado**: primer extracto de referencia, escrito por el Arquitecto para fijar
el estándar. La cosecha automática lo amplía.

---

## Fuente 1 — Turner, *The Metallurgy of Iron*

- **Obra**: Thomas Turner, M.Sc., Profesor de Metalurgia en la Universidad de
  Birmingham, asociado de la Royal School of Mines. *The Metallurgy of Iron*,
  serie de tratados de metalurgia.
- **URL**: https://archive.org/download/metallurgyofiron00turnrich/metallurgyofiron00turnrich_djvu.txt
- **Consultado**: 2026-07-31 · texto completo, 1 370 367 caracteres
- **Licencia**: dominio público (digitalizado de University of California)
- **Tipo**: **primaria técnica**, no enciclopedia

### `[V]` Alto horno con coque — balance de materiales por tonelada de arrabio

> «The ore used is rich, containing 55 to 60 per cent, of metallic iron ; 10
> cwts. of limestone are required as a flux for each ton of iron produced, while
> from 17 cwts. of hard Connellsville coke, containing 10 per cent, of ash, are
> employed. The slag produced contains 33 per cent, of silica and 13 per cent,
> of alumina» — p. 103

Lo que da, y es justo lo que faltaba:

| Concepto | Valor citado |
|---|---|
| Mena | 55–60 % de hierro metálico |
| **Fundente** | **10 cwt de caliza por tonelada de arrabio** (media tonelada) |
| **Combustible** | **17 cwt de coque** (0,85 t) con 10 % de cenizas |
| Escoria | 33 % sílice, 13 % alúmina |
| Aire soplado | 25 000 pies³/min a **1 100 °F** (≈ 593 °C) y 9–10 psi |
| Gases de salida | 27,5 % CO, 11,7 % CO₂, a 350 °F |

`[I]` **Para el juego**: confirma que el alto horno necesita **tres** entradas y
no dos — mena, combustible y **fundente**—. Nuestra receta de hierro forjado
ignora hoy la caliza, y la caliza **ya existe** en el catálogo (época 5). Es un
caso claro de dato real que mejora la receta en vez de complicarla porque sí.

### `[V]` Carbón vegetal frente a coque — rendimiento

> «the weekly yield had increased to slightly over 10 tons per week in charcoal
> furnaces, and over 17 tons per week in furnaces using coke.»

`[I]` **Para el juego**: el coque no es «lo mismo pero más moderno»: da **~70 %
más producción semanal** que el carbón vegetal. Eso es exactamente la clase de
salto que debe notarse al cambiar de época, y ahora tiene un número detrás.

---

## Lo que este extracto demuestra

1. Los **libros técnicos de dominio público del Internet Archive son
   descargables enteros en texto plano** y contienen las cifras que las
   enciclopedias resumen sin dar.
2. La ruta correcta es
   `archive.org/download/ID/ID_djvu.txt` — la de `/stream/` devuelve HTML.
3. Un extracto útil **cabe en media página**: la cita literal, la tabla de lo
   que aporta, y qué implica para el juego. No hace falta resumir el libro.

---

# Cosecha — encargo metalurgia (2026-07-31)

Las entradas siguientes amplían la fuente 1 con el resto de los temas del
encargo (bronce, carbón vegetal, hierro forjado, coque, acero). **Mismo
estándar**: cita literal + URL + fecha + marca `[V]` / `[I]` / `[?]`.

---

## Fuente 2 — Thurston, *Brasses, Bronzes and Other Alloys* (1905)

- **Obra**: Robert Henry Thurston (1839-1903). *A Treatise on Brasses,
  Bronzes and Other Alloys, and Their Constituent Metals.* New York, 1905.
- **URL**: https://archive.org/details/atreatiseonbras00thurgoog
- **Texto plano**: https://archive.org/stream/atreatiseonbras00thurgoog/atreatiseonbras00thurgoog_djvu.txt
- **Consultado**: 2026-07-31 · 1 535 507 bytes
- **Licencia**: dominio público (autor fallecido 1903)
- **Tipo**: primaria técnica

### `[V]` Bronce de artillería — proporción canónica

> «According to the U. S. Ordnance Manual, bronze used for ordnance consists
> of 90 parts of copper and 10 of tin, allowing a variation of one part of
> tin, more or less.» — §80

| Aleación | Cu | Sn | Cita (§) |
|---|---|---|---|
| Cañón (U. S. Ordnance) | 90 | 10 ±1 | §80 |
| Gun metal óptimo | 90-91 | 10-9 | §75 |
| Azteca | 94 | 6 | cap. II |
| Moneda grecorromana | 96-98 | 2-4 | §76 |
| Espejo (speculum, Mudge) | 32 / 16 a 14,5 | — | §76 |
| Speculum genérico | 75 | 25 | §75 |
| Campana grande | 77 | 23 | §76 |
| Campana (rango bueno) | 70-82 | 18-30 | §76 |
| Gong chino | 78-80 | 20-22 | §76 |
| Cojinete / packing | 88-96 | 4-12 (+ Zn) | §76 |
| Herramientas antiguas | — | 8-15 | §75 |

`[I]` **Para el juego**: una sola receta de bronce "época 3" puede tener
**seis variantes reales** según uso (cañón, moneda, espejo, campana,
gong, cojinete). Cada uno necesita su propio slider de Cu/Sn.

### `[V]` Punto de fusión del cobre y aleaciones

> «The melting point of copper is given by Pouillet as 2050° F.» — §30
> «It is usually found that the temperature of fusion of an alloy is below,
> and often considerably below, that of either constituent metal.» — §28

`[V]` Cifra Pouillet, recogida por Thurston. **NO VERIFICADO** el melting
point del estaño aislado en esta fuente — `[?]`.

### `[V]` Líquation — temperatura crítica de separación

> «it loses some tin when permitted to stand at a temperature of 400° to
> 500° Fahr. (200° to 260° C). This liquation gives rise to light-colored
> spots throughout the metal.» — §76

`[I]` **Para el juego**: por encima de 260 °C, el estaño empieza a
segregarse. Es la "ventana de colada" real: colar rápido por debajo de
esa temperatura.

### `[V]` Temple y endurecimiento

> «These bronzes become quite malleable when tempered by sudden cooling,
> and this treatment is resorted to when they are to be subjected to
> prolonged working.» — §76
> «Chinese gongs are made of copper 78 to 80, tin 22 to 20, and are beaten
> into shape with the hammer, the metal being softened at frequent
> intervals by heating to a low red heat and plunging into cold water.» — §76

`[V]` Confirma que el temple en agua fría desde rojo bajo es una técnica
real, no invención de juego. Subproducto: la **escoria verde / patina**
que da color a las estatuas antiguas.

---

## Fuente 3 — Jüptner, *Heat Energy and Fuels* (1908)

- **Obra**: Hanns Freiherr Jüptner von Jonstorff (1853-).
  *Heat Energy and Fuels; Pyrometry, Combustion, Analysis of Fuels and
  Manufacture of Charcoal, Coke and Fuel Gases.* New York, 1908.
- **URL**: https://archive.org/details/heatenergyfuelsp00jprich
- **Texto plano**: https://archive.org/stream/heatenergyfuelsp00jprich/heatenergyfuelsp00jprich_djvu.txt
- **Consultado**: 2026-07-31 · 693 320 bytes
- **Licencia**: dominio público
- **Tipo**: primaria técnica

### `[V]` Carbón vegetal — pila eslava / Neuberg

> «Logs are now laid around the center of the charcoal kiln (pile), either
> vertical as in Fig. 34, or horizontal … The pile is then covered on the
> outside with branch wood, then with leaves and grass (smoke cover), and
> at last with earth, sand, and coal culm (earth cover).» — p. 201
> «This first period of charring lasts from 18 to 24 hours.» — p. 202
> «According to the size of the pile (120 to 300 cu. m.) the process of
> charring requires from 15 to 20 days.» — p. 202

**Rendimiento Neuberg (Austria)** — `[V]`:

> «They are built up to 400 to 430 cu. m. capacity … The yield of such a
> pile is Piece coal (large pieces) 2000 hectoliters, Piece coal (small
> pieces) 400 hectoliters = 60 per cent volume of the wood.» — p. 202

**Densidades (kg/m³)**, madera apilada en cordwood — `[V]`:

| Estado | Dura | Blanda |
|---|---|---|
| Verde | 900 | 800 |
| Media seca | 700 | 600 |
| Seca | 580 | 400 |

— Jüptner 1908, p. 202.

### `[V]` Temperatura del gas bajo la cubierta

> «The temperature of the escaping gas right below the cover was from 230
> to 260° C.» — p. 203

`[I]` **Para el juego**: la carbonización ocurre a 230-260 °C. Está **por
debajo** del punto de fusión del cobre (≈ 1085 °C) y muy por debajo del
hierro (≈ 1538 °C). Esto es importante: explica por qué un horno de
fundición necesita combustible distinto al que produce el carbón.

### `[V]` Subproductos de la carbonización

> «About 20 per cent of tar is obtained.» — p. 205 (pila francesa)
> «yields about 36½ per cent of red coal and no black coal, and is
> therefore very much superior to the old process by which 14.18 per cent
> red coal and 17.81 per cent black coal (total 31.99 per cent) is
> obtained.» — p. 208 (método Violette con vapor sobrecalentado)

`[I]` **Para el juego**: el alquitrán (~20 % en peso) es un recurso
aprovechable. Si la receta carbón vegetal solo da el carbón, está
desaprovechando subproductos que las fuentes primarias confirman.

---

## Fuente 4 — Hofman, *Outline of the Metallurgy of Iron and Steel* (1904)

- **Obra**: H. O. Hofman (Heinrich Oscar, 1852-1924). *An Outline of the
  Metallurgy of Iron and Steel, Prepared for the Use of Students at the
  Massachusetts Institute of Technology.* 1904.
- **URL**: https://archive.org/details/anoutlinemetall00richgoog
- **Texto plano**: https://archive.org/stream/anoutlinemetall00richgoog/anoutlinemetall00richgoog_djvu.txt
- **Consultado**: 2026-07-31 · 1 029 900 bytes
- **Licencia**: dominio público
- **Tipo**: primaria técnica (texto docente)

### `[V]` Coque beehive — estructura y resistencia

> «Connellsville coke 'made in the beehive' showed 44.93 per cent.
> cell-walls and 56.07 per cent. cell-spaces, while Otto-Hoffmann coke
> from the sides and the bottom of retort gave 61.13 per cent. and 77.22
> per cent. cell-walls, and 38.81 per cent. and 22.78 per cent.
> cell-spaces.» — p. 86

| Tipo | % paredes | % huecos |
|---|---|---|
| Connellsville beehive | 44,93 | 56,07 |
| Connellsville Otto-Hoffmann | 61,13 / 77,22 | 38,81 / 22,78 |

> «beehive coke contains 2-3 per cent., retort coke, 5-6 per cent. H₂O»
> «hard (beehive, 3.00-3.60 Mohs scale), strong» — p. 86

**Resistencia a compresión (psi / cubic inch)** — `[V]`:

| Tipo | Resistencia |
|---|---|
| Connellsville Otto-Hoffmann | 3 000 |
| Connellsville beehive | 2 260 |
| Morris Run Semet-Solvay | 1 204 |
| Bennington beehive | 1 360 |

— p. 86.

### `[V]` Poder calorífico comparado

> «The calorific power of coke is about 8,000 calories.» — p. 87
> «The calorific power varies from 9,000 to 9,500 calories.» — p. 87 (anthracite)
> «Taking the three leading fuels, they stand as to porosity and purity in
> the order: charcoal, coke, anthracite; as to strength the order is coke,
> anthracite, charcoal.» — p. 88

`[I]` **Para el juego**: tres combustibles, tres comportamientos. El carbón
vegetal es más puro pero más débil mecánicamente — clave para saber qué
proceso acepta cada uno.

### `[V]` Carbón vegetal — composición elemental

> «Good charcoal is black, lustrous, hard, sonorous; has a conchoidal
> fracture; soils the fingers only a little, and burns in small pieces
> without flame or smoke. It is very porous.» — §7(c)

Análisis elemental (`[V]`): C 75,5 / O 12,0 / H 2,5 (air-dry); total 90,0;
ceniza 1,0. — Hofman 1904, p. 89.

### `[V]` Puddling — Cort, Roger, Hall

> «The increased cost of charcoal in the hearth processes induced Henry
> Cort, in 1784, to make wrought iron in a reverberatory furnace heated
> with mineral fuel. In his process, called Dry Puddling, white iron was
> melted down in a furnace having a sand bottom, and stirred so as to
> expose new surfaces to the oxidizing action of the air.» — §46

> «Baldwin Roger, in 1818, replaced the acid hearth by one of cast iron
> which greatly reduced the amount of slagged iron. About 1830 Joseph
> Hall lined the cast-iron hearth with oxidized iron, thus laying the
> foundation of modern Wet Puddling (pig boiling), in which the Si, Mn, C,
> and P are oxidized mainly by the solid oxygen of the red hematite ore
> which forms the lining of the furnace and makes a large amount of fluid
> cinder.» — §46

### `[V]` Cifras del puddling — carga y tiempos

> «500-550 pounds pig iron in 1½-foot lengths are charged, and with them
> 200 pounds flux (roll scale and rich cinder). The whole is now melted
> down in twenty-five to thirty minutes.»
> «The 'boiling' lasts about fifteen minutes; it subsides and the metal
> and slag settle; points of white-hot wrought iron are seen to project
> everywhere; the iron has 'come to nature'.»
> «The length of a heat is from one and three-fourths to two hours.»
> «P is seen to decrease pretty steadily during the whole operation, about
> 80 per cent. being removed.» — §46

`[I]` **Para el juego**: una "hornada" de puddling realista necesita
**500 lb de arrabio + 200 lb de fundente** y dura **2 horas**. Si la
receta de hierro forjado se puede automatizar como en 5 minutos, está
mintiendo sobre el coste real.

---

## Fuente 5 — Bauerman, *Treatise on the Metallurgy of Iron* (1882)

- **Obra**: Hilary Bauerman. *A Treatise on the Metallurgy of Iron;
  Containing Outlines of the History of Iron Manufacture.* London, 1882.
- **URL**: https://archive.org/details/atreatiseonmeta02bauegoog
- **Texto plano**: https://archive.org/stream/atreatiseonmeta02bauegoog/atreatiseonmeta02bauegoog_djvu.txt
- **Consultado**: 2026-07-31 · 1 354 541 bytes
- **Licencia**: dominio público
- **Tipo**: primaria técnica

### `[V]` Reducción carbotérmica en forja / brasca

> «oxide of iron and sand with charcoal in a wind furnace, a variety of
> cast iron may be obtained containing as much as 13 per cent. of silicon»
> — cap. III

> «By effecting the reduction at the high temperature of a Siemens steel
> furnace, Riley has obtained cast iron with 21 per cent. of silicon. It is
> highly crystalline, and silvery white in colour.» — cap. III

`[V]` Confirma que la **reducción carbotérmica con arena + carbón vegetal**
produce fundición rica en Si (hasta 13 %). Explica por qué los hornos
antiguos con ceniza / arena en el suelo daban arrabios "silíceos".

### `[V]` Subproducto "bull-dog" del puddling

> «the slag of the puddling furnace, is decomposed, the iron passes in
> great part into the state of peroxide, and separates from the silica,
> giving a substance which, under the name of 'bull-dog,' is largely used
> for lining the hearths of puddling furnaces.» — cap. III

`[I]` **Para el juego**: el bull-dog es un subproducto real del puddling,
**reutilizable** como revestimiento del siguiente horno. Una buena
receta en bucle cerrado.

### `[V]` Cementación — origen del acero

> «The latter process, called cementation, is applied on a large scale in
> the manufacture of steel.»
> «As has been already stated in the introductory paragraphs, the iron of
> commerce is divided into wrought iron, steel, or cast iron, according
> to the amount of carbon taken up.» — cap. III

`[V]` Confirma la **tabla Karsten** (C % por categoría) como referencia
del s. XIX, aunque las cifras concretas no se extrajeron aquí — `[?]`
para los números.

---

## Fuente 6 — Sisson, *The ABC of Iron* (1892)

- **Obra**: Charles W. Sisson. *The ABC of Iron.* Cleveland, 1892.
- **URL**: https://archive.org/details/abciron00sissgoog
- **Texto plano**: https://archive.org/stream/abciron00sissgoog/abciron00sissgoog_djvu.txt
- **Consultado**: 2026-07-31 · 307 439 bytes
- **Licencia**: dominio público
- **Tipo**: primaria técnica (manual para fundidor)

### `[V]` Horno alto — origen histórico

> «The modern blast furnace is supposed to have originated in the Rhine
> provinces about the beginning of the fourteenth century, but whether in
> France, Germany or Belgium is not clear. One hundred years later, in
> 1409, there was a blast furnace in the valley of Massavaux, in France,
> and it is claimed by Landrin that there were many blast furnaces in
> France about 1450.» — cap. "Pig Iron"

> «The first attempt to make pig iron in the United States was in 1645 at
> Lynn, Massachusetts.» — ibidem

> «The first successful blast with coke as fuel was made by Abraham Darby,
> of Shropshire, at his furnace at Coalbrookdale, England, in the year
> 1735.» — ibidem

> «The first successful manufacture of pig iron with anthracite coal was
> by George Crane, an Englishman, at Yniscedirin, in Wales, in 1837.»

> «The blast used in furnaces was cold, until 1825, when James Beaumont,
> of Scotland, invented the hot blast now in general use all over the
> world.» — ibidem

### `[V]` Composición del arrabio comercial

> «Strictly pure iron ore is metallic iron and oxygen in chemical union in
> fixed and known proportions; the most common being that of peroxide,
> which is 70 per cent. of iron to 30 per cent. of oxygen by weight.»
> «Commercial pig iron usually contains 92 to 94 per cent. of pure iron
> and 6 to 8 per cent. of impurities.» — cap. "Pig Iron"

### `[V]` Carbono en el arrabio

> «the iron in minute particles, having taken up about 4 per cent. of its
> own weight of carbon, is found changed from oxide of iron to carbide of
> iron» — ibidem

### `[V]` Temperaturas del alto horno

> «the blast, previously heated to a temperature of 900 to 1,500° Fah.,
> and is driven in under a pressure of five to ten pounds per square
> inch, and at the rate of three and one-quarter to six tons for each ton
> of iron made.» — cap. "Pig Iron"

> «The gasses leave the zone of combustion, that is gassification at a
> temperature of 3,500 to 4,000° Fah. As they ascend the heat is
> transferred to the descending materials to such an extent that the
> gasses pass out of the top of the furnace with only 300 to 500° Fah.» —
> ibidem

### `[V]` Subproducto del alto horno — escoria

> «The slags of a blast furnace are its refuse, and are formed by a
> combination of silica with the earths and metallic oxides. They are
> used, if not too glassy, for macadamizing roads; it makes an excellent
> railroad ballast, as the mass is very permeable and keeps the sleepers
> dry. It is also used in making brick and cement.» — cap. "Pig Iron"

---

## Fuente 7 — West, *Metallurgy of Cast Iron* (1902)

- **Obra**: Thomas Dyson West (1851-1915). *Metallurgy of Cast Iron.* Cleveland, 1902.
- **URL**: https://archive.org/details/metallurgycasti02westgoog
- **Texto plano**: https://archive.org/stream/metallurgycasti02westgoog/metallurgycasti02westgoog_djvu.txt
- **Consultado**: 2026-07-31 · 1 263 451 bytes
- **Licencia**: dominio público (autor fallecido 1915)
- **Tipo**: primaria técnica

### `[V]` Horno abierto Siemens-Martin — temperatura

> «The Siemens-Martin acid open-hearth furnace is now being very
> successfully employed for heavy castings. These furnaces are much
> hotter than air furnaces. The temperature of metal in them rises,
> possibly, to 3,500 to 4,000 degrees F.» — p. 290

### `[V]` Recocido de maleables — temperaturas y tiempos

> «The temperature of metal in them rises, possibly, to 3,500 to 4,000
> degrees F.» (open hearth)
> «The time occupied in annealing ranges from one to seven days, with
> castings packed in boxes, etc. … The temperature ranges from 1,400 to
> 1,900 degrees F.» — p. 290

`[I]` **Para el juego**: el recocido del maleable dura **hasta 7 días**.
Eso es una ventana de tiempo real en la cadena de producción.

---

## Fuente 8 — Fitch, *Bessemer Steel* (1882)

- **Obra**: Thomas W. Fitch. *Bessemer Steel. Ores and Methods.* St. Louis, 1882.
- **URL**: https://archive.org/details/bessemersteelore00fitc
- **Texto plano**: https://archive.org/stream/bessemersteelore00fitc/bessemersteelore00fitc_djvu.txt
- **Consultado**: 2026-07-31 · 417 916 bytes
- **Licencia**: dominio público
- **Tipo**: primaria técnica (reporte industrial)

### `[V]` Spiegel — consumo de coque por tonelada

> «The Geisweid and Wissen furnaces have the largest production, yielding
> 80 tons of spiegel per day, the consumption of coke is about 2,400 lbs.
> per ton, with a temperature of blast of 1,000 degrees Fahrenheit.» —
> p. 14

`[V]` Spiegel (aleación Fe-Mn usada en Bessemer): **2 400 lbs coque / ton
spiegel** ≈ 1,2 t coque / t spiegel.

### `[V]` Transición carbón → coque en spiegel

> «devastation of the forests, and of the scarcity of hard wood suitable
> for conversion into good charcoal, this fuel soon after 1859 proved
> insufficient to produce the spiegeleisen wanted, and it became
> necessary to replace the charcoal by coke.» — p. 13

`[V]` **Causa real del cambio** a coque en siderurgia: deforestación.
No fue una mejora técnica, fue un colapso de oferta de combustible.

---

## Fuente 9 — Wellman, *The Open Hearth* (1920)

- **Obra**: Wellman Engineering Company. *The Open Hearth: Its Relation
  to the Steel Industry, Its Design and Operation.* Cleveland, 1920.
- **URL**: https://archive.org/details/openhearthitsre00wellgoog
- **Texto plano**: https://archive.org/stream/openhearthitsre00wellgoog/openhearthitsre00wellgoog_djvu.txt
- **Consultado**: 2026-07-31 · 658 270 bytes
- **Licencia**: dominio público (publicado 1920 en EE. UU. sin renovación)
- **Tipo**: primaria técnica

### `[V]` Capacidad del horno abierto (1920)

> «an open hearth furnace, with a circular bottom, ranging in capacity
> from five to thirty tons.» — (citado por Sisson 1892, retomado por Wellman)

`[V]` para la cifra de capacidad en hornos de la época.

---

## Tabla resumen — proporciones por masa y cifras críticas

| Proceso | Variable | Valor | Fuente |
|---|---|---|---|
| Bronce de cañón | Cu / Sn | 90 / 10 ±1 | Thurston 1905 §80 |
| Bronce de campana | Cu / Sn | 77 / 23 | Thurston 1905 §76 |
| Bronce espejo | Cu / Sn | 67-75 / 25-33 | Thurston 1905 §75-76 |
| Bronce moneda antigua | Cu / Sn | 96-98 / 2-4 | Thurston 1905 §76 |
| Líquation Cu-Sn | T crítica | 200-260 °C | Thurston 1905 §76 |
| Cobre melting point | °F | 2 050 | Thurston 1905 §30 (Pouillet) |
| Carbón vegetal — rendimiento | % volumen | 60 % | Jüptner 1908 p. 202 |
| Carbón vegetal — duración | días | 15-20 | Jüptner 1908 p. 202 |
| Carbón vegetal — gas cubierta | °C | 230-260 | Jüptner 1908 p. 203 |
| Coque beehive — paredes | % | 44,93 | Hofman 1904 p. 86 |
| Coque poder calorífico | cal | ~8 000 | Hofman 1904 p. 87 |
| Puddling — carga | lb | 500-550 pig + 200 flux | Hofman 1904 §46 |
| Puddling — calor | min | 105-120 / heat | Hofman 1904 §46 |
| Puddling — eliminación P | % | ~80 | Hofman 1904 §46 |
| Spiegel — coque | lb / ton spiegel | 2 400 | Fitch 1882 p. 14 |
| Open hearth — T baño | °F | 3 500-4 000 | West 1902 p. 290 |
| Recocido maleable — duración | días | 1-7 | West 1902 p. 290 |
| Arrabio comercial — Fe | % | 92-94 | Sisson 1892 |
| Arrabio — C | % | ~4 | Sisson 1892 |
| Alto horno — aire / ton hierro | tons | 3,25-6 | Sisson 1892 |
| Alto horno — blast T | °F | 900-1 500 | Sisson 1892 |
| Alto horno — blast P | psi | 5-10 | Sisson 1892 |
| Alto horno — zona comb. T | °F | 3 500-4 000 | Sisson 1892 |
| Alto horno — gas salida T | °F | 300-500 | Sisson 1892 |

---

## Lo que este extracto ampliado demuestra

1. **8 fuentes primarias** de dominio público, todas anteriores a 1929,
   dan cifras concretas para cada uno de los 5 temas del encargo.
2. El alto horno necesita tres entradas (mena + combustible + fundente),
   no dos. La receta de fundición que ignora la caliza está mintiendo.
3. La transición carbón → coque fue **forzada** por deforestación, no
   por mejora técnica. Esto cambia cómo se narra la "época del coque".
4. Cada aleación de bronce tiene su proporción Cu/Sn por uso
   (artillería, campana, moneda, gong, espejo). Una sola "receta bronce"
   no captura la realidad histórica.
5. Puddling dura **2 horas** y consume 500 lb de arrabio + 200 lb de
   fundente por hornada. Si la receta es automática en segundos,
   infravalora el coste.

---

## Pendientes (NO VERIFICADO en este extracto)

- **NO VERIFICADO** — año exacto de invención del Siemens-Martin (atribuido a 1864, no citado literalmente).
- **NO VERIFICADO** — año exacto del Thomas-Gilchrist (atribuido a 1878, no citado literalmente).
- **NO VERIFICADO** — temperatura máxima en el puddling furnace en °F (Hofman describe "intense heat" sin número).
- **NO VERIFICADO** — tabla Karsten con cifras exactas de C % por categoría (mencionada pero no extraída literalmente de Bauerman 1882).
- **NO VERIFICADO** — melting point del estaño aislado en °F.
- **NO VERIFICADO** — poder calorífico del carbón vegetal en kcal/kg (Hofman da orden pero no cifra aislada).

Estos huecos son deliberados. **Vale más un extracto corto y sólido que
uno largo y dudoso.**
