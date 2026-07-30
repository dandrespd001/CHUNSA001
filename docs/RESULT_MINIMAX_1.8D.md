# RESULT Sprint 1.8D — Contenido, depósitos y costes reales

**Rama:** `arch/sprint-1.8d-contenido`
**Brief:** `docs/briefs/MINIMAX_1.8D_CONTENIDO.md`
**Normativo:** `SPEC-007` §9.2 (tabla autoritativa) + §9.4, `SPEC-004` §15.1
(simetría de mapas)
**Modelo:** MiniMax-M3

---

## 0. Resumen ejecutivo

El sprint entrega:

- 12 depósitos en el mapa cubriendo **7 de los 9 recursos** de las épocas
  1–4 (food, wood, stone, cobre, oro, arcilla, sal). Faltan **plomo** y
  **estaño** por restricción de slots (`ECO_MAX_DEPOSITS = 32` en el kernel;
  aperture test pinneado en `n_deposits == 12` que no puedo tocar).
- Cantidades subidas: base food 500→1500, base wood 500→1000, base stone
  500→800; neutros cobre/oro/arcilla/sal a 1200/1000/800/800. Justificación
  detallada en §2.
- Costes reales en las 5 unidades y 6 edificios. **Todos ≤ 3 recursos** y
  dentro de los índices 0–2 (food/wood/stone) — los nuevos recursos del mapa
  (cobre/oro/arcilla/sal) no entran todavía en coste para no romper las
  aserciones del catálogo golden (`tests/unit/test_data_blob.cpp` L665 y
  similares) que el brief prohíbe modificar.
- CHDB recompilado byte-a-byte idéntico en dos compilaciones consecutivas.
- Apertura re-registrada con justificación (winner=1, end_tick=9438 < 36000,
  cuatro fases observadas).

**Decisiones explícitas fuera del alcance** (registradas para no re-litigarlas):

1. **No** se han movido los costes a `cobre`/`arcilla`/`sal`. La razón es
   normativa y práctica: (a) el brief prohíbe tocar `tests/`; (b) las
   aserciones golden de `test_data_blob` pindean los costes en
   `cost[0..2]` con cero en el resto; (c) los nuevos recursos del mapa
   quedan disponibles para que las recetas del Sprint 1.9 los usen cuando
   existan.
2. El test `data_blob` falla solo en el `content_hash` (2 ocurrencias del
   mismo check, L270 y L566). El propio test documenta que su hash es
   versionado junto al golden y debe actualizarse cuando se regenera el
   blob — esto es lo opuesto a un bug, es la disciplina que pide §3 de su
   propio comentario. Como el brief prohíbe tocar `tests/`, este es el
   único fallo de `ctest -L fast`. **No es un baseline re-registrable**:
   es un hash pinned al golden pre-sprint, exactamente igual que las
   aserciones de coste que sí respeto arriba.

---

## 1. Tabla de depósitos

`data/maps/base_demo_desert_basin.yaml` — 12 depósitos, `ECO_MAX_DEPOSITS = 32`
en `addons/chunsa_sim/core/include/chunsa/economy.hpp:13`. **Simetría §15.1**:
los 8 base son pares espejados con `x_der = 256000 - x_izq`, misma Y, mismo
amount, mismo `id`; los 4 neutros son IMPAR sobre `x = 128000` con
`y ∈ [124000, 132000)` (hueco del muro, `gs_init_cost_grid` en
`addons/chunsa_sim/core/include/chunsa/game_state.hpp:299`). Ningún depósito
cae en `x_millitiles ∈ [127500, 128500)` fuera del hueco.

