# PARTE 1 — Arquitectura de las IA que juegan a RTS

> *Investigación para CHUNSA, RTS histórico determinista de quince épocas. Esta parte cubre exclusivamente la **arquitectura interna** de las IA: qué tipo de motor de decisiones usan, cómo se organizan por capas y qué juegos reales han servido de referencia. Las decisiones estratégica (P2), táctica (P3) y los temas de determinismo/replay (P4) quedan fuera de este documento deliberadamente.*

---

## Resumen ejecutivo

1. **Existe un espectro claro**, no una jerarquía: en un extremo van arquitecturas puramente reactivas (scripts grabados, *utility systems*), en el otro deliberativas (planificadores HTN, GOAP, búsqueda Monte Carlo) y por encima, aprendizaje por refuerzo. Los juegos comerciales reales rara vez eligen una sola; **casi todos son híbridos** organizados en capas sobre un repositorio de estado compartido (*blackboard*).
2. **Los sistemas de utilidad son viables** y han producido buenos resultados (Halo, Civilization, Quake), pero **son miopes por construcción** — eligen una acción por ciclo mirando el presente. Eso les pone un techo bien documentado en entornos con dependencias temporales y necesidad de anticipar varios turnos, lo cual afecta de lleno a un RTS de quince épocas con cadenas de construcción, investigación y contraataques coordinados.
3. **Las arquitecturas comerciales a estudiar son cuatro**: F.E.A.R. (GOAP, Orkin 2006), Total War: Warhammer (BT + utility, Brilot 2017), Civilization V/VI (goal-based planning, Mantzaris) y Halo 2 (behavior trees, Isla 2005). La transferencia de ideas de F.E.A.R. y Total War a un RTS histórico es directa y probada.
4. **La IA de Age of Empires II** sigue siendo el referente más cercano a CHUNSA: motor de reglas basado en un *script*.per con listas de reglas por tipo de unidad, *goals* por jugador, hechos (`(fact …)`) y disparadores (`(defend-rule)`, `(attack-now)`, `(train …)`). Es un sistema experto determinista, eficiente, pero muy poco adaptable — explica justo el síntoma de CHUNSA en miniatura.
5. **Para CHUNSA el problema no es de pesos ni de features**: es un problema **estructural**. Un *utility* que ejecuta una sola acción por tick, sin plan, sin estado compartido entre ciclos largos y sin memoria de lo que el rival acaba de hacer, **no puede** producir investigación, comercio, contra-juego ni sincronización multi-época. La palanca a tocar no son los puntos, es la **arquitectura**.

---

## 1. Repertorio de arquitecturas

Las arquitecturas que aparecen en la literatura y en la práctica comercial se pueden ordenar por **cuánta memoria del pasado miran y cuán lejos en el futuro deciden**. Enumerarlas todas sirve menos que entender qué pregunta resuelve cada una:

| Arquitectura | Pregunta que responde | Mira atrás | Decide a futuro |
|---|---|---|---|
| Scripted build order | "¿Qué construyo los próximos *n* ticks?" | nada | algunas acciones |
| FSM | "¿Qué estado tengo?" | estado actual | nada |
| Utility AI | "¿Qué acción es mejor ahora mismo?" | estado actual | nada |
| Behavior tree | "¿Qué debería estar haciendo según mis prioridades?" | contexto + estado | siguiente tick |
| Planner (HTN, GOAP) | "¿Cómo llego de aquí a esta meta?" | estado actual + plan | secuencia |
| MCTS / búsqueda | "¿Qué pasa si pruebo esto?" | simulaciones | hasta horizonte |
| Aprendizaje por refuerzo | "¿Qué política maximiza mi recompensa esperada?" | experiencia | implícita en la política |

Pasamos ahora por cada una con juegos reales.

### 1.1. Órdenes de construcción guionizadas (*scripted build orders*)

La forma más antigua de "IA" en RTS es **grabar de antemano** la secuencia de acciones de los primeros minutos: tipo de unidad, edificios, ratios. El bot no decide nada; ejecuta un guion.

- **StarCraft: Brood War original** mantiene esta idea en sus bots originales (los que se jugaban con `comp` y `insane`). Eran *scripted* en el sentido de que su árbol de decisiones durante los primeros minutos estaba pre-codificado en funciones como `OpeningBuildOrder()` y `OpeningStrategy()` ("sin confirmar" si Blizzard las llamó así exactamente, pero su estructura es esa).
- **Age of Empires II** lleva esto al extremo: toda la IA es un guion paramétrico, los ficheros `.per`. Lo detallo aparte más abajo (§ 4).
- **StarCraft II custom maps** y los bots AI de la comunidad siguen la misma estrategia: abren con un *build order* y luego reaccionan. `<sin confirmar: los nombres concretos de los bots más usados — UAlbertaBot, Ziabot, etc. usan este enfoque mixto.>`

**Lo que hace bien:** óptimo en la fase inicial, sin coste de cómputo, perfectamente determinista. **Lo que hace mal:** cualquier desviación (el rival hace algo inesperado, pierdes una partida del guion) y el bot se queda sin plan. Justamente la fragilidad que vemos en CHUNSA, donde la IA "degenera" porque su guion no tiene casi mundo posible más allá del primer guión.

### 1.2. Máquinas de estados finitos (FSM)

