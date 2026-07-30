# SPEC-006 — UI / HUD · Parte I: HUD v1 (Sprint 1.3)

Versión 0.1 (DRAFT ejecutable) · 2026-07-24 · Autor: Arquitecto (Claude)
Jerarquía: INDICE_MAESTRO → SPEC_ARQUITECTURA_BASE v1.1.1 → SPEC-001 → SPEC-004 → **este documento**.
Referencia de diseño: doc 34 (game feel/UI) del corpus de investigación.

## §0 Principio inviolable
La UI es **capa de presentación pura**: lee EXCLUSIVAMENTE el `DemoSnapshot` publicado por
el ring y ENCOLA `RawCommand`s por el mailbox (`pending_player_commands` bajo `input_mutex`).
**Cero lógica de juego, cero lectura/mutación de `GameState` desde el hilo principal, cero
cambios en `addons/chunsa_sim/core/`.** Todo lo que el jugador ve es derivado del snapshot;
todo lo que el jugador hace es un comando que el kernel valida. Esto NO es negociable: es lo
que preserva el determinismo y la futura repetibilidad (replay/red).

## §1 Objetivo del incremento
Convertir la demo técnica en algo que se **controla como un RTS**: cámara movible, minimapa,
panel de selección informativo, grupos de control, y las acciones de partida (mover, construir,
entrenar, investigar, época) accesibles de forma coherente. NO es el pulido final (eso es 1.5),
es la funcionalidad de interfaz que hace jugable el vertical slice.

## §2 Cámara (hoy fija → movible)
La `Camera3D` ortográfica es hoy estática centrada en el mapa. Añadir, en el hilo principal:
- **Pan**: WASD / flechas, y arrastre con botón central; opcional empuje por bordes de pantalla.
  Clamp del centro a la extensión del mapa (256 tiles × 4 px = 1024 px) con margen.
- **Zoom**: rueda del ratón sobre `set_size` (ortográfica), con límites `[ZOOM_MIN, ZOOM_MAX]`
  (p.ej. 300..1200 px de altura visible); el zoom se ancla al cursor (el punto bajo el ratón
  no se desplaza). Sin rotación (ADR-009: profundidad por construcción).
- Ningún estado de cámara toca el kernel ni el snapshot; es puramente de vista.

## §3 Minimapa v1 (sin fog en esta parte)
Un panel rectangular (esquina) que dibuja el mapa completo a escala:
- Terreno: los muros del `cost_grid` (patrón fijo conocido del escenario) en gris.
- Entidades vivas del snapshot: punto por unidad/edificio, **color por owner** (mismo criterio
  que el render 3D: owner 0 azul, owner 1 rojo; ciudadanos amarillo; edificios cuadro).
- Rectángulo del **viewport actual** (derivado de la cámara) superpuesto.
- **Clic** en el minimapa → recentra la cámara en ese punto del mundo. Arrastre = pan continuo.
- **Sin fog of war en v1** (mostrar todo lo del snapshot): el fog es deuda para 1.4/1.5, donde
  se expondrá la rejilla `explored`/`visible` de visión al snapshot. Documentarlo así.

## §4 Panel de selección
Al haber selección (ya existe la maquinaria `is_selected[]`), un panel muestra:
- **Selección única**: tipo (unidad de combate por `unit_class` / ciudadano / edificio por
  `building_id`→nombre del catálogo), **barra de vida** (requiere exponer `hp`/`max_hp` al
  snapshot — ver §7), owner, y si es edificio en construcción, el progreso; si produce, la cola.
- **Selección múltiple**: recuento por tipo (p.ej. "12 caballería, 3 ciudadanos") con iconos
  o texto; barra de vida agregada opcional.
- **Barras de vida sobre el mundo**: barra fina sobre cada entidad viva seleccionada (y
  opcionalmente sobre todas las dañadas), derivada de `hp/max_hp` del snapshot.

## §5 Grupos de control y resolución del conflicto de teclas (DECISIÓN DE DISEÑO)
El HUD del Sprint 1.2 usó `1..8` para TRAIN/RESEARCH. Eso **choca** con los grupos de control
estándar. Resolución canónica de este SPEC:
- **Los números `1..9` se reservan para grupos de control** (estándar RTS): `Ctrl+N` asigna la
  selección actual al grupo N; `N` selecciona el grupo N; doble-`N` además centra la cámara en él.
  Los grupos son estado de PRESENTACIÓN (conjuntos de índices de slot), no del kernel; se
  invalidan por slot muerto (comprobar `alive`+`generation` del snapshot al recuperar).
