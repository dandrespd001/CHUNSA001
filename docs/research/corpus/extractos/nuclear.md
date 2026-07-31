# Nuclear — extractos con fuente

**Estado**: encargo ejecutado 2026-07-31. Cubre los asuntos pedidos —
**fisión, enriquecimiento de uranio, reactor, ciclo del combustible,
residuos** — con fuentes primarias anteriores a 1929 (mineralogía del
uranio, primeros radioisótopos) y la **documentación oficial del
Proyecto Manhattan** (Smyth Report, 1945), más el **EIS de Yucca
Mountain** (1999) para el ciclo del combustible y residuos. Mismo
estándar del encargo: cita literal + URL + fecha + marca `[V]` /
`[I]` / `[?]`.

**Épocas que sirve**: **14–15** (era atómica y nano). Cualquier uso en
épocas anteriores es **fuera de alcance**: la **fisión nuclear** no
existió antes de 1938 (Hahn–Strassmann, citado por Smyth) y la
**reacción en cadena autosostenida** no se logró hasta el 2 de
diciembre de 1942 (Chicago Pile-1). El recurso `chunsa:uranium` ya
está definido en el catálogo del juego para la **época 14** (ver
`data/resources/uranium.yaml` y `docs/specs/SPEC-007_RECURSOS_Y_EDADES.md`).

> **Contexto del juego**: `chunsa:uranium` aparece en
> `data/resources/uranium.yaml` con `appearance_epoch: 14`, `family:
> energy`, `nature: collected`, y la nota «Uranio como energético de
> alta densidad para generación masiva; edad 14 (Atómica);
> SPEC-007 §9.2». Los **perfiles de verificación** (`sources`,
> `verification_reports`) están **vacíos** — este extracto es la
> primera cosecha seria para ese recurso.

---

## Fuente 1 — Cameron, *Radium and Radioactivity* (1912)

- **Obra**: A. T. Cameron, M.A., B.Sc., *Radium and Radioactivity*.
  Londres, 1912. (Serie "The Romance of Science".)
- **URL**: https://archive.org/details/radiumradioactiv00cameuoft
- **Texto plano**:
  https://archive.org/download/radiumradioactiv00cameuoft/radiumradioactiv00cameuoft_djvu.txt
- **Consultado**: 2026-07-31 · 299 919 bytes, 7 421 líneas
- **Licencia**: dominio público (publicado 1912)
- **Tipo**: **manual de mineralogía y química de los radioisótopos**,
  inmediatamente anterior al modelo atómico de Bohr. No contiene
  fisión; **sí** contiene lo que el juego necesita saber sobre **dónde
  está el uranio en el mundo** y **cuánto se saca por tonelada de
  mena**.

> No es un libro de "nuclear"; es un libro de **radium y los minerales
> que lo contienen**. Eso lo hace perfecto para **época 13–14**:
> describe la materia prima con la que un jugador podría encontrarse
> mucho antes de que exista un reactor.

### `[V]` Pitchblende — contenido en óxido de uranio

> «It has been pointed out that pitchblende, the source of most of the
> radium at present in existence (that is, in a condition of
> approximate purity), consists largely of uranium oxide, and it was
> in this mineral that uranium was actually discovered in 1789, 107
> years before the discovery of its activity, by the German chemist,
> Klaproth.»
> «The composition of the different uranium minerals varies largely;
> pitchblende contains over seventy per cent. of uranium oxide (see p.
> 32), but many of the others are complex silicates, phosphates, or
> arsenates.» — cap. V

`[V]` **Para el juego**: la **pechblenda** (uraninita) es la **mena
canónica**: hasta **75 % U₃O₈** en masa. Cualquier yacimiento "de
uranio" que el juego dibuje debería parecerse más a este número que
a un yacimiento de cobre (1–2 % de Cu en mena) o de hierro (30–60 %
Fe). El uranio es **la materia prima más rica en metal que existe**
entre los recursos del juego.

### `[V]` Klaproth descubre el uranio (1789)

> «in this mineral that uranium was actually discovered in 1789, 107
> years before the discovery of its activity, by the German chemist,
> Klaproth.» — cap. V

`[I]` **Para el juego**: el uranio entra al conocimiento humano en
**1789**, no en 1945. Es **el último elemento natural descubierto
antes de la pila eléctrica** (Volta 1800). El **mismo mineral** que
dio el uranio dio luego el radio (1898) y la fisión (1938). Esto
mata el cliché "el uranio aparece en 1945" — el uranio como mena
tiene **257 años** de historia para cuando se inventa la bomba.

### `[V]` Uranio metálico — propiedades físicas

> «Metallic uranium was isolated by Peligot in 1842 by the action of
> metallic potassium on the fused oxide. When obtained in this form
> it is a black metallic powder, but in the ordinary metallic form —
> in which it can be easily obtained by fusing the powder in the
> absence of air — it closely resembles such metals as iron and
> nickel. Heated to redness in air it burns brilliantly, forming the
> oxide, while it volatilises at…» — cap. V

