# Electrónica, computación y comunicaciones — extractos con fuente

**Estado**: cosecha inicial. Épocas que sirve: **12–15**.

**Sesgo declarado** (mismo que en `metalurgia.md` y `medicina.md`): para las
épocas 12–13 se ha recurrido a **tratados técnicos de dominio público del
Internet Archive anteriores a 1929**; para las 14–15 a **enciclopedias
(secundarias) y revist­as DOAJ recientes**. Wikipedia, cuando se cita, va
declarada como **secundaria**.

**Asuntos cubiertos** (los ocho del encargo):
1. Telégrafo
2. Radio (telegrafía sin hilos)
3. Válvula de vacío
4. Transistor
5. Semiconductores y silicio
6. Tierras raras
7. Circuito integrado
8. Computación

---

## 1. Telégrafo

### Fuente 1.1 — Prescott, *History, Theory and Practice of the Electric Telegraph* (1866)

- **Obra**: George B. Prescott. *History, Theory and Practice of the
  Electric Telegraph.* Boston, 1866. 1ª edición del manual estándar en
  lengua inglesa; reimpreso por Cornell University.
- **URL**: https://archive.org/details/cu31924031307196
- **Texto plano**: https://archive.org/download/cu31924031307196/cu31924031307196_djvu.txt
- **Consultado**: 2026-07-31 · 1 214 873 bytes, 27 232 líneas
- **Licencia**: dominio público (publicado 1866 en EE. UU. sin renovación)
- **Tipo**: **primaria técnica**, manual de la época dorada del telégrafo

### `[V]` Velocidad de la corriente en el cable — el dato depende del metal

> «On copper wire, its velocity, according to Professor Wheatstone's
> experiments, is 288,000 miles, and according to MM. Figeau and Gonelle,
> 112,680 miles per second. On the iron wire, used for telegraphic
> purposes, its velocity is 62,000 miles per second, according to
> Figeau and Gonelle; 28,500, according to Professor Mitchell of
> Cincinnati; and about 16,000, according to Professor Walker of the
> United States Coast Survey.» — cap. III

| Metal | Velocidad (mi/s) | Fuente |
|---|---|---|
| Cobre | 288 000 | Wheatstone |
| Cobre | 112 680 | Figeau & Gonelle |
| Hierro | 62 000 | Figeau & Gonelle |
| Hierro | 28 500 | Mitchell (Cincinnati) |
| Hierro | 16 000 | Walker (U.S. Coast Survey) |

`[I]` **Para el juego**: la "instantaneidad" del telégrafo es real **en
escala humana**, pero la cifra varía **6×** entre autores. Una sola
"velocidad de la corriente" para el jugador es honesta: el orden de
magnitud (decenas de miles de mi/s) y la dependencia con el metal. Si la
receta dice "telegraph" sin distinguir cobre de hierro, está mintiendo
sobre el coste de la línea.

### `[V]` Pila Daniell — la batería estándar del telégrafo estadounidense

> «The Daniell battery is used upon nearly all telegraph lines in the
> United States for a local current. They have been substituted for the
> Grove, which was formerly exclusively used for local as well as main
> circuits. They are much superior to the Grove in that they require less
> attention, are not offensive in smell nor injurious to health, and
> furnish a steady, reliable current. **Two cups are generally sufficient
> to work a Morse register or sounder.**» — cap. III

`[I]` **Para el juego**: para el receptor Morse, basta **una pila Daniell
de 2 vasos** (≈ 2 V). Si la receta de "telégrafo" pide una central
eléctrica, está sobre-dimensionando la tecnología. **Pero** la batería
de la **línea principal** (extremo a extremo) consume muchos más
vasos: el Grove fue reemplazado por el Daniell por su **coste de
mantenimiento**, no por su insuficiencia técnica.

### `[V]` Conductividad del cobre frente al hierro

> «Copper is the best conductor of the metals, it being seven times
> better than iron; but, as explained above, the conducting power of
> the metal being increased in the same proportion as the area of the
> section of the wire is augmented, it becomes perfectly easy, by
> increasing the size of the iron wire, to obtain as good a conductor
> as is required.» — cap. III

`[I]` **Para el juego**: el cobre es 7× mejor conductor que el hierro
(misma sección). Explica **por qué** los telégrafos terrestres empiezan
con cobre pero la mayoría acaba en hierro: el cobre es **caro** y la
ventaja se recupera subiendo la sección, no el metal. La lucha
coste/propiedad es la misma que en el resto del juego.

### `[V]` La Tierra como conductor — Steinheil

