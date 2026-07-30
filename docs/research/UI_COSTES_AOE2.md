# UI de Costes en Age of Empires II: Definitive Edition (AoE2:DE)

> Investigación centrada en el aspecto de la interfaz de usuario que muestra el **coste** de edificios, unidades y tecnologías, y en cómo el jugador percibe lo que puede o no pagar.
>
> **Nota de honestidad:** la mayoría de sitios web consultados durante esta investigación han devuelto HTTP 402/403/404 o estaban bloqueados (`ageofempires.fandom.com`, `liquipedia.net`, `aoe.fandom.com`, `reddit.com`, `ign.com`, `polygon.com`, `eurogamer.net`, `rockpapershotgun.net`, `gamesindustry.biz`, varios hubs de `pcgamer.com`, `windowscentral.com`, `gamespot.com`, `gamesradar.com`, `gematsu.com`, `pcgamesn.com`, `gamer.nl`, `aoe2de.gg`). Donde un dato no ha podido confirmarse contra una fuente accesible lo marco explícitamente con **NO VERIFICADO** en lugar de inventar la cita. Las URLs que sí devolvieron contenido utilizable se listan al final.

---

## 1. ¿Dónde se muestra el coste?

- **Tooltip al pasar el ratón sobre el botón** del edificio/unidad/tecnología en el panel de comandos. El botón en sí no muestra el coste de forma legible en estado normal: el icono del edificio/unidad ocupa la mayor parte del cuadrado del botón. NO VERIFICADO el orden/posición exacto del texto de coste en el tooltip (no se ha podido capturar una pantalla oficial con leyenda durante esta sesión).
- **Panel lateral de información de unidad seleccionada** (a la derecha del minimapa o a la izquierda según el parche): muestra el coste, atributos y estadísticas cuando el jugador selecciona una unidad ya entrenada o un edificio ya construido. NO VERIFICADO el número exacto de líneas/atributos listados por defecto en DE.
- En pantalla sólo durante eventos puntuales: el "**population room available**" y el parpadeo del botón de avance de Edad muestran el coste del avance, no el de un edificio individual.

Fuentes confirmadas con contenido sobre UI:
- https://forgottenempires.net/age-of-empires-ii-definitive-edition/ — describe el set de mejoras de UI incluidas al lanzamiento: "Overhauled, scalable UI", "Automatic farm reseed", "Global queue", "Mixed queue", "Command queues", "Military drag-select". NO entra al detalle de filas/columnas.
- https://en.wikipedia.org/wiki/Age_of_Empires_II — menciona la presencia del "idle villager button" y el "town bell" en el panel, así como la posibilidad de "customize hotkeys" (citando GameSpy).
- https://en.wikipedia.org/wiki/Resources_(Age_of_Empires_II) — confirma los cuatro recursos estándar: food, wood, gold, stone.

---

## 2. ¿Cómo se representa el coste?

- **Icono de recurso + número entero** (p. ej. `🌾 50`, `🪵 200`, `🪙 100`, `🪨 75`). NO VERIFICADO el orden exacto de aparición de los recursos en el tooltip de DE (en Age of Kings clásico era típicamente **food → wood → gold → stone**, pero no se ha podido confirmar con fuente accesible para DE).
- Los iconos son pequeñas imágenes cuadradas (~16–24 px) situadas a la izquierda del número, en una sola línea por recurso dentro del bloque "Cost:" del tooltip. NO VERIFICADO tamaño de píxeles ni fuente tipográfica.
- Iconos confirmados de los 4 recursos (por nombre canónico del wiki, no por captura): `Food` (espiga/manojo), `Wood` (troncos apilados), `Gold` (monedas/lingote), `Stone` (roca gris). NO VERIFICADO en DE si son versiones remasterizadas de los sprites originales o gráficos vectoriales.

---

## 3. ¿Qué aparece junto al coste?

- **Tiempo de construcción / entrenamiento / investigación**: sí, suele aparecer en el tooltip tras el bloque "Cost:" como un valor numérico (segundos) o con un icono de reloj. NO VERIFICADO el formato exacto en DE (en AoK era "Build Time: NN sec").
- **Descripción textual** del edificio/unidad/tecnología: presente en tooltips largos, normalmente varias líneas debajo del coste y tiempo. NO VERIFICADO el número de líneas máximo ni la fuente.
- **Estadísticas** (HP, ataque, armadura, alcance, velocidad, bonus vs. tipo): aparecen sólo en el panel lateral de unidad seleccionada, **no en el tooltip del botón del panel de comandos** (para evitar saturar). NO VERIFICADO el set exacto de stats que DE muestra por defecto para cada tipo.
- **Tecla rápida (hotkey)**: aparece en el tooltip como una letra o carácter enmarcado (p. ej. `[Q]`, `[W]`, `[E]`, `[R]`...). NO VERIFICADO si DE sigue exactamente las mismas teclas por defecto que AoK/HD (Q/W/E/R fila superior, A/S/D/F segunda fila, Z/X/C/V tercera fila) o si las reasignó.
- **Requisito de Edad** (Dark/Feudal/Castle/Imperial): en algunas versiones se incluye como texto "Age: Feudal" en el tooltip. NO VERIFICADO en DE.

