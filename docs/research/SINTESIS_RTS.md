# SINTESIS_RTS — Investigación de RTS libres e históricos para CHUNSA

**Autor:** investigador (MiniMax-M3). **Modelo evaluado:** SPEC-007 (recursos y
edades) y SPEC-004 §16–§23 (economía actual).
**Fecha:** 2026-07-28. **Fuentes:** 11 sitios web consultados; páginas crudas en
`docs/research/raw/`. **Estado de las fuentes:** Fandom (AoE2) y trac/wiki de
Wildfire Games y Widelands respondieron HTTP 402 / Anubis challenge — **no leí
esas páginas**. Wikipedia y GitHub funcionaron.

---

## §1 — Hallazgos AoE2 (vía Wikipedia; números finos no leídos)

[V] [wikipedia Age_of_Empires_II] 4 recursos: food/wood/gold/stone; food viene de
caza, bayas, ganado, granjas, pesca; gold de mina, reliquias, mercado; stone
solo de mina → [CHUNSA] nuestro `A, B, Me` se desdobla en 4 canales; el cuarto
(cobre/oro/estaño) abre 1 fuente y 3 sumideros que el SPEC-007 ya contempla.

[V] [wikipedia Age_of_Empires_II] edad por **doble gate**: coste en recursos +
edificios requeridos; nada de "solo un button" → [CHUNSA] SPEC-007 §2 ya separa
recurso/edad; conviene que el avance requiera **al menos 1 tech-epoch + 1
edificio** para no trivializar la escalera.

[V] [wikipedia Age_of_Empires_II] **mercado** con precios que fluctúan por
transacción; carros/barcos generan oro por **distancia recorrida** → [CHUNSA]
canal de comercio ya adulto en la literatura; muy sensible a no-trampa: si
algún día se modela comercio, debe ser determinista (radio entero, no haversine).

[V] [wikipedia Age_of_Empires_II] aldeano exige **edificio de dropoff** (TC,
mining camp, mill, lumber yard); no dropoff = no entrega → [CHUNSA] coherente
con SPEC-004 §16 (dropoff por edificio, fallback legacy); la cadena dep_dropoff
→ edificio en zona aliada (§23) debe **mantenerse** al pasar a 24 recursos.

[?] [aoe2.fandom.com] No leído (HTTP 402). **No cito gather rates por
aldeano** del Fandom. La regla "capacidad 10 / oro 0,38/s" del brief no la
verifiqué aquí; la marca es [?].

[?] [aoe2.fandom.com] Farm (coste, food total, replantación) — no leído. **No
invento** los 175 food por farm del WololoKingdoms. Lo que sí cruza: la propia
Wikipedia confirma que **granjas existen** y son una clase de food source, lo
cual valida que el §5 de SPEC-007 (granjas como "depósito regenerable") modela
correctamente una solución conocida.

[?] [aoe2.fandom.com] Quejas históricas sobre **automatismo de aldeanos** — no
leídas. Por nuestra propia auditoría (SPEC-004 §23.1) sabemos el síntoma: el
aldeano camina a recursos lejanos ignorando los cercanos. Eso es **una queja
del mismo tipo** ya documentada en nuestro repo.

---

## §2 — Open-source / libres

[V] [wikipedia 0_A.D.] 0 A.D. usa **peer-to-peer sin servidor central** → lockstep
forzado → [CHUNSA] la hipótesis lockstep de OpenRA/0ad confirma que un motor
determinista distribuido es la **norma** para RTS competitivo. Nuestro kernel
sin float + save+checksum va en la misma dirección.

[V] [wikipedia 0_A.D.] 0 A.D. calcula un **simulation hash** cuyo cómputo
optimizaron en A28 para reducir stuttering → [CHUNSA] vale la pena **medir el
coste de CHUNSA_STATE_V8** en escenarios grandes antes de ampliar el dominio
(risk: hoy son 1–64 entidades, mañana quizá 1–500 → cuidado con PRECISAR QUÉ
ENTRA).

[V] [wikipedia 0_A.D.] 0 A.D. tiene **3 fases in-game** (Village, Town, City)
que desbloquean unidades, edificios y techs → [CHUNSA] SPEC-007 §2 plantea 15
edades sin gate intermedio; **3 fases más finas** es una alternativa que el
Director debería ver: distingue "transición dura" (Villa→Town) de "transición
suave" (dentro de fase).

