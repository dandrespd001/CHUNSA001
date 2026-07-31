# Repositorios abiertos — registro probado

**Probado el 2026-07-31 desde este equipo.** El estado importa: en este proyecto
ya perdimos tiempo con fuentes cuya portada responde y cuyo contenido da 403.

## Responden y sirven

| Repositorio | Qué tiene | Cómo se consulta | Estado |
|---|---|---|---|
| **Internet Archive** | Libros técnicos escaneados, informes DTIC, actas de sociedades de ingeniería. **8252 textos solo de metalurgia** | API JSON: `archive.org/advancedsearch.php?q=...&output=json` | **200, con API** |
| **Project Gutenberg** | Libros técnicos de dominio público, texto limpio | `gutenberg.org/ebooks/search/?query=` | 200 |
| **Open Library** | Catálogo de libros, API JSON | `openlibrary.org/search.json?q=` | **200, con API** |
| **DOAJ** | Revistas de acceso abierto | `doaj.org/api/search/articles/` | **200, con API** |
| **PubMed Central** | Biomedicina y veterinaria, acceso abierto | `ncbi.nlm.nih.gov/pmc/?term=` | 200 |
| **Wikisource** | Textos históricos transcritos | `en.wikisource.org` | 200 |
| **Survivor Library** | **Repositorio «fin del mundo»**: manuales para reconstruir tecnología desde cero. Justo lo que el Director señaló | `survivorlibrary.com` | 200 |
| **Appropedia** | Tecnología apropiada, construcción y agricultura de bajo coste | `appropedia.org` | 200 |
| **arXiv** | Preprints de física y matemáticas | `export.arxiv.org/api/query?` (usar **https**) | 301 → https |
| **Wikipedia** | Entrada general; **secundaria**, declararlo siempre | — | 200 |

## NO responden — no insistir

| Fuente | Estado | Nota |
|---|---|---|
| `pubs.usgs.gov` (PDF) | **403** | El contenido técnico está justo aquí |
| `britannica.com` | **403** | — |
| `ageofempires.fandom.com` | **402** | Ya conocido de otra investigación |
| `liquipedia.net` | **402** | — |

## Regla de uso

1. **Buscar primero en el corpus local** (`INDICE.md`). No repetir búsquedas.
2. Preferir **dominio público** (Gutenberg, Archive pre-1929): se puede citar y
   guardar el texto entero sin problema de licencia.
3. Wikipedia **solo como entrada**, y diciéndolo.
4. Todo extracto guarda **URL, fecha de consulta y cita literal**. Sin eso no
   entra: este proyecto ya cazó una fabricación de citas.
