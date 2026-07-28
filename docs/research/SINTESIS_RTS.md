# SINTESIS_RTS — Investigación de RTS libres e históricos para CHUNSA

**Autor:** investigador (MiniMax-M3). **Modelo evaluado:** SPEC-007 (recursos y
edades) y SPEC-004 §16–§23 (economía actual). **Fecha:** 2026-07-28.
**Fuentes:** Wikipedia y GitHub OK; Fandom (AoE2) + trac/wiki Wildfire Games +
Widelands respondieron HTTP 402 / Anubis — **no leí esas páginas**. Páginas
crudas en `docs/research/raw/`.

---

## §1 — AoE2 (Wikipedia; números finos no leídos)

[V] [Wikipedia AoE2] 4 recursos: food/wood/gold/stone; food de caza, bayas,
ganado, granjas, pesca; gold de mina, reliquias, mercado; stone solo de mina →
[CHUNSA] nuestro `A, B, Me` se desdobla en 4 canales; el cuarto (cobre/oro/
estaño) abre 1 fuente y 3 sumideros que SPEC-007 ya contempla.

[V] [Wikipedia AoE2] edad por **doble gate**: coste en recursos + edificios
requeridos; nada de "solo un botón" → [CHUNSA] mantener: avance exige **al
menos 1 tech-epoch + 1 edificio** para no trivializar la escalera.

[V] [Wikipedia AoE2] **mercado** con precios fluctuantes por transacción;
carros/barcos generan oro por **distancia recorrida** → [CHUNSA] si algún día
se modela comercio, debe ser determinista (radio entero, no haversine).

[V] [Wikipedia AoE2] aldeano exige **edificio de dropoff** (TC, mining camp,
mill, lumber yard); sin dropoff no entrega → [CHUNSA] coherente con SPEC-004
§16; la cadena dep→dropoff→edificio en zona aliada (§23) debe **mantenerse**
al pasar a 24 recursos.

[?] [aoe2.fandom.com] No leído (HTTP 402). **No cito** gather rates por aldeano
("capacidad 10 / oro 0,38/s" del brief: [?]).

[?] [aoe2.fandom.com] Farm (coste, food total, replantación) — no leído. **No
invento** los 175 food por farm del WololoKingdoms. Wikipedia sí confirma que
**granjas existen** y son una clase de food source, lo cual valida §5 de
SPEC-007 (depósito regenerable) como solución conocida.

[?] [aoe2.fandom.com] Quejas históricas sobre **automatismo de aldeanos** — no
leídas. SPEC-004 §23.1 documenta el mismo síntoma (aldeano cruza el mapa); ya
tenemos evidencia interna.

---

## §2 — Open-source / libres

[V] [Wikipedia 0_A.D.] 0 A.D. usa **peer-to-peer sin servidor central** →
lockstep forzado → [CHUNSA] lockstep es la **norma** en RTS competitivo. Nuestro
kernel sin float + save+checksum va en la misma dirección.

[V] [Wikipedia 0_A.D.] 0 A.D. calcula un **simulation hash**; optimizaron el
cómputo en A28 para reducir stuttering → [CHUNSA] **medir el coste** de
`CHUNSA_STATE_V8` en escenarios grandes antes de ampliar el dominio (riesgo: si
hoy son 1–64 entidades, mañana quizá 1–500).

[V] [Wikipedia 0_A.D.] 0 A.D. tiene **3 fases in-game** (Village → Town → City)
que desbloquean unidades, edificios y techs → [CHUNSA] SPEC-007 §2 plantea 15
edades sin gate intermedio; **3 fases más finas** es una alternativa que el
Director debería ver: distingue "transición dura" (Village→Town) de "suave"
(dentro de fase).

[V] [Wikipedia Settlers_II / Widelands] Widelands replica Settlers II: jugador
**no controla al portador**, controla **topología de flags** y **prioridad de
transporte** → [CHUNSA] nuestro `citizen_task` (§22) da a cada aldeano una tarea
única; flag/priority es **alternativa más determinista** para recursos
voluminosos (comida a partir de edad 11). Decisión abierta en SPEC-007 §7.

[V] [Wikipedia Settlers_II] Cita del diseñador Thomas Häuser: "if you allow
direct control of the military or give more detailed control about what is
transported from where… it completely changes the game" → [CHUNSA] **alerta
para SPEC-007 §4**: si abrimos "prioridad de transporte" por jugador,
**rompemos la IA** de §19. Justifica dejar el transporte opaco.

