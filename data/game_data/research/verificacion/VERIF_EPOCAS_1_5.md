# Verificación factual — culturas de las épocas 1–4 (Sprint 1.22)

Verificador: Arquitecto (Claude Opus 5) · Fecha: 2026-07-31
Motivo: las épocas 1 y 2 no existían para ninguna civilización, y Roma no tenía
nada entre la 1 y la 4. Rellenarlas exige culturas arqueológicas **reales**, no
nombres de ambientación. Cada afirmación que sostiene un dato del juego se
comprueba aquí antes de que el dato entre.

## Resumen

- Afirmaciones revisadas: **12**
- **CONFIRMADAS: 10 · CORREGIDAS: 1 · DISEÑO: 1**
- Veredicto: **APTA**

## Detalle

| # | Afirmación | Veredicto | Fuente |
|---|---|---|---|
| 1 | La cultura Qadan floreció «approximately 15,000 years ago» y persistió «approximately 4,000 years» | **CONFIRMADA** | Wikipedia, *Qadan culture* |
| 2 | Sus sitios van «from the Second Cataract of the Nile to Tushka» | **CONFIRMADA** | íd. |
| 3 | Practicaba «the preparation and consumption of wild grasses and grains», y «grains were not planted in ordered rows» | **CONFIRMADA** | íd. |
| 4 | Se hallaron «grinding stones and blades … with glossy films of silica on them» | **CONFIRMADA** | íd. |
| 5 | El Epigravetiense abarca «~21,000 – 10,000 cal. BP» e incluye Italia | **CONFIRMADA** | Wikipedia, *Epigravettian* |
| 6 | Su utillaje son «microliths, such as backed blades, backed points, and bladelets with retouched end» | **CONFIRMADA** | íd. |
| 7 | Faiyum A es «the earliest farming culture in the Nile Valley», h. 5600–4400 a.C. | **CONFIRMADA** | Wikipedia, *Faiyum A culture* |
| 8 | Las **fosas de almacenamiento comunal** son de Faiyum A | **CORREGIDA** | íd. — ver abajo |
| 9 | La Remedello se fecha en «3400–2400 BCE», Copper Age, valle del Po | **CONFIRMADA** | Wikipedia, *Remedello culture* |
| 10 | Produce «objects in copper and arsenical silver» y tumbas con «arrows, stone daggers and polished stone axes» | **CONFIRMADA** | íd. |
| 11 | La Terramare (c. 1700–1150 a.C.) fundía «bronze in moulds of stone and clay» y cultivaba «beans, grapes, wheat, and flax» | **CONFIRMADA** | Wikipedia, *Terramare culture* |
| 12 | Proporción 3 cobre : 1 estaño de `terramare_bronze_casting`, 260 ticks | **DISEÑO** | Hereda VERIF_BRONCE |

## La afirmación 8, que es la que importa

Iba a llamar al almacén neolítico egipcio **`fayum_silo`** y a documentarlo con
fosas de almacenamiento revestidas de cestería. Al ir a la fuente, el artículo
de Faiyum A **no dice eso**: atribuye las «communal storage pits» a **Merimde**,
otra cultura, y de Faiyum A sólo sostiene que es la agricultura más antigua del
valle del Nilo y que «Near Eastern domesticates were incorporated into a
pre-existing foraging strategy».

Nada de cestería, ni de silos, ni de Faiyum.

El edificio pasó a llamarse **`egipto:merimde_storage`** y la cita atribuye el
almacenamiento a quien la fuente lo atribuye. Es una corrección pequeña y por eso
mismo vale la pena dejarla escrita: era exactamente la forma en que se cuela un
dato falso — un detalle plausible, del periodo correcto, de la región correcta,
colgado del yacimiento equivocado. Nadie lo habría notado nunca dentro del juego.

## Lo que NO se verificó, y por tanto no se afirma

- **No hay dato de rendimiento** (kg por hectárea, cabezas por rebaño) para
  ninguna de las cuatro culturas. Los `amount` de los depósitos del mapa y los
  costes de los edificios son **diseño de balance**, no historia, y así consta en
  cada `rationale`.
- **La lana como «depósito»** es una abstracción declarada: sale de un rebaño, no
  de un yacimiento. Se sustituirá cuando exista el modelo de ganado (Sprint
  1.12). Consta en el comentario del mapa.
- **El nombre de los centros** (`egipto:settlement_center`, `rome:forum_center`)
  se mantiene mientras su ventana se amplía a [1,5]. Un *forum* en el
  Paleolítico es un anacronismo de etiqueta: la deuda queda anotada en
  SPEC-007 §22.3 como tarea de localización por periodo, no se disimula.

## Fuentes localizadas

Todas consultadas el **2026-07-31**:

- https://en.wikipedia.org/wiki/Qadan_culture
- https://en.wikipedia.org/wiki/Epigravettian
- https://en.wikipedia.org/wiki/Faiyum_A_culture
- https://en.wikipedia.org/wiki/Remedello_culture
- https://en.wikipedia.org/wiki/Terramare_culture
