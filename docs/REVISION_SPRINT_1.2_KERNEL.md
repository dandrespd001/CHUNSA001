# Revisión del kernel de producción/tecnología — Sprint 1.2 (implementó: Sonnet · revisó: Arquitecto + Opus)

Fecha: 2026-07-24 · Ramas: `sonnet/k1-replay-v3` (integrada `785408d`) + `sonnet/k2-produccion-tech` (integrada vía `arch/sprint-1.2-k2-integration`)

## Veredicto
**ACEPTADO CON ENDURECIMIENTOS Y 1 FIX DE DATOS.** Dos piezas de kernel entregadas con
RESULT ejemplares (K1: 5 desviaciones; K2: 12 desviaciones, todas defendibles y varias
que destaparon límites reales del contrato). Determinismo intacto en ambas.

## K1 — replay v3 + save v9 + ventana de setup
Ya integrado y reportado (`785408d`). Cerró D8 (replay perdía `unit_id`/`BuildingId`) y
su gemelo en la agenda del save; verificó que el checksum v3 no cubría el `unit_id`
pendiente → bump justificado a `CHUNSA_STATE_V4`; la ventana de setup (`target 0 && t 0
→ eff 0`) hace alcanzable la exención de escenario con `delay=1` de producción.
**Endurecimiento del Arquitecto**: un `GameState` de test en pila segfaulteaba bajo ctest
→ a heap.

## K2 — producción, tecnología y épocas
### Auditoría del parsing tech/capacidades (Opus)
**SIN P0.** Espejo fiel del patrón endurecido de unidades/edificios (unique_ptr, reserve
exacto, resolución diferida de referencias con rechazo del catálogo entero, sin OOB/UAF/
no-determinismo). Punto crítico confirmado correcto: el loader **reordena bytewise** la
tabla de capacidades (el compilador Python la emite en orden longitud-primero de CVE) y
la correspondencia grants↔capacidad se mantiene porque todo se resuelve vía `resolve_id`
sobre el vector ya ordenado+deduplicado; `std::sort` sobre elementos distintos es
determinista. **2 P2 de robustez** (paridad del guard de longitud/no-vacío en la tabla de
capacidades) → **aplicado por el Arquitecto**: `declared_capabilities` ahora rechaza
nombres vacíos o > 0xFFFF bytes, igual que unit/building/tech.

### Fix de datos del Arquitecto (desviación 6 de K2)
Los 6 edificios reales tenían `researches: []` → RESEARCH_TECH era **inusable con el blob
real** (siempre MALFORMED). Poblé `researches` en 4 edificios (cada cuartel su tech
militar, cada centro su tech de economía): `chariotry_stable→composite_bow_program`,
`castra_barracks→marching_drill`, `settlement_center→corvee_logistics`,
`forum_center→road_engineering`. Blob regenerado (content_hash `6c7a283c`), pin del test
actualizado. Es relación de gameplay, no claim histórico → sin cambio de procedencia.

### Fix del adaptador Godot (desviación 9 de K2)
El adaptador enlaza `gs->catalog` a mano y no llamaba `gs_init_epoch_from_catalog` →
`player_epoch=0`. Con los datos reales (época ≥3) el gate §12.4 habría rechazado **toda**
colocación manual de edificios del jugador — regresión de la demo del Sprint 1.1. Añadida
la llamada en `_ready()` (1 línea). Demo headless verde tras el fix (`buildings=2`, 0
errores).

## Verificación independiente (rama de integración, post-endurecimientos)
- Golden **1074/1074** · **G1** alloc_delta=0 `544ae6be04ced646` · **G3** `309d6ecce04905ff`
  · **G4** `86cbed58173a9c80` · **G5** schedule_mismatches=0 replay_v=3 — checksums
  idénticos a los del RESULT de Sonnet (dominio v5 intencional).
- **ctest 16/16** (nuevo `production_tech`) · build `-Werror` limpio · adaptador Godot
  0 warnings · demo headless intacta.
- Save v10, checksum v5 (`CHUNSA_STATE_V5`). Época inicial del blob real = 3 (mínimo
  catálogo-ancho, fórmula documentada).

## Deuda registrada (para Fase 1 restante / 1.3+)
1. **`pop_used` impreciso** (K2 desv. 7): toda muerte de unidad lo decrementa aunque no
   la haya reservado (SPAWN_UNIT/debug). Inofensivo en el slice (solo gatea TRAIN); revisar
   cuando el origen del spawn importe.
2. **Efectos de stats por tech** (Parte III): hoy las capacidades solo gatean contenido.
3. **`epoch_window`/`civ_id` por entidad**: la época inicial es catálogo-ancha, no por
   civilización (el kernel no tipa civ por unidad). Necesario para partidas asimétricas
   reales — candidato a SPEC-TRAYECTORIA / Fase 2.
4. Sin CANCEL de cola/research; sin migración de saves entre versiones (precedentes).
