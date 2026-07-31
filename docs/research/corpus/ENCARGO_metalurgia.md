# Encargo: poblar el corpus de CHUNSA — METALURGIA

Lee primero estos dos ficheros del repositorio, que contienen las reglas y los
repositorios **ya probados**:

- `docs/research/corpus/INDICE.md`
- `docs/research/corpus/FUENTES.md`

Usa `docs/research/corpus/buscar.sh` para consultar Internet Archive, Open
Library y DOAJ. Ejemplos:

```
./docs/research/corpus/buscar.sh archive "charcoal iron smelting"
./docs/research/corpus/buscar.sh archive "bessemer steel process"
./docs/research/corpus/buscar.sh doaj    "bronze alloy composition"
```

## Prioriza libros técnicos antiguos de dominio público

En Internet Archive, **anteriores a 1929**. Describen los procesos **sin dar por
supuesta la infraestructura moderna**, que es justo lo que necesita un juego que
va de la época 1 a la 15, y además se pueden citar literalmente sin problema de
licencia.

Para leer un texto del Archive puedes usar la versión en texto plano:
`https://archive.org/stream/IDENTIFICADOR/IDENTIFICADOR_djvu.txt`

## Qué extraer

Para **bronce**, **carbón vegetal**, **hierro forjado**, **coque** y **acero**:

- Entradas y **proporciones por masa**.
- **Nombre del proceso** real y qué ocurre física o químicamente.
- Qué aporta la **energía** y de dónde sale en esa época.
- **Temperatura o condición crítica**, con cifra.
- **Periodo histórico** en que el proceso se domina.
- **Subproductos** o residuos relevantes.

## Reglas innegociables

1. Cada cifra necesita una **CITA LITERAL** del pasaje que la sostiene, más la
   **URL exacta** y la **fecha de consulta**. Parafrasear un número y perder la
   frase original es como se cuelan los datos inventados.
2. Marca cada afirmación: `[V]` verificada · `[?]` **NO VERIFICADO** · `[I]`
   inferencia propia.
3. Si no encuentras un dato, escribe **NO VERIFICADO**. Nunca un número con
   aspecto creíble.

Este proyecto ya cazó una fabricación de citas y desde entonces la procedencia
se audita. **Vale más un extracto corto y sólido que uno largo y dudoso.**

## Salida

Escribe el resultado en `docs/research/corpus/extractos/metalurgia.md`.
