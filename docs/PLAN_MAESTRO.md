# PLAN MAESTRO — CHUNSA: Ascenso de las Civilizaciones

**Versión:** 1.0 · **Fecha:** 2026-07-22 · **Autor:** Arquitecto Jefe · **Aprobación:** Director (2026-07-22)
**Estado:** VIGENTE — documento vivo, se actualiza al cierre de cada sprint (§7).

---

## §0 Propósito y protocolo

Este documento es la **fuente de verdad del roadmap** desde el estado actual hasta la release 1.0. Ordena en el tiempo lo que las specs definen; no redefine nada técnico.

**Jerarquía documental** (de mayor a menor autoridad):
1. `Investigación/INDICE_MAESTRO.md` — mapa y canonicidad del corpus (fuera del repo).
2. `SPEC_ARQUITECTURA_BASE.md` v1.1.1 — constitución técnica (fuera del repo).
3. `SPEC-001_NUCLEO_SIMULACION_DETERMINISTA.md` v1.1 y SPECs futuras (§3).
4. **Este plan** — secuencia, sprints, DoD y reparto de ejecución.

**Protocolo de actualización**: al cierre de cada sprint se actualizan §1 (estado), §4 (sprint cerrado + re-plan del siguiente) y, si procede, §6 (riesgos). Las fases lejanas se re-planifican al cerrar la fase anterior — la granularidad decrece deliberadamente con la distancia (Fase 1 = detalle sprint a sprint; Fases 3–4 = bosquejo).

**Regla de coste de modelos** (decisión del Director, 2026-07-22): el Arquitecto (Fable/Opus, el más caro) se reserva para contratos, revisión, integración y decisiones de determinismo; TODO el volumen de implementación se delega según §5. Este plan existe precisamente para que cada sprint arranque con contratos ya pensados y la ejecución sea delegable.

---

## §1 Estado actual (2026-07-22)

### Hecho — Fase 0 casi completa

| Sprint | Estado | Contenido |
|---|---|---|
| 0.1A | ✅ COMPLETO | Toolchain pineado, kernel base (Fixed64\<16\>/Wide128, RNG por contador, GameState SoA, Commands, Step(), MovementSystemV1, spatial hash, checksum), golden 1074, gates G1/G2, CI 5 lanes (incl. windows-msvc). |
| 0.1B | ✅ COMPLETO | Saves/envelope + fuzzing, replays, gates G3/G4/G5, ring de snapshots, `chunsa_data_compiler` + `unit.schema.json`. |
| 0.2 | ✅ COMPLETO | Godot 4.7.1 + GDExtension (`ChunsaSimNode`, sim 20 Hz en hilo + ring), SPIKE-RENDER-0 → **ADR-009** (3D ortográfico + depth buffer), FlowField v1 integrado al movimiento, Visión/LoS v1, render de producción con interpolación 60 fps. |
| 0.3 | 🔶 CASI | Combate RPS v1 · moral/pánico v1 · **aggro/persecución v1** (el combate llega a resolución) · economía mínima v1 (A/B/Me, ciudadanos SEEK/HARVEST/RETURN, 6 depósitos, dropoff) · demo showcase integrada · **selección y órdenes del jugador con clic** (hito de interactividad). Save v6, ctest 10/10, G1–G5 verdes. |

### Pendiente del Sprint 0.3 (cierre en §4/Fase 0)

- Verificación factual de las fichas históricas 36a–d (`game_data/research/`, estado `draft-unverified`).
- Replay con `effective_tick` (robustez diferida desde 0.1B/0.2).
- PERF-0 físico (bloqueado por hardware de referencia UHD 620; ADR-011 sigue en TARGET).
- Reporte de cierre `docs/REPORTE_SPRINT_0.3.md`.

---

## §2 Alcance de 1.0 y enmienda ADR-021

### Alcance vinculante de v1.0 (ADR-012, sin cambios)

