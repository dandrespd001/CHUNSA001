# PARTE 4 — Determinismo y pruebas de la IA en RTS

## Resumen en cinco líneas

1. La IA de un RTS lockstep es el componente más frágil del determinismo: contenedores no ordenados, FP, hilos y desempates por dirección rompen la paridad entre clientes aunque la simulación base sea bit-perfect.
2. Una IA determinista puede no ser predecible si su aleatoriedad procede de un PRNG sembrado desde el estado compartido; el truco no es evitar el azar, sino eliminar el *azar libre*.
3. Probar una IA de RTS exige medir trayectorias, no solo resultados: bancos con varias semillas, métricas de eficiencia, detección de bucles por huella de estado y regresión de comportamiento caracterizada.
4. El auto-juego simétrico es bueno para estabilidad y regresión, y malo para equilibrio y "dificultad real"; hay que combinarlo con asimétrico, guiones y repeticiones humanas.
5. Las repeticiones pueden ser prueba de regresión si la IA se versiona y se separa "interpretar órdenes humanas" de "decidir la IA"; sin esa separación, todo cambio de IA invalida el banco.

---

## 1. IA determinista en lockstep: qué rompe el determinismo en la práctica

En lockstep, todos los clientes simulan los mismos ticks y solo se cruzan órdenes. La condición es bit-perfect: dado el mismo estado y las mismas órdenes, **cada cliente debe producir el mismo digest**. La IA es una función pura del estado, pero en la práctica su implementación es una de las fuentes más comunes de desincronización.

### 1.1 Causas reales de desincronización inducidas por la IA

**Coma flotante.** Es el clásico. Distintos FPU, distintos modos de redondeo, distintas optimizaciones del compilador (reordenamiento de operaciones, fusiones mul-add, asociatividad no respetada) producen divergencias en sumas acumulativas. Las partidas largas (10⁵–10⁶ ticks) son especialmente sensibles: un epsilon por suma crece monótonamente. Los casos históricos:

- **Total Annihilation** documentó durante años desincronizaciones ligadas a FP en la simulación física y de proyectiles (post-mortems y reportes de la comunidad TA / Balanced Annihilation).
- **Supreme Commander / Forged Alliance** mantienen un test suite de regresión de determinismo porque un cambio de compilador o de settings de optimización podía romper la paridad (documentación de la comunidad Forged Alliance Forever).
- **StarCraft II** tuvo incidentes documentados de desincronización ligados a ramas con FP (informes en los foros de Blizzard; GDC talks de Blizzard sobre su arquitectura lockstep mencionan este riesgo explícitamente).
- **Age of Empires / Age of Empires II** tienen un capítulo entero sobre determinismo en el célebre GDC talk "1500 Archers on a 28.8: Network Programming in Age of Empires and Beyond" de Paul E. S. Faulkner, donde se discute cómo el orden de operaciones en FP era un riesgo constante.

CHUNSA ya tiene esto cubierto con aritmética de punto fijo y compilador determinista, pero conviene recordar que el punto fijo tampoco es inmune: un cambio de precisión intermedia o de orden de operaciones entre versiones del algoritmo sigue rompiendo paridad.

**Orden de iteración sobre contenedores con tabla hash.** `std::unordered_map`, `std::unordered_set` y equivalentes en otros lenguajes iteran en el orden del *bucket*, que depende del hash seed, de la capacidad del contenedor y del historial de inserciones. Un hash por defecto dependiente de la dirección de memoria (caso típico en MSVC antes de C++20 con `std::hash`) hace que dos procesos con direcciones distintas iteren distinto. La IA, que suele recorrer unidades por algún tipo de índice "para desempate", debe **ordenar explícitamente** por ese índice o usar contenedores ordenados. Una sola línea que recorra `std::unordered_map<UnitId, UnitState>` sin ordenar es una bomba de desincronización.

**Punteros como criterio de desempate.** "Si hay empate, gana la unidad con menor dirección de memoria" parece inofensivo y aparece con frecuencia en el código de IA novato. Las direcciones de memoria **no son estables** entre procesos ni entre ejecuciones: cambian con el ASLR, con el layout de heap, con la versión del allocator, con el orden en que se cargaron las DLL. Es un patrón explícitamente desaconsejado en los motores que mantienen determinismo (Spring/Zero-K, código fuente de Balanced Annihilation, posts técnicos recurrentes en *Gaffer on Games*).

