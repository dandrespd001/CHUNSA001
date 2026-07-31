# Química — extractos con fuente

**Estado**: encargo ejecutado 2026-07-31. Cubre los seis asuntos del
encargo —**pólvora** (salitre + azufre + carbón), **cal viva** y
calcinación, **cemento Portland**, **destilación fraccionada del
petróleo**, **proceso Haber-Bosch** y **ácido sulfúrico**— con fuentes
primarias de dominio público anteriores a 1929, cita literal + URL +
fecha + marca `[V]` / `[I]` / `[?]`.

**Épocas que sirve**: 8–15 (INDICE.md). Pólvora y calcinación pueden
bajar a épocas anteriores si el juego las modela con tecnología
pre-industrial (la pólvora es s. XIII en Europa; la calcinación es
universal), pero las cifras recogidas aquí describen el proceso
industrial moderno de referencia.

---

## Fuente 1 — Guttmann, *Manufacture of Explosives* (1895)

- **Obra**: Oscar Guttmann (1855-1910). *The Manufacture of Explosives:
  A Theoretical and Practical Treatise on the History, the Physical and
  Chemical Properties, and the Manufacture of Explosives.* London, 1895.
  2 vols.
- **URL** (vol. 1): https://archive.org/details/manufactureexpl00guttgoog
- **Texto plano**: https://archive.org/download/manufactureexpl00guttgoog/manufactureexpl00guttgoog_djvu.txt
- **Consultado**: 2026-07-31 · 684 847 bytes
- **Licencia**: dominio público (autor fallecido 1910)
- **Tipo**: **primaria técnica** (manual industrial)

### `[V]` Composición de la pólvora negra — tabla histórica de proporciones

> «In 1774 the mixtures used in Prussia were: — LARGE GRAINED POWDER …
> Saltpetre … 74·4 … Sulphur … 12·3 … Charcoal … 13·3 — FINE GRAINED
> POWDER … Saltpetre … 80 … Sulphur … 10 … Charcoal … 10»

> «At the beginning of this century the following proportions obtained
> in Prussia: — 75 parts of saltpetre, 10 parts of sulphur, and 15 parts
> of charcoal; but they were soon replaced by the following: — 75 parts
> of saltpetre, 11·5 parts of sulphur, and 13·5 parts of charcoal.»

> «Boillot, in France, in his *Modelles d'artifices de feu*, and also de
> Bry in 1619, recommended as the best mixture: — 75 parts of saltpetre,
> 12·5 parts of sulphur, and 12·5 parts of charcoal.»

> «In 1800 the Swiss composition of 76 parts of saltpetre, 10 parts of
> sulphur, and 14 parts of charcoal was adopted; but in 1808 the old
> composition was again resorted to, and this is still used now-a-days.»

— Guttmann 1895, vol. 1, p. 177-178.

**Tabla maestra de composiciones históricas (partes en masa)** `[V]`:

| Año / lugar | KNO₃ | S | C | Fuente |
|---|---|---|---|---|
| 1546 — Alemania (cañones) | 50,0 | 33,3 | 16,7 | Guttmann p. 177 |
| 1546 — Alemania (mosquetes) | 83,4 | 8,3 | 8,3 | Guttmann p. 177 |
| 1555 — Fronsperger | 66,5 | 22,5 | 11,0 | Guttmann p. 177 |
| 1774 — Prusia (grano grueso) | 74,4 | 12,3 | 13,3 | Guttmann p. 178 |
| 1774 — Prusia (grano fino) | 80,0 | 10,0 | 10,0 | Guttmann p. 178 |
| 1619 — Boillot, Francia | 75,0 | 12,5 | 12,5 | Guttmann p. 178 |
| 1686 — Francia | 76,0 | 12,0 | 12,0 | Guttmann p. 178 |
| 1794 — Francia | 76,0 | 9,0 | 15,0 | Guttmann p. 178 |
| 1800 — Suiza | 76,0 | 10,0 | 14,0 | Guttmann p. 178 |
| 1808 — Prusia (uso mantenido) | 75,0 | 10,0 | 15,0 | Guttmann p. 178 |
| ~1800 — Prusia (reemplazo) | 75,0 | 11,5 | 13,5 | Guttmann p. 178 |
| 1720 — Suecia | 73,0 | 10,0 | 17,0 | Guttmann p. 178 |
| 1770 — Suecia | 75,0 | 16,0 | 9,0 | Guttmann p. 178 |
| 1823 — Suecia | 75,0 | 15,0 | 10,0 | Guttmann p. 178 |

