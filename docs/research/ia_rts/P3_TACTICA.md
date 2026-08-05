# Combate táctico en IA de RTS — Parte 3

## Resumen

1. **Decidir la escaramuza** se hace por relación de fuerzas ponderada (HP × counter × terreno) o por simulación forward barata; Lanchester se cita más como guía de balance que como algoritmo de decisión.
2. **La retirada** necesita histéresis explícita (umbrales separados para retirada y re-engage) y un estado de "ruta" absorbente temporal con recuperación costosa; sin esto, ping-pong o fight-to-death están garantizados.
3. **La moral** se modela como máquina de estados con transiciones asimétricas; entrar es rápido, salir es proceso. Company of Heroes (supresión) y Total War (moral + rally) son los referentes; el patrón se repite en Men of War, Dawn of War II, Ultimate General, Northgard.
4. **Composición** se resuelve con matriz de counters + scouting en producción; la simulación de build-trees y el aprendizaje quedan en investigación.
5. **El motivo mecánico de la aniquilación mutua** es la simetría perfecta: si daño, moral y composición son iguales en ambos bandos, no hay realimentación que rompa a uno antes. Los juegos que evitan aniquilación inyectan asimetrías (estado de ruta con DPS=0, supresión que bloquea disparo, contagio, líder unitario, breakpoint temprano).

---

## 1. Decidir la escaramuza

La decisión "ataco, espero o me retiro" se toma en el momento en que dos grupos se avistan. La práctica en RTS comerciales se concentra en un puñado de heurísticas, no en simulación seria.

### 1.1. Variables evaluadas, en orden de prevalencia

- **Fuerza efectiva = Σ (HP × DPS × counter_modifier)**. No se cuentan unidades, se pondera contra la composición enemiga observada. Es la heurística dominante en motores serios (StarCraft II de Blizzard, Age of Empires, la mayoría de bots de AIIDE/SSCAIT).
- **Composición y counters**. Infantería > caballería > piqueros > infantería es el esquema clásico de *Age of Empires II*. StarCraft II codifica counters fuertes por raza (sin confirmar los multiplicadores exactos).
- **Terreno**: chokepoints, altura, cobertura, línea de tiro. El terreno puede multiplicar la fuerza efectiva por factores de 1.5–3× incluso con inferioridad numérica.
- **Refuerzos en camino**: si hay producción activa y un camino de llegada, la IA competente espera aunque localmente pierda; si no, no espera.
- **Valor posicional**: ¿vale el punto? Un objetivo de victoria se lucha; terreno neutral se cede.

### 1.2. Lanchester y por qué se cita

Las leyes de Lanchester (1916, *Engineering* y compilaciones posteriores) son la matemática básica de por qué la concentración importa: N² para combate a distancia, N para melee. No se usan literalmente en la IA tick-a-tick de un juego comercial (sería predecirse a sí misma con variables ocultas), pero sí:

- Como guía de balanceo: los diseñadores calibran DPS y HP sabiendo que dos grupos iguales se destruyen en tiempo ∝ N.
- En algunas simulaciones internas rápidas: se aproxima el choque con una ecuación de Lanchester simplificada antes de comprometer el ejército. (Sin confirmar en qué juegos comerciales.)

Cita útil y verificable: **Buro y Furtak, "Real-Time Strategy Games: A Real-Time Strategic Game AI Competition"** (AIIDE Workshop 2007) menciona Lanchester como referencia explícita. La survey canónica del campo es **Ontañón et al., "A Survey of Real-Time Strategy Game AI Research"** (IEEE Transactions on Computational Intelligence and AI in Games, 2013), que cubre el problema táctico en detalle.

### 1.3. Lo que la práctica no hace

- **Simulación forward Monte Carlo del choque**: existe en papers (Synnaeve 2016 sobre StarCraft; Church 2015 sobre UAlbertaBot) pero el coste es prohibitivo con cientos de unidades por tick.
- **Predicción del replanning enemigo**: se asume trayectoria recta; predecir replanning es raro y caro.