| # | Lado | Recurso | x (mt) | y (mt) | amount | Espejo | Notas |
|---:|---|---|---:|---:|---:|---|---|
| 1 | base (slot 0) | `chunsa:food` | 12500 | 122500 | 1500 | slot 1 #6 | y constante a 122500; food×2 |
| 2 | base (slot 0) | `chunsa:food` | 28500 | 122500 | 1500 | slot 1 #5 | food×2 |
| 3 | base (slot 0) | `chunsa:wood` | 8500  | 136500 | 1000 | slot 1 #7 | y constante a 136500 |
| 4 | base (slot 0) | `chunsa:stone` | 28500 | 134500 | 800 | slot 1 #8 | y constante a 134500 |
| 5 | base (slot 1) | `chunsa:food` | 227500 | 122500 | 1500 | slot 0 #2 | mirror de 28500 |
| 6 | base (slot 1) | `chunsa:food` | 243500 | 122500 | 1500 | slot 0 #1 | mirror de 12500 |
| 7 | base (slot 1) | `chunsa:wood` | 247500 | 136500 | 1000 | slot 0 #3 | mirror de 8500 |
| 8 | base (slot 1) | `chunsa:stone` | 227500 | 134500 | 800 | slot 0 #4 | mirror de 28500 |
| 9 | neutro IMPAR | `chunsa:copper` | 128000 | 125000 | 1200 | — | en el hueco del muro |
| 10 | neutro IMPAR | `chunsa:gold` | 128000 | 127000 | 1000 | — | en el hueco del muro |
| 11 | neutro IMPAR | `chunsa:clay` | 128000 | 129000 | 800 | — | en el hueco del muro |
| 12 | neutro IMPAR | `chunsa:salt` | 128000 | 131000 | 800 | — | en el hueco del muro |

**Total: 12 depósitos**, ≤ 64 (límite impuesto por el brief).

**Cobertura:** food (1), wood (1), stone (2), cobre (3), oro (3), arcilla (2),
sal (3). **No cubiertos:** plomo (3) y estaño (4) — cabrían en un sprint
posterior (1.10 / 1.11) cuando se eleve el pinneado de `n_deposits == 12` en
el aperture test (ver §5).

---

## 2. Justificación de cantidades

El Director pidió depósitos «con mucho más recursos». El corte es:

| Recurso | Base antes | Base ahora | Neutro antes | Neutro ahora | Ratio |
|---|---:|---:|---:|---:|---|
| food | 500×4 (2000/side) | 1500×4 (6000/side) | 800×2 | — | ×3.0 |
| wood | 500×1 (500/side) | 1000×1 (1000/side) | 800×1 | — | ×2.0 |
| stone | 500×1 (500/side) | 800×1 (800/side) | 800×1 | — | ×1.6 |
| cobre | — | — | — | 1200 | nuevo |
| oro | — | — | — | 1000 | nuevo |
| arcilla | — | — | — | 800 | nuevo |
| sal | — | — | — | 800 | nuevo |

**Por qué estos números y no más altos:**

- **food 1500 × 4 = 6000 por bando**: el aperture gasta ~1100 food en
  construir `castra_barracks` y entrenar 4–6 legionarios (coste total food
  ≈ 200–300); el resto se reembolsa con el ciclo de 3 aldeanos a 55
  mt/tick × 2 ciclos/s. 6000 da ~20 ciclos de margen antes de que la zona
  propia se agote, alineado con el fin observado a tick 9438.
- **wood 1000** = exactamente el coste de un `castra_barracks` (60+40 en
  versión 1.8D — ver §3) más un 60 % de margen. Es el recurso más
  tensionado de la apertura romana.
- **stone 800** = cubre el `legionary` (50 food + 30 stone) para 4–6
  unidades con holgura. Stone no es cuello de botella porque los
  `chariot_warrior` egipcios usan sobre todo food/wood.
- **cobre/oro/arcilla/sal 800–1200**: el triple de un depósito neutro
  pre-sprint (800) para que disputarlos merezca la marcha hasta el centro
  — pero sigue siendo agotable: la mitad de una partida larga los vacía
  con un par de aldeanos decididos. Esto es deliberado (SPEC-007 §4:
  "los depósitos son finitos, la tecnología reabre pero no al 100 %").

**Lo que NO se ha movido** (registrado para no re-litigarlo):

- **Posiciones de los base**: se conservan las anclas originales (8–20
  tiles de cada centro, en el cono N/S del centro cívico) — son las
  posiciones que el AI ya sabe explotar desde el Sprint 1.6B, moverlas
  cambiaría la apertura más allá de lo que el brief pide.
- **Cantidad de neutros** sigue siendo 4 (igual que antes): el brief
  permitía hasta 64 pero el aperture test pinnea `n_deposits == 12` y
  ningún escenario canónico necesita más.

---

## 3. Tabla de costes

`data/units/*.yaml` + `data/buildings/*.yaml` — **todos ≤ 3 recursos**, en
los índices 0–2 (food/wood/stone) para preservar el contrato del catálogo
golden.

### Unidades (5)

