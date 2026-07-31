# Evaluación del panel — cadenas de producción (2026-07-31)

**Encargo del Director**: investigar las cadenas reales de los diez materiales
producidos, con **fuentes fiables y científicas**, para que la guía didáctica
(SPEC-007 §21) no enseñe nada falso.

**Veredicto: la investigación NO alcanza el listón pedido.** Sirve como
andamiaje provisional; **no sirve todavía para la guía didáctica**.

## 1. Qué devolvió cada modelo

| Ruta | Estado | Citas |
|---|---|---|
| `claude-minimax` | **Útil** | 50 Wikipedia · 3 EIA · 2 USGS · 2 FAO |
| `gemini-web/gemini-3.1-pro` | Prosa sin fuentes | **1** (YouTube) |
| `gemini-web/gemini-3.5-flash` | Prosa sin fuentes | **0** |
| `gweb/gemini-3.1-flash-lite` | Prosa sin fuentes | **0** |

Las tres rutas Gemini **son las que supuestamente navegan** —así lo dice
`panel.sh`— y aun así devolvieron texto seguro y sin una sola referencia
comprobable. Es exactamente el fallo que el propio script advierte: «un modelo
sin web al que se le pide investigar inventa con tono seguro». **No se usan.**

Además volvieron **corrompidas por el proxy** (3343 líneas con 147 únicas: el
proxy concatena los chunks de forma acumulativa). Se recuperaron a
`*.limpio.md` quedándose con la última aparición de cada línea.

## 2. Por qué MiniMax tampoco basta

Su trabajo es **honesto** —marca `[V]`/`[?]`/`[I]`, dice «fuente secundaria», y
reconoce lo que no encontró, como el combustible concreto de un horno de bronce
o las proporciones de mineral a metal—. Pero sus fuentes son **Wikipedia en un
90 %**, y el encargo pedía manuales técnicos y organismos científicos.

## 3. El obstáculo es del entorno, no del modelo

Comprobado hoy directamente:

- `pubs.usgs.gov` (PDF de Mineral Commodity Summaries) → **HTTP 403**
- `britannica.com` → **HTTP 403**
- `usgs.gov` (página índice) → accesible, pero **el contenido técnico está en
  los PDF que dan 403**
- `wikipedia.org` → accesible y con detalle técnico utilizable

O sea: **desde aquí no se llega a las fuentes que el listón exige.** Wikipedia
da entradas, reacciones y temperaturas aprovechables, pero es secundaria y en
varios casos ni siquiera cita primarias para sus ecuaciones.

## 4. Qué recomiendo

1. **Usar la síntesis de MiniMax como andamiaje** de las recetas, con cada
   afirmación marcada como secundaria. Las recetas quedan **provisionales**.
2. **No publicar la guía didáctica sobre esta base.** Enseñar exige más.
3. Para cerrar el listón hacen falta **una de tres**: acceso a una obra de
   referencia (ASM Handbook, Ullmann's, Kirk-Othmer), verificación por alguien
   con formación en la materia, o limitar la guía a lo que Wikipedia respalde
   con una primaria que sí podamos leer.

**Decisión del Director pendiente.** No se escriben recetas definitivas hasta
que la haya.