### `[V]` Composición teórica estequiométrica (Berthelot)

> «Berthelot has calculated the proportions the three ingredients should
> have in the mixture in order to get the theoretical maximum of heat
> and the minimum of gaseous products. These he found to be 84 parts of
> saltpetre, 8 parts of sulphur, and 8 parts of charcoal. Assuming that
> the charcoal consists of pure carbon, then the stoechiometric
> proportion for the formula, 2KNO₃+S+3C, would be 74·84 parts of
> saltpetre, 11·84 parts of sulphur, and 13·32 parts of charcoal.»

— Guttmann 1895, vol. 1, p. 177.

`[I]` **Para el juego**: la fórmula estequiométrica da **74,84 / 11,84 /
13,32**, muy cercana a las recetas históricas de Prusia (75/10/15) y
Suiza (76/10/14). La "receta única" del juego debería permitir ajustar
estos tres sliders en torno a esos valores. El **carbón nunca es
carbono puro**, así que la receta histórica varía entre 9 % y 17 % de C
según la madera.

### `[V]` Contenido de humedad durante fabricación

> «The grains, which still contain about 8 per cent. of moisture, are
> then dusted … and after dusting contain about 1·25 per cent. of
> moisture.» — Guttmann 1895, vol. 1, p. 217 (sobre el proceso de
> granulado)

> «In France, the proportion of moisture is kept at about 6 [per cent.].
> The powder loses 1¼ per cent. of moisture» — Guttmann 1895, vol. 1,
> p. 422

`[I]` **Para el juego**: la humedad final del polvo (~1-6 %) es la que
mantiene la mezcla manipulable durante la incorporación. Una receta
que ignore la humedad ignora el riesgo de explosión por polvo seco
durante la mezcla.

### `[V]` Subproductos y naturaleza del proceso

> «black powder is not a chemical product but a mechanical mixture, on
> the intimate and careful incorporation of which its complete combustion
> depends.» — Guttmann 1895, vol. 1, p. 177

`[I]` **Para el juego**: la pólvora **no es un compuesto químico**, es
una mezcla mecánica. Esto explica por qué la trituración es la etapa
crítica y por qué la humedad importa: lo que se vende no es un
compuesto nuevo, es una **dispersión íntima** de tres sólidos.

---

## Fuente 2 — Mead, *Portland Cement* (1911)

- **Obra**: Richard K. Mead (1874-). *Portland Cement: Its Composition,
  Raw Materials, Manufacture, Testing and Analysis.* Easton PA, 1911.
- **URL**: https://archive.org/details/portlandcementi00meadgoog
- **Texto plano**: https://archive.org/download/portlandcementi00meadgoog/portlandcementi00meadgoog_djvu.txt
- **Consultado**: 2026-07-31 · 1 091 517 bytes
- **Licencia**: dominio público (publicado 1911 en EE. UU.)
- **Tipo**: **primaria técnica** (texto docente universitario)

### `[V]` Temperatura de clinkerización — efecto del contenido de cal

> «Campbell made numerous experiments in burning mixtures of marl and
> clay in varying proportions in a small rotary kiln fitted with a Le
> Chatelier pyrometer … he fixed the temperature necessary to properly
> burn most commercial cements at **1550° C. or 2822° F.**, while high
> limed cements would require an even greater temperature.»

> «a mixture of clay and marl in which the ratio of the silicates
> (SiO₂ + Al₂O₃ + Fe₂O₃) was to the lime (CaO) as 100 : 228.8, and
> which gave a clinker containing 62·64 per cent. lime, to require a
> temperature of **1549° C.** for proper burning, while a mixture of the
> same clay and marl in which the silicate-lime ratio was 100 : 240.8
> and which gave a clinker analyzing 63·83 required a temperature of
> **1593° C.** A third mixture of these materials having a silicate-lime
> ratio of 100 : 266.4 and giving a clinker analyzing 66·12 per cent.
> lime failed to burn perfectly even at **1625° C.**»

— Mead 1911, p. 184-185.

### `[V]` Temperatura — efecto del tiempo y la fineza de molido

> «the same mixture … which required a temperature of 1549° C. for
> proper burning could be burned at a temperature of **1478° C.** by
> revolving the kiln more slowly.»