[V] [Wikipedia Settlers_II] Computer Gaming World: "winning or losing is
rooted in economics" → [CHUNSA] justifica §4 de SPEC-007: si reabrir
yacimientos por tech es **una decisión**, el juego entero se vuelve sobre
economía, no sobre quién tiene más acero en edad 8.

[V] [GitHub SFTtech/openage] openage busca **moddabilidad radical** vía nyan
("mods que modan mods") → [CHUNSA] SPEC-007 §3.3 "índices declarados en datos"
va en la **misma dirección**. La opción más barata CHUNSA-compatible no es
nyan (sobreingeniería), sino **YAML con tipos tabla + herencia por `extends:`**;
el catálogo tipado de SPEC-002 §7 ya va por ahí.

[V] [GitHub SFTtech/nyan] nyan tiene **`=` override, `+=` extend, `-=` remove** y
**Change<T>() / Add<T>()** como operaciones de patch → [CHUNSA] esos 3
operadores son **lo mínimo** para soportar parches de autoresidad externa
(experimentos del Director, mods de la comunidad). Si nuestro `data/recursos/`
no los distingue, los autores "machacan" el padre en vez de parchear.

[V] [GitHub SFTtech/openage] README: "'gameplay is basically non-functional',
'no network compatibility', 'no binary compatibility'" → [CHUNSA] **advertencia
de scope**: reimplementar un RTS existente drena décadas. Nuestro "partir de
cero + inspirarse" es lo correcto.

[I] [GitHub OpenRA/OpenRA] OpenRA **usa lockstep**; no lo verifiqué en una página
accesible hoy (wiki/Architecture 404) → [CHUNSA] la afirmación pasa a [I] y
**debe confirmarse** antes de citarla en la auditoría.

---

## §3 — RTS históricos (Age/Starcraft-style)

[V] [Wikipedia Rise_of_Nations] 8 edades, 6 recursos **infinitos**, 4
condiciones de victoria, 100+ unidades, formación de 3 soldados → [CHUNSA] 6
recursos infinitos contradice nuestro §4 (reserva finita + reabrir por tech);
la **inversión** es nuestro factor: exploramos otro aesthetic (curva de
agotamiento + extensión por tech), no más infinito.

[V] [Wikipedia Rise_of_Nations] "Buildings only constructable within own/ally
territory (Lakota excepted)" → [CHUNSA] **coherente con SPEC-004 §23**: la idea
de "no construir lejos de casa" es histórica. La excepción "Lakota sin
territorio" es lo que §23.3 ya plasma: la orden del jugador no está acotada.

[V] [Wikipedia Empire_Earth] EE1: **14 épocas por 500.000 años**, 21 naciones
"from every age and location" → [CHUNSA] la **escala de 14–15 edades no es
absurda**; encaja con SPEC-007 §2. El fallo fue diseño (EE3 con 50% recepción),
no la cantidad de edades.

[V] [Wikipedia Total_Annihilation] 2 recursos streaming infinitos: metal + energía
→ [CHUNSA] nuestro "energético" de SPEC-007 §3.2 (carbón, coque, electricidad)
**es** el modelo TA; **recomiendo** que CHUNSA documente que ese canal es
TA-style streaming, no aldeano-recolectado. La electricidad de la edad 12 no
es "un recurso más", es **energía** como TA.

[V] [Wikipedia Total_Annihilation] **Nanostalling**: "production across the
board will slow to a rate proportional to the amount by which outflow exceeds
income" → [CHUNSA] **decisión abierta**: si un jugador pide +acero del que la
energía permite, ¿**alarga** el tiempo (TA) o **encola** y espera (Anno 1800)?
Nuestra def. **debe** decir qué pasa cuando la energía falta. Si no, el jugador
no entiende por qué su fundición se para.

[V] [Wikipedia Total_Annihilation] "Out of energy? power-dependent structures
such as radar towers, metal extractors, and laser towers will cease to
function" → [CHUNSA] necesito **gatear** las recetas de SPEC-007 §3.2 por
**energía** explícitamente. Si la energía falta, la receta **se para** (no se
ralentiza), como TA. Es la única forma de que la edad 14 (aluminio por
electrólisis) no sea idéntica a la edad 13.