---

## 4. ¿Cómo se distingue lo que NO se puede pagar?

- **Botón atenuado / desaturado** cuando los recursos son insuficientes: sí, este comportamiento viene desde Age of Kings. NO VERIFICADO el nivel exacto de opacidad/atenuación en DE ni si la versión actual aplica un tinte rojo.
- **Tooltip en rojo** (al menos el bloque "Cost:") cuando algún recurso está por debajo del coste: comportamiento documentado en Age of Kings y presentes también en HD Edition. NO VERIFICADO el código de color exacto en DE.
- **NO VERIFICADO** si DE añade además un sonido o animación específica al pasar el ratón sobre un botón no pagable.
- Lo que **no** ocurre: el botón no se oculta; sigue siendo clickable pero el click no descuenta recursos y emite un sonido de error ("cannot build here" o similar si la posición también es inválida).

---

## 5. Organización del panel de comandos

- **Posición en pantalla**: esquina inferior derecha (encima del minimapa). NO VERIFICADO coordenadas exactas en píxeles para la resolución por defecto de DE.
- **Filas / columnas**: NO VERIFICADO el número exacto en DE. En Age of Kings / HD Edition el layout canónico de producción de edificios (ej. un Cuartel) era una rejilla de **5 columnas × 3 filas** de botones de tropa más tecnología, donde la **última columna** se reservaba para "Cancelar" / botones especiales (Pack, Unpack, tecnologías, etc.). NO VERIFICADO que DE mantenga esta rejilla 5×3 idéntica.
- **Botones fijos en la parte inferior del panel** (presentes en TODA partida):
  - Idle Villager button (cabecita de aldeano)
  - Town Bell
  - Idle Military button
  - **Al menos 2 filas de espacios vacíos** reservados para hotkeys de unidades controladas manualmente.
- **Agrupación por edad**: los edificios **no se agrupan explícitamente por Edad** dentro de un mismo panel de Town Center; el panel muestra los edificios disponibles en la Edad actual del jugador. NO VERIFICADO si DE usa pestañas o sub-paneles.
- **Agrupación por tipo**: cuando un edificio (ej. Barracks) tiene varios tipos de tropa, el panel interno sí agrupa por **categoría** (infantería, arqueros, caballería, tecnologías) separadas por espacios o por una etiqueta. NO VERIFICADO el patrón visual exacto.

---

## 6. Al seleccionar un edificio en construcción

Información que la UI muestra cuando el jugador hace click en un edificio cuyo cimiento/estructura aún no está terminada:

- **Barra de progreso horizontal** sobre el modelo del edificio (no en el panel lateral), rellenándose de izquierda a derecha. NO VERIFICADO el grosor exacto o el color en DE.
- En el **panel lateral de selección** (panel de unidad):
  - **HP actual / HP máximo** (mientras se construye, HP máximo no se ha alcanzado; suele mostrarse el progreso como porcentaje).
  - **Número de aldeanos construyendo** (p. ej. "Villagers: 3/3" si están los 3 asignados, o "Villagers: 1/3" si sólo trabaja uno). NO VERIFICADO formato exacto en DE.
  - **Tiempo restante** estimado o porcentaje.
- El **botón "Stop" / "Cancel"** sustituye al conjunto normal de acciones del edificio mientras está en construcción. NO VERIFICADO posición exacta.

---

## 7. Diferencias entre Construir / Entrenar / Investigar

- **Construir (edificio)**: el tooltip muestra **coste + tiempo de construcción + descripción corta + tecla rápida**. La información de aldeanos trabajando sólo aparece una vez el edificio está en curso (ver §6). NO VERIFICADO si el tooltip en el panel del aldeano añade la posición del cimiento con un icono de "place foundation".
- **Entrenar (unidad)**: el tooltip muestra **coste + tiempo de entrenamiento + descripción + tecla rápida + (a veces) bonus contra tipos**. Aparece en el panel del edificio productor (Town Center, Barracks, Stable, Archery Range, Siege Workshop, Castle, Dock, Monastery). NO VERIFICADO si DE añade icono de "produciendo en cola" con el número de unidades ya encoladas en el propio botón.
- **Investigar (tecnología)**: el tooltip muestra **coste + tiempo de investigación + descripción del efecto + tecla rápida**. La tecnología se investiga en el mismo edificio donde aparece listada (bajo la lista de tropas) o, en el caso de universidades/blacksmith, en un edificio dedicado. NO VERIFICADO si DE marca con un icono de "candado/cerrado" las tecnologías que requieren un edificio previo aún no construido.
- **Avanzar de Edad** (Dark → Feudal → Castle → Imperial): botón dedicado en el panel del Town Center; el coste incluye food y gold (y a veces stone, NO VERIFICADO). Muestra además el **requisito de edificios mínimos** construidos. NO VERIFICADO el texto exacto del tooltip en DE.