> «A mixture … could not be thoroughly burned even at 1612° C, but
> when reground so that 98 per cent. of it passed a 200-mesh sieve
> proper burning was accomplished at a temperature of **1475° C. or
> 137° C. less.**»

— Mead 1911, p. 185.

### `[V]` Temperatura de clinkerización en horno largo

> «The temperatures in the clinkering zone of a 125 ft. kiln usually
> range between **1,350 and 1450° C.** … The temperatures of most 125
> ft. kilns observed by the author are fully 100° C. lower than those
> employed in the 60 ft. kilns.»

— Mead 1911, p. 186.

`[I]` **Para el juego**: la clinkerización realista necesita **1475-1625
°C** según composición y fineza. Esto está **muy por encima** de lo que
da un horno de fundición de hierro (~1500 °C en la zona de
combustión), y muy por encima de lo que aguanta el refractario
ordinario. La "receta de cemento Portland" que se fabrique en un horno
común está mintiendo sobre la infraestructura requerida.

### `[V]` Cenizas alcalinas como fundente

> «in a small furnace which the writer had he could never quite get the
> temperature up to the point for a thorough burning of the Lehigh
> cement-rock limestone mixtures, but if the small cubes of powdered
> material were made up with water containing enough sodium carbonate
> to make the mixture analyze about **1.5 per cent. soda**, the
> clinkering could easily be accomplished.»

> «Iron always plays an important part in aiding the clinkering. The
> white Portland cements at present on the market are all hard to burn.
> Fluorspar or calcium fluoride, CaF₂, has also the effect of lowering
> the clinkering temperature»

— Mead 1911, p. 187.

`[I]` **Para el juego**: dos **fundentes reales** que bajan la
temperatura de clinkerización: álcalis (~1,5 % Na₂CO₃) y fluorita
(CaF₂). El Fe₂O₃ también ayuda. Una receta que ignora esto pide al
horno temperaturas imposibles.

---

## Fuente 3 — Butler, *Portland Cement* (1899)

- **Obra**: David Butler. *Portland Cement: Its Manufacture, Testing
  and Use.* London, 1899.
- **URL**: https://archive.org/details/portlandcementi00butl
- **Texto plano**: https://archive.org/download/portlandcementi00butl/portlandcementi00butl_djvu.txt
- **Consultado**: 2026-07-31 · 695 277 bytes
- **Licencia**: dominio público (publicado 1899)
- **Tipo**: **primaria técnica** (manual industrial)

### `[V]` Definición técnica del cemento Portland

> «an artificial product consisting chiefly of silicates and aluminates
> of lime, produced by the calcination to incipient vitrifaction of a
> mechanical mixture of chalk and clay, or similar materials containing
> silica, alumina, oxide of iron, and lime, which, generally in
> proportions of Lime … 60 to 64 per cent.»

— Butler 1899, p. 6.

`[V]` El rango histórico de cal en el clinker Portland es **60-64 %**.

### `[V]` Materias primas — proporción chalk:clay

> «three barrows of chalk are required to one barrow of clay» — Butler
> 1899, p. 30 (proporción en volumen en el wash-mill)

> «the most careful chemical control from the very commencement of the
> process, more especially in the first stage, when the raw materials
> are being mixed in the wash mill» — Butler 1899, p. 26

`[I]` **Para el juego**: la mezcla cruda se ajusta por **volumen**
(3:1 chalk:clay) y por **análisis químico**. La "tasa de extracción"
del juego debe distinguir entre el rendimiento en volumen (más
generoso) y el rendimiento en masa de clinker (60-64 % de CaO final).

---

## Fuente 4 — Reid, *Science and Art of Portland Cement* (1877)

- **Obra**: Henry Reid (ingeniero civil). *The Science and Art of the
  Manufacture of Portland Cement, with Observations on Some of Its
  Constructive Applications.* London, 1877.
- **URL**: https://archive.org/details/scienceartofmanu00reiduoft
- **Texto plano**: https://archive.org/download/scienceartofmanu00reiduoft/scienceartofmanu00reiduoft_djvu.txt
- **Consultado**: 2026-07-31 · 910 285 bytes
- **Licencia**: dominio público (publicado 1877)
- **Tipo**: **primaria técnica** (manual fundacional)

### `[V]` Origen histórico del cemento Portland

> «the discovery that the hard, white, pure limestones, hitherto
> considered best for lime making, were in reality inferior to the
> softer and less crystalline varieties … trying a limestone Smeaton
> found near at hand, at Aberthaw, in Cornwall, and the hydraulic lime
> formed by burning this stone was the cement that led the way to the
> Portland cement of the present day» — Reid 1877, p. 5

