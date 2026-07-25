# RESULT — Datos de la apertura económica (MiniMax M3 · Sprint 1.6B)

**Rama**: `mm/datos-apertura-1.6b` (desde `main`).
**Commits**: este sprint se entrega en un único commit (ver pie).
**No mergeado**.

---

## Resumen ejecutivo

1. `data/maps/base_demo_desert_basin.yaml` → `resource_spawns` poblado con **12 depósitos**
   (8 de base espejados + 4 neutrales centrales), respetando el muro y la franja central.
2. `data/buildings/egipto_settlement_center.yaml` → `trains: [egipto:work_crew]`.
3. `data/buildings/rome_forum_center.yaml` → `trains: [rome:camp_work_crew]`.
4. Gate `data_compile`: **PASS en 29/30 tests**. El único fallo es
   `test_repository_release_fixture_matches_versioned_golden`, que es el **golden del blob
   versionado** (cambio intencional de datos — lo regenera el Arquitecto en integración).
   **Ningún error E_SCHEMA / E_REF / E_PROVENANCE de mis ediciones.**

---

## Tabla exacta de los 12 depósitos

Centro del mapa: `x = 128000` mili-tiles (128 · 1000). Starting positions: slot 0 = (20500, 128500),
slot 1 = (235500, 128500).

| # | id | x_millitiles | y_millitiles | amount | servicio        | simetría x_slot1 = 256000 − x_slot0 |
|---|----|-------------:|-------------:|-------:|-----------------|------------------------------------|
| 1 | A  |    28 500    |   122 500    |   500  | base slot 0     | ↔ #5 (227 500)                      |
| 2 | A  |    12 500    |   122 500    |   500  | base slot 0     | ↔ #6 (243 500)                      |
| 3 | B  |     8 500    |   136 500    |   500  | base slot 0     | ↔ #7 (247 500)                      |
| 4 | Me |    28 500    |   134 500    |   500  | base slot 0     | ↔ #8 (227 500)                      |
| 5 | A  |   227 500    |   122 500    |   500  | base slot 1     | espejo de #1                        |
| 6 | A  |   243 500    |   122 500    |   500  | base slot 1     | espejo de #2                        |
| 7 | B  |   247 500    |   136 500    |   500  | base slot 1     | espejo de #3                        |
| 8 | Me |   227 500    |   134 500    |   500  | base slot 1     | espejo de #4                        |
| 9 | A  |   114 000    |    88 000    |   800  | neutral central | ↔ #10 (142 000)                     |
|10 | A  |   142 000    |    88 000    |   800  | neutral central | espejo de #9                        |
|11 | B  |   118 000    |   176 000    |   800  | neutral central | ↔ #12 (138 000) par mixto B/Me     |
|12 | Me |   138 000    |   176 000    |   800  | neutral central | espejo de #11                       |

**Composición**: 2 A + 1 B + 1 Me por jugador (cumple); 2 A + 1 B + 1 Me en neutrales (cumple).

---

## Verificación de simetría (script python)

```
Per-player pairs (slot 0 base (20500,128500), slot 1 base (235500,128500)):
  pair 1 (A): slot0=( 28500,122500) a=500 | slot1=(227500,122500) a=500
      x_mirror=OK y_match=OK amt_match=OK type_match=OK dist_s0=10000mt/10.0t dist_s1=10000mt/10.0t
  pair 2 (A): slot0=( 12500,122500) a=500 | slot1=(243500,122500) a=500
      x_mirror=OK y_match=OK amt_match=OK type_match=OK dist_s0=10000mt/10.0t dist_s1=10000mt/10.0t
  pair 3 (B): slot0=(  8500,136500) a=500 | slot1=(247500,136500) a=500
      x_mirror=OK y_match=OK amt_match=OK type_match=OK dist_s0=14422mt/14.4t dist_s1=14422mt/14.4t
  pair 4 (Me):slot0=( 28500,134500) a=500 | slot1=(227500,134500) a=500
      x_mirror=OK y_match=OK amt_match=OK type_match=OK dist_s0=10000mt/10.0t dist_s1=10000mt/10.0t

Neutral pairs (must be in strip x ∈ [110000, 146000]):
  A pair:    (A)@(114000, 88000) a=800 | (A)@(142000, 88000) a=800
      x_mirror=OK y_match=OK amt_match=OK strip_ok=OK
  B/Me pair: (B)@(118000,176000) a=800 | (Me)@(138000,176000) a=800
      x_mirror=OK y_match=OK amt_match=OK strip_ok=OK
```

Todas las distancias a la base correspondiente caen en el rango pedido **8–20 tiles**:
- 3 pares a 10.0 t, 1 par a 14.4 t.

**Lectura del par B/Me neutro**: para mantener literalmente "1 de B, 1 de Me" en la franja central
y conservar la simetría por parejas, los dos neutros del par están en lados opuestos del eje
x = 128000 (B al oeste, Me al este), formando un par espejado mixto. Si el balance prefiere dos
pares puros (B↔B y Me↔Me) el Arquitecto lo cambia — el formato `resource_spawns` lo soporta sin
tocar schema.

---

## Verificación de muro

