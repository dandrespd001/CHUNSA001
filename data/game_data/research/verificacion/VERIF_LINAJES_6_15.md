# Verificación factual — linajes de las épocas 6-15 (Sprint 1.25)

Verificador: Arquitecto (Claude Opus 5) · Fecha: 2026-07-31
Motivo: el marco de 15 épocas necesita 20 celdas (10 épocas × 2 linajes). Este
informe separa **lo que está verificado** de **lo que es andamiaje**, para que
nadie confunda una cosa con la otra al leer el catálogo.

## Lo primero: la investigación delegada NO cuenta como verificación

Se encargó la búsqueda de los 20 anclajes históricos a un modelo externo. Su
entrega abre con este aviso, textual:

> «No tengo acceso a navegación web en tiempo real, por lo que **no puedo
> transcribir literalmente** pasajes de URLs ni fijar una "fecha de consulta"
> fiable. […] las entradas marcadas **[V]** indican que el dato está documentado
> en la fuente enlazada **según mi recuerdo del artículo**; debes cotejar el
> texto exacto en cada URL antes de incluirlo en el proyecto.»

Es la respuesta correcta y conviene reconocerlo: **prefirió declarar el límite
antes que fabricar 36 citas creíbles**, que es exactamente el fallo que este
proyecto ya cazó una vez. Pero la consecuencia es firme: sus 36 `[V]` son
**pistas de investigación, no procedencia**. La regla del proyecto —cita
literal + URL + fecha— no se cumple con un recuerdo.

Su fichero se conserva como lo que es: una lista de hipótesis a cotejar.

## Lo que SÍ verifiqué, con cita literal

**Actualizado 2026-07-31 (Sprint 1.26): nueve** anclajes cotejados directamente contra la fuente el **2026-07-31**:

| Época | Linaje | Anclaje | Cita literal |
|---|---|---|---|
| 7 | Italia | Arsenal de Venecia | «Construction of the Arsenal began around 1104»; «At the peak of its efficiency in the early 16th century, the Arsenal employed some 16,000 people who apparently were able to produce nearly one ship each day» |
| 7 | Egipto | Ṭirāz fatimí | «the institution of tiraz flourished in the Islamic world under the patronage of the Abbasid and Fatimid caliphs from the late 9th century to the 13th» |
| 8 | Egipto | Khan el-Khalili | «During Barquq's first reign (1382–1389) his Master of the Stables (amir akhur), Jaharkas al-Khalili, demolished the Fatimid mausoleum […] to erect a large khan»; los mercaderes «could store their goods» en el patio interior |
| 13 | Italia | Lingotto (Fiat) | «a 1,500-metre […] long test track» con «spiral concrete access ramps at each end»; los coches se construían «on a line that went up through the building» |
| 6 | Egipto | Nilómetro | «the quality of the year's flood was used to determine the levels of tax to be paid»; el de Roda se erigió en 715 bajo dominio omeya |
| 8 | Italia | Vidriería de Murano | «A law dated November 8, 1291 confined most of Venice's glassmaking industry to the "island of Murano"»; el cristallo es «a soda glass, created during the 15th century» |
| 12 | Egipto | Canal de Suez | abrió el «17 November 1869» tras obras iniciadas el 25 de abril de 1859; mide «193.30-kilometre-long» |
| 12 | Italia | Pirelli | fundada el «January 28, 1872» por «Giovanni Battista Pirelli» en Milán; «initially specialised in rubber and derivative processes» |
| 14 | Egipto | Presa de Asuán | 1960-1970, «to control flooding, increase water storage for irrigation, and generate hydroelectricity»; «12x175 MW» |

URLs:
- https://en.wikipedia.org/wiki/Venetian_Arsenal
- https://en.wikipedia.org/wiki/Tiraz
- https://en.wikipedia.org/wiki/Khan_el-Khalili
- https://en.wikipedia.org/wiki/Lingotto

Dos de ellos encajan además con mecánicas que ya existen: el **ṭirāz** produce
lino, lana, algodón y seda —los cuatro textiles del catálogo desde el 1.9C— y el
**khan** es literalmente un punto de acopio, que es el `kind: dropoff` del
juego. No se forzó: se eligieron anclajes cuya función real ya tenía verbo en
el juego.

## Lo que es ANDAMIAJE, y por qué se deja dicho

### El arco que apareció solo

Merece quedar escrito porque no se planeó. El **nilómetro** de la época 6 mide
la crecida del Nilo para prever la cosecha y fijar el impuesto; la **presa de
Asuán** de la época 14 gobierna esa misma crecida para regar y generar
electricidad. Ocho épocas separan a los dos y el problema es el mismo, que es
justamente el verbo de juego declarado de la civilización
(`egipto:coordinate_flood`).

No hubo que inventar el hilo didáctico que pidió el Director: estaba en la
historia de la civilización elegida, y sólo había que no estropearlo.

### Lo que sigue siendo andamiaje

Las **11 celdas restantes** llevan un edificio genérico por época, con
`evidence: N` —sin fuentes, sin informe— porque **no afirman nada histórico**.
Existen por una razón mecánica declarada: `ADVANCE_EPOCH` exige dos edificios
en la ventana actual, así que sin ellos la civilización queda atrapada y las
épocas 6-15 vuelven a ser filas de una tabla.

`evidence: N` es la marca honesta para esto. La alternativa —ponerles nombre
propio con las pistas sin cotejar— habría llenado el catálogo de datos con
aspecto de verificados. **Un hueco declarado vale más que un dato dudoso**, y
esa es la misma regla con la que se cerró el corpus de 18 temas.

Los periodos jugables nuevos usan el **rango de años de la propia época**
(SPEC-007 §2), que es un dato de diseño del proyecto, y no el de una dinastía
concreta, que sería una afirmación histórica sin cotejar.

## Trabajo pendiente, nombrado

Cotejar las 11 celdas restantes contra fuente y ascenderlas de andamiaje a
contenido con nombre propio. La lista de hipótesis del modelo externo es un
buen punto de partida —los periodos que propone son plausibles y están bien
elegidos— pero cada una necesita su cita literal antes de entrar.