- **La producción/investigación se MUEVE del teclado numérico a botones clicables** del panel
  de acción contextual (patrón AoE/SC: seleccionas el cuartel → el panel muestra un botón por
  unidad de `trains[]` / tech de `researches[]` → clic encola el comando). Hotkeys de LETRA
  opcionales (no numéricas). Esto libera los números y es más legible. `EPOCH_UP` pasa a un
  botón dedicado del HUD (o `E`, que no colisiona). `SET_RALLY` sigue en `R`+clic.
- El resultado del comando se sigue leyendo del mailbox de receipts (verde/rojo), ya existe.

## §6 Acciones y feedback
- Panel de acción contextual según la selección: unidades → (mover ya es clic-derecho, rally
  n/a); ciudadanos → construir (ghost, ya existe); edificio de producción → botones de
  entrenar/investigar; comando de época global.
- **Marcadores de orden**: al emitir MOVE_TO / SET_RALLY, un marcador breve en el destino
  (puramente visual, se desvanece). El rally de un edificio se dibuja como línea/bandera
  persistente mientras esté fijado (`rally_set`/`rally_x/y` ya en el snapshot desde 1.2).
- Cursor/estado de modo visible (construir / rally / normal).

## §7 Exposición de datos nueva (adaptador, NO kernel)
Para §4 el snapshot debe ganar, por slot: `hp[i]`, `max_hp[i]` (barras de vida). Copiados en
`sim_loop` bajo el ring, igual que el resto. Nada más del kernel hace falta para v1 (el minimapa
sin fog usa posiciones ya presentes; el fog llega cuando se exponga la visión, fuera de alcance).

## §8 Gates del sprint (DoD)
1. La cámara se mueve (pan+zoom) con clamp; el minimapa refleja entidades y viewport y permite
   saltar/pan por clic.
2. Selección única y múltiple muestran información correcta; barras de vida reflejan `hp/max_hp`.
3. Grupos de control `Ctrl+N`/`N` funcionan y sobreviven a muertes (no seleccionan slots
   reciclados por el chequeo de generación).
4. Entrenar/investigar/época/construir/mover/rally siguen funcionando vía la nueva disposición
   (botones + letras), todos como comandos validados por el kernel.
5. **Determinismo intacto**: el core no se toca (`git diff` sobre `core/` vacío); demo headless
   exit 0, sin `CHUNSA ERROR`, `buildings=4`; ctest del kernel sigue 16/16 (no debe cambiar).
6. Build del adaptador `-Werror` limpio.

## §9 Reparto
- **Arquitecto**: este contrato, revisión, integración.
- **Codex (GPT-5.6 Luna Max)**: TODA la implementación (es frontend Godot puro). Arranque
  operativo en `docs/ARRANQUE_GPT_LUNA_SPRINT_1.3.md`.
- Sin trabajo de kernel (Sonnet) ni de datos (MiniMax) en esta parte.

---

# Parte II — Fog de presentación (Sprint 1.5A)

## §10 Objetivo y frontera de autoridad
Convertir la visión determinista ya calculada por el kernel en un bucle jugable de
**explorar → detectar → combatir**, sin cambiar la simulación. El fog es presentación pura:
lee la visión de owner 0 desde `DemoSnapshot`; no consulta ni muta `GameState` desde el hilo
principal y no emite comandos.

Este incremento no modifica `addons/chunsa_sim/core/`, formatos de save/replay, checksum,
versiones de estado ni algoritmo de IA. La IA v1 puede seguir leyendo el estado completo:
su omnisciencia está permitida por SPEC-005 §10 y no debe confundirse con la información
mostrada al jugador.

## §11 Contrato del snapshot
`DemoSnapshot` expone, para owner 0, copias por valor de los bitsets deterministas:

```cpp
uint64_t visible[chunsa::VIS_WORDS];
uint64_t explored[chunsa::VIS_WORDS];
```

`sim_loop` los copia desde `gs->vision.visible[0]` y `gs->vision.explored[0]` antes de publicar
el snapshot en el ring. La presentación solo consume esas copias estables. No se publica
visión de otros owners ni se vuelve a calcular LoS en Godot.