[V] [wikipedia Settlers_II / Widelands inventarios] El modelo Widelands/Settlers
II: **el jugador no controla al portador**, controla **topología de flags** y
**prioridad de transporte** → [CHUNSA] nuestro `citizen_task` (§22) le da a cada
aldeano una tarea única; el modelo flag/priority es **una alternativa más
determinista** para recursos voluminosos (comida a partir de edad 11). Decisión
abierta en SPEC-007 §7.

[V] [wikipedia Settlers_II] Cita textual del diseñador: "if you allow direct
control of the military or give more detailed control about what is transported
from where… it completely changes the game" → [CHUNSA] **alerta para SPEC-007**:
si abrimos "prioridad de transporte" por jugador, **rompemos la IA** de §19
hasta nuevo diseño. Esto justifica dejar el transporte opaco (lo que hoy es).

[V] [wikipedia Settlers_II] Computer Gaming World: "winning or losing is rooted
in economics" → [CHUNSA] la afirmación genérica justifica el §4 de SPEC-007:
si reabrir yacimientos por tech es **una decisión**, el juego entero se vuelve
sobre economía, no sobre quién tiene más acero en edad 8.

[V] [github SFTtech/openage] openage busca **moddabilidad radical** vía nyan
("mods que modan mods") → [CHUNSA] SPEC-007 §3.3 "índices declarados en datos"
va en la **misma dirección** que nyan. La opción más barata CHUNSA-compatible
no es nyan (sobreingeniería), sino **YAML con tipos tabla + herencia por
`extends:`**; el catálogo tipado de SPEC-002 §7 ya va por ahí.

[V] [github SFTtech/nyan] nyan tiene **'=  override, += extend, -= remove'** y
**Change<T>() / Add<T>()** como operaciones de patch → [CHUNSA] esos 3
operadores son **lo mínimo** para soportar parches de autoresidad externa
(experimentos del Director, mods de la comunidad). Si nuestro `data/recursos/`
no los distingue, los autores "machacan" el padre en vez de parchear.

[V] [github SFTtech/openage] openage README: "'gameplay is basically
non-functional', 'no network compatibility with the original', 'no binary
compatibility'" → [CHUNSA] **advertencia de scope**: reimplementar un RTS
existente drena décadas. Nuestro modelo de "partir de cero + inspirarse" (no
clonar) es lo correcto.

[I] [github OpenRA/OpenRA + sentido común] OpenRA **usa lockstep**; es público
no lo verifiqué en una página accesible hoy (wiki/Architecture 404) → [CHUNSA]
la afirmación pasa a [I] y **debe confirmarse** antes de citarla en la
auditoría.

---

## §3 — RTS históricos (Age/Starcraft-style)

[V] [wikipedia Rise_of_Nations] 8 edades, 6 recursos **infinitos**, 4
condiciones de victoria, 100+ unidades, formación de 3 soldados → [CHUNSA] 6
recursos infinitos contradice nuestro §4 (reserva finita + reabrir por tech);
la **inversión** es nuestro factor: exploramos otro aesthetic (curva de
agotamiento + extensión por tech), no más infinito.

[V] [wikipedia Rise_of_Nations] "Buildings only constructable within own/ally
territory (Lakota excepted)" → [CHUNSA] **coherente con SPEC-004 §23**: la idea
de "no construir lejos de casa" es histórica. La excepción "Lakota sin
territorio" es exactamente lo que nuestro §23.3 ya plasma: la orden del jugador
no está acotada.

[V] [wikipedia Empire_Earth] Empire Earth 1: **14 épocas por 500.000 años**, 21
naciones "from every age and location" → [CHUNSA] la **escala de 14–15 edades
no es absurda**; encaja con SPEC-007 §2. El fallo fue diseño (EE3 con 50%
recepción), no la cantidad de edades.

[V] [wikipedia Total_Annihilation] 2 recursos streaming infinitos: metal + energía
→ [CHUNSA] nuestro "energético" de SPEC-007 §3.2 (carbón, coque, electricidad)
**es** el modelo TA; **recomiendo** que CHUNSA documente que ese canal es
TA-style streaming, no aldeano-recolectado. Concreta: del §3.2 la **electricidad
de la edad 12** no es "un recurso más", es **energía** como TA.

