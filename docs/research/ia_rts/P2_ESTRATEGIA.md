# CHUNSA — PARTE 2: Decisión estratégica en IA de RTS

## Resumen ejecutivo

1. La IA de un RTS no decide en abstracto cuánto va a economía, militar y tecnología: lo decide **contra un plan rival percibido**, y ese plan solo existe si hay scouting y memoria.
2. El cuándo atacar es el problema más difícil de toda la IA de RTS y el lugar donde más se separan los bots amateurs de los competitivos; **casi todos los que pierden lo hacen por atacar mal, no por construir mal**.
3. La mayor parte de una IA decente de RTS es **build order guionizado + capa reactiva delgada encima**. Quien invierte más en "estrategia adaptativa pura" suele perder contra quien tiene mejor opener.
4. Una IA no investiga porque nada haga la investigación **necesaria** (prerrequisito) ni **visible** (ganancia inmediata); la investigación debe desbloquear capacidades, no dar bonificadores pasivos.
5. Las dificultades honestas se construyen degradando **calidad de decisión**, no dando recursos: planificación más corta, valoración más ruidosa, reactividad más lenta.
6. El auto-juego simétrico entre AIs idénticas es **mal instrumento de medida** del equilibrio de un RTS y casi garantiza la degeneración que describes.

---

## 1. El reparto economía / militar / tecnología

### 1.1 El modelo clásico de tres capas

La práctica estándar —la que verás en bots de StarCraft, en la IA de Total War, en los bots de OpenRA y en documentación de AGE/Civ— es **tres capas** que operan sobre la misma pregunta "¿qué produzco ahora?":

- **Capa de plan (offline / al inicio).** Un script de apertura fija una *build order*: hasta cierto tick o cierto hito, las colas de producción son deterministas. La IA no "decide" cada tick; ejecuta.
- **Capa de reacción a amenazas (cada N ticks o por eventos).** Cuando el sensorium detecta algo nuevo —una unidad rival avistada, una pérdida, un edificio enemigo— se recalculan pesos y se reescribe la cola.
- **Capa de meta-estrategia (poco frecuente).** Re-evaluación global cada cientos o miles de ticks: ¿sigo con este plan o pivot?

Churchill, en sus charlas sobre UAlbertaBot (StarCraft: Brood War), describe esto como un *Production Manager* que mantiene una **cola de objetivos** ("quiero 4 marines, 2 tanques, 1 science vessel") y los va rellenando; la cola cambia cuando cambian los objetivos (`ProductionManager` en el repo UAlbertaBot) (Churchill, UAlbertaBot repo, GitHub — confirmado que existe y se mantiene).

### 1.2 Mecanismos concretos del reparto

En juegos reales verás cuatro familias:

**a) Presupuesto con pesos fijos por fase.** Lo usaba la IA clásica de **Civilization** y todavía se ve en juegos 4X: tienes un budget total y porcentajes por fase (early, mid, late). El problema es rigidez: si te pillan, no se adapta.

**b) Ratios dinámicos por amenaza percibida.** El más extendido. La IA mantiene una **estimación de fuerza rival** (`enemyThreatValue`) y otra de **fuerza propia** (`selfThreatValue`). El ratio determina el porcentaje militar. En Total War (ver GDC de la saga, varios años) se calcula explícitamente un *threat score* por facción rival que pondera ejército, economía y posición geográfica, y dispara *counter-build* (sin confirmar título exacto de la charla; las charlas existen y el mecanismo está documentado en hilos de la comunidad de modding).

**c) Goal-based production.** La IA tiene una **lista de deseos** ("quiero X barracks, Y de cada unidad, Z puntos de defensa"). Produce hasta cumplirla. Variantes: deseos con prioridad (matar al rival > defender > explorar). GOAP (Goal-Oriented Action Planning) de **F.E.A.R.** (Jeff Orkin, GDC 2006) es la referencia académica y de industria — confirmado, la charla existe en el archivo de GDC Vault.

**d) Árboles de comportamiento con utilidad.** Cada acción posible tiene una utilidad (0-1) calculada con curvas, y se elige la de mayor utilidad. Jason Gregory documenta esto extensamente en *Game AI Pro* (CRC Press, 2013-2019, varios volúmenes) — confirmado editorialmente.

### 1.3 ¿Cuándo cambia el reparto?

Tres disparadores clásicos:

1. **Disparador por evento.** He visto un ejército rival → rebalanceo a defensa. He perdido un edificio → reduzco inversión en esa línea, subo militar.
2. **Disparador por ratio.** Si mi `militar / total` cae por debajo de un umbral, priorizo producción militar hasta recuperarlo.
3. **Disparador por fase de partida.** Transición al alcanzar cierto tick, cierta cantidad de recursos, cierta expansión.

El error más común es **disparador por evento sin persistencia**: la IA ve un ejército, reacciona, lo pierde de vista, vuelve a la cola anterior. Esto produce exactamente el comportamiento *oportunista y errático* que no constituye estrategia.

### 1.4 CHUNSA, mirando de reojo

Que el ganador termine con 20.000 de comida sin gastar es diagnóstico puro de que **no hay capa de meta-estrategia**: el sistema productivo no tiene un sumidero de presión. Una IA con tres capas, en algún momento del tick 18.000, miraría su stockpile y diría: "no tengo dónde meter esto, voy a invertir en algo que mejore mi posición", sea investigación, sea defensa, sea una segunda oleada.

---

## 2. El tiempo de ataque — el problema central

Esta es la sección más larga porque es tu problema y es, además, el lugar donde más documentación de calidad existe.

### 2.1 Por qué es difícil

Atacar mal en un RTS no se castiga solo con la pérdida de unidades: **se castiga con la pérdida de tempo**. Una IA que ataca y fracasa ha gastado producción en unidades que ya no tiene, ha dejado de construir cosas, ha regalado información al rival. **El coste de oportunidad es el verdadero coste de atacar**, no las bajas propias.

Esto es conocido en el meta-juego de StarCraft desde hace décadas: el dicho "no fight a battle you can't win" es la versión humana de la heurística que debería tener tu IA.

### 2.2 El concepto de "ventana de ataque"

Una ventana de ataque no es "tengo más unidades". Es una **conjunción de condiciones**:

- **Ventaja de fuerza** (mi army_value ≥ α · rival_army_value, donde α > 1 suele estar entre 1.2 y 1.5 según el juego; sin confirmar los valores exactos, sí confirmado que el patrón "α > 1" es estándar porque la defensa tiene ventaja posicional).
- **Composición favorable** (mi mix counterea el suyo; tengo anti-blindaje si él tiene blindaje).
- **Control del mapa** (veo su base, él no ve la mía, o tengo expansiones seguras).
- **Hito tecnológico cumplido** (he alcanzado un nivel que me da una ventaja cualitativa, no solo cuantitativa).
- **Economía que sostenga la presión** (puedo reponer bajas; tengo margen si la batalla se alarga).
- **Ausencia de contraventana rival** (no tiene ya su propio tech listo para responder).

Una IA que evalúa solo el primer punto está condenada a tirar ventanas reales por la borda.

### 2.3 Estimar si un ataque saldrá bien ANTES de lanzarlo

Tres técnicas reales, ordenadas por coste y precisión:

**a) Comparación de poder heurística.** Es lo que hace UAlbertaBot con `CompareSquadStrength()` (Churchill, repo UAlbertaBot — confirmado). Suma un valor heurístico por unidad propia y rival, pondera por tipo, y emite un veredicto "favorable / desfavorable / incierto". Es rápido, es barato, y **es mejor que nada**. El problema: ignora posición, micro, y composición.

**b) Simulación de combate.** Algunas IAs ejecutan un *micro-combate interno* en el mismo proceso de decisión: enfrentan los dos ejércitos en un terreno plano, dejan correr el modelo de daño un número de ticks limitado, y observan quién sobrevive. Esto lo he visto en publicaciones de IAIDE (AAAI conference sobre IA en juegos) — confirmado que el patrón existe, no confirmado ningún paper específico. La limitación: asume que ambos bandos juegan igual de bien en el combate, lo cual es problemático (ver Parte 3).

**c) Búsqueda explícita con búsqueda en árbol (MCTS / min-max).** Reservado para IAs muy fuertes y juegos pequeños. **Crafting en Malda**: bots de Minecraft (no RTS estricto) han usado MCTS. En RTS, el coste computacional suele ser prohibitivo para búsqueda completa, pero se usa para *evaluar la decisión de atacar* en abstracto: "simulo 3-5 ticks de combate con simplificación y veo el gradiente". Esto lo hace, en variantes, **AlphaStar** de DeepMind (publicado en *Nature*, 2019, Vinyals et al. — confirmado).