Regla del brief: muro vertical en tile x = 128 → prohibido `x_millitiles ∈ [127500, 128500]`
salvo `y_millitiles ∈ [124000, 132000]` (hueco).

```
Wall: x_tile=128, y ∈ [32000, 224000], hueco y ∈ [124000, 132000].
Forbidden: x_millitiles ∈ [127500, 128500] salvo y ∈ [124000, 132000].
  OK: no deposits fall in the wall (all clear of x ∈ [127500, 128500]).
```

Todos los x de los depósitos están fuera de la franja prohibida:

- Bases slot 0: x ∈ {8 500, 12 500, 28 500} (todos ≪ 127 500).
- Bases slot 1: x ∈ {227 500, 243 500, 247 500} (todos ≫ 128 500).
- Neutros: x ∈ {114 000, 118 000, 138 000, 142 000} (114 000 y 118 000 < 127 500; 138 000 y
  142 000 > 128 500).

Ningún depósito viola el muro.

---

## Gate `data_compile`

Comando: `nice -n 19 ctest --test-dir build-gcc -R data_compile --output-on-failure`.

Resumen: 30 tests ejecutados en 4.15 s. **29 OK, 1 FAIL esperado.**

**El único test que falla**:
```
FAIL: test_repository_release_fixture_matches_versioned_golden
  File "tools/data_compile/test_data_compiler.py", line 313, in test_repository_release_fixture_matches_versioned_golden
  AssertionError: b'CHNSDB1\x00\x01\x00\x00\x00\x01\x00\x00[64692 chars]\x00'
                 != b'CHNSDB1\x00\x01\x00\x00\x00\x01\x00\x00[60801 chars]\x00'
```

Esto es **exactamente** el golden del blob versionado: mi cambio intencional de
`resource_spawns` (de `[]` a 12 entradas) y de `trains` (de `[]` a 1 entrada por centro)
altera el blob compilado, así que el fixture dorado queda desfasado. Lo regenera el Arquitecto
en integración, como dice el brief.

**Tests PASS relevantes** (muestran que no hay error de schema/referencias/provenance en MIS
ediciones):

- `test_blob_schema_record_order_and_set_canonicality` — schema OK
- `test_parse_blob_runs_full_semantics` — parseo OK
- `test_semantic_continues_and_typed_references` — referencias tipadas OK
- `test_provenance_report_must_exist_under_source_root` — provenance OK
- `test_material_recipe_map_ai_and_provenance_semantics` — provenance OK
- `test_minimal_fixtures_validate` — fixtures OK
- `test_map_and_ai_bounds` — bounds OK (x/y/amount dentro de rango)
- `test_building_constructibility` — los centros no son `constructible: true`, así que el cambio
  en `trains` no viola la regla "constructible ⇒ cost > 0".
- `test_extra_property_rejected`, `test_float_rejected`, `test_yaml_collection_cap_and_cve_nul` —
  YAML limpio.
- `test_epoch_overlap_shared_periods_and_nonstrategic_tech_material`, `test_tech_evidence_must_
  match_provenance`, `test_availability_requirements`, `test_citizen_rules`, `test_dropoff_
  resources_only_required_for_dropoff`, `test_noninstitution_tech_requires_positive_resource_
  cost`, `test_unit_requires_positive_resource_cost`, `test_suppression_tags_exclusive` —
  contratos de schema OK.

**No hay errores E_SCHEMA / E_REF / E_PROVENANCE introducidos por estas ediciones.**

---

## Verificación de `record_id` de los aldeanos

Comprobados contra `data/units/` (existen como fichero):

- `egipto:work_crew` → `data/units/egipto_work_crew.yaml` (`id: egipto:work_crew`,
  `class: citizen`, `tags: [drop_off_carrier]`). ✓
- `rome:camp_work_crew` → `data/units/rome_camp_work_crew.yaml` (`id: rome:camp_work_crew`,
  `class: citizen`, `tags: [drop_off_carrier]`). ✓

Ambos nombres coinciden con lo indicado en el brief; no hubo renombrado.

---

## Cambios fuera de scope

**Ninguno**. No toqué código C++, schemas, compilador ni `historical_claims`. Tampoco modifiqué
el `status: promoted` del mapa (sigue como el arquitecto lo dejó); solo actualicé `generated_on`
a `2026-07-24` y añadí una línea a `balance_design.rationale` describiendo la simetría, como
indica el brief.

---

## Procedencia (ADR-014)

Los `resource_spawns` y los `trains` son **diseño de escenario/balance**, no afirmaciones
históricas: no requieren fuentes nuevas ni tocar `historical_claims`. El `evidence: N` del mapa
se mantiene (mapa sintético de prueba).

---

## Diff resumido

```
 data/buildings/egipto_settlement_center.yaml |  3 +-
 data/buildings/rome_forum_center.yaml        |  3 +-
 data/maps/base_demo_desert_basin.yaml        | 69 ++++++++++++++++++++++++++--
 3 files changed, 70 insertions(+), 5 deletions(-)
```

---

## Commit

```
$ git log -1 --format='%h %s'
<hash> feat(datos): Sprint 1.6B — 12 resource_spawns simétricos + trains de aldeano en centros
```