[V] [wikipedia Total_Annihilation] **Nanostalling**: "production across the board
will slow to a rate proportional to the amount by which outflow exceeds income"
→ [CHUNSA] **decisión abierta**: si un jugador pide +acero del que la energía
permite, ¿**alarga** el tiempo de producción (TA) o **encola** y espera
(Anno 1800)? Para SPEC-007 §3.2 nuestra def. **debe** decir qué pasa cuando
la energía falta. Si no, el jugador no entiende por qué su fundición se para.

[V] [wikipedia Total_Annihilation] Commanders y estructuras: "Out of energy? power-
dependent structures such as radar towers, metal extractors, and laser towers
will cease to function" → [CHUNSA] necesito **gatear** las recetas de
SPEC-007 §3.2 por **energía** explícitamente. Si la energía falta, la receta
se **para** (no se ralentiza), exactamente igual que TA. Esto es la única forma
de que la edad 14 (aluminio por electrólisis) no sea idéntica a la edad 13.

[V] [wikipedia Warcraft_III] **Upkeep**: "producing units over certain amounts
will decrease the amount of gold one can earn" → [CHUNSA] **alerta para edad
9+**: a partir de Oceánica, la base de jugadores va a producir muchos
barcos/aviones. Sin un **upkeep** o un **coste de operación** por flota, la
LSEA no curva y el late game es un festín. La propuesta de upkeep **no está**
en SPEC-007; es un gap.

[V] [wikipedia Warcraft_II] Town Hall **mejorable 2 veces**, cada nivel **sube
la carga por viaje** → [CHUNSA] alternativa al modelo **granja** de §5: subir
la **carga del aldeano** por age-of-age, no por tech de extracción. Si
adoptamos, el coste de la mejora es menor que el coste de plantar 4 granjas.

[V] [wikipedia Warzone_2100] No árbol de tech visible; tech viene de **artefactos
de enemigos** + investigación incremental → [CHUNSA] **alternativa a SPEC-007
§4.2**: en vez de recovery_pct como número %, permitir que el **depósito mismo**
exponga su tecnología (galería, flotación) y el jugador la descubra al
explotarlo. Reduce la fatiga del árbol.

[V] [wikipedia Warzone_2100] Petróleo en **localizaciones concretas**, produce
ingreso lento; mapa + tiempo = ↓ turtling → [CHUNSA] coherente con nuestra
§4: los depósitos de nuestro mapa son **geometría**, no spawn continuo. La
ventaja CHUNSA es que el mapa tiene **reserva_total** en datos — equilibrio por
diseño del mapa, no por spawn.

[V] [wikipedia Anno_1800] **Distribución por regiones**: Old World = industria+ciudadanos;
New World = materias primas; Enbesa = propias → [CHUNSA] **elección de
arquitectura**: si llegan a 24 recursos, el **HUD no soporta** 24 contadores.
La solución Anno es **regiones**. La solución directa CHUNSA es **grupos** en
el HUD (ej. "metalúrgicos: cobre/estaño/bronce/hierro/acero/coque") — anoto
como decisión §7 abierta.

[V] [wikipedia Anno_1800] **Attractiveness** como tensión; cada industria baja la
atractividad del área → [CHUNSA] si nuestra edad 11+ densifica la producción
industrial, el jugador va a **necesitar** macro-parcelación; un único terreno
homogéneo se vuelve inmanejable. **Stress test del mapa** en 1.10 antes de
ship.

[V] [wikipedia Civilization_VI] Distritos en **hexes separados** = nuevo nivel de
UI; loyalty/free cities redirigen yields → [CHUNSA] **no pertinente** ahora;
Civilization no es un RTS. Lo anoto solo como **anti-patrón**: separar por
hexes exige un plan de UI que no tenemos. Si algún día CHUNSA crece a
4 dimensiones, **primero** consolidar 2.