## §12 Estados visuales y actualización
El mapa se representa mediante bloques de presentación de **8×8 tiles** (32×32 bloques para
el mapa actual de 256×256):

1. **NO EXPLORADO**: ninguna celda del bloque está en `explored`; velo opaco/oscuro.
2. **EXPLORADO**: al menos una celda está en `explored`, pero ninguna en `visible`; velo
   semitransparente/atenuado.
3. **VISIBLE**: al menos una celda está en `visible`; sin velo o con alpha cero.

Los bloques se reclasifican únicamente al aceptar un snapshot nuevo (`tick` distinto), nunca
en cada frame. Cámara, zoom e interpolación pueden redibujar el resultado ya clasificado, pero
no recorrer de nuevo los bitsets.

La granularidad 8×8 es solo visual. La decisión de mostrar una entidad usa celdas exactas:
- Una entidad propia (`owner==0`) está siempre visible.
- Una unidad enemiga se muestra solo si la celda de su posición actual está en `visible`.
- Un edificio enemigo se muestra si al menos una celda de su footprint está en `visible`.
- `explored` por sí solo nunca revela una entidad enemiga ni su última posición.

## §13 Política única de ocultación
La misma función de visibilidad de entidad gobierna todos los consumidores de presentación.
Un enemigo fuera de visión queda oculto simultáneamente en:
- render del mundo;
- minimapa;
- barras de vida, nombres, colas y demás overlays;
- hit-testing y selección.

Si una entidad enemiga seleccionada deja de ser visible, la selección se limpia en el siguiente
snapshot. Las entidades propias permanecen visibles y seleccionables aunque un bitset llegue
transitoriamente sin su celda, para evitar que la presentación contradiga la autoridad del
jugador sobre sus propias unidades.

No se permite que color, marcador, panel, sonido diagnóstico o contador revele presencia,
posición, tipo, vida o actividad de un enemigo oculto.

## §14 Criterios de aceptación Given/When/Then

### AC1 — Inicio oculto

- **Given** el skirmish inicia con las bases separadas
- **When** llega el primer snapshot con visión de owner 0
- **Then** toda entidad enemiga fuera de `visible` está ausente del mundo, minimapa y overlays
- **And** el terreno nunca explorado se muestra en estado NO EXPLORADO.

### AC2 — Primer contacto

- **Given** una unidad propia se aproxima a una entidad enemiga
- **When** la celda de la unidad enemiga, o una celda del footprint del edificio, entra en
  `visible`
- **Then** la entidad aparece en mundo y minimapa en el siguiente snapshot publicado
- **And** la latencia queda acotada por la fase de visión existente, máximo cuatro ticks.

### AC3 — Retorno al fog

- **Given** una zona ya fue explorada y contiene enemigos
- **When** ninguna celda relevante de esos enemigos permanece en `visible`
- **Then** el terreno pasa a EXPLORADO
- **And** los enemigos desaparecen del mundo, minimapa, overlays y selección.

### AC4 — Entidades propias

- **Given** una entidad de owner 0 está viva
- **When** se presenta cualquier snapshot
- **Then** permanece visible y seleccionable con independencia defensiva del bitset.

### AC5 — Granularidad sin fuga

- **Given** un bloque 8×8 contiene alguna celda visible y una entidad enemiga en otra celda no
  visible del mismo bloque
- **When** se clasifica y dibuja el fog
- **Then** el bloque puede verse como VISIBLE por la aproximación gráfica
- **But** la entidad enemiga permanece oculta porque su prueba usa celdas exactas.

### AC6 — Simulación inalterada

- **Given** la misma semilla y ninguna orden humana
- **When** se compara el skirmish antes y después del incremento
- **Then** conserva el mismo ganador y tick final
- **And** `git diff -- addons/chunsa_sim/core/` está vacío
- **And** no cambian save, replay, checksum ni `AI_ALGO_VERSION`.

### AC7 — Regresión y evidencia

- **Given** el incremento completo
- **When** se ejecutan build, ctest y demo headless
- **Then** el adaptador compila con `-Werror`, todos los tests y golden permanecen verdes,
  no aparece `CHUNSA ERROR`, la IA actúa y la partida alcanza `game_over`