`[V]` John Smeaton (ingeniero del faro de Eddystone, 1756-1759)
descubrió que las calizas arcillosas duras dan mejor cal hidráulica
que las calizas blancas puras. Es el origen del cemento Portland.

---

## Fuente 5 — Hamor, *The American Petroleum Industry* (1916)

- **Obra**: William Allen Hamor (1887-). *The American Petroleum
  Industry.* New York, 1916. 2 vols.
- **URL** (vol. 1): https://archive.org/details/americanpetrole01hamogoog
- **Texto plano**: https://archive.org/download/americanpetrole01hamogoog/americanpetrole01hamogoog_djvu.txt
- **Consultado**: 2026-07-31 · 1 233 464 bytes
- **Licencia**: dominio público (publicado 1916)
- **Tipo**: **primaria técnica** (manual industrial)

### `[V]` Fracciones de la destilación seca de petróleo (Mid-Continent)

> «The Average Yield of Products by Dry-Distillation (Mid-Continent
> Petroleum) … Fire-stilling … Crude naphtha (200°F. b.p.) 6 to 8 per
> cent. … Crude heavy naphtha (300°F. b.p.) 13 to 15 per cent. …
> Natural lamp distillate (450°F. b.p.) 16 to 18 per cent. … Gas and
> fuel oil stock 20 per cent. … Tar 42 per cent. … Tar-still
> distillation … Paraffin distillate 22 per cent. … Cracked distillate
> 15 per cent. … Wax tailings 1 per cent. … Coke 4 per cent.»

— Hamor 1916, vol. 1, p. 466 (tabla).

| Fracción | Punto de ebullición | Rend. (% volumen crudo) |
|---|---|---|
| Nafta cruda (gasolina ligera) | ~200 °F (~93 °C) | 6-8 |
| Nafta pesada | ~300 °F (~149 °C) | 13-15 |
| Destilado de lámpara (keroseno) | ~450 °F (~232 °C) | 16-18 |
| Gas y fuel oil stock | — | 20 |
| Alquitrán (a tar stills) | — | 42 |
| **Destilado parafínico** | — | 22 (sobre el tar) |
| **Crackeado** | — | 15 (sobre el tar) |
| **Cera residual** | — | 1 |
| **Coque** | — | 4 |

### `[V]` Destilación fraccional con vapor de agua

> «In the "fractional distillation," or distillation with bottom steam,
> which is employed when it is desired to prevent the decomposition of
> the petroleum as much as possible, particularly when lubricating oils,
> such as cylinder oils, are to be manufactured from paraffin-base
> petroleum, free "dry" steam is introduced into the body of the oil in
> the still by a coil or branched pipe so perforated that the steam is
> directed upon the bottom of the still and that it is evenly
> distributed throughout the oil … Steam distillation is especially
> advantageous, however, because in this process the hydrocarbons come
> over at temperatures below their normal boiling points»

— Hamor 1916, vol. 1, p. 457-458.

`[V]` En la destilación al vapor, los hidrocarburos salen a
**temperaturas por debajo de su punto de ebullición normal** (la presión
parcial sobre los HC es menor que la atmosférica porque el vapor de
agua se la reparte).

### `[V]` Diferencia de temperatura con/sin vapor

> «Under these conditions, the crude naphtha is distilled off … the
> temperature in the steam still is approximately **280° F.**, while,
> without steam, the temperature in the still would be about 437° F.
> The yield from light-colored non-asphaltic crudes is about 13 per
> cent.»

> «The heating is continued, and more and more steam is introduced,
> until the heavy crude naphtha has distilled off; at this point, the
> temperature in the still is about **360° F.**, whereas without steam it
> would be about 475° F.»

— Hamor 1916, vol. 1, p. 469.

| Fracción | T con vapor | T sin vapor |
|---|---|---|
| Nafta cruda sale | ~280 °F (~138 °C) | ~437 °F (~225 °C) |
| Nafta pesada sale | ~360 °F (~182 °C) | ~475 °F (~246 °C) |

`[I]` **Para el juego**: el vapor de agua reduce la temperatura de
salida entre **100 y 150 °C**. Es una decisión de diseño real:
**¿se quiere proteger el producto (lubrificantes) o se quiere crackear
(gasolina)?** Son dos cadenas de producción distintas.