- **4–6 civilizaciones**: slice decidido por el Director = Egipto dinástico · Roma · Imperio de Mali · Tawantinsuyu.
- **Épocas del slice ampliado** (M1–M3), NO las 15 al lanzamiento.
- **1 campaña piloto** + skirmish vs IA (hasta 8), sandbox, escenarios históricos, supervivencia.
- **IA oponente de 3 capas** (doc 30), modding day-one (ADR-018), **PEGI 16** (ADR-013).
- Post-launch (2–3 años): 15 épocas × 12 civs, campañas 2–4, online/ladder, modo documental.

### ADR-021 (NUEVO — propuesto aquí, decisión del Director 2026-07-22)

**Multijugador reducido de "LAN lockstep opcional en v1.0" a "solo preparación".**
- *Contexto*: `SPEC_ARQUITECTURA_BASE.md` §3.9 incluía LAN lockstep opcional en v1.0. El Director decide (2026-07-22) sacar el netcode del alcance del plan.
- *Decisión*: v1.0 NO incluye transporte de red, lobby ni resincronización. La **preparación** sí es vinculante y ya existe: determinismo bit a bit (gates G1–G5 en CI en cada sprint), command-stream como único protocolo de mutación, replays como formato de intercambio. Cualquier sistema nuevo que rompa esos gates es un P0.
- *Consecuencia*: el netcode LAN pasa a la primera entrega post-1.0 (§4/Fase 4-post). El coste de re-habilitarlo se mantiene bajo por diseño.
- *Pendiente operativo*: aplicar esta enmienda al texto de `SPEC_ARQUITECTURA_BASE.md` §3.9 y §7 en el cierre del Sprint 0.3.

---

## §3 Registro de SPECs

| SPEC | Contenido | Estado | Se escribe en | Autor |
|---|---|---|---|---|
| SPEC_ARQUITECTURA_BASE v1.1.1 | Constitución técnica; ADRs 008–020 | APPROVED | — (enmienda ADR-021 en 0.3-cierre) | — |
| SPEC-001 v1.1 | Kernel determinista (§1–§16, gates G1–G5) | APPROVED, ejecutado | — | — |
| **SPEC-002** | Datos y schemas: `unit`/`building`/`tech`/`civ`/`map`/`ai-profile` → blob determinista; `chunsa_data_compiler` completo; procedencia y verificación (ADR-014); versionado de blobs | POR ESCRIBIR | Sprint 0.4 | Arquitecto |
| **SPEC-004** | Sistemas de partida: construcción (placement/colisión/HP), producción (colas/costes), tech y epoch-up (ADR-015 doble gate), población, win-conditions | POR ESCRIBIR | Sprint 1.1 (construcción) + 1.2 (producción/tech) | Arquitecto |
| **SPEC-006** | UI/HUD y game feel: contrato ejecutable desde doc 34 (HUD, control groups, minimapa, cámara, atajos, juice) | POR ESCRIBIR | Sprint 1.3 | Arquitecto |
| **SPEC-005** | IA oponente: contrato ejecutable de las 3 capas del doc 30 sobre el mailbox determinista de SPEC-001 §7 (utility AI estratégica → behavior trees tácticos → LOD) | POR ESCRIBIR | Sprint 1.4 | Arquitecto |
| **SPEC-003** | Pipeline de assets y render: formatos, presupuesto de arte por época (doc 37), integración con modo (c) de ADR-009, audio | BORRADOR en 1.1, FINAL en 1.5 | Sprint 1.5 | Arquitecto |
| **SPEC-TRAYECTORIA** | Contrato final de ADR-016: sucesión/legado de módulos históricos, IDs namespaced, reglas de era | POR ESCRIBIR | Fase 2 (antes de integrar la 3ª civ) | Arquitecto |
| **SPEC-007** | Campaña y escenarios: YAML de misión (doc 08), triggers deterministas como Commands, árbol de decisiones, saves de campaña | POR ESCRIBIR | Fase 3 | Arquitecto |

