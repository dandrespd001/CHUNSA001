# Empire Earth (2001) — progresión de proyectiles y armas por época

**Fecha:** 2026-07-31  
**Alcance:** *Empire Earth* base (14 épocas, I–XIV); *The Art of Conquest* sólo donde se indica (añade XV, Space). La época XIV, no la XV, es Nano. [Wikipedia][scope]  
**Criterio:** `[V]` verificable en la fuente enlazada; `[?]` **NO VERIFICADO**; `[I]` consecuencia de diseño para CHUNSA. EE Heaven es una base comunitaria, no documentación oficial.

## Respuesta ejecutiva

| Pregunta | Respuesta corta |
|---|---|
| Armas por época | No hay una única escalera: hay líneas de arco, armas de fuego y artillería, más ramas paralelas. El cambio tardío más claro es `Gun` → `Laser` en XIII. |
| Precisión | El manual no documenta un atributo `Accuracy`/precisión. Tampoco demuestra que todo proyectil acierte. La fórmula de impacto es **NO VERIFICADA**. |
| Guiado | El manual menciona misiles guiados en jets tardíos, pero no documenta *homing* del motor. Primer `guided=true`: **NO VERIFICADO**. Drones jugables: **NO VERIFICADO**. |
| Velocidad | No se halló velocidad numérica de proyectil. `Speed` en el manual es velocidad de la **unidad**. Flecha frente a misil: **NO VERIFICADO**. |
| Mejoras | Las mejoras por tipo de unidad afectan daño (`Attack`), alcance y área; no se verificaron precisión, cadencia ni velocidad del proyectil. Tecnologías separadas dan `+1` al alcance de torres. |
| Unidades viejas | El avance habilita reemplazos; el jugador ejecuta la mejora en el edificio y **todas** las unidades viejas de esa línea se convierten. No ocurre sólo por pasar de época. |

## 1 — De honda a láser

| Época | Progresión verificada | Lectura mecánica |
|---|---|---|
| I Prehistoric | `Rock Thrower`, `Shock`, alcance 2. [EE Heaven][prehistoric] | Antecesor arrojadizo; no pertenece a la línea de arco. |
| II Stone | `Slinger`, `Arrow`, alcance 3. [EE Heaven][stone] | Honda; primer miembro de la línea que continúa por mejora. |
| III–VII Copper–Renaissance | `Slinger` → `Simple Bowman` (alc. 4) → `Composite Bow` (5) → `Long Bow` (6). [Copper][copper] [Dark][dark] [Middle][middle] | Aumenta el alcance por plantillas discretas, no por cambiar sólo la animación. |
| V–VI Dark–Middle | `Crossbow`, `Arrow`, alcance 7, sin predecesor. [EE Heaven][middle] | La ballesta es rama paralela de largo alcance, no peldaño obligatorio del arco. |
| VII–IX Renaissance–Industrial | `Arquebus` → `Musketeer` → `Grenadier`; `Gun`, alcances 5 → 5 → 6. `Sharpshooter` coexiste con alcance 8. [Renaissance][renaissance] [Imperial][imperial] [Industrial][industrial] | Arcabuz, mosquete y fusilería no son una curva única; aparecen especialistas paralelos. No se verificó una unidad genérica llamada `Rifleman`. |
| IV–IX Bronze–Industrial | `Stone Thrower` → `Ballista`; `Catapult` → `Trebuchet`; después `Culverin` → `Bronze Cannon` y `Basilisk` → `Serpentine`. [Bronze][bronze] [Middle][middle] [Industrial][industrial] | La artillería se divide entre `Field Weapon` y `Siege Weapon`; no es sólo “más daño cada edad”. |
| X–XII Atomic | `Artillery` (alc. 12) y `Howitzer Cannon` (9), ambas `Siege Weapon`; en XII `Sub–Trident` ataca tierra con misiles de alcance 24. [EE Heaven][atomic] | El misil verificado aparece al menos en Atomic–Modern (XII), pero la fuente no lo llama guiado. |
| XIII Digital | `Sentinel` y `Gladiator Tank` usan `Laser`, alcance 6; `Colossus Artillery` alcanza 14. [EE Heaven][digital] | El salto es también de **clase de daño/armadura** (`Laser`), no una subida universal de alcance. |
| XIV Nano | `Guardian` y `Centurion Tank` continúan la línea láser; `Sub–Triton` conserva misil terrestre de alcance 24. [EE Heaven][nano] | Evolución por reemplazo y rol, no por añadir un arma nueva a cada edad. |
| XV Space (AoC) | La expansión añade la época Space. [Wikipedia][scope] | Estadísticas y mecánicas de sus proyectiles: `[?]` **NO VERIFICADO**. |

[V] El manual separa `Arrow Armour`, `Gun Armour` y `Laser Armour`; por tanto, cambiar de arma altera la matriz de counters, no sólo el número de ataque. [Manual pp. 103–104][manual-attributes]