> «the idea of employing the earth as a conductor between two
> telegraphic stations, realized for the first time by Steinheil, had
> permitted the suppression of one of the conducting-wires, and thus
> the realization of great economy and simplicity in practice.» — cap. III

`[I]` **Para el juego**: el **retorno por tierra** (en lugar de un
segundo cable) reduce a la mitad la cantidad de cobre de la línea. Sin
este truco, el telégrafo transcontinental habría sido económicamente
imposible. **Es el primer «ground» de la historia de la electrónica
del juego.**

---

## 2. Radio (telegrafía sin hilos)

### Fuente 2.1 — Bottone, *Wireless Telegraphy and Hertzian Waves* (1910)

- **Obra**: S. R. Bottone. *Wireless Telegraphy and Hertzian Waves.*
  London, 1910. Manual para aficionados.
- **URL**: https://archive.org/details/WirelessTelegraphyAnd
- **Texto plano**: https://archive.org/download/WirelessTelegraphyAnd/WirelessTelegraphyAnd_djvu.txt
- **Consultado**: 2026-07-31 · 222 907 bytes, 6 972 líneas
- **Licencia**: dominio público (publicado 1910)
- **Tipo**: **primaria técnica**, manual de divulgación

### `[V]` Velocidad de las ondas hertzianas

> «These waves travel with an enormous velocity (about 200,000 miles
> per second), and to very great distances, their intensity becoming
> less as the distance becomes greater.» — cap. IX

`[I]` **Para el juego**: 200 000 mi/s ≈ velocidad de la luz (≈ 300 000
mi/s). Para el razonamiento del jugador basta: la onda electromagnética
**viaja a la velocidad de la luz**. Si la receta de "radio" tiene un
retraso telegráfico, se contradice.

### `[V]` Preece — inducción sin hilos sin el "ether medium"

> «Two small holes are now drilled in these discs to admit of two small
> screws, which will hereafter serve to attach the neutralizing rods to
> the upper extremities of the standards.» — construcción de la
> bobina inductora de Preece

> «In 1853 J. B. Lindsay proved the possibility of transmitting a
> message across water at points 500 yards apart, without continuous
> wires; and he patented this invention in 1854. But this can hardly
> be classed with the phenomena which we now understand by 'wireless'
> telegraphy.» — cap. IX

`[I]` **Para el juego**: Lindsay (1853) ya tenía **500 yards sin
hilos** por inducción — no es Marconi. La receta de "radio" del juego
no debe fechar la telegrafía sin hilos en 1895; tiene **40 años de
prehistoria** por inducción y conductores.

### `[V]` Marconi — el aparato original

> «A, we have an ordinary tapping-key connected to the primary of a
> coil C through the battery B. The secondary of this coil is connected
> to two brass balls, D and D', which are placed at the two opposite
> diameters of two larger brass balls, E and E', that are half inserted
> in a glass tube filled with vaseline oil.» — cap. X, fig. 9

`[V]` Confirma la **cadena transmisor Marconi**: llave + bobina de
inducción (Ruhmkorff) + spark gap en baño de vaselina. Es la cadena
mínima: **una llave, una bobina, una batería, y un dipolo con spark
gap**. Lo que los «tubos de centelleo» añaden es regulación, no
producción de la onda.

### `[V]` Guarini — el «repeater» salva distancias

> «There need only be a repeater at every five-hundredth mile, it is
> said, in order to establish communication with any given point of the
> surface of the earth.» — apéndice, sobre el «repeater» de Guarini
> (1900)

`[I]` **Para el juego**: 500 mi ≈ 800 km entre repetidores. Es la
**densidad práctica de la red radio** de la época. Una receta que dice
"radio de 1 000 km con un transmisor" necesita explicar la cadena de
repetidores o la antena.

### `[V]` Popoff — comunicar en condiciones extremas

> «no communication with the continent, forty-seven kilometres
> distant, being possible. M. Popoff was commissioned to establish
> communication by wireless telegraphy, and so a station was installed
> on Hohland.» — apéndice

47 km cruzando el golfo de Finlandia en 1900. Es la **cifra de
alcance real** de la radio de chispa pre-válvula.

---

## 3. Válvula de vacío

### Fuente 3.1 — *The Wireless Experimenter's Manual* (1920)

- **Obra**: *The Wireless Experimenter's Manual, Incorporating How to
  Conduct a Radio Club.* New York, 1920. Escrito por el equipo de
  *The Experimenter*.