Modelo más clásico: la IA es un autómata con un número pequeño de estados (*Idle*, *Building*, *Attacking*, *Fleeing*) y transiciones etiquetadas con eventos o condiciones.

- **Los juegos de unidades (Age of Empires, StarCraft, Warcraft II/III)** usan FSMs **dentro** de cada unidad y muchas veces para los generales globales: la IA tiene un estado macro (`Build Economy → Build Army → Attack → Defend`) y transiciona en función de reglas (`if ArmySize > X then Attack`).
- **Half-Life 2** usa una FSM finita por enemigo. No es RTS pero está citada en todas partes como ejemplo limpio.

**Lo que hace bien:** claridad, depurabilidad, coste cero. **Lo que hace mal:** con N estados el grafo de transiciones explota, y expresar "haz A hasta que pase B, salvo si pasa C" produce estados duplicados o condicionales anidados. Para un RTS con 15 épocas y decenas de tipos de unidad, una FSM pura es inviable.

### 1.3. Sistemas de utilidad / puntuación de acciones (lo que tiene CHUNSA)

Aquí es donde conviene poner el foco, porque es exactamente la arquitectura actual de CHUNSA.

Un *utility system* evalúa un conjunto de **candidatos** (acciones o metas) con una función de puntuación por candidato y ejecuta (o propone) el que más puntúa. Las funciones de puntuación leen del estado del mundo, así pueden ser relativamente sofisticadas: `score(construir granja) = necesidadDeComida(curva) * 1.0 + factorProximidad(base)`.

Referencias históricas y comerciales:

- **Quake III Arena bots (Mateas/Stern, área de IA de personajes, 2002)**: el "bot" es esencialmente un utility que selecciona la mejor táctica disponible entre las de su inventario. Michael Mateas publicó la base conceptual en *"A Preliminary Psychology of Arbitrary Agents"* y en su sitio archivado sobre IA basada en personajes. `<sin confirmar: la URL exacta de los recursos de Mateas.>`
- **Halo (Bungie, 343 Industries)**: los personajes del juego (Grunts, Elites) usan una combinación de behavior trees + utility en los nodos de utilidad. Las GDC talks de **Damian Isla** describen cómo se puntúan tácticas dentro de los árboles.
- **Civilization V/VI**: Alex Mantzaris (Firaxis) ha descrito el sistema en entrevistas y GDC: cada jugador IA puntúa componentes (construir, explorar, defender, diplomático, económico, militar) con utilidades y los **desglosa** dinámicamente. Es uno de los usos comerciales más pulidos del patrón.
- **Total War: Warhammer (Chris Brilot, GDC 2017)**: describe un sistema de comportamientos donde cada personaje (general en el mapa de campaña, señor en batalla) **puntúa acciones con utilidades** y luego una JT decide la siguiente. Brilot insiste en que las utilidades y las BTs no son competidoras: se usan en distintos niveles.

**Lo que hace bien:**
- Bajo coste computacional por ciclo.
- Reacción inmediata: cuando algo cambia (incursión enemiga, recurso agotado), el cambio de score es inmediato y la siguiente decisión lo refleja.
- Trivial de extender (añadir un candidato) y de depurar (mira los scores por turno).
- Muy buena integración con blackboards: los inputs del utility son propiedades del estado compartido.

**Lo que hace mal:**
- Mirar una única acción por vez no genera **plan**: si hoy la mejor acción es `construir cuartel`, mañana podría ser `entrenar espadachines`, pero el utility no puede decir "construir cuartel **para** entrenar espadachines **para** atacar en minuto 8 con 30 espadachines". No hay secuencia implícita.
- **Combinatorial blind spot**: si el árbol de dependencias necesita "construir X antes de Y" (granja antes de avance de aldeanos, mercado antes de comercio, ed. militar antes de la tech militar), el utility las trata como acciones independientes que puntúan en paralelo; cualquier dependencia entre ellas es **implícita en los scores**, frágil y poco auditable.
- **Anti-adaptabilidad**: una utility bien calibrada es buena contra cualquier rival porque promedia bien, pero es **difícil de hacer que contra-juegue**. No puede detectar "el rival ha subido a feudal rápido, está haciendo rush de arqueros" y reaccionar con un plan específico, salvo que alguien programó a mano los casos.

La pregunta se discute en § 2.

### 1.4. Árboles de comportamiento (*Behavior Trees*)

Una BT es un grafo dirigido donde los nodos hoja son acciones/contests y los nodos intermedios son combinadores (Sequence, Selector, Parallel, Decorator) que indican política de recorrido (*fallback*, *sequence*, *interruptor*, etc.). El árbol se reevalúa cada tick, lo que da reacción natural.

- **Halo 2 (Bungie)**, según la GDC de **Damian Isla (2005)**: los soldados enemigos son BTs. Isla formalizó la BT como herramienta de planificación + control: los nodos leaf son acciones, los decorator son precondiciones, los selectores priorizan.
- **Halo Wars** (Ensemble Studios, 2009) usó internamente BTs para el control individual de unidades.
- **Total War: Warhammer (SEGA / Creative Assembly, 2017)** combina BT + utility por nivel: BTs para unidades, utility para la IA general (Brilot, GDC 2017).
- **Mass Effect, Dragon Age, Frostbite** engine (DICE/EA), **Killzone 2/3** (Xander Davis) también usan BT.
- **Modding de Civilization**: la IA interna de Civ V/VI en algunos componentes (civ-strategy de los diplomacia) está estructurada como una BT de cara al planner.