## 2 — Precisión: qué existe realmente

[V] La tabla oficial de atributos enumera `Hit Points`, `Attack`, `Range`, `Speed`, `Area Effect`, cinco armaduras, `Flight Time`, `Cargo Capacity` y `Power`; no enumera `Accuracy`, `Precision`, `Hit Chance` ni `Evasion`. `Speed` se define como rapidez de movimiento de la unidad. [Manual pp. 103–104][manual-attributes]

[?] **NO VERIFICADO:** que el motor use una tirada oculta de precisión, una dispersión o un test de colisión; el manual no da la fórmula. Por ello tampoco es correcto afirmar “siempre aciertan”.

[?] Una fuente fan afirma que las fragatas pueden fallar contra blancos que se mueven y que su láser llega al 100 % desde Digital; usa “hit rate”, pero no prueba que sea una estadística visible ni que la regla sea global. Se conserva sólo como pista, no como hecho de diseño. [NamuWiki, secundaria][namu-navy]

[V] En el catálogo consultado de tecnologías no aparece una mejora de precisión para unidades móviles. Esto verifica el contenido del catálogo, no la ausencia de lógica interna. [Índice][tech-index]

## 3 — Misiles guiados y drones

[V] El manual dice que los jets de décadas posteriores portan “a variety of guided missiles” y que a finales del siglo XXI los láseres sustituyen la mayoría de armas de proyectil. Es contexto de armamento, no una especificación de simulación. [Manual p. 114][manual-air]

[V] La primera unidad cuyo texto recuperado dice explícitamente “long range missiles” es `Sub–Trident`, disponible en Atomic–Modern (XII)–Digital, sólo contra tierra; `Sub–Triton` la reemplaza en Nano. [Atomic][atomic] [Nano][nano]

[?] **NO VERIFICADO:** primera época con seguimiento mecánico, frecuencia de corrección, capacidad de girar, qué ocurre si muere el blanco o si flechas y misiles comparten *homing*.

[?] **NO VERIFICADO:** drones como categoría/unidad del juego base. Las páginas Digital/Nano listan aviones, `Cybers` y satélites, pero no bastan para afirmar que exista un proyectil-dron. [Digital][digital] [Nano][nano]

**Conclusión:** una flecha sí pertenece a la clase `Arrow` y un misil tardío puede existir como armamento; la diferencia autoritativa “trayectoria fija frente a persecución” no está documentada. No debe atribuirse a EE sin una sonda del ejecutable.

## 4 — Velocidad de proyectil

[V] `Speed` de las tablas es movimiento de la unidad, no velocidad del disparo. Las fichas consultadas publican alcance, ataque, área y velocidad de la unidad, pero no velocidad de viaje. [Manual pp. 103–104][manual-attributes] [EE Heaven Atomic][atomic]

[?] **NO VERIFICADO:** valores numéricos o siquiera una razón flecha:misil; tampoco se verificó que todo misil sea más rápido que toda flecha. La fuente fan sólo afirma cualitativamente que la velocidad permite esquivar ciertos disparos navales. [NamuWiki, secundaria][namu-navy]

## 5 — Qué mejora daño, alcance, precisión y cadencia

[V] EE distingue **mejora de atributos** y **upgrade de unidad**. La primera se investiga para un tipo concreto en su edificio, afecta a todas sus unidades y se conserva en sus sucesores. [Manual pp. 104–106][manual-upgrades]

| Eje | Evidencia |
|---|---|
| Daño | `[V]` `Attack` es el daño infligido y es mejorable. [Manual][manual-attributes] |
| Alcance | `[V]` `Range` es la distancia de ataque y es mejorable salvo en melee. [Manual][manual-attributes] |
| Área | `[V]` `Area Effect` es el tamaño de explosión y forma parte de los atributos. [Manual][manual-attributes] |
| Precisión | `[?]` **NO VERIFICADO** como atributo o mejora. No figura en la lista oficial. |
| Cadencia | `[?]` **NO VERIFICADO** como atributo o mejora; no figura `Rate of Fire`/cooldown. |
| Velocidad | `[V]` la mejora documentada es velocidad de la **unidad**; `[?]` velocidad del proyectil **NO VERIFICADA**. [Manual][manual-attributes] |
| Incremento por paso | `[?]` **NO VERIFICADO** para daño/alcance: el manual explica presupuesto de “steps”, costes y topes, pero no un delta universal. [Manual pp. 104–105][manual-upgrades] |

[V] En el árbol de tecnologías, `Walls and Towers` da `+1 Tower Range` en IV, VIII, X y XIII; AoC añade XV. No mejora unidades móviles. [Defense technologies][tech-defense]

## 6 — Cómo evita una lista inmanejable