### `[V]` Naphtha steam-still — torre de fraccionamiento

> «An improvement in naphtha steam stills is to install over the still
> a high tower through which the naphtha vapor from the still ascends
> and meets the cold, crude naphtha which flows down from the top of
> this tower. The heat exchange thus effected has proved a great
> economy in naphtha distillation. By this distillation, the crude
> naphtha is fractioned into a variety of products ranging from
> gasoline with gravities between **80° and 90° Bé.** to ordinary stove
> gasoline with gravities between **70° and 80° Bé.**»

— Hamor 1916, vol. 1, p. 458-459.

`[V]` Las naftas se fraccionan por gravedad Baumé (°Bé), no por
temperatura directa. Gasolina ligera 80-90 °Bé; gasolina estándar
70-80 °Bé.

### `[V]` Cracking — descripción del proceso

> «the "dry" or destructive ("cracking") distillation process is used
> when a large yield of gasoline and illuminating oil is desired. … In
> this process, the crude oil is fire-stilled, and the heavy vapors
> which condense in the top of the still fall back into the superheated
> oil and are thereby "cracked," or partially decomposed.»

— Hamor 1916, vol. 1, p. 455-456.

`[V]` **Cracking** = los vapores pesados condensan en el domo y
caen de nuevo al aceite supercalentado, donde se descomponen
térmicamente en fracciones más ligeras.

---

## Fuente 6 — Ernst, *A Direct Synthetic Ammonia Plant* (1925)

- **Obra**: F. A. Ernst, F. C. Reed, W. L. Edwards (Fixed Nitrogen
  Research Laboratory, Washington). «A Direct Synthetic Ammonia
  Plant.» *Industrial and Engineering Chemistry*, vol. 17, n.º 8,
  agosto 1925.
- **URL**: https://archive.org/details/haber-bosch-ammonia-synthesis
- **Texto plano**: https://archive.org/download/haber-bosch-ammonia-synthesis/ernst1925_repaired_djvu.txt
- **Consultado**: 2026-07-31 · 49 458 bytes
- **Licencia**: dominio público (publicado 1925 en EE. UU.)
- **Tipo**: **primaria técnica** (artículo de journal)

### `[V]` Condiciones del proceso — presión y temperatura

> «With the catalyst developed at the Fixed Nitrogen Research
> Laboratory operating in this system at **300 atmospheres and 475° C.**,
> of 100 volumes of gas leaving the converter, 20 will be considered as
> ammonia, of which 15 will be removed, leaving 80 volumes of (N₂ +
> 3H₂) mixture plus 5 volumes of ammonia to be recirculated.»

— Ernst, Reed, Edwards 1925, p. 776-777.

| Variable | Valor | Cita |
|---|---|---|
| Presión total | **300 atm** | Ernst 1925 p. 776 |
| Temperatura del catalizador | **475 °C** | Ernst 1925 p. 776 |
| Conversión por paso | **20 %** del gas → NH₃ | Ernst 1925 p. 776 |
| Recuperación por condensación | 15/20 = **75 %** | Ernst 1925 p. 776 |
| Recirculación | 80 vol (N₂+3H₂) + 5 vol NH₃ | Ernst 1925 p. 777 |
| Capacidad de la planta | **3 tons/día** | Ernst 1925 p. 775 |

### `[V]` Estequiometría — relación N₂ : H₂

> «Air is introduced into the burner through a positive pressure blower
> in such amount that the resulting gas after the oxygen has been
> burned out is a **3:1 mixture of hydrogen and nitrogen**»

— Ernst 1925, p. 775.

`[V]` La mezcla cruda es **3 H₂ : 1 N₂** (en volumen), que es la
proporción estequiométrica de la reacción
N₂ + 3 H₂ ⇌ 2 NH₃.

### `[V]` Demanda de gas por tonelada de amoníaco

> «At 20° C. and 1 atmosphere the density of a (N₂ + 3H₂) mixture is
> 0·02223 pound per cubic foot. … there will be required 96,000 cubic
> feet of air to produce the required nitrogen. From this amount of air
> there will be 96,000 × 0·2092 = 20,000 cubic feet of oxygen to be
> disposed of, requiring 40,000 cubic feet of hydrogen. The total
> hydrogen requirements, therefore, will be **265,000 cubic feet
> measured at 20° C. and 1 atmosphere**» (por 3 tons/día = ~88 000
> pies³ H₂ / ton NH₃).

— Ernst 1925, p. 776.