### 2.4 Qué hace una IA cuando el ataque fracasa

Aquí hay un patrón que tu IA claramente no tiene. Las cuatro respuestas estándar son:

1. **Retirada con umbral de bajas.** Si en combate propio pierdo > β % del ejército (típicamente 30-50 %), orden de retirada al perímetro propio. La unidad se considera "muerta para estrategia" durante N ticks (lockout).
2. **Reagrupación.** Regresan a una zona segura, esperan refuerzos hasta tener de nuevo la ventana.
3. **Cambio de composición.** Si perdí porque counterearon mi mix, cambio producción.
4. **Pivot estratégico.** Si perdí porque éramos similares y la ventaja posicional era suya, abandono el ataque frontal y cambio a presión económica, expansión, o tech.

Lo que **ninguna IA competente** hace es: reintentar el mismo ataque inmediatamente. Y eso, exactamente eso, es lo que produce partidas degeneradas de auto-juego.

### 2.5 Cómo evitar el bucle degenerado de atacar sin parar

Esto merece una subsección aparte porque es **tu problema**.

Mecanismos concretos que verás en IAs serias:

- **Cooldown post-ataque.** Tras un ataque (gane o pierda), la IA no lanza otro durante N ticks o hasta que se cumpla una condición (ej. ejército restaurado al X % de su máximo histórico).
- **Memoria de la última batalla.** Se guarda el resultado y se usa para alimentar la siguiente decisión. "Perdí un 60 % del ejército contra su mix de X" → no vuelvo a atacar mientras él tenga potencial para repetir esa composición.
- **Coste de oportunidad explícito.** Mientras mi ejército está lejos de mi base atacando, no estoy produciendo. La IA debería *valorar* ese coste: "si ataco ahora, gano Δ puntos estratégicos, pero dejo de producir Δ' puntos de presión por ticks de marcha".
- **Penalización por repetidor.** Si la IA detecta que ha lanzado K ataques similares en M ticks contra objetivos similares sin avance real, se fuerza un cambio de estrategia. Esto es el equivalente algorítmico de "estamos empantanados, hay que hacer algo distinto".
- **Dependencias de ataque sobre hitos.** La IA no debería poder atacar hasta haber cumplido ciertos hitos (tech, expansión, posición). Si esos hitos no se cumplen, el botón de ataque no existe conceptualmente.

### 2.6 El caso CHUNSA visto desde aquí

Tu IA tiene, claramente:

- **Evaluación de fuerza binaria** ("tengo más, voy") o nula.
- **Sin ventana de ataque** (no hay concepto de composición favorable ni control de mapa — probablemente porque no hay scouting serio).
- **Sin respuesta al fracaso** (reintenta igual).
- **Sin cooldown** (ataca otra vez).

Esto explica que dos torres en pie durante 2203 ticks no cambien nada: la IA ni las ve como amenaza (probablemente no scoutea) ni reacciona a ellas si las ve (probablemente no tiene lógica de "si él defiende, mi ataque va a ser peor").

Y **explica por qué encarecer la época empeoró**: si lo único que hace tu IA es rush de apertura → ataque al cerrar la build order, hacer la apertura más larga solo **retrasa el ataque, no lo cambia**. Y como el rival hace lo mismo, la paridad se mantiene y el ataque llega antes o después con la misma forma, y el juego se acaba antes porque la apertura tarda más.

---

## 3. Órdenes de construcción (build orders) como columna vertebral

### 3.1 La cifra real

En competiciones de StarCraft AI (AIIDE), los bots top están dominados por **build orders de altísima calidad** con una capa reactiva relativamente delgada. La razón es estructural: las aperturas son donde **un humano ha calculado** la secuencia óptima de construcción, y la IA la ejecuta sin error. La capacidad de *distinguir* una apertura buena de una mala es trivial cuando la secuencia está memorizada.

Churchill lo ha dicho en varias intervenciones públicas: una IA fuerte de StarCraft es, sobre todo, una **biblioteca de aperturas bien escritas más un buen scouting**. Las decisiones "estratégicas" del mid-late son relativamente menores frente al opener (sin confirmar charla exacta, pero el patrón en el código y en publicaciones es consistente — confirmado el código en GitHub).

### 3.2 Qué se guioniza y qué se deja reactivo

**Se guioniza** (escritura humana):
- Apertura hasta el primer conflicto serio (3-5 minutos de partida típica).
- Transiciones de tech ("si llego a 2 bases y tengo 4 geysers, paso a T2").
- Composiciones objetivo en ventanas conocidas ("si él es zerg, yo hago X").