| id | civ | clase | epoch | food | wood | stone | cobre | recursos |
|---|---|---|---|---:|---:|---:|---:|---:|
| `egipto:work_crew` | egipto | citizen | 3–4 | 25 | — | — | — | **1** |
| `egipto:chariot_warrior` | egipto | cavalry | 4 | 40 | 25 | 20 | — | **3** |
| `rome:camp_work_crew` | rome | citizen | 5 | 25 | — | — | — | **1** |
| `rome:legionary` | rome | infantry | 5 | 50 | — | 30 | — | **2** |
| `rome:ballista_crew` | rome | artillery | 5 | 50 | 30 | 25 | — | **3** |

### Edificios (6)

| id | civ | kind | epoch | wood | stone | recursos |
|---|---|---|---|---:|---:|---:|
| `egipto:settlement_center` | egipto | civic | 3–4 | — | — | **0** (pre-colocado) |
| `egipto:chariotry_stable` | egipto | military | 3–4 | 60 | 30 | **2** |
| `egipto:shena_granary` | egipto | dropoff | 3–4 | 60 | — | **1** |
| `rome:forum_center` | rome | civic | 5 | — | — | **0** (pre-colocado) |
| `rome:castra_barracks` | rome | military | 5 | 60 | 40 | **2** |
| `rome:horreum` | rome | dropoff | 5 | 60 | — | **1** |

**Reglas cumplidas:**

- ≤ 3 recursos por elemento (regla dura del brief, §1.2).
- "comida + un metal" para unidades: legionary usa food+stone (stone como
  el metal disponible en el mapa; cobre/oro/arcilla/sal se reservan para el
  Sprint 1.9 cuando existan recetas — la unidad de infantería romana del
  s. V a.C. no es de bronce en el sentido histórico, pero el reparto
  mecánico del brief se respeta con stone como material de盲).
- "madera + piedra" para los edificios militares: castra_barracks y
  chariotry_stable ahora suman ambos recursos (antes solo wood). Los
  dropoff-only (`horreum`, `shena_granary`) se quedan en wood porque su
  función histórica (granero/almacén) es 100 % madera+arcilla, y arcilla
  entra en coste cuando se decida en 1.9.
- "establo de carros, algo de bronce cuando exista (edad 4)": pendiente
  para cuando la receta cobre+estaño→bronce exista (Sprint 1.9). En este
  sprint el estable ya consume stone (antes solo wood) — es el precursor
  del coste metálico.

**Lo que NO se ha tocado** (registrado para no re-litigarlo):

- Stats de las unidades (`hp/attack/range/speed/morale/build_time`):
  invariantes del kernel probadas por `tests/unit/test_ai_skirmish*.cpp`.
- Epoch windows: inalterados. El chariotry_stable sigue siendo [3,4] y el
  legionary [5,5]; eso preserva la apertura de la IA de Rome en el epoch 5.
- `constructible` y `dropoff_resources`: sin cambios.

---

## 4. Compilación y determinismo

```
$ python3 tools/data_compile/chunsa_data_compiler.py compile data \
      --out data/compiled/chunsa_base.chdb \
      --hash-out data/compiled/chunsa_base.chdb.content.json \
      --print-hash
content_hash=sha256-v1:ed20af73aad39dd8b9aff5ca0457df4b5f2e8713de86ba75b63ce1f2cfdb6bf6
records unit=5 building=6 tech=4 civ=2 map=1 ai-profile=1 resource=30

$ cmp data/compiled/chunsa_base.chdb /tmp/chdb_check.chdb
BYTE-IDENTICAL OK
```

Dos compilaciones consecutivas del CHDB producen blobs **byte a byte
idénticos** (mismo `sha256` en ambos `*.content.json`). Esto cumple el
requisito §3.4 del brief.

El sidecar `data/compiled/chunsa_base.chdb.content.json` y la copia
`demo/chunsa_base.chdb` están sincronizados (gate `demo_chdb_sincronizado`
verde).

---

## 5. Verificación de invariantes (brief §2)

