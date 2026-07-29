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