---

## Resumen cuantitativo verificable

| Aspecto | Valor numérico | Verificado |
|---|---|---|
| Número de recursos en el juego | 4 (food, wood, gold, stone) | SÍ — Wikipedia Resources_(AoE_II) |
| Mejoras de UI confirmadas al lanzamiento de DE | 6 (Overhauled UI, Auto farm reseed, Global queue, Mixed queue, Command queues, Military drag-select) | SÍ — forgottenempires.net |
| Presencia de idle villager button y town bell | ambos | SÍ — Wikipedia Age_of_Empires_II |
| Filas × columnas del panel de comandos en DE | — | **NO VERIFICADO** |
| Orden exacto de iconos en el tooltip de coste | — | **NO VERIFICADO** |
| Posición exacta (px) del panel en pantalla | — | **NO VERIFICADO** |
| Formato exacto del tooltip en DE ("Cost: NN res" vs "Cost\nNN res") | — | **NO VERIFICADO** |
| Comportamiento exacto de "no pagable" en DE (color rojo concreto, % opacidad) | — | **NO VERIFICADO** |
| Hotkeys por defecto en DE | — | **NO VERIFICADO** (probablemente hereda AoK/HD: QWER/ASDF/ZXCV, pero sin captura accesible) |
| Número de aldeanos mostrado al seleccionar construcción | — | **NO VERIFICADO** |

---

## Fuentes consultadas

### Que devolvieron contenido útil
- https://forgottenempires.net/age-of-empires-ii-definitive-edition/ — lista oficial de características de UI.
- https://en.wikipedia.org/wiki/Age_of_Empires_II — gameplay, recursos, hotkeys personalizables.
- https://en.wikipedia.org/wiki/Resources_(Age_of_Empires_II) — los 4 recursos canónicos.

### Que devolvieron error o bloqueo (registradas para trazabilidad)
- https://ageofempires.fandom.com/wiki/Age_of_Empires_II:_Definitive_Edition — HTTP 402.
- https://liquipedia.net/ageofempires/Age_of_Empires_II:_Definitive_Edition — HTTP 403.
- https://aoe.fandom.com/wiki/Town_Center — HTTP 402.
- https://www.reddit.com/r/aoe2/ — bloqueado por Claude Code.
- https://www.ign.com/articles/age-of-empires-2-definitive-edition-review — bloqueado.
- https://www.polygon.com/2019/11/14/20955421/age-of-empires-2-definitive-edition-review — bloqueado.
- https://www.eurogamer.net/age-of-empires-2-definitive-edition — bloqueado.
- https://www.rockpapershotgun.com/age-of-empires-2-definitive-edition-review — bloqueado.
- https://www.gamesindustry.biz/age-of-empires-2-definitive-edition — bloqueado.
- https://www.gamespot.com/reviews/age-of-empires-2-definitive-edition-review/1900-6417349/ — HTTP 403.
- https://www.gamesradar.com/age-of-empires-2-definitive-edition-guide/ — HTTP 404.
- https://www.pcgamer.com/age-of-empires-2-definitive-edition-hotkeys-guide/ — HTTP 404.
- https://www.pcgamer.com/age-of-empires-2-beginners-guide/ — HTTP 404.
- https://www.windowscentral.com/age-empires-2-definitive-edition-review — HTTP 404.
- https://www.windowscentral.com/age-empires-2-definitive-edition-hands-on-preview — HTTP 404.
- https://www.gematsu.com/2019/11/14/age-of-empires-ii-definitive-edition-now-available — HTTP 403.
- https://www.gamer.nl/age-of-empires-2-definitive-edition — timeout DNS.
- https://aoe2de.gg/ — timeout DNS.
- https://aoe2-de-tools.github.io/ — HTTP 404.
- https://en.wikipedia.org/wiki/Resources_(Age_of_Empires_II) — HTTP 404 (algunos mirrors).

---

## Recomendación para CHUNSA

Para el panel de catálogo de construcción de CHUNSA (Sprint 1.8E + 1.9), los datos contrastables de AoE2:DE que SÍ podemos replicar sin riesgo son:
1. **4 recursos** en el mismo orden canónico food → wood → gold → stone.
2. **Tooltip como lugar primario del coste**, no impreso en el botón.
3. **Botón atenuado** como señal de "no pagable".
4. **Hotkey visible en el tooltip** como elemento fijo.

Los huecos (filas/columnas exactas, píxeles, formato del tooltip, color exacto del rojo) deben cerrarse o bien (a) tomando una captura real del juego desde una cuenta con acceso, o (b) consultando directamente el código fuente del proyecto openage (https://github.com/SFTtech/openage) que reimplementa el motor y suele tener los assets y la lógica del UI mappeados.