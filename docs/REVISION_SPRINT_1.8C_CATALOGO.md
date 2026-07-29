# Revisión del Arquitecto — 1.8C-kernel: metadatos de recurso en el catálogo

Fecha: 2026-07-29 · Implementación: GPT-5.6 SOL
Contrato: `SOL_1.8C_CATALOGO_RECURSOS.md`

## Veredicto

**Aprobado e integrado.** Tercera entrega consecutiva con TDD correcto y
**sin ninguna desviación de contrato** — la primera vez que ocurre.

## 1. Fase roja

Fallos de aserción con esperado y obtenido, sobre un stub deliberadamente
incorrecto (`catalog_find_resource()` devolviendo siempre
`INVALID_RESOURCE_ID`):

```text
CHECK_EQ resource_count: esperado=30 obtenido=0
CHECK_EQ copper_family: esperado=3 obtenido=0
CHECK_EQ bronze_nature: esperado=2 obtenido=0
CHECK_EQ resources_with_four_metadata_fields: esperado=30 obtenido=0
CHECK_EQ chunsa:food: esperado=0 obtenido=18446744073709551615
RESOURCE_CATALOG_RED_RC=1
```

La cuarta línea es la prueba que señalé como la importante del contrato: que
los 30 recursos tengan los cuatro campos poblados y **ninguno leído como cero
por defecto**.

## 2. Verificación independiente

```text
BUILD=0     0 avisos
CTEST=0     100% tests passed out of 31   (eran 30; +1 resource_catalog)
apertura    end_tick=9317 winner=1        idéntico
eco         end_tick=1107 winner=1        idéntico
git diff -- gdextension/ data/resources/  vacío
```

## 3. La decisión de versionado, razonada sola

Le dejé la decisión abierta con una pista, no con la respuesta. Concluyó
mantener `SAVE_FORMAT_VERSION` en 14 y `CHECKSUM_ALGO_VERSION` en 9, y lo
justificó así:

> «La tabla descriptiva vive en el catálogo offline y no se serializa en
> `GameState`, no cambia el orden de ningún campo de save, no cambia el dominio
> `CHUNSA_STATE_V9` y no participa en el checksum de estado. […] Subir
> save/checksum aquí declararía **una incompatibilidad de estado inexistente**.»

Es exactamente el razonamiento correcto, y distingue este caso del 1.8A —donde
el dominio sí crecía y el bump era obligado—. Tres sprints seguidos acertando
la decisión de versionado, que es donde más fácil sería bumpear por costumbre.

## 4. Estado de la promesa de SPEC-007 §16.1

Con esta tabla, el HUD puede **leer** familias y edades del catálogo en vez de
cablearlas. La promesa de que «añadir una fuente construida nueva por datos,
sin tocar código, funciona» queda técnicamente sostenible.

Sin este sprint, Luna habría tenido que cablear los 30 recursos en C++ y la
promesa se habría roto en silencio.

## 5. Nota de proceso

Detecté el hueco **antes** de firmar el contrato del HUD, comprobando si el
catálogo exponía lo que la spec afirmaba. Es la corrección que me impuse tras
cuatro defectos de contrato consecutivos, y ha funcionado a la primera: ahorró
una ronda de Luna y evitó una implementación que habría roto el diseño.

**La verificación previa pasa a ser parte del proceso de escribir un brief**, no
un extra.