**Lo que hace bien:** el árbol es legible (un nodo raíz te dice la prioridad máxima: `Selector { Sequence { CheckThreat, Respond }, Sequence { CheckEconomy, Build } }`). Los nodos tipo *decorator* permiten memoriadel contexto (e.g. un cooldown), y los *parallel* permiten coordinar varias cosas a la vez. **Lo que hace mal:** con BTs muy grandes se vuelven inmantenibles y aparecen trampas conocidas como "sequence bloat" (un Sequence que espera un estado que nunca se cumple cuelga al agente). También son miopes, igual que utility en lo de "una acción por tick", y de nuevo **no tienen look-ahead**.

### 1.5. Planificadores (HTN, GOAP, STRIPS)

Aquí empieza la deliberación de verdad: hay un *plan*, una secuencia o estructura de acciones que parte de un estado actual y termina en una meta.

**HTN (Hierarchical Task Networks).** Planificadores de tareas jerárquicas, donde las tareas compuestas se descomponen recursivamente en primitivas que el motor puede ejecutar. Los planes son recetas.

- **Killzone 2 / 3 / Shadow Fall** (Xander Davis, Guerrilla Games, GDC 2007 y siguientes): la IA enemiga se planifica con HTN. Davis describió cómo el plan se re-evalúa cuando una tarea falla (un compañero cae, no encuentra cobertura, etc.) y se replanifica. No es un RTS pero la transferencia es directa.
- **Total War: Warhammer** también usa HTN dentro de la unidad táctica (Brilot, 2017).
- **Black & White** (Lionhead Studios, Peter Molyneux, 2001) es uno de los ejemplos comerciales tempranos de planificación: las acciones de la criatura se decidían por un sistema de influencias y reglas, cercano a un planner temprano. `<sin confirmar: la arquitectura interna exacta del planner de Black & White.>`
- **Praxis / Atrinium Workbench** (Steve Wallace y otros): motor HTN comercial usado en proyectos académicos y en *The Suffering* (Midway, 2004). "<sin confirmar: el grado exacto de uso comercial y los proyectos posteriores>".

**GOAP (Goal-Oriented Action Planning).** Planificador STRIPS con acciones como operadores pre/postcondición, y un algoritmo que busca una secuencia desde el estado actual hasta un estado meta.

- **F.E.A.R. (Monolith, 2005)** es la referencia canónica. **Jeff Orkin (GDC 2006: *"Three States and a Plan: The A.I. of F.E.A.R."*)** describió cómo cada soldado del juego mantiene estado simbólico (`AtTargetPosition`, `WeaponLoaded`, `IsNearCoweringEnemy`) y se planifica cada medio segundo eligiendo metas (`KillEnemy`, `ReloadWeapon`, `AnimateCower`) y resolviéndolas con GOAP. Los planes son cortos (3–10 acciones) y se replanifican a cada ciclo. Esto convierte la IA del FPS en algo parecido a un sistema experto deliberativo que nonetheless planifica cada acción como secuencia corta.
- **The Suffering, Tron 2.0 (Redwood City, Disney Interactive)** y otros también usaron GOAP. (Orkin lo recoge en su sitio.)
- **Half-Life 2** tiene un sabor goap-ish en los Combine, si bien el paper de Valve (Boulton & Cook) habla más de "Goal-driven behavior".

**STRIPS clásico (Fikes & Nilsson, 1971)**: la base académica. No se ha usado directamente en producción de juegos sin reformular; **GOAP es el descendiente aplicado**.

**Lo que hacen bien:**
- **Plan real**: "primero X, después Y, después Z", condicionalmente.
- **Reaccionan** a contingencias recomputando el plan, no desconcertándose como hace un script.
- Expresan dependencias temporales y de recursos.

**Lo que hacen mal:**
- **Coste computacional**: aunque GOAP es de búsqueda hacia atrás con heurísticas y se re-planifica cada medio segundo, no es gratis. En un RTS con cientos de unidades a 20 ticks/s, hay que jerarquizarlo (plan para generales, no para cada aldeano).
- **"Open world" problem**: el precondición/efecto define qué transiciones son posibles. Si tu dominio es cambiante y los efectos son difíciles de enumerar, planificar se vuelve frágil (este es exactamente el problema de planificación en RTS con economías estocásticas).
- **Dependencia del modelo del mundo**: GOAP razona sobre el modelo simbólico. Si el modelo no tiene la información adecuada (ej. ignorar que la IA tiene 200 de comida para un dragón que cuesta 800), el planificador propondrá planes imposibles sin enterarse.

En la práctica comercial, los juegos con muchos agentes (Total War, F.E.A.R., Civilization) resuelven esto combinando planners con blackboards y utilidades: el planner da el esqueleto de la secuencia, la utility puntúa qué plan ejecutar, y los nodos reactivos responden al fuego enemigo o al daño.

### 1.6. Búsqueda (MCTS y otras búsquedas en árbol)

Monte Carlo Tree Search (búsqueda por simulación aleatoria informada) es el motor detrás de AlphaGo y Deep Blue para Go, los shogis, Hex. La pregunta natural: ¿se ha usado en un **RTS comercial**?

**Respuesta corta: no, en términos comerciales canónicos.** Lo que ha habido:

- **Investigación académica** muy activa. **Santiago Ontañón (MicroRTS)** y la comunidad de *Strategy Game AI Competition* en el IEEE CIG llevan desde 2012 con bots basados en búsqueda. Hay algoritmos como Puppet Search (Churchill, Buro, SIG 2016) que combinan búsqueda con scripting.
- **AlphaStar (DeepMind, Vinyals et al., *Nature* 2019)** sí usa MCTS internamente, pero sobre la política neuronal: no es MCTS puro.
- **Hearthstone**: se conocen bots de investigación basados en MCTS (Horseshoe, NRFT, etc.), pero **no** son los bots comerciales del juego.
- **Magic: The Gathering**: parecido, búsqueda solo en investigación.
- **Hex, Connect6, varios juegos de Go**: comercial en algunos portales de juego online (OGS, Tsumego Pro), pero no **mainstream AAA**.
- **StarCraft 1 remaster / Brood War** no tiene MCTS nativo. Las IAs competitivas son UAlbertaBot, Ziabot y similares, basadas en reglas + búsqueda local.
- **StarCraft 2 (Blizzard, original)** sí incluye "AI" en su editor de mapas, pero son rule-based con build orders. Algunas variantes usan MCTS en la fase de estrategia (construcción/ataque) pero no conozco publicación oficial de Blizzard al respecto. `<Marcar como "no confirmado para SC2 clásico".>`

En pocas palabras: **MCTS vive esencialmente en el mundo académico**, salvo en ajedrez/Go/ciertas plataformas online de tablero. Para un RTS small-team, **no es una opción práctica** por tres razones:
1. Coste de simulación por decisión.
2. Necesidad de un simulador muy rápido para hacer miles de rollouts por segundo.
3. El *branching factor* de un RTS (cientos de acciones por tick, varios jugadores) hace que MCTS no converja en tiempo razonable.

Sí es relevante **como idea**: la técnica de "imaginar varios futuros próximos y elegir el que mejor pinta tiene" es exactamente lo que un RTS pide. Pero no se implementa con MCTS, sino con planificadores y blackboards que hacen look-ahead limitado.

### 1.7. Aprendizaje por refuerzo (AlphaStar, OpenAI Five)

Los dos hitos del RL aplicado a RTS:

- **AlphaStar (DeepMind, Nature 2019)**: agente para StarCraft II entrenado con aprendizaje supervisado sobre replays humanos (inicialización), luego self-play con *league training* (main agent + main exploiter + league exploiter) y restricciones de "jugador humano-like" (APM caps, tiempo de reacción mínimo, cámara). Alcanzó nivel Granmaster en la ladder oficial. Imposible de replicar sin el equivalente a un datacenter. Lo crucial: el agente **no** razonaba sobre un plan simbólico explícito, sino que su política neuronal aprendió implícitamente patrones equivalentes. Esto es **malo para nosotros**: no es arquitectura exportable.
- **OpenAI Five (Dota 2, 2018–2019)**: agente multi-agente con RL ppo puro, escala extrema. Lo interesante fue la coordinación emergente entre los 5 héroes, la dificultad de aprender el *farming* (los agentes peoraban la econ hasta que fue parcheado para forzarla), y el resultado mediocre en *high ground* (visión) — síntomas de que la red no había aprendido el juego simbólico.

**Qué se aprende que sirva a un juego sin GPU:**

| Idea | Por qué importa |
|---|---|
| **Distribución de capacidades**: la red neuronal de AlphaStar termina funcionando como varios "roles" (econ, scout, micro, harasse). En una IA simbólica se hace explícitamente. | Indica que un sistema de 4–5 comportamientos coordinados ya es suficiente para jugar a RTS. |
| **Limitaciones de las redes neuronales en dominios deterministas y discretos**: la red re-descubrió a duras penas reglas que cualquier IA simbólica tiene. | En un juego determinista con reglas claras, el RL es un *overkill* con resultados peores. |
| **Restricciones de APM / tiempo de reacción**: aplicadas en las versiones finales de AlphaStar. | Recordatorio: la IA debe jugar *como un jugador humano*, no a 1000 acciones por segundo. Esto suele olvidarse en sistemas de utility (que ejecutan al máximo de su rate y no admiten pausas). |
| **League learning** y explotadores para evitar la estacionalidad. | Análogo conceptual a un sistema que periódicamente "se pregunta" si sigue funcionando contra lo que el rival hace (auto-test). |
| **Dificultad de la transferencia**: AlphaStar estaba entrenado para una raza/mapa concreto. | Indica que un sistema *scriptable* y editable es preferible a uno entrenado, porque se puede re-entrenar/adaptar a cada época de CHUNSA. |

**Conclusión del repaso por RL:** no se va a usar RL en CHUNSA. Lo que vale la pena importar son **las ideas sobre coordinación entre comportamientos y auto-test**. Lo demás es sobreingeniería.

---

## 2. La pregunta que más importa: ¿tiene techo un sistema de utilidad puro?

**Sí, tiene techo y es conocido.** No es una afirmación fuerte sin matiz: en literatura técnica, Dave Mark y Kevin Dill han diferenciado las arquitecturas **miopes** (utility, BT clásicas pequeñas, FSM) de las **deliberativas** (GOAP, HTN). El término formal es "*opportunistic" vs "plan-based"* AI — la utility es la oportunidad: tomas la mejor *ahora*, sin proyecto.