**Hilos y concurrencia.** Incluso con scheduling determinista (lo cual ya es difícil de garantizar), la jerarquía de memoria introduce variabilidad: dos núcleos con caches independientes que reducen operaciones en distinto orden pueden alterar resultados en FP y, en menor medida, en enteros por efectos de saturación de pipelines. La disciplina en RTS lockstep es tajante: **un solo hilo de simulación**, sin paralelismo dentro del paso. Algunas engines (Spring) han experimentado con paralelismo restringido a tareas que no afectan al digest, pero es la excepción y requiere disciplina extrema.

**RNG libre o sin sembrar.** Un `rand()` no sembrado consume entropía del sistema. Dos clientes distintos, o dos arranques del mismo binario, ven secuencias distintas. Cualquier decisión estocástica de la IA debe pasar por un PRNG determinista cuyo estado forma parte del estado replicado.

**Dependencias de estado externo al tick.** Leer el reloj, consultar configuración del usuario, leer un fichero del disco, pedir memoria al heap durante la decisión. Cada uno rompe el determinismo de una forma distinta. Por eso CHUNSA prohíbe el heap dentro del paso: incluso `malloc` puede devolver direcciones distintas entre clientes.

**Orden de evaluación de efectos laterales en actualizaciones "reactivas".** Si la IA emite una orden que se procesa en el mismo tick que la simulación, el orden importa. Una IA que, en el mismo tick, "comprueba que hay sitio para construir" y "manda construir" puede tener un orden distinto a otra IA que primero construye y luego comprueba: si la comprobación usa el estado del propio tick, diverge.

### 1.2 Disciplinas estándar para evitarlo

Recopilando lo que se ve en motores deterministas (Spring/Zero-K, Recoil/TA, código fuente de OpenRA — proyecto de reimplementación de C&C: Red Alert con foco en determinismo de red —, posts de Blizzard sobre SC2 y varios GDC talks sobre networking):

- **Reemplazar `std::unordered_*` por contenedores ordenados o por arrays indexados.** Cuando se necesita "set" o "map", usar contenedores con orden estable por clave, o iterar sobre el array subyacente ordenado explícitamente por índice.
- **Prohibir FP dentro del paso de simulación** (CHUNSA ya lo hace) o usar el flag de redondeo IEEE estricto y un orden de operaciones congelado y verificado por tests.
- **Desempate siempre por índice entero**, nunca por puntero, handle opaco, hash de puntero, ni por orden de inserción sin ordenarlo después.
- **PRNG determinista sembrado desde el estado replicado**, con estado visible en el digest. PCG, xorshift64, SplitMix64 son opciones comunes.
- **Sin heap en el paso**: ni asignaciones ni liberaciones (CHUNSA lo tiene).
- **Sin hilos de simulación**, sin lecturas del sistema, sin acceso a recursos no replicados.
- **Snapshot del digest por tick y comparación** en cada cliente, con un protocolo de re-sync si diverge (en el caso de CHUNSA: al descartar la partida).
- **Versionado del algoritmo de IA en el header del estado**: si cambia cómo decide, se sube la versión y se invalida toda mezcla con partidas de versión anterior.

---

## 2. ¿Puede una IA determinista ser buena (no predecible)?

La pregunta confunde dos cosas distintas: **determinismo** (mismo estado → misma decisión) y **predictibilidad humana** (que un jugador humano pueda anticipar todas las decisiones). Una IA determinista puede ser **completamente impredecible para un humano** y seguir siendo determinista.

### 2.1 Cómo se consigue variedad sin azar libre

El truco es que la aleatoriedad no se elimina, se **siembra desde el estado**. Un PRNG determinista (xorshift, PCG, SplitMix) produce una secuencia infinita y reproducible dado el mismo seed. Si el seed se deriva del estado compartido (por ejemplo, del identificador de partida, del tick actual hasheado, de la posición de la semilla de mapa, o del estado del juego en un momento clave), entonces:

- Dos clientes ven la misma secuencia.
- Pero un humano, que no puede calcular la secuencia completa a 16 ticks vista, percibe la IA como "impredecible".
- Pequeñas variaciones de estado temprano se amplifican: una micro-decisión en el tick 200 hace que en el tick 800 la IA esté en una rama distinta del árbol de decisión.

