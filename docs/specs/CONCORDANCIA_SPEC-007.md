# Concordancia — qué contradice SPEC-007 en las specs vigentes

Fecha: 2026-07-28. Tras la aprobación de SPEC-007 por el Director.

**Para qué sirve.** SPEC-007 cambia el modelo de recursos, y las specs
anteriores fueron escritas asumiendo tres. En vez de reescribirlas ahora —lo
que las dejaría describiendo un juego que todavía no existe— este documento
enumera **cada contradicción concreta**, qué debe pasar a decir, y **en qué
sprint** se corrige.

Regla: mientras un punto no esté marcado como resuelto, **manda la spec
original**, no SPEC-007. Un implementador que lea SPEC-004 §12 hoy debe seguir
implementando tres recursos.

---

## §1 Contradicciones directas (el texto vigente dice algo que dejará de ser cierto)

| # | Dónde | Dice hoy | Debe decir | Sprint |
|---|---|---|---|---|
| C1 | SPEC-004 §4.1 (línea ~37) | `BuildingDefinitionV1` con `cost_a, cost_b, cost_me` | Vector de costes de longitud `RESOURCE_COUNT` | 1.8A |
| C2 | SPEC-004 §4.2 (~96) | «`player_stock[emitter]` cubre `cost_a/b/me`» | Cubre el vector completo de costes | 1.8A |
| C3 | SPEC-004 §11 (~258) | `UnitDefinitionV1` con `cost_a/cost_b/cost_me` | Vector de costes | 1.8A |
| C4 | SPEC-004 §12.1 (~300) | `TechDefinitionV1` con `cost_a/b/me` | Vector de costes | 1.8A |
| C5 | SPEC-004 §12.3 (~327) | `EPOCH_COST_A/B/ME` constantes | Coste de época **por dato**, no constante, y por recurso | 1.8B |
| C6 | SPEC-004 §19 (~473) | La IA cuenta «stock A/B/Me» | Recorre `RESOURCE_COUNT`; prioriza por déficit relativo | 1.8B |
| C7 | SPEC-005 §5 (~80) | Observación de IA incluye «stock A/B/Me» | Vector completo; la observación crece | 1.8B |
| C8 | SPEC-004 §16–§18 | Economía con tres índices de recurso | `RESOURCE_COUNT` índices | 1.8A |
| C9 | SPEC-004 §16–§18, §23 | `eco_assigned_deposit` como índice único en `deposits[]` | `eco_target_kind` + `eco_target_index`: un ciudadano puede ir a depósito **o** a fuente construida (SPEC-007 §15.3) | 1.12 |
| C10 | SPEC-004 §23 | La zona aliada considera solo `deposits[]` | Considera también las fuentes construidas propias (SPEC-007 §15.4) | 1.12 |
| C11 | SPEC-004 §16 | `EcoDeposit.remaining` | `reserve_total` + `extracted` (SPEC-007 §14.1) | 1.11 |

## §2 Huecos (SPEC-007 introduce algo que ninguna spec vigente cubre)

| # | Qué | Dónde debe vivir | Sprint |
|---|---|---|---|
| H1 | **Energía streaming** (§8.1): no es índice de stock, se deriva por tick, parada en seco | SPEC-004, sección nueva de sistemas | 1.10 |
| H2 | **Upkeep** (§10): unidades consumen comida, industria consume energía | SPEC-004, sección nueva | 1.10 |
| H3 | **Recetas y edificio de conversión** (§3.2) | SPEC-004, junto a producción §11 | 1.9 |
| H4 | **Reserva y recuperación**: `reserve_total` + `extracted` + `recovery_pct` | **SPEC-007 §14** ✅ escrita | 1.11 |
| H5 | **Fuentes construidas**: granjas y plantaciones forestales, fuera de `deposits[]` | **SPEC-007 §15–§16** ✅ escritas | 1.12 |
| H6 | **HUD por familias**: 6 grupos, no 26 contadores | **SPEC-006 Parte III** ✅ escrita | 1.8B |
| H7 | **IA con economía extendida**: déficit relativo, recetas, energía, valor residual | **SPEC-005 Parte II** ✅ escrita | 1.8B–1.11 |
| H8 | **Órdenes de combate**: `ATTACK`, `ATTACK_MOVE`, proyectiles | **SPEC-004 Parte V §24** ✅ escrita | 1.13 |
| H9 | **Coste de la selección económica** sin cota superior con fuentes construidas | **SPEC-007 §17** ✅ escrita | 1.12 (medición) |
| H10 | **Presupuestos y límites** de rendimiento, fiabilidad y escala | **SPEC-008** ✅ escrita | 1.8A en adelante |

## §3 Falsos conflictos (parecen contradicción y no lo son)

**`EPOCH_MAX_V1 = 7` frente a «15 edades».** No hay conflicto. La **escalera de
diseño** tiene 15 peldaños; `EPOCH_MAX_V1` acota **el slice jugable según los
datos authored**, que hoy solo cubren Egipto (épocas 3–4) y Roma (5).

**Nadie debe "corregir" `EPOCH_MAX_V1` a 15.** Subirlo sin datos de las
civilizaciones correspondientes produce edades vacías, que es precisamente el
fallo que el panel identificó en Empire Earth. Sube cuando suba el contenido,
no antes.

**Simetría de spawns (SPEC-004 §15.1)** sigue vigente tal cual con 26 recursos.
El panel lo confirmó como práctica estándar de mapas competitivos.

**Zona aliada (SPEC-004 §23)** sigue vigente y se refuerza: el panel encontró
que Rise of Nations restringe la construcción al territorio propio por la misma
razón.

## §3-bis Estado de la documentación (2026-07-29)

Todos los huecos identificados el 2026-07-28 **están ya especificados**. No
queda ningún sistema aprobado sin contrato escrito:

| Documento | Cubre |
|---|---|
| SPEC-007 §1–§11 | Modelo de recursos, edades, upkeep, secuenciación |
| SPEC-007 §12 | Recetas y `CRAFT` |
| SPEC-007 §13 | Energía streaming y upkeep |
| SPEC-007 §14 | Reserva y recuperación |
| SPEC-007 §15–§16 | Fuentes construidas: granjas y reforestación |
| SPEC-007 §17 | Coste y optimización de la selección económica |
| SPEC-004 Parte V §24 | Órdenes de combate (cierre mecánico Fase 1) |
| SPEC-005 Parte II | IA con economía extendida |
| SPEC-006 Parte III | HUD de recursos por familias |
| SPEC-008 | Rendimiento, fiabilidad, escalabilidad |

**Total de criterios de aceptación en formato TDD: 90.** Cada uno es una prueba
que debe poder fallar.

## §4 Orden de aplicación

Cada sprint de §11 de SPEC-007 **cierra** las filas que le corresponden aquí y
las marca como resueltas en este documento. Este fichero es el único sitio
donde se lleva la cuenta; si una fila no está marcada, el cambio no está hecho.

**1.8A** no toca datos ni contenido: C1, C2, C3, C4, C8. Al terminar, el juego
debe comportarse **exactamente igual** —solo cambian los checksums—. Cualquier
otra diferencia observable es un error, no un efecto esperado.