La práctica, en resumen: función de puntuación + umbrales, no búsqueda.

---

## 2. Retirada

Es lo que más te interesa y el problema peor resuelto en la literatura pública. La mayoría de la literatura de RTS AI no modela retirada moral; la ignora.

### 2.1. Por qué la oscilación es el comportamiento por defecto

Una IA que evalúa "ataco o huyo" cada tick con un único umbral oscila matemáticamente. El escenario patológico:

- t=0: ratio 0.9, no ataco ni huyo.
- t=1: enemigo avanza, ratio 0.4, huyo.
- t=2: enemigo persigue, ratio 0.3, sigo huyendo.
- t=3: enemigo se detiene, ratio 1.0, reataco.
- t=4: …

O peor, el caso CHUNSA: pánico a 20 de moral, salida a 50, y mientras en pánico la unidad no hace daño → estado absorbente permanente. Esto es comportamiento por defecto de cualquier modelo con un solo umbral y un daño cero en pánico sin temporizador.

### 2.2. Mecanismos que rompen el ciclo (los cuatro que importan)

**a) Histéresis explícita**

Umbral de retirada distinto del umbral de re-engage. Patrón típico:

- Si `fuerza_amiga / fuerza_enemiga < 0.5` → RETIRAR.
- Si `fuerza_amiga / fuerza_enemiga > 1.2` → ATACAR.
- Entre 0.5 y 1.2 → mantener estado anterior (zona muerta).

Es la solución de libro de texto, documentada en *Game AI Pro* (Steven Rabin, ed., varios volúmenes) y en cualquier texto serio de AI de juegos. **Sin histéresis, la oscilación es matemáticamente inevitable** salvo que ruido la rompa.

**b) Estado "ruta" como absorbente temporal, no permanente**

Una unidad en ruta:

- **DPS = 0** (crítico: la moral va desacoplada del HP).
- No acepta órdenes tácticas durante un cooldown de shock.
- Solo se recupera cuando lleva T ticks sin recibir daño Y está en zona de rally designada (o junto a líder / unidad de moral alta).

Esto convierte el absorbente en **proceso**, no en umbral.

**c) Rally points fijos**

La retirada no es "vuelve al origen de la orden"; es "vete al punto R conocido y seguro". En muchos RTS la unidad vuelve al origen y, si está bajo fuego, no llega nunca. Total War (desde *Rome II* en adelante, sin confirmar versión exacta) implementa rally points a retaguardia gestionados por el general.

**d) Valor posicional asimétrico**

Si la IA tiene noción de "este punto vale más que esas vidas", prefiere ceder terreno a perder ejército. Sin esto, lucha por todo.

### 2.3. Juegos que lo hacen bien

| Juego | Mecánica | Cómo rompe la simetría |
|---|---|---|
| **Total War** (Creative Assembly) | Moral + rally + general | Unidad rota no pega; rally requiere orden + general cercano; sale del campo si no se rally. |
| **Company of Heroes 2** (Relic) | Pin + retreat-to-cover | Squad pinned se retira automáticamente a la cobertura más cercana; rally local. |
| **Men of War** (Best Way) | Supresión + pin + rendición | Unidades pueden rendirse; el bando que rinde transfiere unidades al enemigo. |
| **Ultimate General: Civil War** (Game Labs) | Breakpoint 30–50% + rally | Unidades rotas salen del combate; rally manual. |

### 2.4. Juegos que NO lo hacen (y pagan el precio)

- **StarCraft II**: sin mecánica de retirada explícita. Las unidades luchan hasta morir. La IA de Blizzard no modela pánico; los bots de AIIDE/SSCAIT tampoco lo compensan de forma sofisticada.
- **Age of Empires II**: HP-based, sin moral. Las unidades huyen brevemente al romper formación pero vuelven casi de inmediato.
- **Supreme Commander**: sin moral. Escala tan grande que la moral sería intratable.