- **And** se conserva evidencia visual de NO EXPLORADO, EXPLORADO y VISIBLE.

## §15 Exclusiones de Sprint 1.5A
- Fog autoritativo para la IA o cambios en sus decisiones.
- Última posición conocida, siluetas persistentes o memoria de edificios enemigos.
- Radios de visión por tipo de unidad/edificio, altura, terreno o bloqueo de LoS.
- Nuevos comandos (`ATTACK`, `ATTACK_MOVE` u otros) y cambios de combate/balance.
- Assets finales, audio, partículas, animaciones o rediseño general del HUD.
- Cambios de escenario, pacing, duración de partida, menú, reinicio o guardado desde UI.
- Modificaciones de core, datos, schemas, save, replay, checksum o red.

## §16 DoD de Parte II
1. Los bitsets de owner 0 viajan exclusivamente por el snapshot.
2. Los tres estados de fog se distinguen en mundo y minimapa mediante bloques 8×8.
3. Propios siempre visibles; enemigos ocultos fuera de visión en todos los consumidores.
4. La prueba exacta por entidad evita fugas causadas por la granularidad visual.
5. El resultado determinista del skirmish no cambia y el core permanece intacto.
6. Build `-Werror`, suite completa, golden y headless verdes, con evidencia de los tres estados.

---

# PARTE III — Recursos en el HUD (Sprint 1.8B)

**Hueco H6 de `CONCORDANCIA_SPEC-007.md`**: esta spec no tenía ninguna sección
sobre recursos. Con `RESOURCE_COUNT = 32` deja de ser aceptable.

## §12 El problema

`[V] [Anno 1800]` necesitó **tres regiones** de mapa para sostener 30+ recursos
sin ahogar la interfaz. Nosotros llegaremos a **26 activos en la edad 15**
(SPEC-007 §9.4) en una sola región.

Veintiséis contadores en fila son ilegibles. El jugador no lee números: lee
**si le falta algo**.

## §13 Agrupación por familias

Seis grupos, no veintiséis contadores:

| Familia | Contiene |
|---|---|
| Subsistencia | comida |
| Construcción | madera, piedra, arcilla |
| Metales base | cobre, estaño, oro, mena de hierro, plomo |
| Metalurgia | bronce, hierro forjado, carbón vegetal, coque, acero, aluminio |
| Química | salitre, azufre, pólvora, derivados del petróleo |
| Energía | carbón, petróleo, uranio · **electricidad (flujo, no stock)** |

`data/resources/` declara la familia de cada recurso (SPEC-007 §9.2). **El HUD
no cablea la lista**: la lee del catálogo, igual que hace con unidades y
edificios.

## §14 Reglas de presentación

1. **Solo se muestran los recursos de la edad actual del jugador.** En la edad 3
   no aparece el uranio. La interfaz crece con la partida.
2. **Una familia colapsada muestra su recurso más escaso**, no la suma: sumar
   comida y piedra no significa nada.
3. **Expandible**: al abrir una familia se ven sus recursos individuales.
4. **La electricidad no lleva contador acumulado** sino **producción frente a
   consumo**, porque no es un stock (SPEC-007 §8.1). En déficit, indicador de
   alerta: es la señal de que las fábricas están paradas.
5. **El upkeep se muestra junto a la comida**: consumo por tick frente a
   producción. Es el número que decide si el ejército aguanta.

## §15 Feedback de carencia

Cuando un comando se rechaza por stock, el mensaje debe nombrar **el recurso
concreto y la cantidad**, como ya hace el adaptador desde el Sprint 1.7A:

```text
Faltan 200 B          ← formato actual, con nombre técnico
Faltan 200 de piedra  ← formato objetivo tras 1.8B
```

`[V]` Verificado en la sesión del Director del 2026-07-28: el mensaje
descriptivo fue lo que permitió entender por qué no se podía subir de época.
**Es la funcionalidad de HUD con mejor relación valor/coste de todo el
proyecto** y debe conservarse al ampliar a 32 recursos.

## §16 Criterios de aceptación

1. El HUD lee familias del catálogo; añadir un recurso **por datos** aparece sin
   tocar código.
2. En la edad 3 no se muestran recursos de edades posteriores.
3. Una familia colapsada muestra su recurso **más escaso**.
4. La electricidad se muestra como producción/consumo, **nunca** como acumulado.
5. Un rechazo por stock nombra el recurso y la cantidad que falta.
6. Con 26 recursos activos, el HUD **no desborda** la pantalla a 1920×1080.