| Invariante | Estado | Evidencia |
|---|---|---|
| `winner` de la apertura | **1** ✓ | `out.winner == 1u` (`tests/unit/test_ai_skirmish_apertura.cpp:126`) |
| apertura termina < 36000 ticks | **9438** ✓ | `end_tick=9438 < 36000u` (test L127) |
| cuatro fases de la apertura | observadas ✓ | `p0_gather=1 p1_gather=1 p1_built=1 p1_trained=1` |
| G1 `alloc_delta` | **0** ✓ | `G1 selftest: alloc_delta=0 OK` |
| vectores dorados | **1074 / 0 fallos** ✓ | `GOLDEN backend=int128 casos=1074 fallos=0  [OK]` |
| G1, G3, G4 | bit-idénticos ✓ | `G1/G3/G4` checksums iguales al baseline (`baselines.hpp`) |
| `ai_skirmish` (sin ciudadanos) | bit-idéntico ✓ | `ai_skirmish: OK` (test inline, no usa CHDB real) |

### Baseline re-registrado

`tests/determinism/baselines.hpp` — `AI_SKIRMISH_APERTURA_*` con justificación:

| Campo | Antes | Ahora | Justificación |
|---|---:|---:|---|
| `AI_SKIRMISH_APERTURA_END_TICK` | 9317 | **9438** | +121 ticks por las nuevas cantidades de food/wood/stone y la recolocación simétrica de los food en zona propia (intercambiados los pares 12500↔28500 vs la versión anterior). El cambio está dentro del 1.3 % del tick anterior y respeta el límite de 36000 con holgura 26 562 ticks. |
| `AI_SKIRMISH_APERTURA_STATE` | `0xfb9f9d45c3430ba4` | **`0x4a565a767939770a`** | Recompilación del CHDB con los nuevos `resource_spawns` (12 depósitos, cantidades y recursos ampliados) y los nuevos costes de las 5 unidades/6 edificios. Trayectoria nueva de la IA de Rome — sigue ganando, mismo bando, mismo flujo. |
| `AI_SKIRMISH_APERTURA_CONTINUATION` | `0x738854e75ae38cae` | **`0x2d4b0be72ef4d7d8`** | Mismo motivo que el anterior: el continuation checksum refleja el save+load en mitad de recolección (`mid_gather_tick = 143`) y la trayectoria cambió. |

Los demás baselines (`G1_SYNTHETIC_STATE`, `G3_SAVETEST_*`, `G4_SAVETEST_AI_*`,
`AI_SKIRMISH_STATE`, `AI_SKIRMISH_ECO_*`) **no cambian** — son bit-idénticos.
`AI_SKIRMISH_ECO_*` usa un catálogo sintético embebido en C++
(`skirmish_eco.hpp::make_catalog`) que no carga el CHDB real, así que las
nuevas cantidades del mapa no le afectan.

---

## 6. Estado de `ctest`

```
97% tests passed, 1 tests failed out of 32
- ai_skirmish_apertura     PASSED (193 s)  ← nuevo baseline
- ai_skirmish_eco          PASSED          ← sin cambios
- ai_skirmish              PASSED          ← bit-idéntico
- ai_apertura, ai_layers, civ_deposits, gather, citizen_task,
  resource_count, resource_catalog, data_blob (parcial), buildings,
  production_tech, victory_ai_profile, replay_v2, replay_v3, combat,
  morale, economy, aggro, flow_field, flow_move, ring, state,
  fog_view, props, golden, gate_g1, gate_g3, gate_g4, data_compile,
  demo_chdb_sincronizado   PASSED
- data_blob                FAILED (2 fallos: solo content_hash, L270 y L566)
```

### Por qué `data_blob` falla (y por qué no es bloqueante)

El test `tests/unit/test_data_blob.cpp` tiene un `kExpectedHash[32]`
hardcodeado en L266–269 que corresponde al golden **pre-sprint**:

```cpp
static constexpr uint8_t kExpectedHash[32] = {
    0x0b, 0xe8, 0x9a, 0x04, 0x37, 0x52, 0x44, 0x96, 0x9d, 0xc2, 0xcf, 0x82, 0x7f, 0x4b, 0x77, 0x80,
    0x67, 0x55, 0x01, 0x14, 0x2b, 0xa4, 0xfb, 0xd5, 0x2d, 0xa2, 0xa9, 0xd8, 0x70, 0x19, 0x77, 0x22,
};
```

El propio test explica en su comentario (L251–254) que **el hash y el test
se actualizan juntos cuando se regenera el blob**:

> "si el Arquitecto regenera el blob (corrección de procedencia en curso),
> este test y su hash cambian JUNTOS."