Documentos de diseño canónicos que alimentan cada SPEC (rutas en `/home/adquiod/Imágenes/Project/Investigación/`): `03_EPOCAS` · `24_RECURSOS_Y_CADENAS_PRODUCTIVAS` · `27_CIVILIZACIONES_INSTITUCIONES_Y_TECNOLOGIAS` · `23_ARBOL_TECNOLOGICO_FUNDAMENTADO` · `07_COMBATE` · `30_DISENO_IA_OPONENTE` · `33_METODOLOGIA_BALANCE_Y_ECONOMIA` · `34_GAME_FEEL_UX_Y_DISENO_DE_JUGABILIDAD` · `35_OPTIMIZACION_DEFINITIVA_Y_PRESUPUESTO_UNICO` · `08_CAMPAÑAS` · `09_MODOS_DE_JUEGO`. Consultar SIEMPRE la tabla de canonicidad de `INDICE_MAESTRO.md` §0 antes de citar (04/05/06/11 están supersedidos).

---

## §4 Fases y sprints

> Formato de cada sprint: **Objetivo** · **Entregables** · **Referencias** · **Delegación** · **DoD** (binario).
> Invariante de TODOS los sprints: golden 1074/1074 · G1–G5 verdes · ctest completo · CI 5 lanes verde · revisión independiente del Arquitecto antes de cualquier merge a main.

### FASE 0 — Fundación (cierre)

#### Sprint 0.3-cierre
- **Objetivo**: cerrar formalmente el Sprint 0.3 y saldar sus deudas.
- **Entregables**:
  1. Replay con `effective_tick`: el recorder graba el tick efectivo de cada comando (tras el clamp de §6.2 de SPEC-001) y `verify` reproduce con esa agenda exacta — elimina la ambigüedad de re-clampear al reproducir.
  2. Verificación factual de las fichas 36a–d (Egipto/Roma/Mali/Tawantinsuyu): revisión con fuentes reales, correcciones, y promoción a `Investigación/36_FICHAS_DE_PRODUCCION_SLICE.md` (o rechazo documentado por ficha). Sin esto no hay datos de civ confiables para 0.4.
  3. PERF-0 físico si el hardware de referencia está disponible; si no, se documenta el bloqueo y ADR-011 permanece TARGET.
  4. `docs/REPORTE_SPRINT_0.3.md` + enmienda ADR-021 aplicada a `SPEC_ARQUITECTURA_BASE.md`.
- **Referencias**: SPEC-001 §11/§13/§14 · ADR-014 (verificación factual 100%) · doc 35 (protocolo PERF).
- **Delegación**: effective-tick = **Arquitecto** (toca el contrato de replay). Fichas = **claude-minimax** (acceso web) con veredicto final del Arquitecto. Reporte = Arquitecto.
- **DoD**: fichas promovidas o rechazadas al 100% · `record`/`verify` bit-exactos con effective-tick · reporte commiteado · SPEC base enmendada.

#### Sprint 0.4 — datos reales (fin del hardcodeo)
- **Objetivo**: el kernel deja de recibir stats por payload y pasa a consumirlos de blobs compilados — condición previa de TODO el contenido futuro.
- **Entregables**:
  1. **SPEC-002 escrita y aprobada** (Arquitecto): schemas `building`/`tech`/`civ`/`map`/`ai-profile` (además del `unit.schema.json` existente), formato de blob determinista (enteros/basis points/ticks — el motor JAMÁS parsea YAML, ADR-018), versionado y procedencia.
  2. `chunsa_data_compiler` completo: valida schemas, compila YAML restringido → blob, hash de contenido (ADR-020).
  3. Kernel: `SPAWN_UNIT` por `unit_id` (índice en el blob cargado en `MatchConfig`) — los stats (hp/attack/range/clase/velocidad) se leen del blob; el payload con stats queda como camino de debug. Save v7 (el blob hash viaja en el envelope).
  4. Fixture del slice: las unidades M1 de las 4 civs (desde las fichas verificadas en 0.3-cierre) compiladas y usadas por la demo.
