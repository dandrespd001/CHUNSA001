# Protocolo del panel multimodelo

Herramienta: `scripts_ci/panel.sh` · de-duplicador: `scripts_ci/panel_dedup.py`
Timer semanal: `systemctl --user status chunsa-panel-probe.timer`

## Por qué existe

En la auditoría del Sprint 1.6B K2, Claude Opus 4.7 y Qwen dieron el incremento
por aprobado. GPT-5.6 SOL, con ASan, encontró **un desbordamiento de heap**, y
DeepSeek planteó el caso de carga parcial que resultó ser un **segundo
bloqueante**. La votación mayoritaria habría producido un falso «aprobado».

De ahí la regla que gobierna todo esto: **un solo revisor no basta, y el
consenso no es evidencia**. El valor del panel está en el hallazgo único, no en
que la mayoría coincida.

## Los tres modos

```sh
./scripts_ci/panel.sh probe                 # qué rutas viven y cuáles navegan
./scripts_ci/panel.sh review <fichero>      # revisión adversarial
./scripts_ci/panel.sh review main..HEAD     # ídem sobre un diff
./scripts_ci/panel.sh research <pregunta>   # investigación con fuentes
```

## Cuándo usarlo

**Antes de cada merge de kernel — obligatorio.** Es el uso de mayor valor y el
que ya ha pagado su coste dos veces. `panel.sh review main..HEAD` antes de
integrar cualquier rama que toque `addons/chunsa_sim/core/`.

**Al cerrar una spec.** El panel encontró cinco errores en SPEC-007 que yo no
vi, incluido uno factual (bauxita usada en una receta pero nunca contada) y una
contradicción interna del documento.

**Semanalmente, automático.** El timer sondea rutas y acumula
`docs/research/rutas_historico.log`. No genera ruido: solo mantiene el mapa de
qué está vivo, que cambia mucho.

**Investigación, bajo demanda.** Con una pregunta concreta en un fichero. Sin
pregunta concreta el resultado es paja.

## Lecciones operativas (2026-07-28)

**Solo las rutas Gemini navegan.** `gemini-web/*` y `gweb/*` sí; `qwen-web` y
`zenmux-free/deepseek` **no**. Pedir investigación a un modelo sin web produce
invención con tono de autoridad — el peor resultado posible.

**Los modelos sin web sirven como críticos.** No para traer datos, sí para
razonar sobre diseño. Qwen produjo la mejor crítica del panel de SPEC-007 sin
tocar internet.

**El proxy duplica fragmentos de streaming.** Las respuestas Gemini llegan con
el texto repetido acumulativamente. `panel_dedup.py` lo arregla; sin él son
ilegibles y parecen incoherencia del modelo.

**MiniMax necesita los flags antes del prompt**:
`claude-minimax --allowedTools "WebFetch" -p "..."`. Al revés, el CLI se come
el prompt. Y `WebSearch` **no funciona** contra su endpoint aunque aparezca
listada: solo `WebFetch`, con URLs concretas.

**No preguntar a un modelo si tiene una capacidad: probarla.** Mi primer
diagnóstico sobre MiniMax fue falso porque me fié de su autodeclaración, y
además las herramientas estaban bloqueadas por permisos. Dos errores
independientes en la misma conclusión.

**Fuentes que bloquean:** el wiki Fandom de AoE2 devuelve HTTP 402, y Wildfire
Games y Widelands usan desafío Anubis. Wikipedia y GitHub sí responden.

## Cómo leer los resultados

Cada línea viene marcada `[V]` verificado con fuente citada, `[I]` inferencia o
`[?]` sin confirmar. **Una línea `[V]` sin fuente es un fallo del encargo y no
se acepta.**

Al fusionar hallazgos: primero los **únicos** (lo que solo un modelo vio),
después los **contradictorios** entre modelos —que son información, no ruido—,
y por último el consenso, que es lo menos informativo.

**Verificar antes de actuar.** Ningún hallazgo del panel entra en una spec sin
comprobarlo contra el código o una sonda reproducible. Cuatro entregables
delegados consecutivos traían algo que su propio informe no reflejaba.

## Cola de investigación

Preguntas pendientes, por orden. Al resolver una, se borra y se anota dónde
quedó la respuesta.

1. **Tasas de recolección de AoE2 por recurso y por aldeano.** El Fandom da 402;
   buscar otra fuente accesible. Necesario para calibrar §9 de SPEC-007.
2. **Un `init.lua` real de edificio de producción de Widelands.** Para el diseño
   de recetas del Sprint 1.9.
3. **OpenRA: patrón concreto de detección de desincronización.** El wiki de
   Architecture da 404. Interesa para el gate de determinismo.
4. **Modelos de upkeep comparados** (Total War, Anno, Supreme Commander): qué
   tasas usan y cómo evitan la espiral de la muerte. Para §10.
5. **Cómo presentan 20+ recursos en HUD** Anno 1800 y Songs of Syx. Para el
   agrupado por familias del Sprint 1.8B.