`[V]` Por **3 tons/día de NH₃** se necesitan:
- 75 000 ft³ de N₂
- 225 000 ft³ de H₂ (de electrólisis)
- 40 000 ft³ extra de H₂ para quemar el O₂ del aire
- **265 000 ft³ de H₂ en total**

### `[V]` Consumo eléctrico para producir el H₂ por electrólisis

> «Electrolytic cells of 5000 amperes normal operating capacity can now
> be secured in this country, delivering a 99·5 per cent. or better,
> purity of gas when operating at 60° C. at a power consumption of
> **140 kilowatt hours per 1000 cubic feet of hydrogen.** At 2·24 volts
> per cell and 5000 amperes the power consumption per cell per 24 hours
> is 268·8 kilowatt hours.»

— Ernst 1925, p. 776.

`[I]` **Para el juego**: para producir 265 000 ft³/día de H₂ (suficiente
para 3 tons/día de NH₃) hacen falta **188 celdas electrolíticas × 268,8
kWh = 50 500 kWh/día** sólo para el hidrógeno. Sumando el compresor
(300 atm), la cifra de potencia es enorme. **El proceso Haber-Bosch
sólo es viable con electricidad barata** — algo que el juego debe
capturar.

### `[V]` Quemado de O₂ con H₂ para obtener N₂ puro

> «Hydrogen and air are admitted separately … and meet at a point about
> midway of the length of the body of the burner. Here, by means of a
> spark jumping from an extended lead of a spark plug, the
> hydrogen-oxygen mixture is exploded. … Each volume of oxygen combines
> with two volumes of hydrogen and the resultant water is removed in a
> condenser following the burner.»

— Ernst 1925, p. 777.

`[V]` Para obtener N₂ "gratis", se quema el O₂ del aire con H₂ (2:1).
Esto encarece el consumo de H₂ en un 17 % sobre el estequiométrico
(40 000 ft³ extra sobre los 225 000 ft³ estequiométricos).

---

## Fuente 7 — Lunge, *Sulphuric Acid and Alkali* (1913)

- **Obra**: Georg Lunge (1839-1923). *The Manufacture of Sulphuric
  Acid and Alkali, with the Collateral Branches. A Theoretical and
  Practical Treatise.* Vol. 1, partes 1-2. London, 1913.
- **URL**: https://archive.org/details/manufacturesulp00lunggoog
- **Texto plano**: https://archive.org/download/manufacturesulp00lunggoog/manufacturesulp00lunggoog_djvu.txt
- **Consultado**: 2026-07-31 · 1 308 213 bytes
- **Licencia**: dominio público (autor fallecido 1923)
- **Tipo**: **primaria técnica** (manual clásico, 4ª ed.)

### `[V]` Proceso de contacto — efecto de la temperatura sobre el rendimiento

> «At 408° the reaction was almost quantitative, at 500° only 90 per
> cent., at 530° only 80 per cent. of the SO₂ is transformed into
> SO₃.»

— Lunge 1913, vol. 1, p. 1309.

| Temperatura | Conversión SO₂ → SO₃ |
|---|---|
| 408 °C | ~cuantitativa (~99 %) |
| 500 °C | 90 % |
| 530 °C | 80 % |

`[I]` **Para el juego**: existe un **máximo de actividad** del
catalizador por debajo de 450 °C. Pasarse de 500 °C ya cuesta 10 puntos
de conversión. Es la ventana de operación real.

### `[V]` Composición del "burner gas" ideal

> «the "ideal" burner-gas (**11·69 per cent. SO₂, 5·85 per cent O₂,
> 82·46 per cent. N**)»

— Lunge 1913, vol. 1, p. 1314 (citando a Lucas, *Z. Elektrochem.*,
1905).

`[V]` El gas de tostación ideal para el proceso de contacto es
**11,69 % SO₂, 5,85 % O₂, 82,46 % N₂** — una proporción cercana a 2:1
SO₂:O₂ estequiométrica, diluida con N₂ del aire.

### `[V]` Longevidad del catalizador

> «the same contact-mass has been working in their first large
> contact-apparatus during the last nine years, and still yields a
> **97 per cent. conversion of SO₂ into SO₃**.»

— Lunge 1913, vol. 1, p. 1573 (sobre el catalizador Schroeder-Grillo
de la Badische).

`[V]` Un catalizador de contacto industrial mantenido **97 % de
conversión durante 9 años** sin reemplazo.