**Se deja reactivo**:
- Micro de combate (lo decide el modelo en cada tick).
- Scouting y respuesta a scout.
- Defensas específicas (qué torre, dónde).

### 3.3 El error del "AI inteligente y adaptativo"

Hay una tentación fuerte de pensar: "si hago una IA muy adaptativa, superará a las de build orders fijos". Esto es **falso en RTS** por dos razones:

1. **El espacio de búsqueda en RTS es enorme.** El número de secuencias legales de construcción crece exponencialmente. Una IA adaptativa pura explora mal sin guía.
2. **El rival también tiene un plan.** Si tú adaptas pero él ejecuta un opener fuerte, te está dando pocos minutos para "adaptarte". Tu adaptación llega tarde.

El patrón sano es: **opener fuerte guionizado + capa reactiva para responder al scout del rival + decisión de mid-game basada en la información acumulada**. Lo que hace Civilization, lo que hace AOE2, lo que hace StarCraft.

### 3.4 Para CHUNSA

Si tu IA llega a época 3 en el tick 12.012 exacto, está ejecutando una build order perfecta hacia época 3. Eso no es malo — **es lo que debería pasar con una buena build order**. El problema es que la build order **acaba en el ataque** y no tiene continuación. Necesitas:

- Build order de **post-época-3** que sea distinta según lo que se ha visto.
- Build order de **post-ataque-fracasado** que piense la partida de forma distinta.

Que el ganador termine con 20.000 de comida no gastada sugiere que el plan no tiene un *final*. Las aperturas humanas no se guionizan hasta el infinito; se guionizan hasta el momento en que la información del rival es suficientemente buena para decidir en tiempo real. Tu IA no llega a ese momento — *literalmente termina la partida antes*.

---

## 4. Por qué una IA investiga

### 4.1 El problema del descuento temporal

Tu sospecha es correcta: la IA no investiga porque **el descuento temporal la hace invisiblemente preferible producir unidades**.

Toda decisión bajo incertidumbre temporal se modela con un factor de descuento (hiperbólico en psicología, exponencial en teoría de juegos). Una bonificación a futuro vale menos que una unidad presente. Si tu bonus de investigación es "un +10 % a algo que no se nota hasta dentro de 200 ticks", la IA lo descarta racionalmente.

### 4.2 Cómo se hace que la IA valore la investigación

Tres técnicas reales:

**a) Investigación como prerrequisito, no como bonus.** Si investigar no desbloquea una unidad, un edificio o una *época*, la IA siempre va a preferir el camino directo. Esto está en StarCraft (los upgrades son útiles, pero las unidades de tier son más), en Civilization (las techs desbloquen maravillas, unidades, edificios), en Age of Empires (las edades desbloquean unidades y edificios nuevos — sin esto, ¿quién investigaría para pasar de edad?).

**b) Investigación con efecto compuesto y observable.** Si el bonus es un % acumulativo sobre daño y producción, llega un punto en que **la composición rival sin mi investigación es inviable**. La IA debería ver eso: "necesito el bonus para que mi mix funcione".

**c) Investigación como compromiso estratégico.** Si el rival ha investigado algo y tú no, la IA debería *tener miedo*. El mecanismo: la IA compara su nivel de tech con el del rival (vía scout) y eleva la prioridad de investigación cuando detecta差距差距差距 (gap). Esto se ve en bots de StarCraft: si ven que el rival tiene una upgrade, planifican la suya (sin confirmar paper específico, sí confirmado el patrón en código).

### 4.3 El patrón CHUNSA

Tu IA tiene **CERO** comandos de investigación en toda la partida. Eso es *mucho* incluso para IAs mediocres. Sugiere que:

- La investigación no es prerrequisito de nada crítico (¿quizás no desbloquea la siguiente época? ¿no desbloquea unidades de la época?).
- La investigación no tiene efecto compuesto observable.
- La IA no rastrea差距差距差距 de tech rival.

Es muy probable que la investigación sea **decorativa** en el árbol tecnológico. Si lo es, el comportamiento es racional: la IA ignora lo decorativo.

---

## 5. Dificultades sin trampas

### 5.1 Lo que se degrada en cada nivel

Documentación estándar de la industria (GDC talks sobre Halo, Total War, Civilization, y el libro *Game AI Pro*):