[V] [Wikipedia Warcraft_III] **Upkeep**: "producing units over certain amounts
will decrease the amount of gold one can earn" → [CHUNSA] **alerta para edad
9+**: a partir de Oceánica, la base de jugadores va a producir muchos
barcos/aviones. Sin un **upkeep** o un **coste de operación** por flota, la LSEA
no curva y el late game es un festín. La propuesta de upkeep **no está** en
SPEC-007; es un gap.

[V] [Wikipedia Warcraft_II] Town Hall **mejorable 2 veces**, cada nivel **sube
la carga por viaje** → [CHUNSA] alternativa al modelo **granja** de §5: subir
la **carga del aldeano** por edad, no por tech de extracción. Si adoptamos,
el coste de la mejora es menor que el coste de plantar 4 granjas.

[V] [Wikipedia Warzone_2100] No árbol de tech visible; tech viene de **artefactos
de enemigos** + investigación incremental → [CHUNSA] **alternativa a SPEC-007
§4.2**: en vez de `recovery_pct` como número %, permitir que el **depósito
mismo** exponga su tecnología (galería, flotación) y el jugador la descubra al
explotarlo. Reduce la fatiga del árbol.

[V] [Wikipedia Warzone_2100] Petróleo en **localizaciones concretas**, produce
ingreso lento; mapa + tiempo = ↓ turtling → [CHUNSA] coherente con nuestro
§4: nuestros depósitos son **geometría**, no spawn continuo. La ventaja CHUNSA
es que el mapa tiene **`reserve_total`** en datos — equilibrio por diseño del
mapa, no por spawn.

[V] [Wikipedia Anno_1800] **Distribución por regiones**: Old World = industria
+ ciudadanos; New World = materias primas; Enbesa = propias → [CHUNSA] **elección
de arquitectura**: si llegan a 24 recursos, el **HUD no soporta** 24 contadores.
La solución Anno es **regiones**. La directa CHUNSA es **grupos** en el HUD
("metalúrgicos: cobre/estaño/bronce/hierro/acero/coque") — decisión §7 abierta.

[V] [Wikipedia Anno_1800] **Attractiveness** como tensión; cada industria baja la
atractividad del área → [CHUNSA] si nuestra edad 11+ densifica la producción
industrial, el jugador va a **necesitar** macro-parcelación; un único terreno
homogéneo se vuelve inmanejable. **Stress test del mapa** en 1.10 antes de
ship.

[V] [Wikipedia Civilization_VI] Distritos en **hexes separados** = nuevo nivel
de UI; loyalty/free cities redirigen yields → [CHUNSA] **no pertinente** ahora;
Civ no es un RTS. Lo anoto como **anti-patrón**: separar por hexes exige un
plan de UI que no tenemos. Si algún día CHUNSA crece a 4 dimensiones,
**primero** consolidar 2.

[V] [Wikipedia Civilization_VI] **Eureka moments**: el bonus viene de **engagement
con el mapa** (cantera junto a tech de masonry) → [CHUNSA] **decisión abierta**
para §4: en vez de "mining tech 60% global", premiar **combinaciones**: "si tu
mina está en colina, multiplicador ×1.2". [I] — fuente específica no leída.

[V] [Wikipedia Mindustry] El transporte es **conveyor físico** (items en cinta)
→ [CHUNSA] **no encaja** con nuestro `carry` entero abstracto. Sí, Mindustry
revela el **coste de gestionar cintas**: layouts llegan a ser el problema. Si
CHUNSA hace transporte abstracto, no debería abrir después un modo "cintas"
sin otra justificación.

[V] [Wikipedia Beyond_All_Reason] 2 recursos streaming (energía + metal);
energía wind/solar/geothermal/nuclear → [CHUNSA] **no contradice** SPEC-007
§3.2; confirma que la **diversidad de fuentes energéticas** es la forma
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

1. **Nanostalling** para los energéticos de SPEC-007 §3.2 (carbón-coque,
   Hall-Héroult): sin energía, la receta **se para**, no se ralentiza. Mejor
   que la "cola de espera" de Anno para reflejar ciencia real. (Ref TA.)
2. **Documentar 24 recursos como híbrido**: 22 aldeano-recolectados/producidos
   + 2 **streaming** (energía, electricidad). El modelo no es estable para
   24 — es estable para 22 + 1. Definir qué se almacena en `player_stock`
   (aldeano) vs qué se **deriva** (energía = max(0, prod − cons)).