### `[V]` Capacidad de las cámaras de plomo

> «The total cubic space of the chambers is about 310,000 cub. ft.,
> which, according to the experience with ordinary chambers at the
> majority of German works, is required for the regular daily production
> of **20 tons H₂SO₄** in the shape of chamber-acid. [This is as nearly
> as possible **= 19 cub. ft. per lb. of sulphur burned**, and corresponds
> with the practice of the majority of English works; but much less
> space is required for the "intense" or "high-pressure" style of
> work.]»

— Lunge 1913, vol. 1, p. 1231-1232.

`[V]` **Volumen específico** de la cámara de plomo: **19 ft³ por libra
de azufre quemado**, equivalente a unos **5 m³/kg S**.

`[I]` **Para el juego**: el tamaño de la planta química antigua se
dimensiona por **volumen de cámara**, no por rendimiento. Una receta
de ácido sulfúrico que ignore el tamaño de la cámara de plomo está
mintiendo sobre el coste de capital.

---

## Tabla resumen — cifras críticas por proceso

### Pólvora

| Proceso | Variable | Valor | Fuente |
|---|---|---|---|
| Composición histórica media | KNO₃ / S / C | 75 / 10 / 15 (Prusia 1808) | Guttmann 1895 p. 178 |
| Composición estequiométrica (teórica) | KNO₃ / S / C | 74,84 / 11,84 / 13,32 | Guttmann 1895 p. 177 (Berthelot) |
| Composición teórica max calor | KNO₃ / S / C | 84 / 8 / 8 | Guttmann 1895 p. 177 (Berthelot) |
| Rango histórico de C | % | 9-17 | Guttmann 1895 p. 178 |
| Humedad en grano | % | ~8 (tras granulado), ~1,25 (tras dusting) | Guttmann 1895 p. 217 |
| Naturaleza del producto | — | mezcla mecánica, no compuesto | Guttmann 1895 p. 177 |

### Calcinación / Cemento Portland

| Proceso | Variable | Valor | Fuente |
|---|---|---|---|
| Cemento Portland — temperatura clinkerización | °C | 1475-1625 (según cal y molido) | Mead 1911 p. 184-185 |
| Cemento Portland — cal en clinker | % | 60-64 | Butler 1899 p. 6 |
| Cemento Portland — efecto molido fino (98 % < 200 mesh) | Δ T | -137 °C | Mead 1911 p. 185 |
| Cemento Portland — efecto álcalis (1,5 % Na₂CO₃) | cualitativo | permite clinkerizar | Mead 1911 p. 187 |
| Materia prima — proporción chalk:clay | vol | 3:1 | Butler 1899 p. 30 |

### Petróleo

| Proceso | Variable | Valor | Fuente |
|---|---|---|---|
| Nafta cruda — punto de ebullición | °F | ~200 (~93 °C) | Hamor 1916 p. 466 |
| Nafta pesada — punto de ebullición | °F | ~300 (~149 °C) | Hamor 1916 p. 466 |
| Keroseno — punto de ebullición | °F | ~450 (~232 °C) | Hamor 1916 p. 466 |
| Nafta cruda — rendimiento | % vol crudo | 6-8 | Hamor 1916 p. 466 |
| Nafta pesada — rendimiento | % vol crudo | 13-15 | Hamor 1916 p. 466 |
| Keroseno — rendimiento | % vol crudo | 16-18 | Hamor 1916 p. 466 |
| Tar (a tar stills) — rendimiento | % vol crudo | 42 | Hamor 1916 p. 466 |
| Coque — rendimiento | % del tar | 4 | Hamor 1916 p. 466 |
| Destilación al vapor — Δ T | °F | -100 a -150 | Hamor 1916 p. 469 |

### Haber-Bosch

| Proceso | Variable | Valor | Fuente |
|---|---|---|---|
| Síntesis NH₃ — presión | atm | 300 | Ernst 1925 p. 776 |
| Síntesis NH₃ — temperatura | °C | 475 | Ernst 1925 p. 776 |
| Síntesis NH₃ — conversión por paso | % | 20 | Ernst 1925 p. 776 |
| Síntesis NH₃ — relación N₂:H₂ | vol | 1:3 | Ernst 1925 p. 775 |
| Consumo H₂ por ton NH₃ | ft³ (a 20 °C, 1 atm) | 88 333 (=265 000/3) | Ernst 1925 p. 776 |
| Consumo eléctrico para H₂ | kWh/1000 ft³ H₂ | 140 | Ernst 1925 p. 776 |
| Capacidad de planta típica | t/día | 3 | Ernst 1925 p. 775 |