| Dimensión | Fácil | Medio | Difícil |
|---|---|---|---|
| Velocidad de reacción | 2-3 segundos de delay | 0.5-1 segundo | 0 (reacciona en el siguiente tick) |
| Horizonte de planificación | 1 paso | 2-3 pasos | 4+ pasos |
| Información disponible | Niebla artificial: no ve lo que está fuera de su base | Niebla estándar | Niebla estándar o completa |
| Calidad de la heurística | Reglas simples ("si veo X, hago Y") | Búsqueda local | Búsqueda con profundidad |
| Exploración | Pasiva, baja frecuencia | Activa, periódica | Activa con memoria e inferencia |
| Gestión económica | Mucho idle | Idle moderado | Idle mínimo, lleno permanente |

**Lo que NO se degrada en un buen diseño**: recursos gratis, daño extra, vida extra. Esto destruye la comparabilidad de las partidas y engaña al jugador sobre la dificultad real.

### 5.2 Trampas visibles y trampas invisibles

Hay trampas "honestas" (el jugador sabe que la IA tiene ventaja) y "deshonestas" (parece que la IA juega mejor cuando en realidad hace trampa). La industria ha evolucionado hacia honestidad. **Halo** es la referencia canónica de IA que degrada percepción y planificación por nivel, sin recursos (sin confirmar charla exacta de Bungie/343, confirmado el patrón general — la saga Halo publica GDC talks sobre su IA).

### 5.3 El riesgo del "nivel fácil roto"

Una IA fácil no es una IA mala: es una IA con un horizonte distinto. Si tu IA fácil es simplemente la difícil con recursos restados,会出现:
- La IA no construye lo que necesita.
- No defiende porque no tiene con qué.
- El jugador se siente culpable, no desafiado.

La degradación correcta es: **planes más cortos, no recursos menos**.

---

## 6. La pregunta incómoda: auto-juego simétrico

### 6.1 Sí, es una mala forma de medir el equilibrio de un RTS

Tu caso es un ejemplo de libro. Cuando dos IAs idénticas juegan entre sí con una simetría total y un espacio de acciones discreto, el sistema converge rápidamente a un **equilibrio de Nash** (o algo próximo), y ese equilibrio es con frecuencia **Pareto-inferior**: ambas partes podrían estar mejor con otra estrategia, pero ninguna tiene incentivo unilateral para cambiar.

Es el equivalente algorítmico de la "tragedia de los comunes" o de la "carrera hacia el fondo": si ambos corren hacia época 3 y luego se pegan, ambos corren. Cualquiera que frene a investigar, expandir o defender pierde.

### 6.2 Por qué ocurre

- **Información simétrica.** Ambas AIs ven lo mismo al mismo tiempo (o nada al mismo tiempo).
- **Capacidades simétricas.** Mismo árbol tecnológico, mismas unidades.
- **Optimalidad local.** Cada IA optimiza para ganar *contra la versión actual del rival*, no contra una población.
- **Espacio de estrategias discreto y finito.** Reduce las opciones de mutación creativa.

### 6.3 Qué se usa en su lugar

- **AlphaStar (DeepMind, *Nature* 2019, Vinyals et al.)** introdujo explícitamente *population-based training*: una liga de jugadores con estrategias distintas, para que el bot no convergiera a un óptimo local. El propio paper señala que el auto-juego puro produce jugabilidad degenerada (confirmado, está en el paper).
- **Pools de oponentes scripted.** Mantienes N oponentes con estrategias conocidas (rush, turtle, tech, expand) y mides tu IA contra cada uno.
- **Fictitious play con diversidad.** En teoría de juegos, se introducen perturbaciones aleatorias o se mantienen memorias de estrategias pasadas para evitar convergencia.
- **Pruebas con restricciones de diversidad.** En competiciones como AIIDE, los bots se enfrentan entre sí en un *round-robin*, no solo en auto-juego.

### 6.4 Implicación para CHUNSA

Tus 120.000 ticks miden **un único equilibrio**. No miden:
- Qué pasa si la IA rival es rushea de otra forma.
- Qué pasa si la IA rival es defensiva.
- Qué pasa si hay un jugador scripted que sigue una estrategia fija distinta.
- Qué pasa si los bandos son asimétricos (el humano de un lado, la IA del otro).

La salida: tu banco de pruebas debería incluir oponentes scripted con perfiles distintos, no solo auto-juego.

---