- **Referencias**: SPEC-001 §3/§6/§11 · ADR-018/-020 · doc 24 (recursos) · fichas 36 promovidas.
- **Delegación**: SPEC-002 = **Arquitecto**. Schemas+compiler = **MiniMax** (spec cerrada, su nicho). Integración kernel/blob = **Sonnet** (juicio, toca Step()). Datos YAML del slice = **MiniMax**. Revisión+merge = Arquitecto.
- **DoD**: demo showcase corre 100% data-driven (cero stats hardcodeados) · un cambio de YAML recompilado cambia la partida sin recompilar C++ · gates verdes · save v7 con hash de contenido.

**Gate de FASE 0**: kernel determinista completo + pipeline de datos operativo + demo interactiva data-driven. *(Tras 0.4 la fase queda cerrada.)*

---

### FASE 1 — Vertical slice jugable (M1-INT: Egipto vs Roma)

**Meta de fase**: una partida COMPLETA y ganable de 30+ minutos, Egipto vs Roma (época M1), humano vs IA, con UI real — el primer momento en que CHUNSA es un juego y no una demo.

#### Sprint 1.1 — edificios y construcción
- **Objetivo**: el mapa deja de ser plano — edificios con presencia física y ciudadanos que los construyen.
- **Entregables**:
  1. **SPEC-004 (parte construcción)**: placement sobre cost_grid (celdas ocupadas = FF_WALL para el flowfield — reutiliza la integración existente), validación de solape/terreno, HP/destrucción de edificios (entran al combate como objetivos), comandos `PLACE_BUILDING`/`CANCEL_BUILD` (append-only al enum).
  2. Kernel: entidad-edificio (unit_class nueva banda, sin movimiento), estado de construcción por progreso en ticks, dropoff real como edificio (sustituye el punto fijo de la economía v1).
  3. Ciudadanos constructores: extensión de la state machine económica (SEEK_SITE/BUILD) con el patrón autocontenido de `economy.hpp`.
  4. Render de edificios (quads/cajas por tipo, mismo rig ADR-009) + ghost de placement en el cursor.
- **Referencias**: doc 24 (edificios económicos) · doc 07 (edificios en combate) · SPEC-001 §8 (orden de sistemas).
- **Delegación**: SPEC-004 = **Arquitecto**. Kernel placement/HP/destrucción = **Sonnet**. Módulo constructor autocontenido = **MiniMax**. Render+ghost+input de placement = **Kimi**.
- **DoD**: partida donde ciudadanos construyen un dropoff y un edificio militar, los edificios bloquean el pathfinding, y destruirlos los elimina · save v8 · gates verdes.

#### Sprint 1.2 — producción y tecnología
- **Objetivo**: los edificios producen unidades y la tecnología avanza — el bucle económico-militar completo.
- **Entregables**:
  1. **SPEC-004 (parte producción/tech)**: colas de entrenamiento (coste en recursos, tiempo en ticks, rally point), árbol tecnológico M1 del slice (doc 23 → schema `tech` de SPEC-002; tecnología = paquete de capacidad, NO porcentajes), epoch-up con doble gate de ADR-015 (edificios visibles + tiempo de mundo).
  2. Kernel: `TRAIN_UNIT`/`RESEARCH_TECH`/`SET_RALLY` commands, colas serializadas, aplicación de paquetes de capacidad al blob-estado, población/límite (casas o equivalente del slice).
  3. Datos: tech tree M1 de Egipto y Roma en YAML (desde doc 23 + fichas 36a/36b).
- **Referencias**: doc 23 (árbol canónico) · ADR-015 · doc 24 (costes) · doc 33 (metodología de balance).
- **Delegación**: SPEC = **Arquitecto**. Colas/epoch-up en kernel = **Sonnet**. Datos YAML tech = **MiniMax**. Botones provisionales de producción = **Kimi**.
- **DoD**: de 3 ciudadanos iniciales a un ejército entrenado pasando por 2 epoch-ups, sin tocar código · save v9 · gates verdes.