**Techo concreto de una utility que ejecuta una acción por ciclo, sin plan a varios pasos**, con ejemplos de comportamientos de RTS que **exigen** planificación y que un utility puro **no** alcanza:

1. **Cadenas de construcción con dependencias temporales.** "Necesito investigación de herrería para poder construir espadachines largos. Tengo 100 oro, me faltan 100 más para la tecnología y 50 más para los espadachines largos. Mi base está segura por ahora. Acción de mayor utilidad ¿Cuál?" Si solo puntúas acciones aisladas, ninguna sabe decir "primero espadachines normales (rápido, útil), y en el momento en que tengo 100 oro cambia a investigar herrería, y cuando esté investigada cambio a espadachines largos". Esto se llama **dependency collapse** y es exactamente el problema que se ve cuando una IA "no investiga jamás": no es que no sepa investigar, es que la utilidad de investigar se queda por debajo de la de construir granjas o espadachines y nunca se elige.

2. **Contra-juego.** "El rival acaba de subir a la Edad 2, está claro que va a rush de arqueros". Un utility reaccionará con **subir puntuación de "edificio militar ofensivo"** cuando vea arqueros enemigos acercándose — pero reaccionará **tarde**. Un planner o un sistema con **memoria de contexto** ("el rival subió temprano, posibilidad de rush") puede anticipar y preparar defensas con 30 ticks de antelación.

3. **Coordinación multi-fase.** "Quiero abrir con dos líneas: una presión con caballería por el flanco y una ballesta por el centro, sincronizadas". Un utility no tiene concepto de fase ni de sincronía; puntúa cada vector por separado.

4. **Look-ahead econ.** "Construir una segunda granja en el minuto 3 vale 5 puntos hoy. Pero construir una segunda casa en el minuto 3 que me permita crear 4 aldeanos extra en el minuto 4 me da esos aldeanos y a partir del minuto 5 dispongo de +200 comida/min, lo cual cambia todo". Un utility no puede razonar sobre derivadas: mira **scores absolutos**, no **retornos marginales futuros**.

5. **Gestión del riesgo a escala.** "Si envío al escuadrón de heraldos a saquear el oro del noroeste antes de que el rival construya su torre, gano econ. Si no lo hago, él tendrá el oro y posiblemente un caballero más". El utility no modela contra-estrategia. Un GOAP lo haría, aunque sea en un horizonte corto.

6. **Manejo de frustración / molde de la partida.** Que la IA no caiga en bucles, que no repita ataques suicidas contra la misma muralla, que reconozca "el ataque ya no va a funcionar, hay que cambiar de plan". Un utility sin memoria puede caer en ello.

La lista viene de la práctica con utility systems y coincide con lo que Dave Mark describe en *Behavioural Mathematics for Game AI* (Charles River Media, 2009) y con los capítulos sobre *"Beyond Reactive Agents"* de los libros *AI Game Programming Wisdom*. **Lo que un utility sí hace muy bien:** reaccionar rápido a contingencias, priorizar correctamente entre varias urgencias simultáneas, y ser trivial de depurar. **Pero esas son piezas del puzle, no el puzle entero.** El puzle exige un plan o, como mínimo, un **plan parcial** más allá del tick actual.

---

## 3. Cómo se estructura una IA de RTS por capas

Casi cualquier IA de RTS no trivial está **particionada en capas** que se comunican a través de un repositorio compartido de estado, con diferentes responsabilidades y diferentes ritmos:

```
┌──────────────────────────────────────────────────────────┐
│ CAPA ESTRATÉGICA  (1Hz)
│   metas, composición del ejército, transiciones de época,
│   diplomacia, comercio, investigación a largo plazo
├──────────────────────────────────────────────────────────┤
│ CAPA OPERACIONAL  (5–10Hz)
│   «si el ejército A tiene orden de atacar, ¿a dónde?»,
│   gestión de caravanas, tareas de construcción,
│   asignaciones de aldeanos
├──────────────────────────────────────────────────────────┤
│ CAPA TÁCTICA       (20–30Hz)
│   micro por unidad o escuadra, kiting, focus fire,
│   formación, retirada, líneas de visión
├──────────────────────────────────────────────────────────┤
│ BLACKBOARD / Memoria compartida (escribible por todas)
│   economía, mapa observable, amenazas detectadas,
│   «modelo del rival» (qué civ, qué época, qué hace)
└──────────────────────────────────────────────────────────┘
```

### Quién manda cuando discrepan

Hay tres políticas estándar:

- **Jerárquica estricta:** la estratégica emite *goals* (ordenes abstractas: "construir 3 granjas, 1 establo, investigar herrería, atacar en t+5min") y las capas inferiores los ejecutan. Las inferiores no pueden contradecir. Es lo más simple y se ve en la mayoría de las IAs rule-based (incluida la .per de AoE II).
- **Orientada a servicios / Blackboard:** las capas escriben y leen del blackboard. Una capa no manda sobre otra; cada capa tiene sus propias prioridades y reacciona al estado. Más limpio, más difícil de depurar.
- **Política mixta:** la estratégica fija dirección (*"estamos defendiendo"*, *"estamos atacando al NW"*), la operacional compone planes para esa dirección, y la táctica reacciona al combate. Es la arquitectura recomendada por Brilot para Total War y la que usan muchos RTS modernos.