[V] El avance de época hace disponibles unidades nuevas; “in most cases” son reemplazos de viejas. El jugador selecciona el botón de upgrade en el edificio; al completarlo, **todas las unidades viejas son reemplazadas**, se entrena ya la nueva y las mejoras acumuladas se heredan. [Manual p. 106][manual-upgrades]

[V] No todo se comprime en una sola cadena: `Crossbow` convive con `Long Bow`, y artillería de campo y de asedio ocupan ramas distintas. [Middle][middle] [Industrial][industrial]

[?] **NO VERIFICADO:** qué hace el motor con una unidad sin sucesor cuando sale de su intervalo de épocas. Las fichas sí delimitan intervalos de producción, pero el manual citado no describe borrado automático por antigüedad.

## 7 — Aplicación concreta a CHUNSA (determinista, enteros, 15 edades)

[I] Modelar cuatro ejes separados: `unit_upgrade_line`, `damage_class` (`ARROW/GUN/LASER/...`), `projectile_profile` y mejoras enteras de atributos. EE demuestra que mezclar los cuatro bajo “arma de edad N” oculta decisiones importantes.

[I] Datos mínimos por plantilla: `epoch_from`, `epoch_to`, `upgrade_to`, `attack`, `range`, `area`, `cooldown_ticks`, `projectile_speed_upt`, `guided`. La conversión masiva debe ser una orden explícita y determinista; no dispararse implícitamente al avanzar de edad.

[I] Conservar ramas paralelas sólo cuando cambie el rol: ballesta de largo alcance, artillería de campo contra unidades y asedio contra edificios. Retirar del panel de producción el predecesor al comprar su upgrade, pero convertir todos los existentes y heredar mejoras.

[I] No añadir `accuracy` para “imitar EE”: no está verificada. El contrato vigente de `SPEC-004 §24.5` — apuntar a posición predicha, proyectil no guiado que no persigue y resolución en destino — ya produce impacto/fallo geométrico sin RNG ni floats.

[I] Si aparece guiado tarde, debe ser un flag excepcional (misiles/drones authored), con corrección entera por tick y reglas explícitas de pérdida de blanco. Una flecha conserva `target_point`; un guiado conserva `target_id`. Esto es propuesta CHUNSA, **no dato verificado de EE**.

[I] No inventar 15 velocidades. Usar pocos perfiles (`THROWN`, `ARROW`, `SHELL`, `MISSILE`, `ENERGY`) y cambiar sólo en hitos; ajustar números mediante sondas del propio kernel. EE no aporta valores copiables.

[I] La progresión útil es: **más counters y roles + reemplazos discretos**, no inflación monotónica. El láser puede cambiar clase de daño sin necesitar más alcance que un arco; los datos verificados de EE muestran exactamente ese patrón.

## Fuentes

[scope]: https://en.wikipedia.org/wiki/Empire_Earth_(video_game)
[manual-attributes]: https://archive.org/download/Empire_Earth_-_Manual_-_PC/Empire_Earth_-_Manual_-_PC.pdf#page=104
[manual-upgrades]: https://archive.org/download/Empire_Earth_-_Manual_-_PC/Empire_Earth_-_Manual_-_PC.pdf#page=105
[manual-air]: https://archive.org/download/Empire_Earth_-_Manual_-_PC/Empire_Earth_-_Manual_-_PC.pdf#page=115
[prehistoric]: https://ee.heavengames.com/eeh/gameinfo/units/prehistoric/
[stone]: https://ee.heavengames.com/eeh/gameinfo/units/stone/
[copper]: https://ee.heavengames.com/eeh/gameinfo/units/copper/
[bronze]: https://ee.heavengames.com/eeh/gameinfo/units/bronze/
[dark]: https://ee.heavengames.com/eeh/gameinfo/units/dark/
[middle]: https://ee.heavengames.com/eeh/gameinfo/units/middle/
[renaissance]: https://ee.heavengames.com/eeh/gameinfo/units/renaissance/
[imperial]: https://ee.heavengames.com/eeh/gameinfo/units/imperial/
[industrial]: https://ee.heavengames.com/eeh/gameinfo/units/industrial/
[atomic]: https://ee.heavengames.com/eeh/gameinfo/units/atomic/
[digital]: https://ee.heavengames.com/eeh/gameinfo/units/digital/
[nano]: https://ee.heavengames.com/eeh/gameinfo/units/nano/
[tech-index]: https://ee.heavengames.com/eeh/gameinfo/technologies/
[tech-defense]: https://ee.heavengames.com/eeh/gameinfo/technologies/defense/
[namu-navy]: https://en.namu.wiki/w/%EC%97%A0%ED%8C%8C%EC%9D%B4%EC%96%B4%20%EC%96%B4%EC%8A%A4/%ED%95%B4%EC%83%81%EC%9C%A0%EB%8B%9B