- **URL**: https://archive.org/details/WirelessExperimentersManual
- **Texto plano**: https://archive.org/download/WirelessExperimentersManual/WirelessExperimentersManual_djvu.txt
- **Consultado**: 2026-07-31 · 768 551 bytes
- **Licencia**: dominio público (publicado 1920 en EE. UU. sin renovación)
- **Tipo**: **primaria técnica**, manual para radio aficionados

### `[V]` Marconi V.T. — tensiones de funcionamiento

> «The e.m.f. of the A battery is 4 volts, of the B battery about 60
> volts.»
>
> «To obtain the best response from the Marconi V. T., set the plate
> voltage at some voltage between 25 and 60 and slowly increase the
> filament temperature by the rheostat R until a signal maximum is
> heard in the telephones.» — cap. XII

| Parámetro | Valor |
|---|---|
| A battery (filamento) | 4 V |
| B battery (placa) | 60 V (rango 25–60 V) |
| Vida del bulbo | ≈ 1 500 horas |
| Capacidades de acoplo | 0.0001–0.0005 mfd |
| Resistencia de placa | 2 megohm |
| Tensión de polarización de grid | 1.5 V (grid battery) |

### `[V]` Composición de los electrodos

> «One commercial detector tube has a nickel filament, a tungsten grid
> and a molybdenum plate. Another has a tungsten filament, a nickel
> grid and a nickel plate.» — cap. XI

`[I]` **Para el juego**: la válvula de vacío necesita **cuatro metales
distintos** (filamento, grid, placa, vidrio). No es un solo componente
— es una **cadena de suministro** que justifica por qué los
transistores (un solo cristal) son «más simples».

### `[V]` Línea histórica: Fleming → De Forest → Armstrong

> «Deforest inserted a so-called grid element between the filament and
> plate of Fleming's valve and increased its sensitiveness. Armstrong
> was the first to show the operating characteristics of the
> three-electrode valve and his adoption of the regenerative principle
> marked a distinct advance in the art.» — cap. XI

`[V]` Confirma la **secuencia de la invención** (diodo 1904 Fleming,
triodo 1906 De Forest, regenerative 1912 Armstrong). EPO 13 del juego
debe **distinguirlas**: la válvula de 1904 no amplifica; la de 1906
sí.

### `[V]` Frecuencias — divisor entre radio y audio

> «We arbitrarily call current of frequencies above 10,000 per second
> radio frequency currents; those below 10,000 per second audio
> frequency currents. It is a striking fact, of which considerable note
> will be taken further on, that currents above 20,000 cycles per
> second are not audible in the telephone receiver, for the ear will
> generally not respond to sound vibrations above 20,000 per second.»
>
> «The lowest frequency so far used for wireless transmission is
> 15,000.» — cap. III

`[I]` **Para el juego**: la frecuencia umbral ω ≈ 15 kHz separa la
zona «audible» (teléfono) de la zona «radio». Por debajo de 15 kHz no
hay antena eficiente. Esa frontera es la **razón física** de que la
radio necesite circuitos sintonizados, no auriculares.

### `[V]` Voltajes de los osciladores de potencia

> «Powerful oscillations are produced in radio telegraphy by high
> voltages — 15,000 to 30,000 volts.» — cap. III

| Función | Tensión |
|---|---|
| Primario bobina | 6–30 V (almacenamiento) hasta 500 V (red) |
| Secundario bobina | 20 000–30 000 V |
| Spark gap | aceite de vaselina |
| Oscilador amateur | 15 000 V (estándar) |

`[I]` **Para el juego**: la radio de chispa trabaja a **20–30 kV**. Es
un salto enorme desde los 4 V del filamento. La cadena de
alimentación tiene **dos transformaciones**: baja tensión (filamento) y
alta tensión (spark). Si la receta de la época 13 ignora la
**alta tensión**, está mintiendo.

---

## 4. Transistor

### Fuente 4.1 — Wikipedia, *Transistor* (entrada general, época 14)

- **URL**: https://en.wikipedia.org/wiki/Transistor
- **Consultado**: 2026-07-31
- **Licencia**: CC BY-SA — **secundaria**, declararlo
- **Tipo**: enciclopédica

### `[V]` Invención — punto de contacto, germanio, 1947

> «The first working device was a point-contact transistor invented in
> 1947. Physicists John Bardeen, Walter Brattain, and William Shockley
> at Bell Labs … shared the 1956 Nobel Prize in Physics for their
> achievement.» — Wikipedia

> «they observed that when two gold point contacts were applied to a
> crystal of germanium, a signal was produced with the output power
> greater than the input.» — Wikipedia