### 2.5. Nota sobre las IAs de competencia

Los bots top de **SSCAIT** (Student StarCraft AI Tournament) y **AIIDE** StarCraft AI Competition no implementan retirada moral: heredan el problema de StarCraft y lo compensan con mejor micro y mejor macro. Es decir, **la retirada moral es un problema no resuelto, no un problema resuelto que se ignora**.

---

## 3. Moral y supresión

### 3.1. La morale no es un número, es una máquina de estados

El patrón canónico (compuesto a partir de CoH, Total War y Ultimate General):

```
[Calm]
  --(daño, aliados cayendo, sin cobertura)--> [Suprimido]
[Suprimido]
  --(sin daño T seg, opcionalmente en cobertura)--> [Calm]
  --(daño crítico / bajas próximas)--> [Pin]
[Pin]
  --(sin daño T seg, en cobertura)--> [Suprimido]
  --(daño sostenido)--> [Ruta]
[Ruta]
  --(sin daño T seg, en rally point, líder cerca)--> [Calm]
  --(daño continuo)--> [Ruta]   // absorbente temporal
```

La asimetría clave: **entrar es casi instantáneo; salir es proceso con requisitos múltiples**.

### 3.2. Company of Heroes — el referente moderno

Mecánica documentada en materiales de Relic y en charlas GDC (sin confirmar años ni oradores específicos):

- Recibir disparos llena una barra de supresión.
- Una squad con supresión llena está **pinned**: no dispara, se mueve lento, no captura puntos.
- La supresión decae si no recibe fuego.
- La cobertura reduce supresión entrante.

La retirada es **emergente** del sistema de supresión: una squad pinned no puede pelear, se va a cover, se recupera si no recibe más fuego. No hay decisión explícita de "huir"; el comportamiento emerge.

### 3.3. Total War — el referente histórico

Cada unidad individual tiene barra de moral. Mecánicas documentadas en *Total War Academy* y materiales de Creative Assembly:

- Ataques por retaguardia, de noche, contra general ausente, sin comer, sin formación: moral baja.
- Muerte de unidades cercanas: moral baja.
- Cruzar el umbral → unidad rota, huye del campo, **no hace daño**.
- General cercano, comer, victoria parcial, terreno alto: moral sube.
- Rally requiere orden explícita y general cerca.

La asimetría decisiva es la misma: una unidad rota tarda minutos de juego en volver, durante los cuales causa cero daño. Esto produce el "uno se rompe primero" del mundo real.

### 3.4. Cómo evitar el estado absorbente (el bug que tuviste)

Tu caso CHUNSA es exactamente el patrón patológico: pánico a 20, salida a 50, mientras en pánico la unidad no ataca. Es el peor de los mundos: estado absorbente con daño cero, sin temporizador, sin zona segura.

Soluciones que aparecen en la literatura y en los juegos que lo resuelven:

1. **Pánico temporal con cooldown**. Mientras la unidad está en pánico, corre T ticks sin re-evaluar. Sale del pánico cuando lleva T ticks sin recibir daño Y está a más de D tiles del enemigo. Si no, sigue corriendo.
2. **Pánico asimétrico en magnitud**. Entrar es rápido (umbral bajo). Salir requiere (a) tiempo en zona segura, (b) presencia de líder, (c) shock residual (moral_max < 100 durante un tiempo post-pánico). La unidad "sale" pero rinde menos durante un rato.
3. **Moral por escuadrón**, no por unidad individual. Con cientos de unidades, la moral individual es prohibitiva.
4. **HP y moral desacoplados**. Una unidad con HP lleno puede estar en pánico y ser inofensiva. Esta es la palanca que más te falta.

### 3.5. Otros juegos con moral notable