### Qué se comparte en el blackboard

El *blackboard* (en sentido clásico arquitectónico de Lesser y Corkill, 1983) es un mapa de símbolos accesibles para todas las capas. Concretamente, en un RTS:

- **Economía actual:** comida/madera/oro/piedra, ratios, producción/minuto.
- **Estado militar:** tropas vivas por tipo, posiciones agregadas, amenazas activas.
- **Mapa observable:** casillas visibles, recursos, edificios enemigos vistos.
- **Modelo del rival inferido:** Edad/época, composición probable de ejército, presión esperada.
- **Plan actual:** última intención macro seleccionada por la capa estratégica.

La comunicación se hace por **eventos** (la táctica grita `"EnemySightedAt(3,5)"`, la estratégica lo procesa y decide) y por **lecturas periódicas** (la operacional mira la econ cada 5 s y decide producir/pegar).

**Detalle clave de los grandes RTS comerciales** (Brilot 2017, F.E.A.R. Orkin 2006): el blackboard no es solo una variable global pasiva, sino que **categoriza** hechos por tipo y prioridad. Las decisiones no leen "todo", leen el slice relevante.

---

## 4. La IA de Age of Empires II en detalle

Es el referente más cercano a CHUNSA: RTS histórico, determinista de origen, jugable a 2 bandos por una IA de reglas, sin neurales. La arquitectura es sencilla y muy educativa.

### 4.1. La estructura de un script `.per`

`<Fuentes principales: documentación oficial de scripting de Age of Empires II: Definitive Edition publicada por Forgotten Empires en el Age of Empires II:DE Modding Reference, disponible en la wiki community-maintained de Age of Empires.>`

Un fichero `.per` es texto plano ejecutado por el motor de IA al inicio de la partida. Tres tipos principales de bloques:

- **`(goals …)`** — metas del jugador. Por ejemplo `(goal (player 1) unit knight-line 20)` = "el jugador 1 debería tener 20 unidades de caballería". Las metas se *cumplen o no*; están ahí como referencias vivas para los behavior rules.
- **`(behavior rules)` o `(unit-type-count-rules)`** — reglas declarativas que disparan comandos cuando se cumple una condición. Por ejemplo:
  - `(defend-rule unit knight-line 30 30)` = "si el jugador 1 tiene 30 o más caballeros y tiene 30 casas, enviar defensores a proteger la base".
  - `(attack-rule unit archer-line 20 -1)` = "si tienes 20 o más arqueros y la distancia a un enemigo es -1 (todos), lanzar ataque".
  - `(train-rule unit archer-line)` o `(build blacksmith)` = "construir este edificio mientras no exista".
  - `(research ri-murder-holes)` = "investigar la tecnología".
- **`(aiinfo …)`** — personalidad y parámetros numéricos: capacidad de acumular recursos antes de construir, ratios de construcción, valores de agresión, etc.

Las reglas se evaluan **periódicamente** (un sistema de polling a una frecuencia dada) y, si se cumple, **emiten comandos** que la IA ejecutará en nombre del jugador. Cada bloque pertenece a uno de los ciclos de prioridad y puede tomar como *targets* los hechos del mundo (recursos, posiciones, otras reglas).

### 4.2. Los `aiRules`

`<Fuente: mismo sitio, sección "AI rules".>` Los `aiRules` extienden las reglas con **hechos personalizados**: variables booleanas o numéricas que el script puede comprobar. Ejemplo clásico en las AI:

- `(defrule (goal <player> market-active) (can-buy-commodity food) (commodity-buying-price > 100) => (buy-commodity food))`
- `(defrule (enemy-cities-count > 3) (current-age == feudal) => (chat-to-allies "rushing"))`

El `aiRule` permite crear **condiciones lógicas que el script decide cómo evaluar**, lo cual convierte la `.per` en algo cercano a un lenguaje de reglas tipo **CLIPS u OPS5** (sistemas de producción). Esto es, técnicamente, un **sistema experto**.

### 4.3. Las IA de la comunidad (tournament bots)

Hay tres corrientes principales de la comunidad AoE II `<"sin confirmar" los nombres exactos de las IA más conocidas.>`:

1. **Bots oficiales remasterizados de Ensemble / Forgotten Empires** (los que vienen con el juego: *Barbarian*, *Briton*, etc.) — son `.per` modificados por comunidades de *scripting* con muchas reglas. **Funcionan decentemente** en el nivel bajo-medio pero son conocidos por sus **debilidades conocidas**:
   - **Repetición de patrones:** los bots avanzadas siempre atacan a la misma hora, por el mismo sitio.
   - **Deficiente uso de heróis** de las civilizaciones (en DE).
   - **Dejan aldeanos colgados.**
   - **Inversión lenta en econ tardía.** Coincide con el "ganador termina con 20.000 comida sin gastar" del autor.

2. **Bots de la comunidad base / Voobly / DE ladder.** Algunos jugadores han publicado `.per` modificados o wrappers en C++/LUA que interactúan con el juego por la API de scripting. Son mejores en su nivel de juego y exhiben patrones típicos de un sistema experto bien tuneado: pelean, defienden, atacan, pero **no improvisan** porque todo está en reglas.

