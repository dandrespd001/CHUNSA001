# Investigación — materiales en CHUNSA (panel 2026-07-29 10:00)

Formato: `[marca] afirmación con dato concreto -> [CHUNSA] qué hacemos`.
Una línea por hallazgo. Fuentes citadas en cada `[V]`.

## 1. Materiales «invisibles» — ¿falta alguno indispensable?

[V] Caliza/dolomita como fundente: «limestone (or dolomite), to remove the accompanying rock gangue as slag» (https://en.wikipedia.org/wiki/Smelting). -> Añadir como insumo de la receta «hierro forjado» en edad 5; el jugador técnico lo nota si falta.
[V] Arcilla refractaria: fire clay funde a 1580–1780°C, cubre el rango 1100–1500°C del hierro (https://en.wikipedia.org/wiki/Refractory). -> Si «arcilla» (edad 2) cubre el espectro, no añadir nuevo recurso; si se distingue, separarla en edad 3–4 al aparecer los primeros hornos altos.
[V] Sal: «the best-known food preservative, especially for meat, for many thousands of years» (https://en.wikipedia.org/wiki/Salt). -> Subir de no aparecer a edad 3–4; ejércitos en campaña sin sal rompen verosimilitud histórica.
[V] Haber-Bosch: producción industrial 1913 en Oppau; «Half of the nitrogen in human tissues originated from the Haber-Bosch process» (https://en.wikipedia.org/wiki/Haber%E2%80%93Bosch_process). -> Añadir nitrógeno fijado como recurso en edad 12–13 (1900–1918); sin él, la pólvora de la edad 8 aguanta pero los fertilizantes del s. XX chirrían.
[I] Agua industrial (vapor, electrólisis): ninguna fuente RTS/Web encontrada confirma impacto en mecánicas; se modela mejor como «energía» derivada que como stock. -> Mantener como derivada, no como recurso recolectable.
[I] Fijación biológica previa al Haber-Bosch: leguminosas ya fijaban N (https://en.wikipedia.org/wiki/Animal_husbandry). -> Recolectable «forraje/leguminosas» podría sustituir nitrógeno en edad 8–10; no añadir stock nuevo.

## 2. Errores en la lista actual

[V] Bronce = 88% cobre + 12% estaño, clásica; pero los bronces históricos eran altamente variables (As, Zn, Pb) (https://en.wikipedia.org/wiki/Bronze). -> La receta cobre+estaño es defendible para un juego; nota interna: aceptar varianza histórica silenciosa.
[V] Coque industrial: Abraham Darby I, 1709; el coque no «sustituye» al carbón vegetal, coexisten; coque desplaza al carbón vegetal en alto horno hacia 1850 (https://en.wikipedia.org/wiki/Coke_(fuel)). -> Mover coque de edad 11 a edad 9 (1700) y dejar carbón vegetal hasta edad 11 (1850) donde aún se usa en fraguas pequeñas.
[V] Acero: crisol Huntsman 1740s, Bessemer 1855 (https://en.wikipedia.org/wiki/Steel). -> «Acero = hierro forjado + coque» en edad 12 es coherente (1850–1900); no es históricamente exacta (el crisol lleva carbón vegetal, no coque) pero la simplificación se perdona.
[V] Plomo: fundido desde el 7º milenio a.C. (https://en.wikipedia.org/wiki/Lead). -> Plomo en edad 8 es anacrónicamente tardío; mover a edad 2–3 o fusionar con estaño en un «metales blandos».
[V] Aluminio: Hall-Héroult 1886 (https://en.wikipedia.org/wiki/Aluminium). -> Edad 14 (informática/era digital) es demasiado tarde; debería entrar en edad 12 con la electrificación.
[V] Plástico sintético: Bakelite 1907; масштаб industrial 1950s (https://en.wikipedia.org/wiki/Plastic). -> «Derivados del petróleo» cubre plásticos a partir de edad 13 (1900–1950); bien ubicado.
[V] Pólvora china: fórmula 808, uso militar 904, llega a Europa 1267 (https://en.wikipedia.org/wiki/Gunpowder). -> Edad 8 es razonable; salitre + azufre + carbón vegetal correcto.

## 3. Fusiones aceptables — el criterio

[V] Tierras raras: 17 elementos «casi indistinguibles» pero «each one occupies a unique technological niche that nothing else can» (https://en.wikipedia.org/wiki/Rare-earth_element). -> CRITERIO: si dos materiales se obtienen del mismo yacimiento/mineral y el jugador no toma decisiones distintas, fusionar. Tierras raras como un solo recurso en edad 15 pasa el corte (no hay decisión de «¿extraigo neodimio o europio?»); un jugador muy técnico lo notará, pero no hay mecánicas alternativas.
[V] Derivados del petróleo: plástico, combustible, lubricante vienen todos del refinado; un jugador medio no distingue. -> Fusionar los tres en «derivados del petróleo» (ya está). El CRITERIO es: misma cadena upstream + misma decisión downstream = fusionable.
[I] Salitre / azufre: ambos son recolectables, distintos, sin subcadena común. -> NO fusionar; el jugador los cuenta por separado en su HUD y la salitre tiene uso alternativo (fertilizante).
[I] Cobre / oro en edad 3: ambos metales preciosos pero con cadenas distintas; en un RTS se distinguen por rareza. -> Mantener separados; un jugador espera ver oro y cobre.
[I] Madera / carbón vegetal: el carbón vegetal se produce en edad 5 a partir de madera. -> NO fusionar: el jugador debe decidir producir carbón vegetal o usar madera directamente (ejércitos vs. construcción).

## 4. Cómo lo resuelven otros juegos

[V] Age of Empires II: 4 recursos (food, wood, gold, stone), los 4 activos en Imperial Age (https://en.wikipedia.org/wiki/Age_of_Empires_II). -> Referencia de «mínimo viable»: 4 es el suelo de la industria.
[V] Rise of Nations: 6 recursos, todos infinitos, cada unidad usa 2 (https://en.wikipedia.org/wiki/Rise_of_Nations). -> Técnica: la mayoría de unidades cuesta solo 2 recursos; nuestro sistema podría copiar esto en edades tempranas.
[?] Empire Earth, Anno 1800, Factorio, Foundation, Manor Lords: las páginas Wikipedia consultadas no detallan el conteo exacto; Anno 1800 menciona «production chains» y «supply chains» sin enumerarlas (https://en.wikipedia.org/wiki/Anno_1800). -> No se pudo confirmar el conteo exacto de Anno 1800, Foundation ni Manor Lords. Factorio confirmado como sistema de cadenas profundas sin conteo fijo (https://en.wikipedia.org/wiki/Factorio).
[V] Factorio: «complex crafting chains and material complexity»; cada nivel se construye sobre el anterior (https://en.wikipedia.org/wiki/Factorio). -> La profundidad de cadena compensa la cantidad de tipos: menos tipos por edad, pero más eslabones por tipo.

## 5. Umbral de saturación

[?] No se encontró en las fuentes consultadas (gamedeveloper.com, Wikipedia de juegos) un estudio cuantitativo con número concreto de «máximo de recursos simultáneos» antes de rechazo del jugador. -> Sin dato duro, usar la regla práctica observada: AoE2=4, RoN=6, ambos sin quejas. Tomar 8 simultáneos como techo experimental para CHUNSA; medir en playtest.
[I] Inferencia por comparación: si AoE2 funciona con 4 y nadie publica quejas cognitivas, y Anno 1800 soporta ~25+ bienes en endgame con éxito comercial, el techo real está entre 8 y 25. -> Nuestro pico de 26 cae en el límite superior conocido; depende de agrupación, no de conteo bruto.

## Añadir (ordenado por cuánto duele)

1. Caliza (fundente) — sin ella, la receta de hierro forjado chirría al primer ingeniero que juegue.
2. Sal — mover a edad 3–4; ejércitos en campaña sin sal son históricamente absurdos.
3. Nitrógeno fijado (Haber-Bosch) — edad 12–13; fertilizantes y explosivos modernos lo exigen.
4. Forraje/leguminosas como precursor de nitrógeno pre-Haber-Bosch — opcional, suaviza la transición.

## Fusionar o quitar

1. Plomo fuera de edad 8 — fusionar con estaño en «metales blandos» (edad 3) o eliminar (su rol moderno es residual: baterías, blindaje).
2. Tierras raras — mantener como un solo recurso en edad 15; el criterio de «un yacimiento, una decisión» lo permite.
3. Derivados del petróleo — ya fusionados (plástico+combustible+lubricante); validar que la mecánica downstream no necesite separarlos.
4. Salitre / azufre — NO fusionar; decisión de uso distinto downstream.
5. Carbón / coque / carbón vegetal — los tres son distintos; el coque no sustituye al carbón vegetal, lo complementa. Mover coque a edad 9 (1709).
6. Plástico explícito — quitar si «derivados del petróleo» ya lo cubre en edad 13; si no, añadir como subproducto etario, no recurso base.

## Nota metodológica

Fuentes no cargadas: el artículo «Salt in History» devolvió 404 (usado «Salt» general). Las páginas específicas de Anno 1800, Empire Earth, Foundation y Manor Lords no enumeran recursos en Wikipedia; faltarían wikis de comunidad para conteos exactos. No se inventaron cifras.