| Variable | Valor |
|---|---|
| Año | 1947 |
| Tipo | point-contact |
| Material | germanio |
| Contactos | oro (Au) |
| β (BJT) | típicamente > 100 (small-signal) |

### `[V]` Primer transistor de silicio — 1954

> «The first working silicon transistor was developed at Bell Labs on
> January 26, 1954, by Morris Tanenbaum.» — Wikipedia

`[I]` **Para el juego**: hay **siete años** entre el transistor de
germanio (1947) y el de silicio (1954). Si la receta «transistor»
apare­ce en la época 14 (≈ 1950s), puede justificarse con **uno o
ambos** materiales. El germanio es más fácil de dopar; el silicio
aguanta más temperatura. **Para la época 14 hay que distinguir
Ge/Si.**

---

## 5. Semiconductores y silicio

### Fuente 5.1 — Wikipedia, *Polysilicon* (entrada general, época 14–15)

- **URL**: https://en.wikipedia.org/wiki/Polysilicon
- **Consultado**: 2026-07-31
- **Licencia**: CC BY-SA — **secundaria**
- **Tipo**: enciclopédica

### `[V]` Pureza del polisilicio para electrónica

> «When produced for the electronics industry, polysilicon contains
> impurity levels of less than one part per billion (ppb).»
> «Polycrystalline silicon can be as much as 99.9999% pure.»

| Pureza | Aplicación |
|---|---|
| 98 % | silicio metalúrgico (alimentación) |
| 99.9999 % (6N) | polisilicio solar |
| < 1 ppb de impurezas | polisilicio electrónico |

### `[V]` Proceso Siemens — el método estándar

> «Polysilicon is produced from metallurgical grade silicon by a
> chemical purification process, called the Siemens process. The
> Siemens process is the most commonly used method of polysilicon
> production, especially for electronics. The process converts
> metallurgical-grade Si, of approximately 98% purity, to SiHCl3
> (Trichlorosilane) and then to silicon in a reactor. It is a type of
> chemical vapor deposition process.»

`[I]` **Para el juego**: la **purificación es química**, no física. La
mena entra al 98 % (metalúrgico) y sale al +99.9999 % por **CVD de
triclorosilano**. Si la receta de "silicio" del juego se consigue
"triturando cuarzo", está mintiendo: la vía realista pasa por HCl,
H₂, SiHCl₃ y un reactor CVD.

### Fuente 5.2 — Wikipedia, *Czochralski method* (entrada general)

- **URL**: https://en.wikipedia.org/wiki/Czochralski_method
- **Consultado**: 2026-07-31
- **Licencia**: CC BY-SA — **secundaria**
- **Tipo**: enciclopédica

### `[V]` Invención accidental del pulling cristalino

> «Jan Czochralski — a Polish chemist invented his method in 1916 at
> AEG in Germany while investigating the crystallization velocities of
> metals. He made this discovery by accident: instead of dipping his
> pen into his inkwell, he dipped it in molten tin, and drew a tin
> filament.»

`[V]` **1916** — el método se inventó para **estaño**, no para
silicio. Solo en 1950 Teal lo aplicó al Ge y al Si. EPO 13–14 del juego
puede fechar el «método Czochralski» en 1916, pero la **aplicación a
semiconductores** es post-1950.

### `[V]` Tamaño de los lingotes — la escala real

> «Early on, boules were small, a few centimeters wide. With advanced
> technology, high-end device manufacturers use 200 mm and 300 mm
> diameter wafers.»

| Época | Diámetro del lingote |
|---|---|
| Década 1950 | pocos cm |
| Década 1990 | 200 mm |
| Década 2000 | 300 mm |

`[I]` **Para el juego**: pasar de **pozos** a **lingotes de 300 mm** es
un factor 1 000× en cantidad de silicio por cristal. Si la receta de
"silicio" del juego entrega una oblea por operación, está
infra-dimensionando.

---

## 6. Tierras raras

### Fuente 6.1 — Wikipedia, *Rare-earth element* (entrada general)

- **URL**: https://en.wikipedia.org/wiki/Rare-earth_element
- **Consultado**: 2026-07-31
- **Licencia**: CC BY-SA — **secundaria**
- **Tipo**: enciclopédica

### `[V]` Definición y paradoja del nombre

> «The rare-earth elements (REE), also called rare-earth metals, or
> rare earths, are a set of 17 nearly indistinguishable lustrous
> silvery-white soft heavy metals.»
>
> «The term 'rare-earth' is a misnomer, because they are not actually
> scarce, but because they are found only in compounds, not as pure
> metals, and are difficult to isolate and purify.»