3. **Proyectos de IA basada en datos / planning** que han aparecido en Wave-of-Light (`{"sin confirmar": nombre y existencia exacta de un proyecto reciente de IA basada en GOAP o HTN para AoE II.}`).

### 4.4. Lo que hace bien y lo que hace mal la IA de AoE II

**Lo que hace bien:**

- **Determinismo**: dado el mismo script y misma semilla, juega idéntico. Excelente para *replays*.
- **Cero coste**: la IA no usa ni un ápice de cómputo pesado; es literalmente texto + un evaluador de reglas.
- **Transparencia**: cualquier jugador puede leer el `.per` y entender qué reglas se aplican.
- **Suficiente para enseñar a jugar** y para jugar partidas iniciales en single-player.

**Lo que se sabe que hace mal** (coincidente con el síntoma de CHUNSA):

- **Aprendizaje cero**: cada script es un retrato instantáneo de su autor. No aprende del rival, no se adapta a la civ elegida por el rival (más allá de reglas generales), no se auto-balancea.
- **Reglas congeladas**: si añades una nueva unidad al juego (los DLC recientes de Return of Rome, las campañas) el `.per` no sabe qué hacer con ella hasta que alguien lo extiende.
- **Sin priorización elegante**: los rules se ejecutan en orden textual. Si dos reglas pugnan, el orden del fichero decide quién gana. Esto es frágil y reproducible como bug.
- **Investigación y comercio olvidados**: los bots oficiales no investigan tecnologías relevantes de la mitad del árbol y casi no comercian. **Es la misma enfermedad** que el "nadie investiga jamás" del síntoma descrito para CHUNSA.
- **No escala a multi-época**: los bots están pensados para el ritmo feudal → castillo → imperial; en un juego con 15 épocas explícitas, la IA de AoE II no sobreviviría.

### 4.5. Por qué es el referente para CHUNSA

CHUNSA y AoE II comparten pivotes: RTS determinista, histórico, economía de recursos discretos, transición de épocas, facciones con árboles tech. Si AoE II puede tener éxito con un simple motor de reglas para las primeras épocas, **es buena noticia**: prueba que no hace falta ML ni GA para jugar a RTS en condiciones razonables. Pero el caso AoE II también demuestra los límites: hay un techo del utility/scripting que coincide exactamente con la descripción del autor de CHUNSA.

---

## 5. Fuentes y referencias

> Cuando una referencia no se ha podido verificar más allá del conocimiento general, se marca con **"(sin confirmar)"**.

### GDC Talks y presentaciones

- **Jeff Orkin (Monolith, MIT)** — *"Three States and a Plan: The A.I. of F.E.A.R."*, GDC 2006. Disponible en GDC Vault: <https://www.gdcvault.com/play/1014661/>
- **Damian Isla (Bungie)** — *"Managing AI in the Middle of Halo 2", GDC 2005* y posteriores. GDC Vault.
- **Michael Mateas (UC Santa Cruz)** — varios artículos sobre utility AI y personajes sintéticos, c. 2002–2003.
- **Xander Davis (Guerrilla Games)** — *"Killzone 2: Aiming at a Moving Target with HTN Planning"*, GDC 2007. Aplica HTN.
- **Alex Mantzaris (Firaxis)** — presentaciones Civ V/VI AI en GDC y Game Developers Conference; descripción pública del uso de goal-based planning y utilities. Sin URL única cerrada — (sin confirmar) ubicación exacta de la charla, sí en CV del autor.
- **Chris Brilot (Creative Assembly)** — *"Total War: Warhammer — An AI Postmortem"*, GDC 2017. GDC Vault: <https://www.gdcvault.com/play/1024548/>
- **Santiago Ontañón y colaboradores** — competições de RTS AI en IEEE CIG. Sitio: <http://www.real-time-rts.com/> (sin confirmar URL exacta, pero la dirección es referencial).
- **Andrew Chambers et al. (Activision Blizzard)** — *"StarCraft II AI: Build Order and Beyond"*; los nombres exactos son (sin confirmar).

### Artículos y libros

- *AI Game Programming Wisdom*, varias series (Charles River Media / CRC). Capítulos de Dave Mark, John Manslow, Michael Buro.
- Dave Mark, *Behavioural Mathematics for Game AI*, Charles River Media, 2009. Discute techos del utility y combinatorics.
- Jeff Orkin, *"Applying Goal-Oriented Action Planning to Games"*, capítulo en *AI Game Programming Wisdom*, 2005–2006. Su sitio personal archivado: `<(sin confirmar)> alumni.media.mit.edu/~jorkin/`.
- Robert J. Stroud, *"Behavioural Game Design"*, 2014. Ediciones independientes.
- Mark, Pérez, Amaya & Calderón, *"HTN Planning for Games"*, libro blanco de Atrinium (2011/2014). (Sin confirmar URL exacta.)
- Steve Rabin, *Game AI Pro*, vols. 1–3 (CRC, 2013–2016). Capítulos de utilidad y planners.

### Investigación académica abierta

- Vinyals et al. (DeepMind), *"Grandmaster level in StarCraft II using multi-agent reinforcement learning"*, *Nature* 575, 350–354 (2019). <https://www.nature.com/articles/s41586-019-1724-z>
- Berner et al. (OpenAI), *"Dota 2 with Large Scale Deep Reinforcement Learning"*, 2019. <https://openai.com/five/>
- Churchill, Buro, *"Puppet Search"*, IEEE Conference on Computational Intelligence and Games (CIG), 2016.
- Michael Buro y grupo, serie *AIIDE / CIG papers* sobre bots de StarCraft.