---

# PARTE IV — El HUD expone lo que el kernel ya puede (Sprint 1.8E)

**Origen:** sesión de juego del Director (2026-07-30). Tres carencias reportadas
que, al investigarlas, resultaron ser **todas de interfaz**: el kernel ya
soporta lo que falta.

## §17 El diagnóstico

| Reportado | Estado real del kernel | Qué falta |
|---|---|---|
| «No se muestra el coste hasta realizar la acción» | El catálogo tiene todos los costes | **Mostrarlos** antes de comprometerse |
| «Los edificios aparecen por arte de magia» | `construction_system` **ya exige proximidad**: comprueba distancia y camina a la obra | **Asignar constructores** al colocar, y decirlo en pantalla |
| «No están implementados diversos edificios» | **4 edificios son `constructible: true`** con 500–600 ticks de obra | El adaptador ofrece **solo 1** |

**La conclusión importante: el kernel es más capaz de lo que la interfaz
expone.** Ninguno de los tres necesita tocar la simulación.

## §18 Costes visibles antes de comprometerse

Hoy el jugador descubre el precio **solo cuando el comando se rechaza**. Es
información que el catálogo ya tiene.

1. Todo elemento accionable —edificio a colocar, tecnología a investigar,
   subida de época, unidad a entrenar— muestra su **coste completo** antes de
   seleccionarlo.
2. El coste se muestra **por recurso y con nombre real**: «100 de madera,
   50 de piedra», no «100/50/0».
3. **Asequibilidad visible**: lo que no puedes pagar se distingue a simple
   vista, y **cuánto te falta**. El mensaje de rechazo ya lo hace bien
   (`Faltan 20 de Madera`); llevarlo al momento **anterior** a la decisión.
4. Los requisitos no económicos también: «Requiere época 4» debe verse **antes**
   de intentarlo, no después.

**Criterio de diseño**: el jugador no debe aprender el juego por rechazos. Un
rechazo es una red de seguridad, no un canal de información.

## §19 Construcción: quién la hace y desde dónde

El kernel es realista; la interfaz no lo cuenta.

1. Al colocar un edificio, el adaptador **asigna constructores**: los
   ciudadanos seleccionados, o si no hay selección, los ociosos más cercanos.
2. Un sitio de obra **sin constructor asignado** se distingue visualmente y
   dice **por qué no avanza**. Una obra parada sin explicación es el origen de
   la sensación de «magia» al revés: aparece y no pasa nada.
3. Se muestra **progreso** de obra y **cuántos constructores** trabajan en ella.
4. Los constructores **caminan** hasta el sitio — ya lo hacen; hay que
   **verlo**.

## §20 Todos los edificios construibles

El adaptador debe ofrecer **todos** los `constructible: true` del catálogo que
pasen dos filtros, no uno fijo:

1. **Civilización**: `civ_id` == la del jugador, si tiene una asignada. Hoy la
   demo **no llama a `gs_set_player_civ`**, así que `player_civ` es
   `INVALID_CIV_ID` y el filtro no descarta nada — el propio adaptador ya trata
   ese caso en `append_civilization_detail`.
2. **Época**: `epoch_window` debe contener la época actual del jugador.

**Cuidado con el conteo, que yo mismo me equivoqué aquí**: los cuatro
construibles **no coexisten nunca**. Verificado en los datos:

| Edificio | Civ | `epoch_window` | Obra |
|---|---|---|---|
| `egipto:chariotry_stable` | egipto | **[3, 4]** | 600 |
| `egipto:shena_granary` | egipto | **[3, 4]** | 500 |
| `rome:castra_barracks` | rome | **[5, 5]** | 600 |
| `rome:horreum` | rome | **[5, 5]** | 500 |

En la época 3–4 de la demo salen **dos**; los romanos aparecen al llegar a la
**época 5**. Que la lista crezca al subir de época no es un fallo: es la
Parte III («la interfaz crece con la partida») aplicada a los edificios.

La lista sale del catálogo; **añadir un edificio por datos debe aparecer en la
UI sin tocar código**, igual que los recursos en la Parte III.

## §21 Criterios de aceptación

1. El coste de un edificio se ve **antes** de colocarlo, por recurso y con
   nombre real.