Esta es la base de los "comportamientos estocásticos" en Total War, Civilization (Firaxis ha hablado de esto en GDC), y en la IA de campañas de muchos 4X. En RTS competitivos, donde la IA enemiga es lo que el jugador "lee" para aprender, esta impredecibilidad determinista es **deseable**.

### 2.2 Variación entre partidas iguales

Aquí aparece un compromiso real. Si el seed se deriva **solo** del estado inicial de la partida, dos partidas idénticas (mismo mapa, mismas posiciones, misma IA) son idénticas bit a bit. Eso es malo para bancos de pruebas: el banco de CHUNSA tiene siempre la MISMA trayectoria.

Cómo se resuelve en otros juegos:

- **Seed externo al estado.** Se permite que el jugador (o el sistema de emparejamiento) aporte un seed explícito que se incluye en el estado replicado. El banco de pruebas inyecta seeds distintos por run.
- **"Matchmaking seed".** En el estado inicial se guarda un RNG seed acordado, y se usa para las decisiones estocásticas de la IA. Esto es estándar en Total War y en muchos RTS (sin confirmar para todos los títulos).
- **Hashing del estado en puntos concretos.** Algunos motores dejan que la IA "remezcle" el PRNG tomando un hash del estado actual en eventos concretos (al primer contacto con el enemigo, al llegar a una era). Esto da variedad sin salirse del determinismo.
- **Distintos perfiles de IA** (agresivo, defensivo, económico, rush) seleccionados por el seed inicial. Cada perfil es determinista, pero entre partidas la IA juega "distinto".

### 2.3 Compromisos

- **Determinismo vs. exploración.** Para entrenamiento y balance, querer explorar el espacio de estrategias exige variación. Pero variación + lockstep = variación coordinada, lo que en bancos de auto-juego se traduce en "necesito muchas semillas para cubrir el espacio".
- **Determinismo vs. aleatoriedad de matchmaking.** El seed externo choca con la idea de "partida verdaderamente aleatoria" en multijugador. La solución estándar es que el seed se comunique en el handshake y forme parte del estado replicado, igual que las órdenes.
- **Determinismo vs. debugging.** Que la misma secuencia se reproduzca siempre es oro para encontrar bugs, pero puede dar la falsa sensación de que la IA "siempre funciona" porque solo se ve una rama del árbol de decisión.

---

## 3. Cómo se prueba una IA de RTS (lo que más necesita CHUNSA)

El banco actual (120.000 ticks, IA contra IA, una sola trayectoria por escenario) es una herramienta de **estabilidad y regresión de no-crash**, pero no de calidad. Esta sección recoge las técnicas estándar en la literatura y en la práctica de estudios.

### 3.1 Métricas para decir que una IA "juega bien"

Las métricas que se ven en papers y post-mortems (GDC talks sobre AI, blogs de estudios como *MobyGames* y *AI and Games* — el sitio de Tommy Thompson, investigador de IA de juegos —, papers sobre StarCraft AI como los de DeepMind y los bots de la comunidad BWAPI/AAI):

**Métricas económicas (proxy de "está jugando al juego"):**

- EPM/UPM (economy per minute, units per minute): recoge la IA está aprovechando los recursos.
- Ratio de gasto: recursos_mined / recursos_spent cerca de 1.0 indica que no se acumula ni se desperdicia (en RTS de economía no-acumulativa como SC2 el óptimo es gastarlo todo, en otros como AoE tener stockpile es válido).
- Eficiencia del worker: workers activos / workers ociosos a lo largo del tiempo.
- Tech progression: ¿la IA llega a las eras esperadas en los ticks esperados?

**Métricas militares (proxy de "está aplicando presión"):**

- Censo militar por tipo de unidad a intervalos regulares.
- Ratio kill/loss contra un oponente conocido.
- Daño económico infligido / recibido.
- Capacidad de ataque (fuerza militar estimada) en comparación con la del enemigo: si la IA tiene 3:1 y no ataca, es un bug de comportamiento.

**Métricas de control de mapa:**

- Porcentaje de mapa bajo control o bajo visión.
- Número de bases activas.
- Presión sobre puntos estratégicos.

**Métricas de "juega como un humano experto":**

- Curvas temporales de eventos clave (primer ataque, primer tech-up, primer expansión) comparadas con distribuciones de partidas humanas de referencia.
- Variedad estratégica: distribución de estrategias elegidas bajo semillas distintas (rush, tech, eco, mixto).