#### Sprint 1.3 — UI/HUD v1
- **Objetivo**: interfaz de juego real — lo que convierte la interacción de "demo técnica" en "RTS jugable".
- **Entregables**:
  1. **SPEC-006** (Arquitecto, desde doc 34): arquitectura del HUD (GDScript/escenas Godot leyendo snapshots; JAMÁS mutando estado — solo Commands), contrato de eventos de presentación (journal de SPEC-001 §9).
  2. HUD: panel de recursos (A/B/Me + población), panel de selección (retrato/stats/acciones contextuales), minimapa (visión v1 ya existe), botones de producción/construcción reales, tooltips.
  3. Control: control groups (Ctrl+1..9), doble clic = seleccionar tipo, cámara pan (bordes+teclas) y zoom, rally points visibles, banda de selección renderizada (hoy es lógica sin visual).
  4. Atajos de teclado según doc 34.
- **Referencias**: doc 34 (canónico de UX) · SPEC-001 §9 (snapshots/journal).
- **Delegación**: SPEC-006 = **Arquitecto**. TODO el HUD/input = **Kimi** (su nicho; contratos por lotes pequeños para sobrevivir a su cuota). Extensiones del snapshot (recursos/colas hacia la UI) = **Sonnet** o Arquitecto según toque al ring.
- **DoD**: partida completa jugable SIN consola (toda la información y acciones en pantalla) · 60 fps con HUD en la máquina dev · gates verdes (la UI no toca el kernel).

#### Sprint 1.4 — IA oponente v1 + partida completa
- **Objetivo**: un rival que juega — cierre del bucle de partida.
- **Entregables**:
  1. **SPEC-005** (Arquitecto): las 3 capas del doc 30 aterrizadas en el contrato de IA de SPEC-001 §7 (jobs deterministas sobre `AiDecisionInputV1`, decisiones = Commands por el mailbox, `AI_INPUT_DELAY_TICKS`, serialización del `AiRuntimeState` — G4 ya lo garantiza).
  2. Capa estratégica: utility AI (economía→milicia→expansión→ataque) emitiendo metas.
  3. Capa táctica: behavior trees por meta (recolectar, construir, entrenar, atacar en grupo usando el aggro existente).
  4. Win/lose conditions: destrucción de edificios clave o rendición; pantalla de victoria/derrota.
  5. Perfil de IA en YAML (`ai-profile` schema de SPEC-002) — dificultad por mecánicas, NO por trampas de stats (doc 30).
- **Referencias**: doc 30 (canónico) · SPEC-001 §7 · gates G4/G5 (la IA vive en el stream de comandos: los replays la graban, ADR-019).
- **Delegación**: SPEC-005 = **Arquitecto**. Utility AI estratégica = **Sonnet** (juicio). Behavior trees = **MiniMax** (módulos autocontenidos con el patrón economy.hpp). Perfiles YAML = **MiniMax**. Integración+revisión = Arquitecto.
- **DoD — GATE DE FASE 1**: partida 1v1 Egipto vs Roma de 30+ min, jugable y ganable (y perdible) contra la IA · replay íntegro de la partida verificado (G5) · save/load a mitad de partida con IA activa (G4) · gates verdes.

#### Sprint 1.5 — arte, audio y feel
- **Objetivo**: que el vertical slice se VEA y SIENTA como un juego — y decisión de presupuesto de arte.
- **Entregables**:
  1. **SPEC-003 final** (Arquitecto): pipeline de assets (formatos, importación, presupuesto por época del doc 37, atlas/batching sobre el modo (c) de ADR-009), plan de audio.
  2. Primer pase de assets reales del slice M1 (sprites/modelos low-poly de unidades y edificios de Egipto y Roma) — **aquí el Director decide presupuesto**: encargo externo, asset packs adaptados, o generación asistida (doc 37 da los números por civ).
  3. Audio mínimo: feedback de órdenes, combate, construcción, música de época M1.
  4. Juice del doc 34: animación de selección, proyectiles visibles, muertes, partículas mínimas.
  5. **PERF-1**: perfilado con assets reales; objetivo 600u@60fps en tier mínimo (doc 35 §35.7 — la única tabla válida).