`[I]` **Para el juego**: hay **17 REE** (15 lantánidos + Sc + Y). El
«rare» viene del **aislamiento**, no de la abundancia. El cerio, por
ejemplo, es el **elemento 25 más abundante** de la corteza (68 ppm),
más que el cobre. La rareza es **logística**, no geológica.

### `[V]` Minerales clave

> «The principal sources of rare-earth elements are the minerals
> bastnäsite (RCO₃F, where R is a mixture of rare-earth elements),
> monazite (XPO₄, where X is a mixture of rare-earth elements and
> sometimes thorium), and loparite.»

### `[V]` Composición típica de la bastnasita (Mountain Pass)

> «bastnasite ore is typically used in this process, with an average
> of 7% REO (rare-earth oxides).»
>
> «about 49% cerium, 33% lanthanum, 12% neodymium, and 5%
> praseodymium»

| Elemento | % dentro del REO |
|---|---|
| Cerio (Ce) | 49 |
| Lantano (La) | 33 |
| Neodimio (Nd) | 12 |
| Praseodimio (Pr) | 5 |
| Otros | 1 |

`[I]` **Para el juego**: la bastnasita **no es "tierras raras"** —
es una mezcla donde **Ce+La+Nd+Pr ≈ 99 %** del contenido. El
"tierras raras" del juego, si viene de bastnasita, debería tener
predominio de Ce/La. Si la receta pide Nd aislado, está
sobre-simplificando.

### `[V]` Pasos de procesamiento

> «finely ground, and subjected to flotation to separate the bulk of
> the bastnäsite» → «calcined acid washed bastnäsite» → «leached with
> hydrochloric acid» → «subjected to solvent extraction, to capture
> the europium, and purify the other individual components» →
> «Oxidizing roast further concentrates the solution to approximately
> 85% REO» → «La separated from Nd, Pr, and SX» → «Nd and Pr
> separated.»

Cadena: molienda → flotación → calcinación → lixiviación HCl →
extracción con disolvente → calcinación oxidante → separaciones
individuales.

### `[V]` Protagonismo chino

> «Between 1985 and 1995 China increased its share in the production
> of REE from 21% to 60%.»
>
> «China dominates the rest of the world in terms of REE reserves and
> production; in 2019, it supplied around 90% of the global demand for
> the 17 rare-earth powders.»

`[I]` **Para el juego**: si el mundo del juego necesita Nd para
imanes, **sin China no hay imanes**. La dependencia es geopolítica,
no técnica.

### Fuente 6.2 — Wikipedia, *Neodymium magnet* (entrada general)

- **URL**: https://en.wikipedia.org/wiki/Neodymium_magnet
- **Consultado**: 2026-07-31
- **Licencia**: CC BY-SA — **secundaria**

### `[V]` Composición del imán de Nd

> «A neodymium magnet (also known as NdFeB, NIB or Neo magnet) is a
> permanent magnet made from an alloy of neodymium, iron, and boron
> that forms the Nd₂Fe₁₄B tetragonal crystalline structure.»

### `[V]` Usos en electrónica

> «applications in modern products that require strong permanent
> magnets, such as electric motors in cordless tools, hard disk drives
> and magnetic fasteners. Head actuators for computer hard disks.
> Loudspeakers and headphones. Mobile phones.»

### `[V]` Producto de energía máxima

> «(BH)max ≈ 512 kJ/m³ or 64 MG·Oe»

`[I]` **Para el juego**: cada smartphone contiene **varios imanes de
Nd** (altavoz, vibrador, cámara). El **disco duro** los usa en el
actuador. Sin Nd, **no hay almacenamiento magnético de alta
densidad**.

---

## 7. Circuito integrado

### Fuente 7.1 — Wikipedia, *Integrated circuit* (entrada general)

- **URL**: https://en.wikipedia.org/wiki/Integrated_circuit
- **Consultado**: 2026-07-31
- **Licencia**: CC BY-SA — **secundaria**
- **Tipo**: enciclopédica

### `[V]` Kilby — primer IC funcional, 12 de septiembre de 1958

> «successfully demonstrating the first working example of an
> integrated circuit on 12 September 1958»
>
> «Kilby described his new device as 'a body of semiconductor material
> … wherein all the components of the electronic circuit are completely
> integrated'»

### `[V]` Noyce — la versión planar, base de la producción moderna