**Métricas compuestas (ELO-like):**

- Win-rate head-to-head contra IAs de referencia (A ba te B, B es estable, ver cambio en A).
- TrueSkill o ELO en torneos auto-organizados entre múltiples versiones de la IA.

### 3.2 Detección de bucles degenerados

Una IA puede "ganar" y aun así jugar mal (camp turtle, afk, farming sin avanzar), o perder sin haber realmente intentado. Los bucles degenerados son uno de los síntomas más reportados en post-mortems de IA de RTS. Técnicas de detección:

**Huella de estado ("state fingerprinting").** Un hash de la parte relevante del estado de la IA: composición del ejército, posiciones de unidades, recursos, ratios de producción, decisiones de los últimos N ticks. Si dos huellas separadas por Δt son demasiado parecidas durante demasiado tiempo, es un bucle.

**Distancia de Wasserstein sobre histogramas.** Comparar la distribución de tipos de unidades producidas en una ventana móvil contra la distribución "esperada". Una IA que solo produce una unidad (el bug clásico de "se atasca en una build order") tiene una distribución degenerada.

**Entropía de la cola de producción.** Si la cola de producción tiene baja entropía (siempre las mismas 2–3 unidades en el mismo orden) sobre una ventana larga, es señal de que la IA no se está adaptando al estado.

**Detección de ciclos de acción.** Hash de la última acción significativa de cada unidad; un test de Boyer–Moore sobre esa secuencia detecta repeticiones literales; técnicas más finas (distancia de edición sobre secuencias de acciones) detectan repeticiones "morfológicas".

**Stagnation score.** Métrica compuesta: ratio de tiempo sin atacar AND ratio de censo militar sin crecer AND ratio de investigación sin avanzar. Si los tres están bajos durante una ventana grande, la IA está "muerta en vida".

**Comparación contra línea base humana.** En partidas de referencia humanas, la entropía de las acciones en ventanas de 1000 ticks tiene una distribución conocida. Comparar la entropía de la IA contra esa distribución detecta cuándo la IA es "demasiado uniforme".

### 3.3 Escenarios de prueba: sintéticos, contra referencia, repeticiones humanas

En la práctica, los estudios combinan los tres, no usan solo uno:

**Escenarios sintéticos** ("unit tests de IA"):

- Micro-decisiones aisladas: "dado este estado de combate, la IA debe decidir X". Verificable, determinista, rápido.
- Escenarios de "smoke test" donde hay una sola acción buena clara (huir con la última unidad, construir un edificio defensivo cuando hay una amenaza). Estos son los que más fácilmente descubren regresiones en la función de utilidad de la IA.
- Escenarios diseñados para forzar una rama del árbol de decisión: por ejemplo, "IA con tres bases contra ninguna amenaza, debe expandir y no quedarse en turtle".

**Partidas contra IAs de referencia:**

- A contra B donde B es la versión anterior o un bot de la comunidad. Mide progreso relativo.
- Gauntlet: una IA contra N bots de estilos distintos (agresivo, defensivo, económico, mixto). Detecta especialización excesiva.
- Es el método usado por las competiciones de SSCAIT (Student StarCraft AI Tournament) y CWAI (Code War AI), y por la SC2 AI competition de la comunidad (sin confirmar que la organización oficial las use como benchmark interno).

**Repeticiones de partidas humanas:**

- Se ejecuta la replay inyectando las órdenes humanas (extraídas de la replay) y se deja que la IA tome las decisiones del bando no humano. Esto mide "qué haría mi IA en una partida humana".
- Variante: dos IAs de versiones distintas juegan el mismo bando de una replay humana, se comparan métricas. Esto es regresión de comportamiento pura.
- Es el método más exigente, pero exige una infraestructura de extracción de órdenes y de versionado de IA (ver sección 6).

**Una práctica cada vez más extendida (sin confirmar su adopción industrial): differential testing.** Dos versiones de IA juegan la misma partida desde el mismo estado; se comparan distribuciones de métricas, no valores puntuales. Si las distribuciones difieren más allá de un umbral en alguna métrica, hay regresión.

### 3.4 Cómo evitar que un cambio en la IA rompa en silencio algo que funcionaba