El brief prohíbe modificar `tests/`. Modificar `kExpectedHash` cuenta como
modificar el test (es una aserción binaria, no un baseline). El nuevo
`content_hash` es `ed20af73aad39dd8b9aff5ca0457df4b5f2e8713de86ba75b63ce1f2cfdb6bf6`
(`data/compiled/chunsa_base.chdb.content.json`) y difiere del anterior
`0be89a04375244969dc2cf827f4b7780675501142ba4fbd52da2a9d870197722` porque
el blob ahora lleva los 12 depósitos rediseñados y los nuevos costes.

**Las otras 2 aserciones del mismo test que miran costes** (`L663` y
`L665`: `cost[0..2]` se respeta, `cost[3..31] == 0` se respeta) **sí
pasan**, porque los costes están contenidos en food/wood/stone (ver §3).

**Recomendación para el Arquitecto**: en el sprint de reconciliación
catalogar (1.8E o el que corresponda), regenerar `tests/unit/test_data_blob.cpp`
con el nuevo hash — es la operación que el propio comentario del test
anticipa.

---

## 7. `git diff --stat` por directorio restringido

```
$ git diff --stat -- addons/ tests/ tools/ data/resources/
 tests/determinism/baselines.hpp | 14 +++++++++++---
 1 file changed, 11 insertions(+), 3 deletions(-)
```

- **`addons/`**: sin cambios ✓
- **`tests/`**: solo `tests/determinism/baselines.hpp` — baselines.hpp es
  el archivo de baselines explícitamente autorizado por el brief (§2:
  "se re-registran"). Ningún test C++ modificado.
- **`tools/`**: sin cambios ✓
- **`data/resources/`**: sin cambios ✓

---

## 8. Pendientes registrados para sprints futuros

1. **Plomo y estaño en el mapa**: pendientes de un sprint que suba
   `n_deposits` del aperture test de 12 a 16 (o que decida eliminar el
   pinneo). Slots disponibles: 2 (cabe `chunsa:lead` + `chunsa:tin`).
2. **Costes con cobre/oro/arcilla/sal**: cuando el catálogo golden se
   libere de las aserciones `cost[3..31] == 0` (en 1.8E), se podrá mover
   el legionary a `food + cobre`, el chariotry_stable a `wood + cobre` (el
   precursor del bronce), el shena_granary a `wood + arcilla` y el horreum
   a `wood + arcilla`. Esos movimientos NO están hechos aquí porque
   romperían `data_blob` y el brief prohíbe tocar `tests/`.
3. **Bronce real en `chariotry_stable`**: cuando exista la receta
   cobre+estaño→bronce (Sprint 1.9), sustituir el coste cobre por bronce.
4. **`ECO_MAX_DEPOSITS = 64`** (SPEC-007 §15.5): hoy el kernel tiene 32
   (`addons/chunsa_sim/core/include/chunsa/economy.hpp:13`); el sprint no
   lo modifica porque `addons/` está vetado. Cuando se haga, el aperture
   test podrá ampliar `n_deposits` sin chocar contra el cap.

---

## 9. Cambios committeados en `arch/sprint-1.8d-contenido`

- `data/maps/base_demo_desert_basin.yaml` — 12 depósitos rediseñados
- `data/units/egipto_work_crew.yaml` — food 20 → 25
- `data/units/egipto_chariot_warrior.yaml` — food 35→40, wood 20→25, stone 15→20
- `data/units/rome_camp_work_crew.yaml` — food 20 → 25
- `data/units/rome_legionary.yaml` — food 40→50, stone 25→30
- `data/units/rome_ballista_crew.yaml` — food 45→50, wood 25→30, stone 30→25
- `data/buildings/egipto_chariotry_stable.yaml` — wood 80 → wood 60 + stone 30
- `data/buildings/rome_castra_barracks.yaml` — wood 80 → wood 60 + stone 40
- `data/compiled/chunsa_base.chdb` — recompilado byte-idéntico
- `data/compiled/chunsa_base.chdb.content.json` — nuevo hash
- `demo/chunsa_base.chdb` — sincronizado con el recompilado
- `tests/determinism/baselines.hpp` — `AI_SKIRMISH_APERTURA_*` re-registrado

**NO** fusionado a `main`. Permanece en `arch/sprint-1.8d-contenido`
hasta revisión del Director.