- **Dawn of War II** (Relic): supresión y moral, entre CoH y StarCraft.
- **Men of War** (Best Way): supresión, pin, rendición.
- **Ultimate General: Civil War** (Game Labs): morale con breakpoint al 30–50% de efectivos; rotura por formación.
- **Northgard** (Shiro Games): moral simple pero presente; afecta daño y se recupera con victoria o banquete.
- **Tooth and Tail** (Pocketwatch Games): moral como stat.
- **Age of Wonders** (Triumph Studios): moral afecta stats.
- **Total War: Warhammer** (Creative Assembly): moral con mecánicas mágicas adicionales (miedo, etc.).

---

## 4. Composición de ejército

### 4.1. Métodos, en orden de prevalencia en producción

**a) Tabla de counters (rock-paper-scissors).**
Lo más común. Matriz N×N de modificadores de daño. La IA mira composición enemiga observada y maximiza daño esperado. Implementación simple, debuggeable. Ejemplos claros: *Age of Empires II*, *Warcraft III*, *StarCraft II* (la IA Blizzard tiene esto codificado por raza).

**b) Coste esperado ponderado.**
Suma de (coste × counter_modifier_enemigo). Más fino que la tabla pura, mismo coste computacional. Es el refinamiento natural del método (a).

**c) Scouting-driven reactive.**
La IA espera al scout, identifica composición, cambia build. *StarCraft II* hace esto a nivel macro: si ve starport, hace vikings. *Company of Heroes* también.

**d) Búsqueda en build tree / forward simulation.**
Buscar la secuencia de producción que maximiza fuerza esperada en T minutos. Carísimo. Usado en investigación (Tartan, "Neuroevolution for RTS Games", 2010; UAlbertaBot con búsqueda de build orders), no en producción de juegos comerciales.

**e) Aprendizaje por imitación o refuerzo.**
- **DeepMind AlphaStar** (StarCraft II, 2019): aprende de replays pro + self-play. Crítica común: micro "inhumano", APM excesivo, parches posteriores lo redujeron.
- **Facebook CherryPi** (2019, sin confirmar estado actual): bot SC2 para competition, ML ligero + reglas.

**f) Composición espejo.**
La IA produce lo mismo que el enemigo independientemente. Red de seguridad contra combinaciones desconocidas. La IA "novata" de muchos juegos hace esto.

### 4.2. Para CHUNSA, lectura realista

**b + c** es lo que implementa un estudio pequeño con buena relación señal/ruido. Una matriz de counters (probablemente asimétrica: caballería pesada se rompe distinto con lanzas que con picas) más scouting para identificar la composición rival y re-orientar la producción. ML y simulación quedan fuera por coste y por lo poco que aportan contra un humano casual.

---

## 5. Concentración de fuego y micro

### 5.1. Cuánto gana la IA con micro

En *StarCraft II*, el micro es el factor diferencial número uno entre bots. Un bot con micro de nivel pro (kiting, stutter-step, focus fire) gana a uno con macro perfecto y micro nulo. La diferencia reportada en la literatura de AIIDE es "amplia, del orden de varios cientos de elo".

AlphaStar (DeepMind, 2019) demostró que micro fino puede prácticamente doblar el DPS efectivo de una unidad: un marine con kiting pega cerca del doble que uno estático contra zealots. Pero esto fue criticado como no humano y los parches posteriores limitaron el APM efectivo.

### 5.2. Qué merece la pena para un RTS histórico contra humano casual

**Lo que sí merece la pena:**

- **Focus fire en el líder / oficial / águila del rival**. Mata una unidad concreta en vez de pegar por todo. Mata antes; maximiza letalidad del ejército.
- **Concentración direccional**. Perforar un frente en lugar de atacar por todo el perímetro. Concentra DPS en el punto de contacto.
- **Retirar unidades heridas** antes de que mueran. Preservar fuerza viva: una unidad al 30% de HP aporta poco DPS y se pierde por una ráfaga.