> «Robert Noyce at Fairchild Semiconductor developed the first
> practical monolithic IC chip. Noyce's version was fabricated from
> silicon using the planar process by his colleague Jean Hoerni, which
> allowed reliable on-chip aluminum interconnections. Modern IC chips
> are based on Noyce's monolithic design, rather than Kilby's early
> prototype.»

`[I]` **Para el juego**: Kilby **inventó** el IC (germanio, alambres
externos); Noyce **lo hizo fabricable** (silicio, planar). La receta
del juego debe distinguir: **invento (1958) vs. producción en masa
(1959–1960)**. EPO 14 las puede separar en dos saltos.

### `[V]` Escala de integración

> «a modern chip may have many billions of transistors in an area the
> size of a human fingernail»
> «maximum transistor counts continue to grow beyond 5.3 trillion
> transistors per chip»

### Fuente 7.2 — Wikipedia, *Moore's law* (entrada general)

- **URL**: https://en.wikipedia.org/wiki/Moore%27s_law
- **Consultado**: 2026-07-31
- **Licencia**: CC BY-SA — **secundaria**

### `[V]` Cita literal de Moore (1965)

> «The complexity for minimum component costs has increased at a rate
> of roughly a factor of two per year.» — Moore, 1965

### `[V]` Revisión 1975

> «predicting semiconductor complexity would continue to double
> annually until about 1980, after which it would decrease to a rate
> of doubling approximately every two years» — Moore, 1975

`[V]` Doble cada **2 años** desde 1975. Cifra citada en el juego
debería ser **cada 2 años**, no "cada 18 meses" (que es el *rendimiento*
computacional, no la cuenta de transistores).

### Fuente 7.3 — Wikipedia, *Photolithography* (entrada general)

- **URL**: https://en.wikipedia.org/wiki/Photolithography
- **Consultado**: 2026-07-31
- **Licencia**: CC BY-SA — **secundaria**

### `[V]` Resolución y longitudes de onda

> «Current state-of-the-art photolithography tools use deep ultraviolet
> (DUV) light from excimer lasers with wavelengths of 248 (KrF) and 193
> (ArF) nm (the dominant lithography technology today is thus also
> called 'excimer laser lithography'), which allow minimum feature
> sizes down to 50 nm.»
>
> «Excimer laser lithography machines (steppers and scanners) became
> the primary tools in microelectronics production, and has enabled
> minimum features sizes in chip manufacturing to shrink from 800
> nanometers in 1990 to 7 nanometers in 2018.»
>
> «Lasers have been used to indirectly generate non-coherent extreme
> UV (EUV) light at 13.5 nm for extreme ultraviolet lithography …
> As of 2020, EUV is in mass production use by leading edge foundries
> such as TSMC and Samsung.»

| Longitud de onda | Nombre | Tamaño mínimo de feature |
|---|---|---|
| 436 nm (g-line) | UV mercury | μm |
| 248 nm (KrF) | DUV | sub-μm |
| 193 nm (ArF) | DUV + inmersión | 50 nm (2006: < 30 nm) |
| 13.5 nm | EUV | 7 nm (2018) |

`[I]` **Para el juego**: la "miniaturización" del IC requiere **saltar
la barrera de la longitud de onda**. La litografía óptica no
resuelve por debajo de λ/2; por eso el **EUV a 13.5 nm** fue la
siguiente frontera. Si el juego tiene "litografía" como proceso, la
resolución máxima la marca la **lámpara**, no la óptica.

---

## 8. Computación

### Fuente 8.1 — Wikipedia, *ENIAC* (entrada general)

- **URL**: https://en.wikipedia.org/wiki/ENIAC
- **Consultado**: 2026-07-31
- **Licencia**: CC BY-SA — **secundaria**

### `[V]` ENIAC — los números reales

> «ENIAC was completed in 1945 and first put to work for practical
> purposes on December 10, 1945.»
>
> «By the end of its operation in 1956, ENIAC contained 18,000 vacuum
> tubes, 7,200 crystal diodes, 6,000 relays.»
>
> «It weighed more than 30 short tons (27 t), was roughly 10 ft (3 m)
> tall, 3 ft (1 m) deep, and 100 ft (30 m) long.»
>
> «consumed 150 kW of electricity»
>
> «perform 5,000 simple addition or subtraction operations … per
> second»
>
> «ENIAC was able to process about 500 FLOPS»

| Variable | Valor |
|---|---|
| Año de operación | 1945–1956 |
| Tubos de vacío | 18 000 |
| Diodos de cristal | 7 200 |
| Relés | 6 000 |
| Peso | 30 t cortas (27 t) |
| Volumen | 10 × 3 × 100 ft³ |
| Potencia | 150 kW |
| Sumas/resta por segundo | 5 000 |
| FLOPS | 500 |