[V] [wikipedia Civilization_VI] **Eureka moments**: el bonus viene de **engagement
con el mapa** (cantera junto a tech de masonry) → [CHUNSA] **decisión abierta**
para SPEC-007 §4: en vez de "mining tech 60% global", premiar **combinaciones:**
"si tu mina está en colina, multiplicador ×1.2". Esto sí es justo y **no leo
de fuente específica** — [I].

[V] [wikipedia Mindustry] El transporte es **conveyor físico** (items en cinta)
→ [CHUNSA] **no encaja** con nuestro `carry` entero abstracto. Eso sí, Mindustry
revela el **coste de gestionar cintas**: layouts llegan a ser el problema. Si
CHUNSA hace transporte abstracto, no debería abrir después un modo "cintas"
sin otra justificación.

[V] [wikipedia Beyond_All_Reason] 2 recursos streaming (energía + metal);
energía wind/solar/geothermal/nuclear → [CHUNSA] **no contradice** SPEC-007
§3.2; sólo confirma que la **diversidad de fuentes energéticas** es la forma
estándar de esconder un solo recurso (energía) bajo varias mecánicas.

---

## §4 — Tabla comparativa de modelos de recurso

| Juego | Recursos | Finito/Infinito | Modelo de aldeano | Fuente |
|---|---|---|---|---|
| AoE2 | 4 | Finito (minas), renewable (granjas) | Capacidad, dropoff-edificio | [?] fandom · [V] Wikipedia |
| Rise of Nations | 6 | Infinito | Citizens en territorio | [V] Wikipedia |
| Empire Earth | n/d, 14 épocas | n/d | n/d | [V] Wikipedia |
| 0 A.D. | n/d | n/d | 3 fases (Village→Town→City) | [V] Wikipedia |
| Total Annihilation | 2 (metal+energía) | Ambos infinitos | Commander + extractors | [V] Wikipedia |
| Warcraft III | 3 (oro/madera/food) | Oro finito, madera finita | Upkeep soft-cap | [V] Wikipedia |
| Warcraft II | 3 (oro/wood/oil) | Finitos | Hall upgrade sube carga | [V] Wikipedia |
| Warzone 2100 | 1 (petróleo) | Por localización | n/d | [V] Wikipedia |
| Anno 1800 | 30+ | Por tier | Tier-locked | [V] Wikipedia |
| CHUNSA hoy | 3 (A/B/Me) | Finito | §18 GATHER, §23 zona aliada | SPEC-004 |
| CHUNSA propuesto | 24 | Finito + reabrible por tech | (id.) | SPEC-007 |

---

## §5 — Edad como gate (cuadros)

| Juego | Avance requiere | Mecanismo dual | Fuente |
|---|---|---|---|
| AoE2 | Coste en recursos + edificios | **Sí** (doble gate) | [V] Wikipedia |
| CHUNSA propuesto | `epoch_window` + `tech.epoch ≤ player_epoch` | **Sí** (recurso + tech) | SPEC-007 §2 |
| Empire Earth | Cambio de época con civ-pick | Ambiguo | [V] Wikipedia |
| Rise of Nations | Monumento/avance + coste | **Sí** (recurso + edad previa) | [V] Wikipedia |
| 0 A.D. | Phase por trigger (¿building?) | n/d (no leído) | [V] Wikipedia |

---

## §6 — Aplicable a CHUNSA ya (10)

1. **Nanostalling** para los energéticos de SPEC-007 §3.2 (carbo­eléctrica,
   Hall-Héroult): sin energía, la receta **se para**, no se ralentiza. Mejor
   que la "cola de espera" de Anno para reflejar ciencia real. (References
   Total Annihilation.)
2. **Documentar 24 recursos como híbrido**: 22 aldeano-recolectados/producidos
   + 2 **streaming** (energía, electricidad). El modelo no es estable para
   los 24 — es estable para 22 + 1 (energía). Definir **qué** se almacena en
   `player_stock` (aldeano) vs **qué** se deriva (energía = max(0, prod −
   cons)).
3. **Town-Hall-upgrade-style** como **alternativa** a §5 granjas: en lugar de
   edificios renovables, subir la **capacidad del aldeano** por edad. Más
   barato en `ECO_MAX_DEPOSITS` (sigue 32), más coherente con Warcraft II.
