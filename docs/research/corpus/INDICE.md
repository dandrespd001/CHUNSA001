# Corpus de CHUNSA — índice

**Para qué existe**: que una búsqueda se haga **una vez**. Antes de investigar
cualquier cosa, mirar aquí. Si ya está, se usa; si no, se busca, se extrae y se
añade.

**Cómo buscar**: `./buscar.sh archive|openlib|doaj "consulta"`.
Repositorios probados y su estado: `FUENTES.md`.

## Reglas de un extracto

Un fichero de `extractos/` no vale si le falta algo de esto:

1. **URL exacta** y **fecha de consulta**.
2. **Cita literal** del pasaje que sostiene cada cifra. Parafrasear un número y
   perder la frase original es cómo se cuelan los datos inventados.
3. Marca por afirmación: `[V]` verificada · `[?]` **NO VERIFICADO** · `[I]`
   inferencia nuestra.
4. **Licencia o estado de dominio público**, si se copia texto extenso.


## Estado del corpus (2026-07-31)

**17 de 18 temas poblados** · **612 afirmaciones verificadas** · **148
declaradas NO VERIFICADO** · **2705 citas literales**.

| Tema | `[V]` | NO VERIF. | Citas | Fuentes principales |
|---|---:|---:|---:|---|
| agricultura | 21 | 7 | 118 | Archive, PubMed |
| alimentacion | 41 | 12 | 196 | Archive, DOI |
| ancestrales | 32 | 5 | 115 | DOI, OpenEdition, Springer |
| ceramica_vidrio | 31 | 11 | 146 | Archive, DOAJ |
| construccion_hidraulica | 40 | 9 | 216 | Archive |
| economia | 51 | 6 | 194 | Archive |
| electronica | 44 | 9 | 196 | Archive, Wikipedia |
| energia | 50 | 7 | 258 | Archive |
| historia | 23 | 7 | 133 | Archive |
| medicina | 23 | 8 | 63 | Archive, DOAJ |
| metalurgia | 39 | 8 | 147 | Archive |
| militar | 27 | 10 | 120 | Archive |
| mineria | 48 | 7 | 107 | Archive |
| nuclear | 34 | 16 | 130 | Archive |
| quimica | 38 | 7 | 164 | Archive |
| textiles | 45 | 8 | 265 | Archive |
| transporte | 25 | 11 | 137 | Archive |
| **aeronautica** | — | — | — | **pendiente, relanzado** |

**La proporción de NO VERIFICADO importa tanto como la de verificado.** 148
huecos declarados en vez de rellenados con números creíbles es la señal de que
el corpus se puede usar. `nuclear` es el que más tiene (16), lo cual es
coherente: buena parte de sus datos operativos no están en fuentes abiertas.

## Temas

Ampliado el 2026-07-31 con medicina, aeronáutica, transporte y **técnicas
ancestrales**, por directriz del Director: interesa todo lo que **afecte al
desarrollo de la historia**, de las técnicas ancestrales a la manufactura y el
transporte.

| # | Tema | Fichero | Épocas que sirve | Estado |
|---|---|---|---|---|
| 1 | **Técnicas ancestrales** de civilizaciones | `extractos/ancestrales.md` | 1–4 | pendiente |
| 2 | Metalurgia y manufactura | `extractos/metalurgia.md` | 3–13 | **iniciado** |
| 3 | Construcción e hidráulica | `extractos/construccion_hidraulica.md` | 2–15 | pendiente |
| 4 | Energía y motores | `extractos/energia.md` | 5–15 | pendiente |
| 5 | **Transporte** terrestre, naval y ferroviario | `extractos/transporte.md` | 1–15 | pendiente |
| 6 | **Aeronáutica** | `extractos/aeronautica.md` | 11–15 | pendiente |
| 7 | Agricultura y siembra | `extractos/agricultura.md` | 1–15 | pendiente |
| 8 | Química y materiales | `extractos/quimica.md` | 8–15 | pendiente |
| 9 | **Medicina** y veterinaria | `extractos/medicina.md` | 1–15 | pendiente |
| 10 | Militar y estrategia | `extractos/militar.md` | 1–15 | pendiente |
| 11 | Economía | `extractos/economia.md` | 1–15 | pendiente |
| 12 | Historia, arqueología y religión | `extractos/historia.md` | 1–15 | pendiente |

### Ampliación del 2026-07-31 (segunda ronda)

Salen de **cotejar los temas con los recursos que el juego ya tiene**. No son
completismo: cada uno cubre un hueco donde hoy hay datos del juego que **ningún
tema sostiene**.

| # | Tema | Fichero | Épocas | Por qué |
|---|---|---|---|---|
| 13 | **Minería y extracción** | `extractos/mineria.md` | 1–15 | **SPEC-007 §14** (reserva y recuperación: sacar más de un yacimiento agotado con tecnología) es un sprint planificado y **no tiene ningún tema que lo alimente** |
| 14 | **Cerámica, vidrio y no metálicos** | `extractos/ceramica_vidrio.md` | 2–15 | La **arcilla** es recurso desde la época 2 y ningún tema la cubre |
| 15 | **Alimentación, conservación y ganadería** | `extractos/alimentacion.md` | 1–15 | La **comida** es el único recurso de subsistencia y atraviesa las 15 épocas. Salazón, secado, fermentación, conserva; y domesticación, que además da el caballo |
| 16 | **Electrónica, computación y comunicaciones** | `extractos/electronica.md` | 12–15 | **Silicio** y **tierras raras** son recursos de las épocas 14–15 y ningún tema explica para qué sirven |
| 17 | **Nuclear** | `extractos/nuclear.md` | 14–15 | El **uranio** es recurso de la época 14 sin tema propio |
| 18 | **Textiles y vestido** | `extractos/textiles.md` | 1–15 | **No hay recurso textil todavía.** Es un sector económico enorme —la Revolución Industrial empieza ahí— y la investigación debe decir **si merece un recurso propio** o se queda fuera del alcance |

**El 18 es distinto de los demás**: no documenta algo que ya existe, sino que
**decide si algo debe existir**. Su entregable incluye una recomendación.


### El tema 1 es el difícil, y conviene decirlo

Los tratados técnicos del Archive son en su abrumadora mayoría **occidentales y
de 1850–1930**. Sirven de maravilla para las épocas 5–13 y **no sirven** para
las 1–4 ni para las civilizaciones no europeas: un manual británico de fundición
no explica la metalurgia andina, ni el riego nilótico, ni la agricultura
mesoamericana.

Para el tema 1 hay que ir a **arqueología y etnografía**, no a ingeniería:
informes de excavación, arqueología experimental, DOAJ y PubMed antes que
Internet Archive. Y **decir cuándo un dato viene de reconstrucción moderna** y
no de una fuente de la época, porque en técnicas antiguas esa diferencia es
enorme.

Riesgo a vigilar: si el corpus se llena solo de fuentes occidentales
industriales, el juego heredará ese sesgo y las civilizaciones no europeas
quedarán descritas con categorías ajenas.

## Por qué los repositorios «fin del mundo» importan aquí

La **Survivor Library** y **Appropedia** recogen manuales para reconstruir
tecnología **desde cero y sin infraestructura previa**. Es exactamente el
problema del juego: cómo se hacía algo cuando no existía lo que hoy damos por
supuesto. Un manual de 1900 sobre fundición con carbón vegetal describe el
proceso que necesitamos para la época 5 mucho mejor que un artículo moderno que
lo da por sabido.

Ventaja añadida: casi todo es **dominio público**, así que se puede citar
literalmente y guardar sin problema de licencia.