- **Referencias**: doc 37 (coste de arte) · doc 34 (juice) · doc 35 (presupuesto de rendimiento) · ADR-009.
- **Delegación**: SPEC-003 = **Arquitecto**. Integración de assets/animación/audio en Godot = **Kimi**. Herramientas de pipeline (import/atlas scripts) = **MiniMax**. Arte en sí = decisión de presupuesto del Director (externo o asistido).
- **DoD**: el vertical slice con arte y sonido reales mantiene 600u@60fps en tier mínimo medido · un tráiler de 60s es grabable de la partida real · gates verdes.

---

### FASE 2 — Slice completo (4 civs × épocas M1–M3) — *granularidad media, re-plan al cerrar Fase 1*

- **Sprint 2.1 — Mali y Tawantinsuyu**: **SPEC-TRAYECTORIA** escrita ANTES de integrar la 3ª civ (contrato final ADR-016: sucesión/legado, IDs namespaced). Datos+IA+arte de ambas civs por el pipeline ya probado (fichas 36c/36d verificadas → YAML → blob). Delegación: spec=Arquitecto, datos=MiniMax, arte-integración=Kimi, ajustes kernel=Sonnet.
- **Sprint 2.2 — épocas M2–M3**: tech/unidades/edificios de 2 épocas más por civ (doc 23 + doc 03), recursos completos del doc 24 (productos intermedios y cadenas productivas: bronce/acero), epoch-up multi-época pulido.
- **Sprint 2.3 — mapas y (si el slice lo exige) naval**: generador de mapas básico + mapas hechos a mano; combate naval del doc 07 SOLO si las civs/épocas del slice lo requieren (decisión de diseño al llegar).
- **Sprint 2.4 — modding alpha + balance**: gate de ADR-018 ("un mod de terceros crea una civ sin tocar código") cumplido con `mod-template/`; campañas de balance con `tools/balance_sim` (doc 33, fix del KP-rush); **candidato: combate v2** — posición/formaciones/flanqueo (directriz del Director 2026-07-22: "los números no lo son todo"), sobre doc 07 (cobertura direccional, formaciones).
- **Gate de FASE 2**: 4 civs × 3 épocas jugables, diferenciadas y balanceadas; un mod externo funciona.

### FASE 3 — Beta de contenido — *bosquejo, se detalla al cerrar Fase 2*

- **SPEC-007** + campaña piloto (doc 08: misiones YAML, árbol de decisiones, triggers deterministas como Commands — los saves de campaña heredan G3/G4).
- Modos: escenarios históricos, supervivencia, sandbox (doc 09).
- Onboarding por época (doc 34), localización ES/EN, accesibilidad básica.
- Beta cerrada: replays de testers como telemetría de balance (ventaja directa del determinismo).
- **Gate de FASE 3**: beta externa estable, campaña piloto completable, feedback incorporado.

### FASE 4 — Endurecimiento y 1.0 — *bosquejo*

- **PERF-2**: medición en los 3 tiers reales (ADR-011 deja de ser TARGET y se convierte en contrato).
- Estabilidad: fuzzing ampliado de saves/replays/mods, soak tests de partidas largas.
- Certificación PEGI 16 (ADR-013), página de tienda, demo pública (el vertical slice de Fase 1 es la demo natural).
- **Release 1.0**: 4–6 civs, épocas M1–M3, 1 campaña, modding day-one.
- **Post-1.0** (fuera de este plan, orden ya decidido): netcode LAN (levanta ADR-021) → +civs (Aqueménida primero) → épocas hacia 15 → campañas 2–4 → online/ladder → modo documental.