4. **Doble gate de edad**: avanzar de Feudal a Castle ya exige edificio +
   recurso; **no relajar** eso en SPEC-007 §2. La "Scala de Anubis" de AoE2
   funciona porque el jugador **siempre** tiene 2 cosas que pensar.
5. **Eureka moments** (Civ VI) sobre la curva de extracción: si la mina está
   en colina, multiplier ×1.2; si la mina de oro está en una colina cercana a
   la base, multiplicador ×1.1. Insignificante para el jugador medio, **vital**
   para el competitivo. Costo de implementación: 1 campo más en `deposits[]`.
6. **Boost de fase** (0 A.D.): en vez de 15 edades planas, **3 fases** (Village /
   Town / City) con **transición dura** entre ellas. Cada fase = 5 edades,
   pero la fase marca un evento narrativo (desbloquea un cinematic simple).
   Reduce la monotonía visual sin tocar economía.
7. **Attractiveness / spacing** (Anno 1800): para 1.10, **test del mapa** con
   4 jugadores y cada uno con 8 edificios industriales. Si el mapa queda
   "rallado", ampliar a `ECO_MAX_DEPOSITS = 128` **antes** de la edad 11.
8. **Town/Hall upgrade** vs **recovery_pct** (Warcraft II + SPEC-007 §4): la
   propuesta de "tecnología reabre yacimiento" es **una de las dos** formas de
   modelar progreso. La otra (Warcraft II) es **el aldeano crece**. Si
   nuestro objetivo es **distinguir civilizaciones**, la primera (recovery_pct
   por civ) es mejor; si el objetivo es **curva de edad**, la segunda (carga
   del aldeano por edad) es mejor. **Decisión abierta**.
9. **Hash de simulación con coste medido** (0 A.D. A28): antes de ampliar el
   dominio del checksum (sprint 1.8→1.10), **medir** cuánto tiempo le lleva
   a `CHUNSA_STATE_V8` en un escenario con 4 jugadores, 64 entidades cada
   uno, 32 depósitos. Si > 10 µs / tick, **dividir** el dominio antes de 1.10.
10. **Simetría de spawns** (Warcraft II + Anno): SPEC-004 §15.1 ya exige
    simetría en `resource_spawns`. La regla de **conteo impar → eje central**
    es **exactamente** lo que Warcraft II/AoE2 llaman "mapa mirror". Si
    mantenemos esa regla en 1.8 (24 recursos), `data/maps/*.yaml` debe
    consignar la simetría **en metadata**, no solo en posiciones.

---

## §7 — Contradice nuestra propuesta

Esta sección es lo más valioso del informe. Lee antes de fusionar SPEC-007.

1. **[V] [wikipedia Rise_of_Nations] "6 recursos infinitos"** → nuestro
   **postulado base** de SPEC-007 §4 (depósito como `reserve_total` finito que
   se reabre por tech) **NO es estándar**. RoN explícitamente eligió infinito
   para no penalizar al jugador por exhausting. Si nuestra razón de ser es
   **distinguir decisiones**, finito+reabrible es defendible; si la razón es
   **balance puro**, **replantear**.
2. **[V] [wikipedia Total_Annihilation] "los 2 recursos son infinitos"** → un
   RTS competitivo serio (TA, BAR, SupCom) **no modela agotamiento**. Si
   nuestro target es el mismo jugador (competitivo-hardcore), el modelo
   de SPEC-007 §4 puede generar **rechazo** por "esfuerzo obligatorio". El
   juego de Anno 1800, único que sí modela agotamiento, **lo aguanta** por ser
   casual-cívico, no RTS.
3. **[V] [wikipedia Total_Annihilation] "Out of energy?… structures will cease
   to function"** → SPEC-007 §3.2 dice que la energía **es un insumo** más
   (igual que cobre o estaño). Eso es **incorrecto físicamente y
   computacionalmente**: la energía **no se almacena por aldeano**, no se
   "agota como insumo"; **se gasta al instante**. La distinción de datos
   "Producido vs Recolectado" del §3.1 **no captura** el caso energía. Hay
   que añadir **"Streaming"** como tercera naturaleza o **no** modelar
   energía como recurso (modelarla como propiedad de la `GameState`,
   no como `player_stock` índice).