Esto es, en la práctica, el problema más difícil. El código pasa tests, pero la IA "ya no juega como antes". Disciplinas estándar:

- **Tests de caracterización ("characterization tests")**: en cada escenario sintético se registra la salida actual (métricas, decisiones, huellas) como baseline. Cualquier cambio significativo a la baseline es candidato a regresión. Esto es el equivalente IA de los *characterization tests* de Michael Feathers (*Working Effectively with Legacy Code*).
- **Bancos multi-semilla**: en lugar de "una partida larga", un *grid* de semillas. Cubre variabilidad. Coste: N veces más cómputo.
- **Snapshot de digest por tick**: si la nueva versión produce un digest distinto en el tick T respecto a la baseline, se sabe exactamente cuándo divergió. Esto requiere que la baseline esté en almacenamiento accesible (ficheros versionados, no solo logs efímeros).
- **Métricas agregadas con tolerancias explícitas**: definir umbrales por métrica (p. ej., "win-rate contra la IA anterior debe estar en [0.40, 0.60] en el gauntlet"). Si se sale, alerta.
- **Pruebas A/B en simulaciones largas**: durante el desarrollo, la IA nueva y la IA anterior corren partidas largas en paralelo desde el mismo seed. Las métricas se comparan.
- **Versionado obligatorio del algoritmo**: el campo "AI version" en el digest permite que las herramientas de CI detecten mezclas accidentales.

---

## 4. Auto-juego simétrico como herramienta de medida

El banco de CHUNSA es, hoy, IA contra IA. Vale la pena detenerse en qué mide bien y qué mide mal, porque es la herramienta sobre la que se apoya toda la evaluación actual.

### 4.1 Qué mide bien

- **Estabilidad de simulación**: la partida termina sin desincronización, sin crashes, sin bucles infinitos. Esto es lo más valioso del banco actual y debería preservarse.
- **Indicadores económicos absolutos**: una IA que en 120.000 ticks no llega a saturar su economía está claramente rota, con independencia del oponente.
- **Detección de empantanamientos**: si dos IAs juegan 120.000 ticks y nadie gana, hay un problema de "déficit de agresividad" o de stalled economy.
- **Líneas base de cómputo**: cuánto tarda un tick, cuánto tarda una decisión, qué varianza hay. Esto es diagnóstico, no calidad.
- **Reproducibilidad bit-perfect**: dado el mismo seed, mismo resultado. Bueno para regresión de no-crash.

### 4.2 Qué mide mal

- **Equilibrio del juego.** El auto-juego simétrico mide un equilibrio de Nash entre dos instancias del mismo jugador, no el equilibrio entre estrategias humanas. Dos IAs idénticas en un mapa espejado tienden a espejar sus jugadas: si la IA tiene un sesgo defensivo, la otra lo aprovecha, y la métrica final es ruido.
- **Variedad estratégica.** Como ambas IAs usan la misma función de utilidad, sus decisiones se correlacionan fuertemente. No emergen las interacciones cross-estilo (rush vs. turtle, tech vs. rush) que son la sal del género.
- **"Dificultad real".** Una IA juega contra sí misma con su propia información perfecta; no hay niebla de guerra, no hay errores de clic, no hay desconexiones. El test es mucho más fácil que un humano medio.
- **Bugs específicos de interacción asimétrica**: si el balance de unidades solo se rompe cuando hay asimetría (p. ej., el nerf a una unidad en Tier 2 solo afecta si el rival llega antes), el simétrico no lo detecta.
- **Mapas no espejados.** Si el banco solo prueba mapas simétricos (un artefacto común para "controlar" el test), sesga la evaluación hacia el caso más fácil y menos realista.

### 4.3 Artefactos de simetría

- **Mirror strats**: ambas IAs eligen la misma composición por reflejo, la métrica "diversidad estratégica" cae a cero sin que sea un bug.
- **Paradoja del estancamiento**: en auto-juego simétrico, una IA que ha encontrado un "stalemate seguro" lo mantiene indefinidamente, porque la otra no sabe romperlo. No se ve qué pasa cuando un humano rompe la simetría con una micro-decisión.
- **Convergencia rápida a un único Nash**: si la IA tiene tres estrategias y dos son perdedoras contra la tercera en auto-juego, el banco solo verá la ganadora. La diversidad queda invisible.

### 4.4 Cuál es la buena herramienta (complementos, no sustitutos)