**Lo que NO merece la pena para CHUNSA:**

- **Kiting fino**. En RTS histórico las unidades tienen formación; kiting rompe el modelo mental y la simulación visual.
- **Stutter-step**. Formaciones rígidas lo impiden.
- **Micro individual por unidad con cientos de unidades**. No escala.

---

## 6. La pregunta clave: por qué no se aniquilan mutuamente

### 6.1. Tu problema, formalizado

Tienes dos ejércitos parejos: mismo HP total, misma composición, mismo terreno. La simetría es perfecta. Si el daño es simétrico y la moral no diferencia, los dos llegan a cero al mismo tick. Aniquilación mutua determinista.

### 6.2. Por qué en la guerra real casi nunca pasa

Factores que rompen la simetría en combate real:

- **Asimetrías previas al choque**: uno llega cansado, el otro fresco; uno con munición baja, el otro no; uno sin comer, el otro no. No son grandes, pero sesgan la primera escaramuza.
- **Breakpoint por debajo del 100%**: la doctrina militar cita que una unidad se rompe al 30–50% de bajas, no al 100%. Los supervivientes están "técnicamente vivos" pero son combativamente inertes.
- **Contagio**: una unidad que ve a otra huir tiene 2–5× más probabilidad de huir ella misma. El bando que rompe una unidad ve cómo se rompe una segunda, luego una tercera (efecto dominó).
- **Mando y control**: ejército sin líder (general muerto) tiene más probabilidad de romperse. La destrucción de un punto (estandarte, carro) rompe una formación entera.
- **Coste asimétrico de la retirada**: en el mundo real, huir es caro (equipo, suministros, posición), pero el bando ganador no persigue al mismo coste porque la asimetría ya está creada.

### 6.3. Mecanismos que los RTS usan para reproducir esto

| Mecanismo | Juego(s) | Cómo rompe la simetría |
|---|---|---|
| Estado de pánico con DPS=0 | Total War | Ejército con X% en fuga pierde X% de DPS → realimentación positiva. |
| Supresión que bloquea disparo | CoH, DoW II | Squad pinned no pega; efecto emergente sin decisión explícita. |
| Breakpoint temprano (30–50%) | Ultimate General | Las unidades se van antes de morir; el bando con más unidades rotas cae en DPS linealmente. |
| Contagio espacial | Implementable | Squads adyacentes a squads en pánico tienen probabilidad de entrar en pánico. |
| Pérdida de unidad-líder | Total War, muchos | Matar al general causa caída global de moral; concentrador mecánico. |
| Producción asimétrica | StarCraft II | La batalla importa menos que la economía; los bots AIIDE ganan por macro, no por táctica. |

### 6.4. Cuál funciona en un RTS con cientos de unidades

Con cientos de unidades, los mecanismos viables son:

- **Moral por squad** (5–20 unidades), no por unidad individual.
- **HP y moral desacoplados**: HP por unidad, moral por squad.
- **Supresión agregada**: una squad acumula supresión conjunta; al pin, todas las unidades pinneadas.
- **Contagio espacial**: squads adyacentes a squads en pánico tienen probabilidad de entrar en pánico, modulada por su propia moral y por liderazgo.
- **Unidad-líder**: una unidad (o pocas) concentra la moral del ejército; su pérdida es catastrófica.

Lo que NO escala:

- Moral individual para 300 unidades.
- Decisión de retirada por unidad en cada tick.
- Búsqueda hacia adelante por unidad.

---

## Qué implicaría para CHUNSA

Sin plan de sprints, solo implicaciones de diseño.

1. **La aniquilación mutua es estructural, no numérica**. Mientras el daño y la composición sean simétricos, ambos ejércitos llegarán a cero a la vez. La solución no es más HP, más DPS o más moral: es una asimetría mecánica que rompa la simetría.