4. **[V] [wikipedia Settlers_II] "if you allow direct control of the military
   or give more detailed control about what is transported from where… it
   completely changes the game"** → SPEC-007 §4 (per_jugador
   `recovery_pct`) **es control fino del jugador** sobre cada yacimiento. El
   Diseñador de Settlers II **advirtió** explícitamente que este tipo de
   control cambia el género. O SPEC-007 §4 es opt-in (no afecta al jugador
   casual) o **rompe** la paciencia del principiante. Recomendación: derivado
   de `player_caps` (como propone §4.3) **solo** — nunca editable a mano.
5. **[V] [wikipedia Warzone_2100] "no hay árbol de tech visible; los tech se
   descubren por artefactos"** → el **vector de presión** de SPEC-007 §4 (reabre
   minas) es **decorativo** sin un sistema de descubrimiento. Si los
   `recovery_pct` los "activa" linealmente el paso del tiempo, la edad 11
   queda **vacía** de decisiones. El Director tiene que responder: ¿el
   jugador **elige** cuándo investigar la galería, o el juego **se la da**
   al subir de edad? Si lo segundo, no es estudio del mapa, es un botón.
6. **[V] [wikipedia Warcraft_III] "upkeep: producing units over certain amounts
   will decrease the amount of gold one can earn"** → nuestro SPEC-007 §2 no
   tiene **ningún mecanismo de upkeep** post-edad 9. Sin eso, el late game
   es **incontrolable**. No basta con "menos aldeanos por edificio": hay que
   añadir un **coste de mantenimiento** (oro/oro-equivalente) que crezca con
   cada edad.
7. **[V] [wikipedia 0_A.D.] "3 fases in-game: Village / Town / City"** →
   nuestra escalera de **15 edades** es **fina de más** para un RTS. Si 0 A.D.
   (que sí es RTS histórico) lo simplifica a 3 fases, **15 es excesivo**.
   Cada Edad sin mecánica propia es **ruido**. La mitad de las edades de
   SPEC-007 §2 (la 6, 9, 10 y posiblemente la 9 y la 14) **no introducen
   recurso**; admiten que "no toda edad tiene que hacerlo", pero 15 edades
   sin mecánica aditiva plantearon **15 discursos de phase change** sin
   payoff claro. **Recomiendo 7–9 edades** + 3 fases narrativas.
8. **[V] [wikipedia Anno_1800] "distribución por regiones"** → nuestro plan
   de 24 recursos en **un solo mapa** no escala. Anno 1800 necesitó **3
   regiones** para aguantar 30+ recursos simultáneos. Si CHUNSA va a 24
   en una sola región, **la pantalla inferior del HUD** (4 columnas × 3 filas
   en el mejor caso) **se vuelve inmanejable**. Recomiendo **dividir el
   árbol visual** en 4 familias: trópicos (comida, madera, piedra, granjas),
   metales-base (cobre, estaño, mena de hierro, carbón), metalurgia (bronce,
   hierro forjado, coque, acero, aluminio), energético (electricidad,
   pólvora). **5 subí­ndices** × 1 línea = legible.
9. **[V] [wikipedia Empire_Earth] "Empire Earth III: 50% peor recibida"** →
   la serie homogénea de **3 juegos con 14+ edades** no sobrevivió al
   mercado. **Empire Earth** (82%) → **EE2** (79%) → **EE3** (50%). El
   mensaje: añadir **más granularidad** a la Edad **no mejora** el juego;
   el techo de público está en **8–10 edades**.
10. **[I] [OpenRA, sentido común, no leído de fuente accesible]** OpenRA
    confirma lockstep como **estado del arte** RTS open-source. CHUNSA está
    en la **posición correcta** con su `CHECKSUM_ALGO_VERSION`. Pero el
    siguiente juego de la rama (CHUNSA II) **debería** heredar el hábito de
    **publicar una métrica de "ms por checksum"** en cada sprint. Es el
    único modo de no degradar lockstep silenciosamente.

---

## §8 — Pendientes (no investigados)

1. **Gather rates de AoE2 por aldeano** (food/wood/gold/stone) — el Fandom
   devuelve 402. [?]. Si una página de la comunidad está accesible por
   otro canal, **valorar**.