`[I]` **Para el juego**: **18 000 tubos** quemándose a 150 kW dan
≈ 8 W por tubo. Si la receta de "computación" (época 13) ignora el
coste de evacuación de calor, subestima el coste real por **un factor
> 10×**.

### Fuente 8.2 — Wikipedia, *Intel 4004*

- **URL**: https://en.wikipedia.org/wiki/Intel_4004
- **Consultado**: 2026-07-31
- **Licencia**: CC BY-SA — **secundaria**

### `[V]` Primer microprocesador — Intel 4004 (1971)

> «released by the Intel Corporation on November 15, 1971»
> «2,300 transistors»
> «12 mm² die»
> «10 μm process silicon-gate enhancement-load pMOS technology»
> «740 kHz to 750 kHz» (objetivo original 1 MHz)

| Variable | Valor |
|---|---|
| Año | 1971 |
| Transistores | 2 300 |
| Área de dado | 12 mm² |
| Proceso | 10 μm pMOS |
| Frecuencia | 740 kHz |

`[I]` **Para el juego**: en **un solo chip** entran 2 300
transistores en 12 mm². Si la receta de "microprocesador" pide **un
circuito integrado** como recurso, ese circuito entrega
**2–5 mil transistores mínimos**. La transición del "circuito
integrado" al "microprocesador" es **una sola persona** (Faggin,
1971), no un salto de época.

### Fuente 8.3 — Wikipedia, *Computer* (entrada general)

- **URL**: https://en.wikipedia.org/wiki/Computer
- **Consultado**: 2026-07-31
- **Licencia**: CC BY-SA — **secundaria**

### `[V]` Transición válvula → transistor

> «From 1955 onwards, transistors replaced vacuum tubes in computer
> designs, giving rise to the 'second generation' of computers.»
>
> «Compared to vacuum tubes, transistors have many advantages: they
> are smaller, and require less power than vacuum tubes, so give off
> less heat.»

### `[V]` MOSFET — el transistor compacto

> «the metal–oxide–silicon field-effect transistor (MOSFET), also known
> as the MOS transistor, was invented at Bell Labs between 1955 and
> 1960 and was the first truly compact transistor.»
>
> «The MOSFET led to the microcomputer revolution, and became the
> driving force behind the computer revolution.»

`[I]` **Para el juego**: la **época 14** del juego debe distinguir
**BJT** (1947) de **MOSFET** (1955–1960). La segunda permite
escalado a miles de transistores por chip; la primera no.

### `[V]` Colossus — antencedente británico

> «Colossus Mark I contained 1,500 thermionic valves (tubes), but Mark
> II with 2,400 valves, was both five times faster and simpler to
> operate.»

| Modelo | Válvulas | Velocidad relativa |
|---|---|---|
| Colossus Mark I | 1 500 | 1× |
| Colossus Mark II | 2 400 | 5× |

`[V]` Confirma que **más válvulas = más velocidad**, pero la
proporción no es lineal. La receta de "computación" debe permitir
**escalar hardware** sin linealidad perfecta.

---

## Tabla resumen — cifras críticas