2. **La morale debe ser una máquina de estados, no un valor continuo con dos umbrales**. Entrar en pánico debe ser casi instantáneo; salir debe ser proceso con requisitos múltiples (tiempo sin daño, zona segura, líder cercano, shock residual). Mientras esto no se implemente, la morale seguirá siendo decorativa.

3. **Las unidades en pánico deben hacer daño cero**. Esta es la palanca decisiva de los juegos que evitan aniquilación mutua. Si una unidad en pánico sigue pegando al 50%, el estado se autocura y el efecto dominó no ocurre.

4. **El contagio espacial es lo que produce el "uno se rompe primero"**. Sin contagio, dos ejércitos A y B pierden unidades al mismo ritmo y nunca se desencadena la realimentación que colapsa a uno. Con contagio, el primero en perder una unidad tiene probabilidad de perder dos, luego tres.

5. **La asimetría debe venir de la composición, el terreno o los refuerzos, no solo de la morale**. La morale amplifica diferencias, no las crea de la nada. Una IA que produce cuádriga contra infantería ligera debe ganar mecánicamente antes de que la morale entre en juego.

6. **La retirada necesita histéresis explícita + rally point + cooldown de re-engage**. Sin histéresis, oscilación. Sin rally, unidades que mueren en la retirada. Sin cooldown, re-engage prematuro antes de reorganizarse.

7. **El foco de fuego rentable es el oficial / unidad-líder del rival**, no cualquier unidad. Si la IA concentra fuego en lo que rompe la moral del ejército rival, el combate termina antes y más asimétricamente. Esta es la palanca más rentable contra humano casual.

8. **El combate determinista no excluye asimetrías previas al choque**. Hambre, fatiga del ejército, moral base diferente por facción, ventaja de terreno al entrar en contacto. Cualquier asimetría pre-choque amplifica las mecánicas anteriores.

9. **El shock residual post-pánico impide que la simetría se restaure rápido**. Si una unidad que sale de pánico rinde al 70% durante 30 segundos, el ejército rival mantiene su ventaja local durante el resto del choque.

---

## Referencias (verificables)

- **Ontañón, S., et al.** "A Survey of Real-Time Strategy Game AI Research". *IEEE Transactions on Computational Intelligence and AI in Games*, 2013. Survey canónica del campo.
- **Buro, M. y Furtak, T.** "Real-Time Strategy Games: A Real-Time Strategic Game AI Competition". AIIDE Workshop, 2007. Cita Lanchester explícitamente.
- **Churchill, D.** "Build Order Optimization in StarCraft". AIIDE, 2015. Sobre búsqueda en build trees para UAlbertaBot.
- **Synnaeve, G. et al.** "Forward Modeling for Partial Observation Strategy Games". 2016. Sobre predicción forward del resultado de choques en StarCraft.
- **Vinyals, O. et al.** "Grandmaster level in StarCraft II using multi-agent reinforcement learning". *Nature*, 2019. AlphaStar.
- **Rabin, S. (ed.).** *Game AI Pro* (varios volúmenes). Capítulos sobre RTS AI, hysteresis y state machines.
- **Mark, D.** *Behavioral Mathematics for Game AI*. Cubre hysteresis y FSMs aplicadas a decisiones de retirada.
- **Relic Entertainment.** Materiales GDC sobre *Company of Heroes* y *Company of Heroes 2* (sin confirmar talk específico): modelo de supresión, pin, retreat-to-cover.
- **Creative Assembly.** *Total War Academy* (YouTube y web) y manuales de las sagas *Total War: Rome II*, *Attila*, *Warhammer*. Modelo de moral, rally, breakpoint.
- **Game Labs.** Manual y materiales de *Ultimate General: Civil War*. Modelo de morale con breakpoint al 30–50%.
- **AIIDE StarCraft AI Competition** y **SSCAIT** (Student StarCraft AI Tournament). Competiciones anuales; resultados y código abierto de bots como referencia.