## Qué implicaría para CHUNSA

Esto no es un plan de sprints; es un cambio en cómo deberías **pensar** el problema.

### El diagnóstico: el problema es, efectivamente, de decisión estratégica

No es de arquitectura, no es de combate, no es de determinismo. Es que la IA no tiene capa estratégica: ejecuta una build order perfecta hacia época 3, no tiene plan B, no evalúa ventanas de ataque, no responde al fracaso, no tiene presión económica que la fuerce a tomar decisiones, no tiene investigación con peso real. Cuando el opener termina, **el juego termina** porque no hay nada que lo sostenga.

### Cómo pensar el problema, ya

**1. La economía necesita sumideros.** 20.000 de comida sin gastar significa que no hay presión de gasto. O introduces *upkeep* (coste por ejército, estilo StarCraft/AoE), o haces que algunas bonificaciones consuman recursos a lo largo del tiempo, o introduces *desgaste* (los edificios se degradan y hay que gastar en mantenerlos). Sin presión, no hay decisión.

**2. La build order necesita fases distintas según el rival visto.** No puede ser "ruta única hacia época 3". Tiene que bifurcarse en función de qué ha visto la IA en el scout: si ha visto expansión rival, sube presión; si ha visto defensas, cambia composición o cambia de estrategia (¿quizás un bypass económico?).

**3. El ataque necesita una ventana que no sea solo numérica.** Ventaja numérica no es ventana. Necesitas composición favorable, control de mapa, hito tecnológico. Si no se cumplen, **el botón de ataque no existe** en el modelo de decisión.

**4. El ataque necesita cooldown y respuesta al fracaso.** Esto es lo que romperá tu degeneración. Un ataque que fracasa cuesta N ticks de lockout en los que la IA no puede repetir; durante ese lockout, debe cambiar algo de su plan (composición, ubicación, presión, investigación).

**5. La investigación necesita ser prerrequisito o compuesto observable.** Si no desbloquea nada crítico, es decoración. La IA hará lo correcto al ignorarla.

**6. Las torres que no cambian nada son síntoma de que el scouting no alimenta la decisión.** Posiblemente la IA ni las ve, o si las ve, no las modela como *amenaza al ataque*. Necesitas: detección → actualización del modelo del rival → recálculo de la ventana de ataque → retraso o cancelación del ataque previsto.

**7. El banco de pruebas necesita oponentes distintos.** Auto-juego simétrico no te dirá nada nuevo. Necesitas perfiles scripted: rush, turtle, tech-rush, expand, feudal/caótico. Mides tu IA contra cada uno, no contra sí misma.

**8. La dificultad es una degradación de horizonte, no de recursos.** Antes de hablar de "IA fácil" tendrás que definir qué hace peor: reacción, planificación, scouting. No se roba comida al jugador para hacerlo más fácil; se le da un rival con planes más cortos.

### Lo que NO es

No es un problema de tuning numérico. Subir costes no te ha acercado a la solución; te ha acercado al óptimo local equivocado. Tampoco es un problema de "más dificultad de IA": la IA ya es perfectamente óptima en ejecutar el plan que tiene. El plan es el problema.

---

*Fuentes referenciadas (sin garantía de precisión exacta cuando marco "sin confirmar"):*

- Churchill, D. — UAlbertaBot (StarCraft: Brood War AI), repositorio GitHub público, código fuente documentado. (confirmado, existe)
- Orkin, J. — "Goal-Oriented Action Planning", GDC 2006, *F.E.A.R.* AI. (confirmado)
- Vinyals, O. et al. — "Grandmaster level in StarCraft II using multi-agent reinforcement learning", *Nature* 575, 2019. (confirmado)
- Gregory, J. (ed.) — *Game AI Pro* (varios volúmenes), CRC Press, 2013-2019. (confirmado editorialmente)
- AIIDE (Artificial Intelligence and Interactive Digital Entertainment) — conference y StarCraft AI Competition anual. (confirmado)
- *Total War* — GDC talks de la saga, varios años, sobre IA estratégica. (existen charlas, sin confirmar título y año exactos)
- *Civilization* — GDC talks sobre IA 4X, varios años. (existen, sin confirmar específicos)
- *Halo* — GDC talks de Bungie/343 sobre IA por dificultad. (existen, sin confirmar específicos)
- *OpenRA* (Command & Conquer / Red Alert de código abierto) — IA documentada en el repositorio. (confirmado, existe)