### Recursos sobre AoE II

- Wiki community-maintained de AoE II scripting (no enlazo por cambios frecuentes, accesible vía búsqueda "age of empires 2 ai scripting rules").
- Foro AoEZone (aoezone.com) — hilos históricos sobre *AI behaviour rules* y `.per`.
- Foro r/aoe2 / r/aoe4 en Reddit — referencias a tournaments y a proyectos de IA de la comunidad.

### Recursos sobre arquitectura de IA en juegos en general

- AI Game Dev (sitio mantenido por Alex Champandard, ahora *gamedeveloper.com* / *gamaSutra*), multitud de artículos por autor: <https://www.gamedeveloper.com> y archivos previos.
- Daniele Giampà, *"Behavior Trees for Game AI"*, tesis (sin confirmar URL exacta).
- Blog de Andy Hendrickx, *"Behavior trees and beyond"*.

---

## Qué implicaría para CHUNSA

Sin proponer planes de sprints, sí procede señalar **qué cambia en cómo debería pensarse el problema**.

**1. El problema es de arquitectura, no de pesos.** Más features, mejores curvas, recalibraciones puntuales no van a sacar al utility del sitio donde está. Las listas tipo "construir 30 granjas, luego establo" pueden mejorar micro-decisiones, pero no generan contra-juego, comercio, investigación progresiva, ni alianzas entre ejércitos. Eso son decisiones **estructurales**. Tratar el problema como un ejercicio de ML/optimización de pesos para el utility existente es, probablemente, el camino que peor rinde por hora invertida.

**2. La unidad mínima útil probablemente ya no es "una acción por ciclo".** Es un *plan* de horizonte finito (de aquí al próximo cambio de época, o de aquí a 30 ticks): una secuencia de acciones que la IA se compromete a ejecutar salvo shock. Cuando el shock llegue, replanifica. Esa es la unidad que da investigación coherente, comercio, contraataques, sincronizaciones. Se obtiene con un planner corto (HTN para el esqueleto por época, GOAP para micro-shocks) y con un utility de selección entre planes disponibles — exactamente el patrón que F.E.A.R., Total War y Civilization documentan.

**3. Hay que introducir memoria compartida entre ciclos.** Un blackboard rico que cualquier capa puede leer y escribir, con al menos: economía actual, modelo del rival inferido (época, presión esperada, composición), plan actual en ejecución y eventos pendientes. Sin ese blackboard, las capas no se hablan entre sí, y CHUNSA будет el síntoma de siempre: cada capa útil corre aislada, ninguna coopera.

**4. Hay que pasar de "decidir una acción por tick" a "componer un plan por época".** En un juego de 15 épocas, **la unidad natural de decisión macro es la época**, no el tick. Cada época es un *goal context*: producir hasta entrar en la siguiente, defenderse si nos atacan, completar investigaciones pendientes si sobra econ, acumular ejército si toca打仗. Esta decisión macro se hace a 1 Hz (o menos), y dentro del epoch la IA ejecuta planes del nivel "5 acciones o así" antes de reevaluar. Es **exactamente** la descomposición que la .per de AoE II intenta, pero sin blackboard rico ni HTN — y por eso se queda corta donde se queda corta.

**5. CHUNSA tiene ventajas que ningún AAA tenía en su momento.** (a) Determinismo fuerte: con misma semilla, misma partida. Eso permite look-ahead real sobre un árbol finito (no necesitas MCTS probabilístico). (b) Dominio discreto, simbólico, mucho más pequeño que un SC2. (c) 15 épocas bien tipadas: en vez de "1 super-goal", tienes 15 metas estructurables. Estas tres ventajas combinan mejor con un **planner deliberativo** que con un *utility*. Cabe imaginar en CHUNSA un planner de horizonte parcial, alimentado por una función de utilidad, sobre un simulador rápido determinista.

**6. La IA no debe mirar a AlphaStar, debe mirar a F.E.A.R. y Total War.** El RL resolvió problemas exponenciales con costos también exponenciales; un RTS pequeño y determinista es justamente el caso donde la IA simbólica gana: conocemos el dominio, sus reglas y sus costes; queremos comportamiento legible y editable por un humano (no entrenamiento opaco). Las ideas de AlphaStar que sí importan (separación de roles, restricciones de tiempo, auto-test) se pueden reimplementar trivialmente en una arquitectura simbólica.

**7. La medida del éxito cambia.** El éxito ya no debería medirse por "tick-utilities más estables" sino por:
   - **Mide-tasa por época.** ¿La IA llega a época N en una distribución de turnos razonable?
   - **Comportamiento emergente.** ¿La IA investiga, comercia, responde a incursiones, monta contra-juego, abandona ideas que no funcionan?
   - **Cobertura en replays.** ¿Cuál es el porcentaje de replays en que la IA hace A → B → C cuando tiene oportunidad, frente al porcentaje en utility puro?

Cerrada esta puerta sobre arquitectura, las próximas partes —qué decidir cuándo (estrategia), cómo pelear (táctica) y cómo probar todo eso de forma determinista (P4)— pueden ya asumir que la IA tiene blackboard, capas y un planner.

---

*Fin de la PARTE 1.*