Para diagnóstico, una IA idéntica es útil; para calidad de IA, no. La práctica estándar combina:

- **Auto-juego asimétrico**: una IA "A" contra una IA "B" deliberadamente distinta (versión anterior, bot de la comunidad, IA scripted). Aquí sí emergen interacciones.
- **Torneos multi-agente**: N IAs distintas juegan entre sí todas contra todas. Mide ELO/TrueSkill y diversidad. Es el método de SSCAIT, CWAI, y de las competiciones de la comunidad SC2 (sin confirmar adopción industrial cerrada).
- **Repeticiones humanas como oráculo**: ver 3.3.
- **Bots scripted deliberadamente subóptimos como "jugador humano simulado"**: un script que comete errores a una tasa configurable, se desconecta a veces, hace micro mal. Esto mide robustez.
- **FFA (free-for-all)**: 4 IAs en el mismo mapa. Mide jugadas bajo presión multi-direccional, mucho más cercanas al caos real.

---

## 5. Presupuesto de cómputo

El cómputo por tick es un recurso escaso y compartido con la simulación física, el pathfinding, las animaciones y el netcode. La IA suele ser la convidada de piedra.

### 5.1 Magnitudes típicas (con cautela)

- **StarCraft: Brood War (1998)**: la IA oficial tiene un presupuesto muy limitado por frame, del orden de unos pocos ms por bando en máquinas de la época (sin confirmar cifra exacta; los bots de la comunidad SSCAIT reportan timeouts si exceden ~55 ms por frame en competiciones modernas, lo que sugiere que el budget oficial era aún más bajo).
- **StarCraft II**: las IAs competitivas (SunaRTS, Steamhammer, etc.) suelen auto-imponerse límites de 40–50 ms por frame en el compute de decisión (sin confirmar contra documentación oficial de Blizzard, pero es práctica estándar en bots competitivos de BWAPI / SSCAIT / SC2 API). El artículo de DeepMind AlphaStar (DeepMind blog, 2019) menciona que AlphaStar usaba TPUs, pero el cuello de botella en producción era diferente.
- **Age of Empires II (1999)**: presupuesto muy bajo, las IAs populares (el AI del juego original) se conocen por tener patrones rígidos precisamente por restricción de cómputo. La IA "DE" del Definitive Edition tiene un presupuesto mayor pero sigue medida en ms.
- **Supreme Commander / Forged Alliance**: la IA es notoriously pesada por la escala; el FAF community reporta que la IA oficial puede consumir varios segundos de wall-time en los picos de mid-game con cientos de unidades, y se han hecho optimizaciones para bajarlo.
- **Command & Conquer (varios títulos)**: reportados tiempos de IA del orden de 5–20 ms por tick en máquinas modernas para partidas pequeñas, más en las grandes.

**(Sin confirmar):** las cifras exactas por título comercial son difíciles de obtener; los estudios no suelen publicar presupuestos internos. Lo que se confirma por la práctica de la comunidad de bots es que **un orden de magnitud razonable para RTS modernos en hardware de jugador es de 1–50 ms por tick para la IA completa**, con picos tolerables de hasta ~100 ms sin afectar la jugabilidad percibida.

### 5.2 Cómo se reparte el trabajo entre ticks

La disciplina estándar, presente en motores como Spring/Zero-K y reflejada en GDC talks sobre IA de RTS:

- **Planificación jerárquica con refrescos a frecuencias distintas:**
  - Plan estratégico (build order, elección de tech path): cada K ticks, donde K puede ser 200–1000.
  - Plan táctico (composición del ejército, asignación de bases): cada M ticks, M menor que K.
  - Decisiones reactivas (micro, comandos inmediatos): cada tick o cada pocos ticks.
