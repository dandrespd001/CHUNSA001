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