2. Lo mismo para tecnología, entrenamiento y subida de época.
3. Un elemento no asequible se distingue y muestra **cuánto falta**.
4. Un requisito de época se ve **antes** de intentar la acción.
5. Colocar un edificio **asigna constructores** automáticamente.
6. Un sitio sin constructores lo indica en pantalla.
7. Se ve el progreso de obra y el número de constructores.
8. La lista de edificios sale del **catálogo**, filtrada por civilización y
   `epoch_window`: en la época 3–4 aparecen los **dos** egipcios, y al llegar a
   la **época 5** aparecen los dos romanos. Una lista fija de uno, o de cuatro,
   es un fallo por igual.
9. Marcar un quinto edificio como `constructible` en datos **aparece en la UI
   sin recompilar**.
10. Verificado en **captura de pantalla real**, no en headless: el HUD de
    costes no desborda a 1920×1080.

---

# Parte V — Panel de comandos al estilo AoE2 (Sprint 1.8H)

**Origen**: corrección del Director tras ver el 1.8E — *«para ver los costos no
quiero que sea como está, debe ser parecido a AoE2, tanto para construcciones,
para investigaciones y entrenar unidades como funciona ahí»*.

## §22 Qué falla en lo que hay

El 1.8E resolvió el problema de fondo —el coste se ve antes de actuar— pero con
la forma equivocada.

**Corrección de una afirmación mía errónea**: escribí que entrenar e investigar
«no muestran coste ninguno». **Es falso**, y lo comprobé después en el código:
`draw_selection_panel` sí los muestra, en botones propios, con
`cost_summary(costs).left(29)`. El problema real no es la ausencia sino la
**incoherencia**: dos formas distintas para la misma pregunta, y ambas
truncadas.

| | Hoy | AoE2 |
|---|---|---|
| Forma | Lista vertical de texto (edificios) **y** botones de otro estilo (entrenar/investigar) | Una sola rejilla de botones con icono |
| Cuándo se ve el coste | **Siempre**, ocupando pantalla | Al **pasar el ratón** |
| Coste | Frase: «30 de Piedra · 60 de Madera» | **Icono + número**, uno por recurso |
| Truncado | `.left(29)`, `.left(30)` — corta a media palabra | No aplica: el tooltip crece |

### §22.1 Un defecto real encontrado al revisar

`draw_selection_panel` decide si una tecnología está disponible con
**igualdad exacta** de época:

```cpp
epoch_ok = snap_curr.player_epoch == catalog.techs[id].epoch;   // MAL
```

El kernel usa `tdef.epoch > player_epoch ⇒ ILLEGAL_STATE` (`step.hpp:602`), es
decir **`<=`**. Con los datos actuales las tecnologías egipcias son de época 4:
al llegar a la época 5 la interfaz diría «Requiere época 4» sobre una
tecnología que el kernel investigaría sin objeción. Es exactamente el fallo que
`button_state` existe para impedir —la UI y el kernel discrepando sobre el
motivo— y se corrige en este sprint.

Que el coste esté permanentemente en pantalla es además lo que agrava el
solapamiento del §1.8G: menos texto fijo, mapa más legible. Las dos cosas se
arreglan a la vez.

## §23 Lo verificado y lo que decido yo

Investigación en `docs/research/UI_COSTES_AOE2.md`. **Las fuentes de AoE2
devolvieron 402/403 casi en bloque**, así que muchos detalles quedaron
**NO VERIFICADO** y el investigador lo declaró en vez de inventar citas.

**Verificado o firme**: el coste vive en un **tooltip al pasar el ratón**, no en
el botón · se representa como **icono de recurso + número entero** · junto al
coste aparecen **tiempo** y **tecla rápida** · lo impagable se **atenúa** y el
coste se pone en **rojo**, pero el botón **no se oculta ni se desactiva** · el
panel va **abajo a la derecha** · un edificio en obra muestra **barra de
progreso sobre el edificio** y **número de aldeanos** en el panel de selección.

**NO verificado, así que lo decido yo y lo digo**: rejilla de **5 columnas × 3
filas** (es el canon de Age of Kings, y con nuestro contenido sobra) · teclas
`Q W E R T / A S D F G / Z X C V B` por posición · tamaño de botón 56 px.

## §24 El panel

