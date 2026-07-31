# Plantilla de encargo — poblar un tema del corpus

Sustituye `historia`, `historia.md` y `cronologia de las civilizaciones del juego, continuidad historica de Egipto y Roma hasta hoy, religion y su papel institucional` y lánzalo.

---

Lee primero `docs/research/corpus/INDICE.md` y `docs/research/corpus/FUENTES.md`:
contienen las reglas y los repositorios **ya probados**. No busques por tu
cuenta repositorios nuevos sin comprobar que responden.

Consulta con `docs/research/corpus/buscar.sh archive|openlib|doaj "consulta"`.

**Texto completo de un libro del Internet Archive**:
`https://archive.org/download/IDENTIFICADOR/IDENTIFICADOR_djvu.txt`
(la ruta `/stream/` devuelve HTML y no sirve).

## Tema: historia

Asuntos a cubrir: cronologia de las civilizaciones del juego, continuidad historica de Egipto y Roma hasta hoy, religion y su papel institucional

## Qué buscar, según la época que sirva el tema

- **Épocas 1–4** → **arqueología y etnografía**, no ingeniería. Informes de
  excavación, arqueología experimental, DOAJ, PubMed. Un tratado técnico
  británico de 1900 **no** describe la técnica andina, nilótica ni mesoamericana.
  **Di siempre si un dato viene de reconstrucción moderna** y no de una fuente
  de la época: en técnicas antiguas esa diferencia lo es todo.
- **Épocas 5–13** → tratados técnicos de dominio público del Internet Archive,
  **anteriores a 1929**. Describen el proceso sin dar por supuesta la
  infraestructura moderna.
- **Épocas 14–15** → acceso abierto reciente: DOAJ, arXiv, PubMed Central.

## Qué extraer de cada proceso o técnica

- **Entradas y proporciones** con cifra.
- **Nombre del proceso** y qué ocurre física, química o biológicamente.
- **Energía**: qué la aporta y de dónde sale en esa época.
- **Condición crítica** con cifra: temperatura, presión, tiempo, dosis.
- **Periodo y lugar** en que se domina.
- **Subproductos**, residuos o riesgos.

## Reglas innegociables

1. Cada cifra necesita **CITA LITERAL**, **URL exacta** y **fecha de consulta**.
   Parafrasear un número y perder la frase original es como se cuelan los datos
   inventados.
2. Marca cada afirmación: `[V]` verificada · `[?]` **NO VERIFICADO** · `[I]`
   inferencia propia.
3. Si no encuentras un dato, escribe **NO VERIFICADO**. Nunca un número con
   aspecto creíble.
4. **Vale más un extracto corto y sólido que uno largo y dudoso.** Media página
   por fuente basta: cita, tabla de lo que aporta, y qué implica para el juego.

Ejemplo del estándar esperado: `docs/research/corpus/extractos/metalurgia.md`.

## Salida

`docs/research/corpus/extractos/historia.md`