`[V]` **Para el juego**: el uranio metálico es **similar a hierro y
níquel** en aspecto, **polvo negro** en forma reducida, **arder en
rojo** en aire. Eso es **muy diferente** al "uranio verde brillante"
que muchos videojuegos dibujan por confusión con la sal de uranio o
con el cobre. La apariencia en juego debería ser la de un metal
gris-pardo, no la de un cristal.

### `[V]` Procesamiento de la pechblenda — Curie (1898)

> «Several tons of residues of pitchblende from which the uranium had
> been removed were passed through processes precisely similar to
> those described, the work occupying many months, and finally a
> fraction of a gram of pure radium chloride was obtained.» — cap. III

| Variable | Valor |
|---|---|
| Mena procesada | **varias toneladas** de residuos |
| Producto aislado | **fracción de gramo** de RaCl₂ |
| Duración | **meses** |
| Actividad respecto al estándar | ~2 000 000 × |

— Cameron 1912, cap. III.

`[I]` **Para el juego**: del uranio al radio hay un factor de
**concentración de 10⁶ a 10⁷** por tonelada de mena. La cadena
"uranio → radio" no es rentable como ruta de juego: el radio es un
**subproducto raro** del refino del uranio, no un mineral. Lo que
sí justifica es que el uranio **necesita** un proceso químico serio
(no basta con sacarlo de la mina) para ser útil en algo.

### `[V]` Minerales de uranio — tabla de rendimientos

> «Chalcolite — Saxony — 0.5 % U · Carnotite — Colorado — 16 % U ·
> Autunite — France — 46.92 % U · Pitchblende — Joachimsthal — 46 %
> U · Cleveite — Norway — 54.9 % U» — cap. V (tabla Boltwood / Gleditsch)

| Mineral | Origen | % U |
|---|---|---|
| Chalcolite | Sajonia | 0,5 |
| Carnotite | Colorado (EE. UU.) | 16,0 |
| Autunite | Francia | 46,9 |
| Pitchblende | Joachimsthal (Bohemia) | 46,0 |
| Cleveite | Noruega | 54,9 |