1. **Uno solo**, abajo a la derecha, encima del minimapa. Rejilla 5×3.
2. **Su contenido depende de la selección**, como en AoE2:
   - Aldeanos seleccionados → **edificios construibles**.
   - Edificio productor seleccionado → **unidades que entrena** y
     **tecnologías que investiga**, en ese orden.
   - Centro seleccionado → además **subir de época**.
   - Nada seleccionado → panel vacío, no un panel de construcción colgado.
3. **Botón**: icono, tecla rápida en una esquina, y el número en cola si lo hay.
4. **Filtrado por civilización y `epoch_window`**, como en la Parte IV. Lo
   bloqueado por época **se ve atenuado**, no se oculta: enseñar lo que viene
   es parte de enseñar el juego.

## §25 El tooltip — es la pieza central

Al pasar el ratón, junto al puntero:

```
Establo de carros                        [Q]
─────────────────────────────────────────────
Coste    ● 60 Madera   ● 30 Piedra
Obra     600 t
─────────────────────────────────────────────
Faltan 60 Madera                      (rojo)
```

1. **Un renglón por recurso**, icono a la izquierda y número entero. **Nunca
   una frase corrida**: es lo que obligaba a truncar a media palabra.
2. **El recurso que falta va en rojo**, solo ese, no el bloque entero. Es más
   preciso que AoE2 y conserva el «cuánto falta» que ya funcionaba bien.
3. **Tiempo** siempre, en ticks.
4. **El motivo de bloqueo no económico** en la última línea: «Requiere época 5»,
   «No pertenece a tu civilización». Sigue valiendo el criterio §18: *el
   jugador no debe aprender el juego por rechazos*.
5. El tooltip **no se sale de la pantalla**: si no cabe a la derecha del
   puntero, se dibuja a la izquierda. Sin truncar nunca.

## §26 Iconos sin arte

No tenemos arte, y esto **no debe bloquear el sprint**.

- Un icono es un **disco de color de la familia** (`resource_family_color`, ya
  existe) con la **abreviatura de dos letras** del recurso encima. Distingue
  cobre de estaño, que comparten familia y color.
- **Toda la pintura de iconos pasa por una única función** `draw_resource_icon`.
  Sustituir esto por arte real debe ser cambiar **esa función y nada más**.

## §27 Obra en curso

- **Barra de progreso sobre el edificio** en el mundo, no una línea de texto.
  Sustituye a la línea actual y **quita ruido del centro del mapa**.
- Barra **roja y parada** si no hay constructores; el texto «SIN CONSTRUCTOR»
  pasa al panel de selección.
- El panel de selección muestra **aldeanos trabajando** y **tiempo restante**.

## §28 Qué es probable y qué no

La política pura va a `command_panel_view.hpp`, con pruebas, siguiendo
`fog_view` / `outcome_view` / `affordability_view`:

- `panel_items_for(selección, civ, época, catálogo)` → qué botones y en qué
  orden.
- `button_state(asequibilidad, bloqueo)` → `NORMAL` · `ATENUADO_POR_COSTE` ·
  `ATENUADO_POR_EPOCA` · `NO_DISPONIBLE`.

Ambas son funciones puras de sus entradas. **Lo que no se puede probar así es
solo el dibujado**, y ése se verifica con captura mirada.

## §29 Criterios de aceptación

1. El coste **ya no está permanentemente en pantalla**: aparece al pasar el
   ratón.
2. El tooltip muestra **un renglón por recurso** con icono y número.
3. **Construir, entrenar e investigar** usan el mismo panel y el mismo tooltip.
4. Entrenar e investigar **muestran coste**, cosa que hoy no ocurre.
5. Lo impagable se **atenúa** y marca en rojo **solo el recurso que falta**.
6. Lo bloqueado por época **se ve atenuado**, con el motivo en el tooltip.
7. El contenido del panel **cambia con la selección**.
8. El tooltip **nunca** se sale de la pantalla ni se trunca a media palabra.
9. Un edificio en obra muestra **barra de progreso**, roja si nadie trabaja.
10. Añadir un edificio, unidad o tecnología **por datos** aparece en el panel
    **sin recompilar**.
11. `panel_items_for` y `button_state` tienen pruebas, en **fase roja primero**.
12. Verificado en **captura real mirada** a 1920×1080, sin mojibake.