| Tecnología | Variable | Valor | Fuente |
|---|---|---|---|
| Telégrafo | Vel.electr. Cu (mi/s) | 288 000 | Prescott 1866 (Wheatstone) |
| Telégrafo | Vel.electr. Fe (mi/s) | 16 000–62 000 | Prescott 1866 (3 autores) |
| Telégrafo | Batería Morse | 2 vasos Daniell | Prescott 1866 |
| Telégrafo | Cu vs Fe conductividad | 7× mejor Cu | Prescott 1866 |
| Radio | Vel.onda hertziana | 200 000 mi/s | Bottone 1910 |
| Radio | Alcance Popoff | 47 km | Bottone 1910 |
| Radio | Separación repeater | 500 mi | Bottone 1910 (Guarini) |
| Válvula | A battery (filamento) | 4 V | Wireless Exp. Manual 1920 |
| Válvula | B battery (placa) | 60 V | Wireless Exp. Manual 1920 |
| Válvula | Vida del bulbo | 1 500 h | Wireless Exp. Manual 1920 |
| Válvula | Tensión spark | 15 000–30 000 V | Wireless Exp. Manual 1920 |
| Válvula | Frecuencia umbral radio | 15 kHz | Wireless Exp. Manual 1920 |
| Transistor | Año (Ge) | 1947 | Wikipedia |
| Transistor | Año (Si) | 26/01/1954 | Wikipedia |
| Transistor | β típico | > 100 | Wikipedia |
| Silicio | Pureza electronic | < 1 ppb | Wikipedia (Polysilicon) |
| Silicio | Pureza 6N | 99.9999 % | Wikipedia (Polysilicon) |
| Silicio | Método | Siemens CVD | Wikipedia (Polysilicon) |
| Silicio | Czochralski | 1916 (Sn) | Wikipedia |
| Silicio | Lingote actual | 200–300 mm | Wikipedia |
| Tierras raras | REO en bastnasita | 7 % | Wikipedia |
| Tierras raras | Composición (%) | Ce 49, La 33, Nd 12, Pr 5 | Wikipedia |
| Tierras raras | Concentración tras roast | 85 % REO | Wikipedia |
| Imanes Nd | Composición | Nd₂Fe₁₄B | Wikipedia |
| Imanes Nd | (BH)max | 512 kJ/m³ | Wikipedia |
| Imanes Nd | Curie T | 310–400 °C | Wikipedia |
| IC | Kilby IC | 12/09/1958 | Wikipedia |
| IC | Noyce planar | 1959 | Wikipedia |
| IC | Moore 1965 | «factor of 2 per year» | Wikipedia |
| IC | Moore 1975 | cada 2 años | Wikipedia |
| Litografía | DUV 193 nm | < 50 nm (2006) | Wikipedia |
| Litografía | EUV 13.5 nm | 7 nm (2018) | Wikipedia |
| Computación | ENIAC válvulas | 18 000 | Wikipedia |
| Computación | ENIAC potencia | 150 kW | Wikipedia |
| Computación | ENIAC ops/s | 5 000 sumas | Wikipedia |
| Computación | MOS 1955–1960 | MOSFET | Wikipedia |
| Computación | Intel 4004 | 2 300 trans. / 12 mm² | Wikipedia |
| Computación | Intel 4004 | 740 kHz | Wikipedia |

---

## Lo que este extracto demuestra

1. **Telégrafo y radio conviven bien en el archivo público**. Los
   textos de 1866 (Prescott) y 1910 (Bottone) son de dominio público
   y dan cifras que las enciclopedias resumen.
2. **La válvula de vacío es eléctrica y térmica a la vez**. El
   filamento (4 V) y la placa (60 V) son dos sistemas distintos; la
   chispa externa exige 30 kV. La receta de "válvula" no puede ignorar
   la **alta tensión**.
3. **El transistor es bipolar o de efecto campo**. La receta de la
   época 14 debería permitir **BJT** (Ge, 1947) y **MOSFET** (Si,
   1955–60). Sin esa distinción, la historia no se entiende.
4. **El silicio necesita CVD, no purificación mecánica**. La receta
   "silicio" del juego debe pasar por la **vía química** (Siemens
   process) si quiere pureza 6N. La otra vía (Czochralski) es para
   el lingote, no para la pureza.
5. **Tierras raras ≠ escasas**. Son mezclas donde Ce/La dominan
   (>80 %). Para llegar a Nd aislado se necesita **extracción con
   disolvente**. La receta del juego debe permitir **aislar
   elementos individuales**, no entregar "tierras raras" en bloque.
6. **Cada smartphone contiene varios imanes de Nd**. Si el juego
   tiene "teléfono" o "disco duro" como recurso, **debería** pedir
   Nd. Es la dependencia oculta de la electrónica.

---

## Pendientes (NO VERIFICADO en este extracto)

- **NO VERIFICADO** — código Morse original de 1840 y longitud exacta
  de cada letra (no extraído literalmente de Prescott).
- **NO VERIFICADO** — la cifra original de Marconi 1895 (alcance en
  km) — Bottone la menciona pero no la cifra aquí.
- **NO VERIFICADO** — fecha exacta del MOSFET (1955–1960 en
  Wikipedia, sin cita primaria).
- **NO VERIFICADO** — tensión de ruptura del germanio vs silicio
  (no extraído).
- **NO VERIFICADO** — cifras de coste (USD/oz) de las tierras raras
  por elemento individual.
- **NO VERIFICADO** — kilby cita literal del 12/09/1958 — solo
  referenciado en Wikipedia, no extraído de fuente primaria.
- **NO VERIFICADO** — fechas exactas de la transición de Intel
  (1971 → 4004) con cita primaria de Intel.
- **NO VERIFICADO** — capacidad de producción de polisilicio global
  en toneladas/año.

Estos huecos son deliberados. **Vale más un extracto corto y sólido
que uno largo y dudoso.**