### Ácido sulfúrico

| Proceso | Variable | Valor | Fuente |
|---|---|---|---|
| Proceso contacto — T óptima | °C | ~408 (cuantitativa) | Lunge 1913 p. 1309 |
| Proceso contacto — T a 90 % conversión | °C | 500 | Lunge 1913 p. 1309 |
| Proceso contacto — T a 80 % conversión | °C | 530 | Lunge 1913 p. 1309 |
| Proceso contacto — composición burner gas | % SO₂ / O₂ / N | 11,69 / 5,85 / 82,46 | Lunge 1913 p. 1314 |
| Catalizador — duración con 97 % conv. | años | ≥ 9 | Lunge 1913 p. 1573 |
| Cámaras de plomo — volumen específico | ft³/lb S quemado | 19 | Lunge 1913 p. 1231-1232 |
| Planta de cámaras — capacidad | t H₂SO₄/día para 310 000 ft³ | 20 | Lunge 1913 p. 1231 |

---

## Lo que este extracto demuestra

1. **Pólvora** es una mezcla mecánica de tres sólidos, no un compuesto.
   Su estequiometría (74,84 / 11,84 / 13,32) coincide con la receta
   histórica de Prusia (75 / 10 / 15). Una receta de pólvora que no
   permita ajustar las tres proporciones está simplificando demasiado.

2. **Cemento Portland** necesita **1475-1625 °C** y **60-64 % de CaO**
   en el clinker. La temperatura exacta depende de la fineza del molido
   (-137 °C con 98 % < 200 mesh) y de los álcalis como fundente. Una
   clinkerización "instantánea" en un horno de fundición es físicamente
   imposible.

3. **Destilación fraccionada del petróleo** produce fracciones con
   puntos de ebullición crecientes (nafta ~93 °C → keroseno ~232 °C →
   alquitrán). El **vapor de agua** reduce la temperatura de salida
   100-150 °C. La diferencia entre destilación fraccionada
   (lubrificantes) y cracking (gasolina) es la decisión de diseño
   básica del refino.

4. **Proceso Haber-Bosch** requiere **300 atm, 475 °C, catalizador**
   y consume **88 333 ft³ de H₂ por ton de NH₃**. El H₂ por electrólisis
   son 140 kWh/1000 ft³. Sin electricidad barata, el proceso no es
   viable. Esto condiciona la época en que el NH₦ sintético puede
   aparecer en el juego.

5. **Ácido sulfúrico** tiene dos procesos vivos a principios del s. XX:
   **cámaras de plomo** (~5 m³ de cámara por kg de S quemado, da ácido
   al ~65 %) y **contacto** (~408 °C, 97 % conversión, ácido
   concentrado). La elección depende del mercado: cámaras para
   fertilizantes, contacto para ácido fumante y explosivos.

6. **Las fuentes primarias son claras**: el Internet Archive conserva
   manuales de 1877 a 1925 en texto plano descargable. Las cifras
   concretas (temperaturas, presiones, proporciones, rendimientos)
   están ahí; no hay que inventarlas.

---

## Pendientes (NO VERIFICADO en este extracto)

- **NO VERIFICADO** — poder calorífico de la pólvora en kcal/kg
  (Guttmann describe la reacción pero no extracté cifra aislada).
- **NO VERIFICADO** — temperatura exacta de descomposición del CaCO₃
  (Mead/Butler describen la calcinación pero no extracté el umbral
  en °C; bibliografía habitual da ~898 °C pero **NO** se cita aquí).
- **NO VERIFICADO** — fecha exacta del primer horno rotatorio de
  cemento (Mead menciona su uso en 1911 pero no la invención).
- **NO VERIFICADO** — presión de vapor del agua a 100 °C y superiores
  para los cálculos de destilación con vapor (Hamor da la descripción
  cualitativa, no las cifras parciales).
- **NO VERIFICADO** — año exacto en que Carl Bosch escaló el proceso
  Haber a planta industrial (atribuido a 1913, no citado literalmente
  en Ernst 1925).
- **NO VERIFICADO** — composición media del coque metalúrgico usado
  en tostación de piritas (Lunge describe el proceso pero no
  extracté aquí la composición).

Estos huecos son deliberados. **Vale más un extracto corto y sólido que
uno largo y dudoso.**