2. **Datos finos de Warzone 2100** (capacidad de oil derrick, tiempo de
   investigación) — Wikipedia solo describe mecánicas generales.
3. **Widelands — un fichero `init.lua` real** de un edificio de producción —
   no encontré URL accesible al repositorio con fetch directo. **Obtener**
   por `gh` o descarga directa.
4. **OpenRA — wiki de Arquitectura** — la página de wiki/Architecture no
   se carga (404 en una rama distinta). No confirmé el patrón de Sync hash.
5. **Cantidad histórica de Aldeanos en el clásico AoE2** — 50 aldeanos es
   "óptimo" en estrategia moderna; **no leí cifras**.
6. **Datos concretos de Rise of Nations (costes, tiempos)** — no leídos.
7. **Songs of Syx** — Wikipedia no tiene página en este momento según
   mi fetch; no pude recabar material.

---

## §9 — Fuentes consultadas (índice)

| URL | Bloque | Estado |
|---|---|---|
| ageofempires.fandom.com/wiki/Resources_(Age_of_Empires_II) | AoE2 | HTTP 402 |
| ageofempires.fandom.com/wiki/Villager_(Age_of_Empires_II) | AoE2 | HTTP 402 |
| ageofempires.fandom.com/wiki/Farm_(Age_of_Empires_II) | AoE2 | HTTP 402 |
| ageofempires.fandom.com/wiki/Age_(Age_of_Empires_II) | AoE2 | HTTP 402 |
| ageofempires.fandom.com/wiki/Gold_Mine | AoE2 | HTTP 402 |
| ageofempires.fandom.com/wiki/Stone_Mine | AoE2 | HTTP 402 |
| en.wikipedia.org/wiki/Age_of_Empires_II | AoE2 | OK |
| en.wikipedia.org/wiki/0_A.D._(video_game) | 0 A.D. | OK |
| trac.wildfiregames.com/wiki/Manual_Settings | 0 A.D. | Anubis |
| trac.wildfiregames.com/wiki/SimulationArchitecture | 0 A.D. | Anubis |
| gitea.wildfiregames.com/0ad/0ad | 0 A.D. | Anubis |
| play0ad.com | 0 A.D. | OK, baja señal |
| en.wikipedia.org/wiki/Widelands | Widelands | OK (resumen) |
| en.wikipedia.org/wiki/Settlers_II | Widelands | OK (estructura) |
| www.widelands.org/wiki/Economy/ | Widelands | Anubis |
| www.widelands.org/wiki/Buildingtypes/ | Widelands | Anubis |
| www.widelands.org/wiki/Economy_tutorial/ | Widelands | Anubis |
| github.com/SFTtech/openage | openage | OK |
| github.com/SFTtech/nyan | openage | OK |
| openage.dev / openage.sft.lol | openage | OK (página principal) |
| github.com/OpenRA/OpenRA | OpenRA | OK (raíz) |
| github.com/OpenRA/OpenRA/wiki | OpenRA | OK (índice) |
| github.com/OpenRA/OpenRA/wiki/Architecture | OpenRA | 404 |
| en.wikipedia.org/wiki/Beyond_All_Reason | BAR | OK |
| en.wikipedia.org/wiki/Rise_of_Nations | RoN | OK |
| en.wikipedia.org/wiki/Empire_Earth | EE | OK |
| en.wikipedia.org/wiki/Total_Annihilation | TA | OK |
| en.wikipedia.org/wiki/Warcraft_III:_Reign_of_Chaos | WC3 | OK |
| en.wikipedia.org/wiki/Warcraft_II:_Tides_of_Darkness | WC2 | OK |
| en.wikipedia.org/wiki/Warzone_2100 | WZ | OK |
| en.wikipedia.org/wiki/Anno_1800 | Anno | OK |
| en.wikipedia.org/wiki/Civilization_VI | Civ VI | OK |
| en.wikipedia.org/wiki/Mindustry | Mindustry | OK |
| en.wikipedia.org/wiki/List_of_open-source_video_games | panorama | OK |

**Páginas crudas:** `docs/research/raw/{aoe2_resources_fandom_HTTP402,
aoe2_wikipedia, 0ad_wikipedia, widelands_settlers2_wikipedia, openage_github,
openra_github, rts_panorama_wikipedia}.md`.