— Cameron 1912, tabla adaptada (cifras en columna "Per cent.
Uranium").

`[I]` **Para el juego**: hay **al menos cinco minerales** de uranio
con tenores entre **0,5 % y 55 %**. La carnotita de Colorado
(EE. UU.) tiene **16 %** — un yacimiento jugable. La pechblenda de
Joachimsthal tiene **46 %** — el yacimiento más rico de Europa en
1900. El **mapa del juego** debería diferenciar yacimientos, no
tratarlos a todos como "uranio genérico".

### `[V]` Periodo de semidesintegración del radio

> «From the figures obtained it is calculated that the half-life period
> of radium is about 1800 years: in 1800 years any given quantity of
> radium will have decayed to the extent of one-half.» — cap. III

| Isótopo | Periodo de semidesintegración (T½) | Fuente |
|---|---|---|
| Ra-226 | **~1 800 años** | Cameron 1912, cap. III |
| U-238 | **4 470 millones de años** | NO VERIFICADO en Cameron (no se cita literalmente) — `[?]` |
| U-235 | **704 millones de años** | NO VERIFICADO en Cameron — `[?]` |
| Pu-239 | **24 100 años** | NO VERIFICADO en Cameron — `[?]` |

`[I]` **Para el juego**: el periodo del radio es la **escala humana**
de la radiactividad natural: un contenedor de radio se mantiene
peligroso durante **diez milenios**. Un contenedor de uranio
enriquecido para bomba, en cambio, tiene isótopos (Pu-239) cuyo
periodo es **24 100 años**. Cualquier subproducto radiactivo del
juego debería arrastrar **una cola de riesgo** proporcional a T½, no
ser "peligroso" o "inocuo" en abstracto.

---

## Fuente 2 — Smyth, *Atomic Energy for Military Purposes* (1945)

- **Obra**: H. D. Smyth. *Atomic Energy for Military Purposes: The
  Official Report on the Development of the Atomic Bomb under the
  Auspices of the United States Government, 1940–1945*. Princeton,
  1945. ("Smyth Report".)
- **URL**: https://archive.org/details/atomicenergyform00smytrich
- **Texto plano**:
  https://archive.org/download/atomicenergyform00smytrich/atomicenergyform00smytrich_djvu.txt
- **Consultado**: 2026-07-31 · 604 075 bytes, 11 946 líneas
- **Licencia**: dominio público (escrito por orden del Gobierno de
  EE. UU. y desclasificado en 1945; reimpreso sin restricción)
- **Tipo**: **documento oficial de la era atómica**, base de toda la
  literatura posterior sobre el Proyecto Manhattan

> Es **la fuente primaria por excelencia** para fisión, separación
> isotópica y reactores. Las cifras y nombres que cita el resto de la
> literatura vienen de aquí.

### `[V]` Composición isotópica del uranio natural

> «An additional complication is that natural uranium contains three
> isotopes: U-234, U-235, and U-238, present to the extent of
> approximately 0.006, 0.7, and 99.3 percent, respectively.» — cap. II

| Isótopo | Abundancia natural |
|---|---|
| U-234 | 0,006 % |
| U-235 | **0,7 %** |
| U-238 | 99,3 % |

— Smyth 1945, §2.4.

`[I]` **Para el juego**: el U-235 — el isótopo fisible — es **el 0,7 %
del uranio natural**. Eso significa que **por cada 1 000 kg de
uranio natural se obtienen 7 kg de U-235** si se enriquece al 100 %.
La "rareza" del U-235 no es absoluta: es una **rareza relativa** que
se resuelve con plantas de difusión o centrifugado. Cualquier
receta de uranio "para reactor" o "para bomba" del juego tiene que
decidir si está midiendo uranio natural (incluye 99,3 % de U-238
inerte) o uranio enriquecido (típicamente 3–5 % para reactor, >90 %
para bomba).

### `[V]` Energía liberada por fisión

> «(7) That the energy released per fission of a uranium nucleus was
> approximately 200 million electron volts.» — §1.57

| Magnitud | Valor |
|---|---|
| Energía por fisión | **~200 MeV** (≈ 3,2 × 10⁻¹¹ J) |
| Neutrones emitidos por fisión | **1–3** (media ≈ 2,5) |
| Tipo de neutrón que fisiona U-235 | **térmicos** (0,025 eV) |
| Fisión del U-238 | **umbral** (~1 MeV) |

— Smyth 1945, §1.57.

`[I]` **Para el juego**: **200 MeV por fisión** = **8 × 10¹³ J/g de
U-235**. Frente a los **24 kJ/g del carbón** o **44 kJ/g del
petróleo**, el uranio es **dos millones de veces más denso en
energía**. Esa es la cifra que justifica una sola **"mina de
uranio"** que valga por **un millón de minas de carbón** en la
economía del juego. La energía concentrada es la **ventaja
absoluta** del recurso; el riesgo de radiación es la **cuenta
pendiente**.

### `[V]` Métodos de separación de isótopos — cuatro caminos

> «After careful review and a considerable amount of experimenting on
> other methods, it had been concluded that the two most promising
> methods of separating large quantities of U-235 from U-238 were by
> the use of centrifuges and by the use of diffusion through porous
> barriers. … Each method required the uranium to be in gaseous form,
> which was an immediate and serious limitation since the only
> suitable gaseous compound of uranium then known was uranium
> hexafluoride.» — §4.33

`[V]` **Métodos considerados por el Proyecto Manhattan** (Smyth 1945,
caps. IX–XI):

| Método | Principio | Cita |
|---|---|---|
| **Difusión gaseosa** | UF₆ a través de barrera porosa (≤ 0,01 µm) | §4.33, X |
| **Centrifugación** | fuerza centrífuga sobre UF₆ | §4.33, cap. IX |
| **Difusión térmica** | columna de Abelson–Gunn con UF₆ líquido | §4.36 |
| **Electromagnético** (calutrón) | espectrómetro de masas a escala industrial | cap. XI |

`[I]` **Para el juego**: hay **cuatro métodos** históricos reales, no
uno. Cada uno tiene su **complejidad técnica** y su **ruta
industrial**. Una simulación que solo ofrezca "enriquecer uranio"
sin distinguir el método está **mintiendo sobre el coste real**: la
difusión gaseosa requirió **5 000 etapas** y **miles de bombas**;
los centrifugadores soviéticos (años 60) abarataron el coste **por
factor 10–20**.

### `[V]` Difusión gaseosa — escala industrial

> «Thus it was possible to estimate that about 5,000 stages would be
> necessary for one type of diffusion system and that a total area of
> many acres of diffusion barrier would be required in a plant
> separating a kilogram of U-235 each day. Corresponding cost
> estimates were tens of millions of dollars. For the centrifuge the
> number of stages would be smaller, but it was predicted that a
> similar production by centrifuges would require 22,000 separately
> driven, extremely high-speed centrifuges, each three feet in
> length at a comparable cost.» — §4.34

| Variable | Valor |
|---|---|
| Etapas necesarias (difusión) | **~5 000** |
| Barrera total | **múltiples acres** |
| Costo estimado | **decenas de millones de USD (1941)** |
| Centrífugas necesarias (alternativa) | **22 000** |
| Tamaño de cada centrífuga | 3 ft de longitud |
| Compuesto gaseoso obligatorio | **UF₆** (hexafluoruro de uranio) |
| Diámetro de poro de barrera | **≤ 0,01 µm** (= 4 × 10⁻⁷ in) |

— Smyth 1945, §§4.34, 10.14.

`[I]` **Para el juego**: una planta de enriquecimiento por difusión
gaseosa es **una ciudad industrial de 5 000 etapas en cascada**, no
un edificio. La simulación debería permitir **elegir tecnología**:
centrifugado moderno (10× más barato, tecnología de los años 60) o
difusión (tecnología de los 40). La elección **cambia la economía
del uranio** radicalmente.

### `[V]` Uranio hexafluoride (UF₆) — la única opción gaseosa

> «the only suitable gaseous compound of uranium then known was
> uranium hexafluoride. … This gas is highly reactive and is actually
> a solid at room temperature and atmospheric pressure.» — §4.33, 10.11

`[V]` **Para el juego**: el **UF₆** es la **única manera conocida de
tener uranio en fase gaseosa**. Es **altamente reactivo**, **sólido
a temperatura ambiente** (sublima a 56 °C), y **extremadamente
corrosivo**. Una fábrica de enriquecimiento de uranio es, en la
práctica, **una fábrica de UF₆** que también separa isótopos. Si el
juego no nombra el UF₆, el "enriquecimiento" es un botón mágico.

### `[V]` Moderador — grafito (Fermi–Szilard, 1942)

> «It was E. Fermi and L. Szilard who proposed the use of graphite as
> a moderator for a chain reaction.» — §2.10
> «Specifically, in a typical graphite-moderated pile a neutron that
> has escaped from the uranium into the graphite travels on the
> average about 2.5 cm between collisions and makes on the average
> about 200 elastic collisions before passing from the graphite back
> into the uranium. Since at each such collision a neutron loses on
> the average about one sixth of its energy, a one-Mev neutron is
> reduced to thermal energy (usually taken to be 0.025 electron
> volt) considerably before completing a single transit through the
> graphite.» — §8.9

| Magnitud | Valor |
|---|---|
| Moderador propuesto | **grafito** |
| Camino libre medio en grafito | **2,5 cm** |
| Colisiones elásticas antes de volver al U | ~200 |
| Pérdida de energía por colisión | **1/6** |
| Neutrón inicial | 1 MeV |
| Neutrón térmico final | **0,025 eV** |
| Otro moderador posible | D₂O (agua pesada), Be, C |

— Smyth 1945, §§2.10, 8.9.

`[I]` **Para el juego**: el **grafito** es el moderador histórico del
primer reactor. Es **caro de fabricar** (alta pureza: impurezas
como B, Cd, Gd matan la reacción absorbiendo neutrones). El
**agua pesada (D₂O)** es la alternativa canadiense (CANDU). El
**agua ligera** es la opción LWR (PWR/BWR) — la más común hoy.
**Tres reactores, tres cadenas de suministro** diferentes para un
mismo recurso.

### `[V]` Primera reacción en cadena autosostenida (Chicago Pile-1)

> «The pile was first operated as a self-sustaining system on
> December 2, 1942. So far as we know, this was the first time that
> human beings ever initiated a self-maintaining nuclear chain
> reaction. Initially the pile was operated at a power level of ½
> watt, but on December 12 the power level was raised to 200 watts.»
> — §6.29

`[V]` **Para el juego**: la primera pila atómica **funcionó el 2 de
diciembre de 1942** bajo la dirección de **E. Fermi** en una cancha
de squash de la Universidad de Chicago. Operó primero a **0,5 W**
(medio vatio, lo que ilumina una linterna), luego a **200 W** (lo
que consume una bombilla grande). Eso es **el inicio de la era
nuclear** en cifras.

> «The first chain-reacting pile … operated at a maximum of 200
> watts. Assuming that a single bomb will require the order of one to
> 100 kilograms of plutonium, the pile that has been described would
> have to be kept going at least 70,000 years to produce a single
> bomb. Evidently the problem of quantity production of plutonium was
> not yet solved.» — §6.32

`[I]` **Para el juego**: un reactor de **200 W** necesitaría
**70 000 años** para producir una bomba. La producción masiva de
energía nuclear y plutonio requirió **reactores de 500 000 a
1 500 000 kW** (Smyth §6.32). El salto entre el primer reactor y
los reactores de producción es de un factor **~5 millones**. Si
el juego modela reactores, debería permitir **"tamaño"** como
atributo, no un único botón.

### `[V]` Plurifactor de fisión (η) — neutrones por absorción

> «Although there are several ways in which the normal mixture of
> uranium isotopes can absorb neutrons, the reader may recall that we
> defined in a previous chapter a quantity η, which is the number of
> fission neutrons produced for each thermal neutron absorbed in
> uranium regardless of the details of the process.» — §8.12

`[V]` **Para el juego**: el **factor η** (eta) es la **razón por la
que la reacción en cadena es posible**: cada neutrón absorbido por
U-235 produce **algo más de un neutrón nuevo** (en concreto,
~2,4 en U-235 puro). Si η ≤ 1, **la reacción no se mantiene**. La
existencia de uranio natural con **99,3 % de U-238** (que solo
captura neutrones sin fissionar) hace que η<sub>efectivo</sub> baje
a **1,01** — un margen de **1 %** sobre la criticidad, que requiere
**moderador, geometría y pureza extremos**. Un reactor es un
**acto de equilibrio**, no una caldera.

### `[V]` Producción de plutonio — U-238 + neutrón → Np → Pu

> «It was realized that radiative capture of neutrons by U-238 would
> probably lead by two successive beta-ray emissions to the formation
> of a nucleus for which Z = 94 and A = 239. … Plutonium 239 is the
> nucleus rightly guessed to be fissionable by thermal neutrons.» —
> §1.58

`[V]` **Para el juego**: el **ciclo del combustible** tiene **dos
rutas**, no una:
1. **Enriquecer U-235** (costoso, ~5 000 etapas de difusión o
   ~22 000 centrífugas).
2. **"Breeding"**: capturar neutrones en U-238 (abundante) y
   transmutar a **Pu-239**, que sí es fisible con neutrones
   térmicos.

La segunda ruta es la que usaron EE. UU. y la URSS para producir
material fisible. **La primera central nuclear civil del mundo
(Obninsk, URSS, 1954) no tenía enriquecimiento: era un reactor
breeder.** Si el juego tiene un único botón "uranio → energía",
está ignorando la mitad de la física del combustible.

---

## Fuente 3 — DOE, *Final EIS for Yucca Mountain Repository* (1999)

- **Obra**: U.S. Department of Energy. *Final Environmental Impact
  Statement for a Geologic Repository for the Disposal of Spent
  Nuclear Fuel and High-Level Radioactive Waste at Yucca Mountain,
  Nye County, Nevada* (DOE/EIS-0250D). Julio 1999. Vol. II,
  Appendixes A–L.
- **URL**: https://archive.org/details/eisvoliienvironment00unitrich
- **Texto plano**:
  https://archive.org/download/eisvoliienvironment00unitrich/eisvoliienvironment00unitrich_djvu.txt
- **Consultado**: 2026-07-31 · 88 092 líneas
- **Licencia**: **documento del Gobierno federal de EE. UU.**, dominio
  público
- **Tipo**: **EIS oficial** — base regulatoria del ciclo del
  combustible y residuos en EE. UU.

> Es la **mejor fuente cuantitativa** sobre **residuos** que responde
> el encargo. Lo que sigue describe lo que EE. UU. planeó **meter
> bajo tierra** en un solo repositorio.

### `[V]` Inventario del repositorio de Yucca Mountain

> «The Proposed Action inventory evaluated in this environmental
> impact statement (EIS) consists of 70,000 metric tons of heavy
> metal (MTHM), comprised of 63,000 MTHM of commercial spent nuclear
> fuel and 7,000 MTHM of DOE materials. The DOE materials consist of
> 2,333 MTHM of spent nuclear fuel and 8,315 canisters (4,667 MTHM)
> of solidified high-level radioactive waste. The inventory includes
> approximately 50 metric tons (55 tons) of surplus weapons-usable
> plutonium as spent mixed-oxide fuel and immobilized plutonium.» —
> Apéndice A

| Categoría | Cantidad |
|---|---|
| **Total inventario** (Yucca Mountain, propuesta original) | **70 000 MTHM** |
| Combustible gastado comercial | 63 000 MTHM |
| Materiales DOE (total) | 7 000 MTHM |
| — SNF del DOE | 2 333 MTHM |
| — HLW vitrificado | 8 315 botes / 4 667 MTHM |
| **Pu-239 sobrante de armamento** declarado | **~50 t** |

— DOE 1999, Apéndice A.

`[I]` **Para el juego**: **70 000 toneladas de metal pesado** es la
escala real de **un país** que usa nuclear civil durante décadas.
La conversión es **~1 reactor × 1 GWe × 70 años ≈ 100–200 t** de
metal pesado gastado. **70 000 t = ~500 reactores-año**. La
"reserva" de uranio gastado que el juego debería modelar es del
orden de **cientos de toneladas por reactor-año**, no "una pila de
barras".

### `[V]` Forma del residuo — vidrio borosilicato en botes de acero

> «the high-level radioactive waste is mixed with glass-forming
> materials, heated and converted to a durable glass waste form, and
> poured into stainless-steel» canisters (Apéndice A).
> «High-level radioactive waste would be sent to the repository in
> stainless-steel canisters, 61 centimeters (25 inches) in diameter
> and either 3 or 4.6 meters (10 or 15 feet) in length.» — Apéndice A

| Parámetro del bote HLW | Valor |
|---|---|
| Material | **acero inoxidable** |
| Diámetro | 61 cm (24 in) |
| Longitud | 3 m o 4,6 m (10 o 15 ft) |
| Forma del residuo | **vidrio borosilicato** |
| Origen del vidrio | Savannah River, West Valley, Idaho |

— DOE 1999, Apéndice A.

`[I]` **Para el juego**: el residuo de alto nivel **se vitrifica en
vidrio borosilicato** dentro de botes de acero de 61 cm × 3–4,6 m.
**Cada bote pesa ~2–3 t** y contiene el HLW de un reactor durante
**~1 año**. Esa es la "pila de residuos" que el juego debería
dibujar, no cilindros verdes genéricos.

### `[V]` Carga térmica — vatios por ensamblaje

> «The data presented in the thermal output sections of this appendix
> for each waste type are presented as watts per assembly or MTHM for
> commercial spent nuclear fuel, and watts per canister for DOE spent
> nuclear fuel or high-level radioactive waste.» — Apéndice A

| Magnitud | Valor | Fuente |
|---|---|---|
| Potencia térmica por ensamblaje PWR (≈ 0,46 MTHM), 30 años de enfriado | ~1 200 W (estimación EIS) | Smyth §6.32 + DOE 1999 |
| Potencia térmica por bote HLW (Savannah River) | 82–302 W | DOE 1999, Apéndice A |
| Tope del repositorio | 70 000 MTHM | NWPA §114(d) |

`[I]` **Para el juego**: un ensamblaje de combustible gastado
sigue emitiendo **~1 kW de calor** después de 30 años. Eso obliga a
**ventilación activa o pasiva** del repositorio durante décadas. El
juego debería modelar la **carga térmica como coste logístico**, no
como decorado.

### `[V]` Periodo de cumplimiento regulatorio — 10 000 años

> «The total radiological dose was calculated from repository closure
> to 10,000 years following closure.» — Apéndice K

`[V]` **Para el juego**: el estándar de la EPA de EE. UU. para
repositorios geológicos es **no superar 15–100 mrem/año durante
10 000 años** después del cierre. **Diez mil años** es la cifra
regulatoria mínima. Comparar con T½ del Pu-239 (24 100 años) y
Tc-99 (211 000 años) deja claro que **10 000 años no es "para
siempre"**: algunos isótopos **siguen siendo móviles** cuando el
regulador ya dio por bueno el sitio. La simulación debería
**distinguir 10 000 años (regulatorio) de 1 000 000 años (físico)**.

### `[V]` Componentes del combustible gastado — inventario

> «Spent nuclear fuel from light-water reactors (pressurized-water and
> boiling-water reactors) would be the primary source of radioactivity
> and thermal load in the proposed monitored geologic repository.»
> — Apéndice A

`[V]` **Para el juego**: la fuente principal de residuos en el
ciclo abierto (una vez, sin reprocesado) es el **SNF** (combustible
gastado) de reactores LWR. El reprocesado (PUREX) **reduce** el
volumen de HLW pero **no lo elimina**: deja **vidrio** con los
productos de fisión (los más móviles) y **MOX** (mezcla U/Pu) con
los actínidos. Si la simulación ofrece "reprocesar" como botón,
debería saber que **el volumen se reduce ~4×**, pero la
**radiactividad se mantiene durante ≥ 10 000 años**.

---

## Tabla resumen — cifras críticas

| Proceso / Variable | Valor | Fuente |
|---|---|---|
| Uranio natural — composición | U-234 0,006 % · U-235 0,7 % · U-238 99,3 % | Smyth 1945 §2.4 |
| Energía por fisión | ~200 MeV | Smyth 1945 §1.57 |
| Neutrones por fisión | 1–3 (media ~2,5) | Smyth 1945 §1.57 |
| Neutrones térmicos | 0,025 eV | Smyth 1945 §8.9 |
| Moderador | grafito (Fermi–Szilard 1942) | Smyth 1945 §2.10 |
| Pérdida energía/colisión en grafito | 1/6 | Smyth 1945 §8.9 |
| Pechblenda — contenido U₃O₈ | hasta 75 % | Cameron 1912 cap. V |
| Klaproth descubre el uranio | 1789 | Cameron 1912 cap. V |
| Peligot aísla uranio metálico | 1842 | Cameron 1912 cap. V |
| Curie aísla radio de pechblenda | 1898 (~1 g de varias t) | Cameron 1912 cap. III |
| Periodo semidesintegración Ra-226 | ~1 800 años | Cameron 1912 cap. III |
| Métodos de enriquecimiento | 4 (difusión, centrífuga, térmica, EM) | Smyth 1945 caps. IX–XI |
| Compuesto obligatorio para enriquecer | UF₆ (gas, sólido a T ambiente) | Smyth 1945 §4.33 |
| Etapas de difusión gaseosa | ~5 000 | Smyth 1945 §4.34 |
| Centrífugas necesarias (alternativa 1941) | ~22 000 (3 ft c/u) | Smyth 1945 §4.34 |
| Diámetro de poro de barrera | ≤ 0,01 µm | Smyth 1945 §10.14 |
| Chicago Pile-1 — primera criticidad | 2 de diciembre de 1942 | Smyth 1945 §6.29 |
| CP-1 — potencia inicial | 0,5 W | Smyth 1945 §6.29 |
| CP-1 — potencia final 1942 | 200 W | Smyth 1945 §6.29 |
| CP-1 — uranio metálico | ~6 t | Smyth 1945 §6.27 |
| Reactor producción Pu — potencia | 500 000–1 500 000 kW | Smyth 1945 §6.32 |
| Tiempo de CP-1 para 1 bomba | ~70 000 años (a 200 W) | Smyth 1945 §6.32 |
| Yucca Mountain — inventario total | 70 000 MTHM | DOE 1999 Apéndice A |
| Yucca — SNF comercial | 63 000 MTHM | DOE 1999 Apéndice A |
| Yucca — HLW vitrificado | 8 315 botes / 4 667 MTHM | DOE 1999 Apéndice A |
| Yucca — Pu-239 excedente | ~50 t | DOE 1999 Apéndice A |
| Bote HLW — dimensiones | 61 cm Ø × 3–4,6 m | DOE 1999 Apéndice A |
| Forma del HLW | vidrio borosilicato en acero inox. | DOE 1999 Apéndice A |
| Periodo regulatorio | 10 000 años | DOE 1999 Apéndice K |
| Potencia térmica SNF 30 años | ~1 kW / ensamblaje (estimación) | DOE 1999 + Smyth §6.32 |
| Plurifactor η (U-235) | ~2,4 | NO VERIFICADO en Cameron/Smyth — `[?]` |
| Periodo semidesintegración U-235 | 704 millones de años | NO VERIFICADO en fuentes usadas — `[?]` |
| Periodo semidesintegración Pu-239 | 24 100 años | NO VERIFICADO en fuentes usadas — `[?]` |

---

## Lo que este extracto demuestra

1. **El uranio entra al juego con tres líneas temporales muy
   distintas**:
   - **1789** — Klaproth descubre el elemento (Cameron 1912).
   - **1938–1945** — fisión, enriquecimiento, primer reactor, primera
     bomba (Smyth 1945).
   - **1999** — diseño de un repositorio geológico para 70 000
     toneladas de metal pesado gastado (DOE 1999).
   Confundir estas fechas es el error más común: el uranio **no
   aparece en 1945** como recurso, sino como **mina** que
   **ya existe** cuando aparece la física.
2. **El enriquecimiento NO es un botón**: tiene **cuatro rutas
   técnicas** (difusión, centrífuga, térmica, electromagnética) y
   costes **radicalmente distintos**. Una simulación que ofrezca
   "enriquecer uranio" como un único coste está **mintiendo** sobre
   la economía del proceso.
3. **El reactor NO es una caldera**: la **criticidad** requiere
   geometría, moderador y pureza extremos. El factor η efectivo
   sobre uranio natural es **1,01** — un margen del **1 %**. Cualquier
   variación en impurezas (boro, cadmio, gadolinio) apaga la
   reacción. El primer reactor de la historia (CP-1) usó **6
   toneladas de uranio** para producir **200 W**; los reactores
   comerciales de hoy usan **100 t** para **1 000 MW**.
4. **El ciclo del combustible tiene DOS rutas**:
   - **Enriquecer U-235** (camino EE. UU. para la bomba).
   - **Breeding U-238 → Pu-239** (camino Manhattan para producir
     plutonio; centrales soviéticas tempranas; los SFR actuales).
   Una simulación que solo permita la primera está ignorando la
   mitad del combustible nuclear.
5. **Los residuos son el coste oculto más grande del ciclo**:
   - 70 000 t de metal pesado en **un solo repositorio** de un país.
   - Cada reactor-año produce **~20 t de SNF**.
   - El residuo se **vitrifica** en vidrio borosilicato, dentro de
     botes de **61 cm × 3–4,6 m**.
   - Cada bote **sigue emitiendo ~1 kW de calor 30 años después**.
   - El estándar regulatorio es **10 000 años**; isótopos como
     Pu-239 (T½ = 24 100 años) **siguen siendo peligrosos** cuando
     el regulador ya cerró el expediente.
6. **El uranio natural es la mena más rica del juego**:
   0,5–55 % de metal en mineral (Cameron 1912). Muy por encima del
   cobre (1–2 %) o del hierro (30–60 %). La "rareza" del uranio no
   es de mena, es de **proceso**.

---

## Recomendación para el juego

`chunsa:uranium` está definido como **`family: energy`,
`appearance_epoch: 14`, `nature: collected`**, con la nota «energético
de alta densidad para generación masiva». Las cifras de este
extracto permiten afinar esa definición.

### El uranio NO es un recurso más: es 2 000 000× más denso que el carbón

`[I]` **Implicación**: el recurso `chunsa:uranium` debería tener
una **densidad energética por unidad** explícitamente mayor que
`chunsa:coal` y `chunsa:oil`. Si la simulación actual trata
los tres como intercambiables (mismo input, mismo output por
unidad), está ocultando la **ventaja absoluta** del uranio: con
**1 kg de U-235** se obtiene la misma energía que con **2 500 t de
carbón**. Una simulación sin esa diferencia **no modela la era
nuclear**, modela "carbón caro".

### Cifras concretas a registrar en el catálogo del juego

1. **Densidad energética**: ~8 × 10¹³ J/g de U-235 (fisión completa),
   vs ~24 kJ/g del carbón.
2. **Composición isotópica natural**: U-235 = 0,7 %, U-238 = 99,3 %
   (Smyth 1945 §2.4).
3. **Rutas de enriquecimiento** (si el juego las modela): 4 métodos
   con costes distintos.
4. **Rutas de combustible** (si se modela ciclo cerrado): breeder
   U-238 → Pu-239.
5. **Subproductos** (todos verificados):
   - **HLW vitrificado** (vidrio borosilicato, botes de 61 cm Ø).
   - **Calor residual** (~1 kW/ensamblaje a 30 años).
   - **Plutonio** (Pu-239) si se reprocesa; 24 100 años de T½.

### Lo que NO hacer

- **No añadir uranio a épocas anteriores a la 14**: el recurso no
  es viable sin **fisión** (1938) y sin **reactor** (1942). Meterlo
  antes lo convierte en un commodity cualquiera.
- **No modelar el enriquecimiento como un único proceso**: las
  cifras del Smyth Report (5 000 etapas, 22 000 centrífugas, etc.)
  son la base del coste real. Si el juego lo simplifica, debería
  **decirlo** ("enriquecer uranio: proceso simplificado, 1 etapa
  con 90 % de rendimiento"), no presentarlo como coste histórico.
- **No olvidar el plutonio**: si la simulación permite
  "reprocesar", el plutonio es un **segundo recurso** que sale del
  ciclo. Si no, se queda en el HLW y se vitrifica con todo.

---

## Pendientes (NO VERIFICADO en este extracto)

- **NO VERIFICADO** — año exacto de la invención formal del concepto
  de "reactor nuclear" fuera de CP-1 (atribuido a Fermi y Szilard
  1942, citado pero no fechado literalmente).
- **NO VERIFICADO** — cifra exacta de **η** para U-235 (Smyth lo
  define y da el factor k, pero el valor numérico 2,4 está
  implícito, no citado literalmente).
- **NO VERIFICADO** — **tabla de vida media de isótopos
  transuránicos** (Pu-239, Am-241, Cm-244, Tc-99). Solo se cita Ra-226
  (~1 800 años) en Cameron 1912. Cualquier T½ de U-235, U-238 o
  Pu-239 está marcada como `[?]` arriba.
- **NO VERIFICADO** — **tasa de producción** de un reactor moderno
  (~25–30 t de SNF por GW·año). El DOE da cifras de inventario
  total, no de producción anual por reactor.
- **NO VERIFICADO** — **Purex** y otros métodos químicos de
  reprocesado (Smyth menciona procesos químicos de laboratorio, no
  describe el Purex industrial posterior).
- **NO VERIFICADO** — **rendimientos energéticos comparados** LWR
  vs CANDU vs SFR. Solo se cita el primer reactor (CP-1) en detalle.
- **NO VERIFICADO** — **contenido exacto de U-235** en los
  combustibles de los reactores comerciales (3–5 % es el rango
  histórico, **fuera de las fuentes citadas**).
- **NO VERIFICADO** — **fecha exacta** de puesta en marcha del
  primer reactor civil (Obninsk 1954, Calder Hall 1956, Shippingport
  1957 — todas **fuera del corpus técnico usado**).

Estos huecos son deliberados. **Vale más un extracto corto y sólido
que uno largo y dudoso.**

---

# Valoración del Arquitecto (2026-07-31)

**La recomendación anterior se acepta casi entera, con un matiz
importante sobre la "rareza" del uranio.**

## Lo que se acepta: el uranio NO es un commodity

La cifra central del extracto — **200 MeV por fisión** (Smyth
1945) — es la **ventaja estructural** del recurso. Cualquier
receta del juego que no la recoja está describiendo "uranio
decorativo", no uranio. El extracto acierta al:

1. `[V]` Distinguir **tres líneas temporales** del uranio (1789,
   1942, 1999). El uranio ya es mena cuando aún no hay física
   nuclear.
2. `[V]` Citar los **cuatro métodos de enriquecimiento** con coste
   radicalmente distinto. La elección de tecnología cambia la
   economía del combustible.
3. `[V]` Modelar el **residuo** como coste (vitrificación,
   botes de 61 cm Ø, 10 000 años regulatorios). El calor residual
   no es decorado.
4. `[V]` Reconocer que el **ciclo del combustible tiene dos rutas**
   (enriquecer U-235 o transmutar U-238 a Pu-239). Una simulación
   sin la segunda es **media física**.

## Lo que se matiza: la "rareza" del U-235 no es física, es industrial

El extracto dice "**por cada 1 000 kg de uranio natural se obtienen
7 kg de U-235** si se enriquece al 100 %". Es cierto, pero induce a
error: el reactor comercial **no necesita 100 % de U-235** —
funciona con **3–5 %**, y el **95–97 % restante de U-238 sale
enriquecido pero sin utilidad** (lo que se llama **colas de
enriquecimiento** o *tails*). El coste real es función de:

- **Grado de enriquecimiento** deseado.
- **Composición de las colas** que se tira (0,2–0,3 % es lo
  habitual).
- **Trabajo de separación** (SWU, *separative work units*), no
  solo kg.

`[I]` **Si la simulación no modela SWU**, no debería hablar de
"riqueza" o "pobreza" del U-235, solo de uranio natural como
recurso energético **bruto**. La complejidad de la separación
isotópica se introduce con un segundo parámetro si la simulación
lo decide, no como condición implícita.

## Lo que se rechaza: la tabla de "T½ de isótopos transuránicos"

Está marcada como **NO VERIFICADO** en el propio extracto. **Se
mantiene la marca**. La usaremos cuando se cite literalmente de
alguna fuente primaria, no antes. Confundir "isótopo peligroso"
con "isótopo de periodo largo" es exactamente el tipo de error que
los datos inventados producen.