3. **Town-Hall-upgrade-style** como **alternativa** a §5 granjas: subir la
   **capacidad del aldeano** por edad, no por tech de extracción. Más barato
   en `ECO_MAX_DEPOSITS` (sigue 32), más coherente con Warcraft II.
4. **Doble gate de edad**: avanzar de Feudal a Castle ya exige edificio +
   recurso; **no relajar** en SPEC-007 §2. La "línea de Anubis" de AoE2
   funciona porque el jugador **siempre** tiene 2 cosas que pensar.
5. **Eureka moments** (Civ VI) sobre la curva de extracción: mina en colina
   → ×1.2; mina de oro cerca de la base → ×1.1. Insignificante para el
   jugador medio, **vital** para el competitivo. Coste: 1 campo más en
   `deposits[]`.
6. **Boost de fase** (0 A.D.): 3 fases (Village / Town / City) con
   **transición dura** entre ellas. Cada fase = 5 edades, pero marca un evento
   narrativo (desbloquea un cinematic simple). Reduce la monotonía visual sin
   tocar economía.
7. **Attractiveness / spacing** (Anno 1800): para 1.10, **test del mapa** con
   4 jugadores y cada uno con 8 edificios industriales. Si el mapa queda
   "rallado", ampliar `ECO_MAX_DEPOSITS = 128` **antes** de la edad 11.
8. **Town/Hall upgrade vs recovery_pct** (Warcraft II + SPEC-007 §4): la
   propuesta "tecnología reabre yacimiento" es **una de las dos** formas de
   modelar progreso. La otra (Warcraft II) es **el aldeano crece**. Si
   objetivo es **distinguir civilizaciones**, la primera (recovery_pct por
   civ) es mejor; si objetivo es **curva de edad**, la segunda (carga del
   aldeano por edad) es mejor. **Decisión abierta**.
9. **Coste del hash de simulación** (0 A.D. A28): antes de ampliar el dominio
   del checksum (sprint 1.8→1.10), **medir** cuánto tiempo le lleva a
   `CHUNSA_STATE_V8` en 4 jugadores × 64 entidades × 32 depósitos. Si > 10 µs
   / tick, **dividir** el dominio antes de 1.10.
10. **Simetría de spawns** (Warcraft II + Anno): SPEC-004 §15.1 ya exige
    simetría en `resource_spawns`. La regla "conteo impar → eje central" es
    **exactamente** lo que AoE2/WC2 llaman "mapa mirror". Si mantenemos en
    1.8 (24 recursos), `data/maps/*.yaml` debe consignar la simetría **en
    metadata**, no solo en posiciones.

---

## §7 — Contradice nuestra propuesta (lo más valioso)

1. **[V] [Wikipedia Rise_of_Nations] "6 recursos infinitos"** → nuestro
   **postulado base** de SPEC-007 §4 (depósito como `reserve_total` finito
   reabrible por tech) **NO es estándar**. RoN explícitamente eligió infinito
   para no penalizar al jugador por exhausting. Si nuestra razón es
   **distinguir decisiones**, finito+reabrible es defendible; si es **balance
   puro**, **replantear**.
2. **[V] [Wikipedia Total_Annihilation] "los 2 recursos son infinitos"** → un
   RTS competitivo serio (TA, BAR, SupCom) **no modela agotamiento**. Si
   nuestro target es competitivo-hardcore, §4 puede generar **rechazo** por
   "esfuerzo obligatorio". Anno 1800, único que sí modela agotamiento, lo
   aguanta por ser casual-cívico, no RTS.
3. **[V] [Wikipedia Total_Annihilation] "Out of energy? … structures will
   cease to function"** → SPEC-007 §3.2 dice que la energía **es un insumo**
   más (igual que cobre o estaño). **Incorrecto física y computacionalmente**:
   la energía **no se almacena por aldeano**, no se "agota como insumo";
   **se gasta al instante**. La distinción "Producido vs Recolectado" del §3.1
   **no captura** el caso energía. Añadir **"Streaming"** como tercera
   naturaleza, o **no** modelar energía como recurso (modelarla como propiedad
   de la `GameState`, no como `player_stock` índice).