---

## §5 Doctrina de ejecución (resumen operativo de `docs/DELEGACION_MODELOS.md`)

| Rol | Agente | Nicho | Coste |
|---|---|---|---|
| Arquitecto | Fable/Opus | Contratos/SPECs, revisión línea a línea, integración, determinismo — **indelegable** | Máximo — usar lo mínimo |
| Kernel con juicio | Sonnet 5 (`claude --model sonnet -p`) | Sistemas del kernel, integraciones delicadas | Medio |
| Frontend/Godot | Kimi K3 (`kimi -p`) | UI/HUD, render, input, escenas | Cuota limitada (403 recurrente) |
| Volumen | MiniMax M3 (bridge MCP / `claude-minimax`) | Módulos spec-cerrada autocontenidos, datos YAML masivos, investigación web | Mínimo (~1/20) |

**Reglas permanentes** (aprendidas en Fase 0, no negociables):
1. **Térmica**: builds `nice -n 19` y `-j2` máximo; una tarea pesada a la vez (el equipo se apaga).
2. **Interrupciones**: tras cualquier tarea killed/fallida, `git status` ANTES de cualquier acción; nunca asumir trabajo perdido.
3. **Verificación independiente**: el Arquitecto rebuild+re-verifica TODO lo delegado antes de merge (`--no-ff` desde rama dedicada); jamás confiar en números autorreportados.
4. **Briefs con API literal**: firmas/structs copiados del código real, nunca prosa (M3 y Kimi inventan nombres).
5. **Patrón autocontenido** para MiniMax: módulos de funciones puras sin dependencia de GameState (como `economy.hpp`); el wiring lo hace el Arquitecto.
6. **Enum de comandos append-only**: jamás renumerar.

---

## §6 Riesgos y mitigaciones

| Riesgo | Impacto | Mitigación |
|---|---|---|
| **Arte** = mayor coste real del proyecto | Fase 1.5 puede estancar la release | Decisión de presupuesto explícita en 1.5 con los números del doc 37; estilo low-poly/estilizado reduce coste; el slice M1 acota el volumen inicial |
| Cuotas de modelos (Kimi 403 recurrente; MiniMax 600s en bridge) | Sprints de UI se alargan | Briefs por lotes pequeños; fallback documentado (2 fallos → Arquitecto o reasignación); `claude-minimax` para tareas largas |
| PERF-0/2 sin hardware físico de referencia | ADR-011 sigue TARGET; sorpresas tarde | Mantener perfilado continuo en máquina dev; conseguir acceso a UHD 620 antes de Fase 2 |
| Scope creep hacia 15 épocas / 12 civs | Nunca se lanza | ADR-012 protege: TODO contenido extra es post-1.0; este plan es el instrumento de control |
| Determinismo roto por sistemas nuevos (IA, triggers, campaña) | Se pierde la preparación MP y los replays | G1–G5 en CI en cada sprint = P0 bloqueante; todo muta vía Commands |
| Combate simplista (directriz del Director: posición/formaciones deben contar) | Profundidad insuficiente en beta | Candidato combate v2 en Sprint 2.4 sobre doc 07; registrado en memoria `chunsa-diseno-combate-futuro` |
| Docs canónicos vs viejos (04/05/06/11 supersedidos) | Datos generados desde fuente equivocada | Todo brief de datos cita el doc canónico por ruta y sección; INDICE_MAESTRO §0 manda |

---

## §7 Protocolo de cierre de sprint

1. Gates completos (golden · G1–G5 · ctest · CI) — bloqueante.
2. `docs/REPORTE_SPRINT_X.md` (hecho/desviaciones/lecciones).
3. Actualizar **este plan**: §1 estado, §4 sprint cerrado y re-plan del siguiente, §6 si hay riesgos nuevos.
4. Actualizar memoria persistente del Arquitecto.
5. **OK del Director** para arrancar el siguiente sprint.