- **Amortización:** las decisiones costosas (búsqueda de camino, evaluación de build orders) se hacen una vez y el resultado se cachea, invalidándolo solo cuando el estado cambia significativamente.
- **Time-slicing:** una decisión larga se corta en N sub-piezas que se ejecutan a lo largo de N ticks. Cada sub-pieza tiene un presupuesto fijo y produce un estado intermedio determinista (lo que CHUNSA necesita para preservar bit-perfectness).
- **Influencia maps:** actualización a menor frecuencia (cada N ticks), consulta cada tick. Es la disciplina clásica de los RTS (origen en *Strategy Game Programming* de Rex Hartson, y en papers como los de Dave Pottinger sobre IA de RTS en Ensemble Studios / Age of Empires).
- **Evaluación por lotes en puntos concretos del juego:** la IA planifica al inicio, en hitos (primer combate, primera base enemiga descubierta, cambio de era), y se limita a ejecutar el plan entre hitos, ajustando reactivamente.
- **Async entre ticks:** parte del cómputo de decisión se hace entre ticks usando el tiempo de "frame libre" sin que afecte al determinismo, porque el resultado se aplica de forma ordenada al siguiente tick.

Lo crítico para CHUNSA: el determinismo exige que **el resultado sea el mismo independientemente de en cuántos ticks se amortice**. Eso requiere que las sub-piezas sean funciones puras con orden de aplicación estable. La forma estándar de lograrlo es: cada decisión larga se divide en (precomputo invariante, evaluación con presupuesto fijo por sub-pieza, commit en orden de índice al final del tick N).

---

## 6. Repeticiones (replays) como prueba

### 6.1 Cómo se usan en la industria

Las repeticiones son **pruebas de regresión canónicas** en RTS comercial, especialmente en las franquicias largas:

- **StarCraft II**: Blizzard mantiene bancos de replays de referencia; un cambio en el motor o en la lógica que altera el resultado de una replay es un regression bug. Las replays de referencia se usan en QA continua. (Sin confirmar el pipeline interno exacto, pero es práctica declarada en GDC talks sobre QA de Blizzard).
- **Age of Empires II (DE)**: la comunidad y Forgotten Empires usan replays como tests de regresión del motor.
- **Warcraft III**: tiene sistema de replays deterministas; los reportes de bugs comunitarios a menudo incluyen replays mínimos.
- **Civilization** (más 4X que RTS, pero la disciplina es similar): las replays se usan para detectar regresiones de IA entre parches (Firaxis ha hablado de esto en GDC talks sobre AI de 4X).
- **Supreme Commander / Forged Alliance**: la comunidad FAF ha desarrollado herramientas de validación de replays para detectar desincronizaciones.

### 6.2 La trampa: qué se guarda en la replay

Aquí está el problema central para CHUNSA. Una replay puede guardar dos cosas muy distintas:

1. **Las órdenes humanas (inputs) y el estado inicial.** La IA se vuelve a ejecutar al reproducir. Esto es "replay lógica": ocupa poco, es portable entre versiones de IA, pero exige que la simulación siga siendo determinista.
2. **Las decisiones de la IA y los eventos resultantes.** Es "replay grabada": permite reproducción exacta incluso si el motor cambia, pero ocupa mucho, y **cualquier cambio en la IA invalida toda la colección**.

La práctica estándar en RTS comercial es **(1)**, no (2). Las replays guardan:

- Versión del motor (y de la IA, como submódulo).
- Seed inicial y mapa.
- Orden de jugadores.
- Stream de órdenes por tick.
- Opcionalmente, hashes de estado por tick para detectar desync.

Cuando se reproduce, la IA re-decide en cada versión. Esto significa que una replay de 2010 no se reproduce idéntica en la versión de 2024 de la IA, **pero sí se reproduce determinista**: dos clientes con la misma versión de IA ven la misma partida, aunque esa partida difiera de la original.

Para CI, esto obliga a distinguir dos bancos:

- **Banco de replays de no-desync**: replays que se ejecutan en CI en la versión actual; lo que se valida es que el digest en cada tick es idéntico entre dos procesos paralelos (no que coincida con la replay histórica).
- **Banco de replays de comportamiento**: replays donde se inyectan las órdenes humanas a la IA actual y se mide su comportamiento sobre ese guion; aquí sí cambia la partida, pero se mide calidad, no paridad.

### 6.3 Cómo gestionar que un cambio legítimo en la IA invalide las repletas

Disciplinas estándar:

- **Versionado obligatorio de IA en la replay.** El header incluye "AI version". Las herramientas de CI distinguen replays de versión N y N+1.
- **Política explícita de compatibilidad.** Algunas IAs definen "compatibilidad de comportamiento": la versión nueva garantiza, sobre las replays de la versión vieja, que las métricas agregadas están dentro de una banda. Si se sale, se documenta como breaking change.
- **Bifurcación de bancos en CI.** Cada release tiene su propio banco de replays de referencia. Las replays de la release N+1 se validan contra la IA N+1; las de N contra N. Mezclar requiere override explícito.
- **Snapshots de métricas en replays.** Además del digest bit-perfect, la herramienta de replay extrae métricas agregadas (win-rate, censo, eventos clave). Esas métricas se almacenan junto a la replay y forman la "línea base" contra la que se compara la nueva versión.
- **Replays "AI-only" y "AI-vs-AI" separados.** Las replays que son IA contra IA se almacenan con un digest por tick y se usan como regression test de simulación. Las que son humano contra IA se almacenan como oráculo de comportamiento. La mezcla de ambos usos es una fuente común de confusión.

---

## Qué implicaría para CHUNSA

> Sin plan de sprints. Solo cómo debería **medirse** el problema.

**Sobre la medición actual (banco de 120.000 ticks IA vs IA, una semilla):**

- Es una herramienta válida para estabilidad, regresión de no-crash, y líneas base de cómputo. No es una herramienta válida para calidad de IA. Esta distinción debería explicitarse en los criterios de aceptación, porque confunde ambos usos.
- Una sola trayectoria por escenario **no detecta regresiones que solo aparecen en otras ramas**. Si una decisión cambia su comportamiento en una rama minoritaria, el banco no la ve. La medida actual es ciega a esto.

**Sobre cómo debería medirse la calidad:**

- Introducir **métricas con umbral explícito**, no solo éxito/fracaso de la partida. EPM, ratio de gasto, censo militar en checkpoints, tiempo al primer combate, entropía de producción. Sin umbrales, "la IA gana" no dice nada sobre "juega bien".
- Introducir **detección de bucles degenerados** vía huella de estado y entropía de producción. Es el síntoma más común de IA "muerta en vida" que pasa desapercibida en auto-juego simétrico porque ambas IAs se empantanan juntas.
- Introducir **auto-juego asimétrico**: una IA nueva contra la IA de la versión anterior, contra bots scripted subóptimos, contra una IA deliberadamente agresiva o defensiva. El simétrico puro no basta para medir equilibrio.

**Sobre la diversidad de medición:**

- Reemplazar "una partida por escenario" por "N semillas por escenario", donde N es al menos del orden de 10–30 para cubrir ramas. Esto multiplica el coste de cómputo del banco por N; hay que decidir si el banco se ejecuta en CI completo o solo en nightly.
- Introducir **partidas IA contra replay humana** (las órdenes humanas se inyectan, la IA decide el resto). Esto mide qué hace la IA en contextos humanos reales y es donde aparecen los bugs más impactantes para el jugador.

**Sobre la regresión de comportamiento:**

- Versionar el algoritmo de IA explícitamente en el digest. Cualquier mezcla accidental entre versiones debe ser detectada en CI, no en producción.
- Caracterizar el comportamiento actual en cada escenario sintético y guardarlo como baseline. Las baselines son tests: cualquier desviación significativa exige revisión.
- Para replays: separar el banco en dos, **banco de desync** (replay vs replay, paridad bit-perfect) y **banco de comportamiento** (replay humana + IA actual, métricas sobre resultado). Mezclar los dos usos es lo que invalida las colecciones de replays ante cualquier cambio.

**Sobre el presupuesto de cómputo:**

- Caracterizar el presupuesto por tick hoy. Si no se mide, no se sabe cuánto hay para mejorar. La métrica debe entrar en CI como cualquier otra regresión.
- Si la IA va a crecer en sofisticación, evaluar **time-slicing determinista**: repartir trabajo entre varios ticks preservando bit-perfectness. Sin esto, la complejidad de la IA está limitada por el peor caso de un solo tick.

**Sobre la disciplina de determinismo (que CHUNSA ya tiene bien):**

- El punto fijo y el desempate por índice son la base correcta; mantenerlo bajo presión de "queremos añadir aprendizaje/heurísticas más expresivas" será el reto. Cada nueva decisión que se introduzca debe pasar por el mismo filtro: ¿es una función pura del estado? ¿su orden es estable? ¿su aleatoriedad viene de un PRNG sembrado?
- El PRNG, si existe, debe tener su estado en el digest, y debe sembrarse desde el estado replicado. Sin esto, en el momento que se añada cualquier decisión estocástica se rompe la propiedad más cara del sistema.