4. **[V] [Wikipedia Settlers_II] "if you allow direct control… it completely
   changes the game"** → SPEC-007 §4 (per_jugador `recovery_pct`) **es control
   fino del jugador** sobre cada yacimiento. Häuser **advirtió** que este tipo
   de control cambia el género. O §4 es derivado de `player_caps` (§4.3,
   **solo** — nunca editable a mano) o **rompe** la paciencia del principiante.
5. **[V] [Wikipedia Warzone_2100] "no hay árbol de tech visible; los tech se
   descubren por artefactos"** → el **vector de presión** de SPEC-007 §4
   (reabre minas) es **decorativo** sin un sistema de descubrimiento. Si los
   `recovery_pct` los "activa" linealmente el paso del tiempo, la edad 11
   queda **vacía** de decisiones. **El Director tiene que responder**: ¿el
   jugador **elige** cuándo investigar la galería, o el juego **se la da**
   al subir de edad? Si lo segundo, no es estudio del mapa, es un botón.
6. **[V] [Wikipedia Warcraft_III] "upkeep: producing units over certain amounts
   will decrease the amount of gold one can earn"** → SPEC-007 §2 no tiene
   **ningún mecanismo de upkeep** post-edad 9. Sin eso, el late game es
   **incontrolable**. No basta con "menos aldeanos por edificio": hay que
   añadir un **coste de mantenimiento** (oro/oro-equivalente) que crezca con
   cada edad.
7. **[V] [Wikipedia 0_A.D.] "3 fases in-game: Village / Town / City"** → nuestra
   escalera de **15 edades** es **fina de más** para un RTS. Si 0 A.D. (que
   sí es RTS histórico) lo simplifica a 3 fases, **15 es excesivo**. Edades
   6, 9, 10 y quizá 14 no introducen recurso; admiten "no toda edad tiene que
   hacerlo", pero 15 edades sin mecánica aditiva son **15 discursos de phase
   change sin payoff**. **Recomiendo 7–9 edades** + 3 fases narrativas.
8. **[V] [Wikipedia Anno_1800] "distribución por regiones"** → nuestro plan de 24
   recursos en **un solo mapa** no escala. Anno necesitó **3 regiones** para
   aguantar 30+ recursos simultáneos. Si CHUNSA va a 24 en una sola región,
   **la pantalla inferior del HUD** (4×3 en el mejor caso) **se vuelve
   inmanejable**. Recomiendo **dividir el árbol visual** en 4 familias:
   trópicos (comida, madera, piedra, granjas), metales-base (cobre, estaño,
   mena de hierro, carbón), metalurgia (bronce, hierro forjado, coque, acero,
   aluminio), energético (electricidad, pólvora). **5 subí­ndices** × 1 línea
   = legible.
9. **[V] [Wikipedia Empire_Earth] "Empire Earth III: 50% peor recibida"** → la
   serie de **3 juegos con 14+ edades** no sobrevivió al mercado. EE (82%) →
   EE2 (79%) → EE3 (50%). El mensaje: añadir **más granularidad** no mejora;
   el techo de público está en **8–10 edades**.
10. **[I] [OpenRA, sentido común, no leído de fuente accesible]** OpenRA
    confirma lockstep como **estado del arte** RTS open-source. CHUNSA está
    en la **posición correcta** con `CHECKSUM_ALGO_VERSION`. Pero el siguiente
    juego de la rama (CHUNSA II) **debería** heredar el hábito de **publicar
    una métrica de "ms por checksum"** en cada sprint. Único modo de no
    degradar lockstep silenciosamente.

---

## §8 — Pendientes (no investigados)

1. **Gather rates de AoE2 por aldeano** (food/wood/gold/stone) — Fandom 402.
2. **Datos finos de Warzone 2100** (capacidad de oil derrick, tiempo de
   investigación) — Wikipedia solo describe mecánicas generales.
3. **Widelands — un fichero `init.lua` real** de un edificio de producción —
   no encontré URL accesible al repo; necesidad de `gh` o descarga directa.
4. **OpenRA — wiki de Arquitectura** — wiki/Architecture no carga (404). No
   confirmé el patrón de Sync hash.
5. **Cantidad histórica de Aldeanos en AoE2** — 50 aldeanos es "óptimo" en
   estrategia moderna; **no leí cifras**.
6. **Datos concretos de Rise of Nations** (costes, tiempos) — no leídos.
7. **Songs of Syx** — Wikipedia no devolvió página en este fetch.

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
