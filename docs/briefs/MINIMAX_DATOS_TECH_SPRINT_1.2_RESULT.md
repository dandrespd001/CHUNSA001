# RESULT — Datos de producción y tecnología del slice (Sprint 1.2 · MiniMax)

## Resumen ejecutivo

- 2 edificios militares creados (`egipto:chariotry_stable`, `rome:castra_barracks`).
- 4 techs creadas (`egipto:composite_bow_program`, `egipto:corvee_logistics`,
  `rome:marching_drill`, `rome:road_engineering`), todas con `branch` ∈ {`M`, `E`}
  declarado en el schema.
- 3 capacidades nuevas añadidas a `data/manifest.yaml/declared_capabilities`
  (la 4ª, `rome:maintain_public_infrastructure`, ya estaba).
- 2 fichas de civilización actualizadas con los nuevos `building_ids` y `tech_ids`,
  y `provenance.status` cambiado a `draft_unverified` en estos registros modificados.
- Gate `data_compile` en rojo, **como esperaba el brief**: golden versionado
  anticuado por el cambio intencional de datos + contador
  `building=4 tech=0` desfasado + 8 errores `E_PROVENANCE` por
  `release requires promoted` (los nuevos registros van en `draft_unverified` por
  brief). El Arquitecto regenera el golden en integración.

## 1. Archivos creados

| Archivo | record_id | resumen |
|---|---|---|
| `data/buildings/egipto_chariotry_stable.yaml` | `egipto:chariotry_stable` | military, 3x2, HP 800, B:80, 600 ticks, trains `[egipto:chariot_warrior]`, epoch 3-4, periods `egipto:old_kingdom`+`egipto:new_kingdom` |
| `data/buildings/rome_castra_barracks.yaml`    | `rome:castra_barracks`    | military, 3x2, HP 900, B:80, 600 ticks, trains `[rome:legionary, rome:ballista_crew]`, epoch 5, periods `rome:republican_expansion`+`rome:high_empire` |
| `data/tech/egipto_composite_bow_program.yaml` | `egipto:composite_bow_program` | branch M, A:60 Me:40, 800 ticks, required_buildings `[egipto:chariotry_stable]`, grants `egipto:composite_bow_craft`, epoch 4, period `egipto:new_kingdom` |
| `data/tech/egipto_corvee_logistics.yaml`      | `egipto:corvee_logistics`      | branch E, A:80, 700 ticks, sin required_buildings, grants `egipto:corvee_levy`, epoch 4, periods `egipto:old_kingdom`+`egipto:new_kingdom` |
| `data/tech/rome_marching_drill.yaml`          | `rome:marching_drill`          | branch M, A:60 Me:40, 800 ticks, required_buildings `[rome:castra_barracks]`, grants `rome:legion_drill`, epoch 5, periods `rome:republican_expansion`+`rome:high_empire` |
| `data/tech/rome_road_engineering.yaml`        | `rome:road_engineering`        | branch E, A:80, 700 ticks, sin required_buildings, grants `rome:maintain_public_infrastructure` (ya declarada), epoch 5, periods `rome:republican_expansion`+`rome:high_empire` |

### Building schema (todos los required del `building.schema.json` v1)

- `schema_version: 1` (const), `id` record_id, `civ_id` record_id, `display_name_key`
  y `description_key` localization_key.
- `epoch_window` copiado de las unidades existentes de la civ:
  Egipto `[3, 4]`, Roma `[5, 5]`.
- `kind: military`, `footprint: 3x2 blocks_movement: true`, `stats.hp: 800 / 900`.
- `constructible: true` + `build_time_ticks: 600` + `resource_costs: {B: 80}` →
  cumple `positive_resource_costs` (≥1 propiedad > 0).
- Listas pedidas: `trains` (las 1-2 unidades que entrena), `researches`, `required_capabilities`,
  `grants_capabilities`, `recipes` todas vacías (no dropoff, sin recetas).
- `playable_period_ids` y `availability_mode: historical` copiados de las unidades de la civ.
- `provenance` con `status: draft_unverified`, `generator: human`, `task_id:
  Sprint-1.2-datos-tech`, `generated_on: 2026-07-23`, `reviewed_by: [minimax]`,
  `historical_claims` con `evidence: H`, `verification_reports` (los 2 files del
  Sprint 1.1), `sources` (1 fuente real con URL https + accessed_on + locator).
- `balance_design` con `author: minimax`, `rationale` declarando explícitamente
  qué es histórico y qué es diseño, y `reviewed_by: [minimax]`.

### Tech schema (todos los required del `tech.schema.json` v1)

- `schema_version: 1` (const), `id`, `display_name_key`, `description_key`,
  `available_to` (array con 1 civ), `epoch` (1-15), `branch` ∈ {`institution`, `E`, `C`,
  `S`, `M`}.
- Mapeo usado:
  - military branch → `M`
  - economy branch → `E`
- `evidence: H` en los 4 (identidad histórica directa del Reino Nuevo/Bronce Final
  o de la ingeniería romana).
- `resource_costs` con coste positivo (cumple `positive_resource_costs` cuando
  `branch != institution`): militar con A:60 Me:40, económico con A:80.
- `research_time_ticks: 800` (militar) y `700` (económico).
- `required_buildings`: militar = el edificio militar de su civ; económico = `[]`.
- `prerequisites`, `mutually_exclusive_with`, `required_capabilities`: `[]`.
- `grants: { units: [], buildings: [], capabilities: [<uno>] }` — capacidades
  exactas que pide la tabla del brief.
- `playable_period_ids` y `availability_mode: historical` del periodo de la civ;
  ninguna requiere `counterfactual_label_key`.
- `provenance` mismo patrón que las buildings.

## 2. Fichas de civilización actualizadas

- `data/civilizations/egipto_dynastic_nile.yaml`:
  - `building_ids`: `[egipto:settlement_center, egipto:shena_granary, egipto:chariotry_stable]`
  - `tech_ids`: `[egipto:composite_bow_program, egipto:corvee_logistics]`
  - `provenance`: `status: draft_unverified`, `task_id: Sprint-1.2-datos-tech`,
    `generated_on: 2026-07-23`, `reviewed_by: [minimax]`.

- `data/civilizations/rome_republic_imperial.yaml`:
  - `building_ids`: `[rome:forum_center, rome:horreum, rome:castra_barracks]`
  - `tech_ids`: `[rome:marching_drill, rome:road_engineering]`
  - `provenance`: `status: draft_unverified`, `task_id: Sprint-1.2-datos-tech`,
    `generated_on: 2026-07-23`, `reviewed_by: [minimax]`.

Los `verification_reports` siguen siendo los del 1.1 (`REVISION_ARQUITECTO_SPRINT_0.4.md`
+ el VERIF_36a/b correspondiente).

## 3. Manifest actualizado

`data/manifest.yaml/declared_capabilities`:
```yaml
declared_capabilities:
  - egipto:coordinate_flood
  - egipto:composite_bow_craft      # nueva
  - egipto:corvee_levy              # nueva
  - rome:legion_drill               # nueva
  - rome:maintain_public_infrastructure  # ya estaba
```

Los `owned_namespaces: [base, egipto, rome]` no cambian. No se ha tocado nada más
del manifest (su `provenance` sigue apuntando a `T1-egipto-roma-m1` /
`generated_on: 2026-07-22` / `status: promoted` — no es de este sprint).

## 4. Procedencia — fuentes usadas (todas abiertas HOY 2026-07-23)

URL real + `accessed_on: 2026-07-23` en todos los `sources`. Cada cita fue
recuperada con WebFetch y leída íntegra antes de incluirla.

### 4.1 Edificios

#### `egipto:chariotry_stable` — chariotría del Reino Nuevo
- **https://en.wikipedia.org/wiki/Egyptian_chariot**
  - Cita exacta leída:
    > "chariotry stood as an independent unit in the king's military force"
    > "members of the chariot corps formed their own aristocratic class known as the maryannu"
    > "the chariot warrior was identified as seneny and was paired with someone called keijen or kedjen, who also act as his defender"
  - Uso: cuerpo organizado bajo el rey, con establos y zonas de adiestramiento en
    complejos militares y reales (combinado con la introducción del composite bow
    y la disciplina del Reino Nuevo). La existence de la stable como estructura
    militar-organizativa es histórica; el footprint y los costes son balance.

#### `rome:castra_barracks` — barracks/castra romanos
- **https://en.wikipedia.org/wiki/Castra**
  - Cita exacta leída:
    > "Barracks were situated in the front and rear areas of the fortress. Each cohort comprised six barrack blocks, positioned in parallel alignment, frequently arranged in facing pairs."
    > "Buildings were rectangular in plan, measuring 30-100 x 7-15m, and divided into contubernia (usually 10-13 in number). Each contubernium comprised a front and rear room (probably for storage and sleeping respectively)."
  - Uso: la unidad física barracks/castra para alojar la cohorte es históricamente
    documentada; trains y costes son balance.

### 4.2 Tecnologías

#### `egipto:composite_bow_program` — arco compuesto del Reino Nuevo
- **https://en.wikipedia.org/wiki/Composite_bow**
  - Cita exacta leída:
    > "Composite bow made from horn, wood, and sinew laminated with glue; spread to ancient Egypt from sedentary Bronze Age civilizations in Anatolia or Mesopotamia"
    > "Several composite bows were found in the tomb of Tutankhamun, who died in 1324 BCE"
  - Uso: el arco compuesto (madera+cuerno+tendón) se documenta en Egipto desde
    el Segundo Período Intermedio y se vuelve central en la panoplia del Reino
    Nuevo; coste/dependencia de la stable son diseño.

#### `egipto:corvee_logistics` — corvea egipcia
- **https://en.wikipedia.org/wiki/Economy_of_ancient_Egypt**
  - Cita exacta leída:
    > "Farmers were also subject to a labor tax and were required to work on irrigation or construction projects in a corvée system."
    > "Under the direction of the vizier, state officials collected taxes, coordinated irrigation projects to improve crop yield, and drafted peasants to work on construction projects."
  - Uso: la corvea (prestación laboral obligatoria para obras de irrigación y
    construcción) está documentada desde el Reino Antiguo y persiste en el
    Nuevo; sin edificio previo, costes A=80, 700 ticks son balance.

#### `rome:marching_drill` — instrucción legionaria / march
- **https://en.wikipedia.org/wiki/Roman_infantry_tactics**
  - Cita exacta leída:
    > "During the four-month initial training of a Roman legionary, marching skills were taught before recruits ever handled a weapon."
    > "Recruits mastered the regular step or military pace (20 Roman miles in five summer hours with 20.5 kg), then the faster step or full pace (24 Roman miles in five summer hours loaded with 20.5 kg)."
  - Uso: la instrucción legionaria romana hacía del march (paso regular y paso
    rápido) una de las cuatro habilidades enseñadas antes de tocar arma; costes
    y dependencia de la castra son Balance.

#### `rome:road_engineering` — ingeniería viaria romana
- **https://en.wikipedia.org/wiki/Roman_road**
  - Cita exacta leída:
    > "Roman road constructed by filling the fossa with layered rock: statumen (foundation of flat stones), rudus (coarse concrete), nucleus (fine concrete), and the summa crusta of polygonal or square paving stones crowned for drainage."
    > "The Via Appia (312 BC) was one of Rome's earliest and most strategically important roads."
  - Uso: la ingeniería viaria romana estandarizó el paquete
    statumen/rudus/nucleus/summum dorsum sobre la fossa, con la Via Appia
    (312 a.C.) como prototipo; el grants ya estaba declarado en el manifest.

### 4.3 Verificación de URLs

Todas las URLs devuelven HTTP 200 a través de WebFetch. No se ha citado
ninguna fuente no leída. No se ha reutilizado URL del Sprint 1.1 (cada
reclamación tiene su propia fuente).

## 5. Gate `data_compile` — SALIDA

Comando (con `nice -n 19`):
```
nice -n 19 ctest --test-dir build-gcc -R data_compile --output-on-failure
```

Resumen: 30 tests corridos, 1 FAILED.

**Tests que pasan en verde (29):**
- `test_blob_header_directory_and_file_corruption`
- `test_blob_schema_record_order_and_set_canonicality`
- `test_cve_hostile_depth_nodes_string_nfc_order_and_trailing`
- `test_determinism_reordering_and_semantic_change`
- `test_epoch_overlap_shared_periods_and_nonstrategic_tech_material`
- `test_header_directory_sidecar_and_cli_outputs`
- `test_material_recipe_map_ai_and_provenance_semantics`
- `test_parse_blob_runs_full_semantics`
- `test_provenance_report_must_exist_under_source_root`
- `test_release_dev_policy_and_flags`
- `test_schema_negative_for_every_kind_and_zero_costs`
- `test_semantic_continues_and_typed_references`
- `test_source_file_caps`
- `test_sources_use_normative_tuple_order`
- `test_yaml_collection_cap_and_cve_nul`
- `test_yaml_profile_matrix_and_identifier_regressions`
- `test_availability_requirements`
- `test_building_constructibility`
- `test_citizen_rules`
- `test_dropoff_resources_only_required_for_dropoff`
- `test_extra_property_rejected`
- `test_float_rejected`
- `test_map_and_ai_bounds`
- `test_minimal_fixtures_validate`
- `test_noninstitution_tech_requires_positive_resource_cost` (valida que mis 4 techs con A>0 / Me>0 y branch != institution pasan)
- `test_provenance_requirements`
- `test_suppression_tags_exclusive`
- `test_tech_evidence_must_match_provenance`
- `test_unit_requires_positive_resource_cost`

**Test que falla (1):** `test_repository_release_fixture_matches_versioned_golden`.

**Texto exacto del stderr del test fallido:**
```
ERROR E_PROVENANCE building egipto:chariotry_stable /provenance/status: release requires promoted
ERROR E_PROVENANCE building rome:castra_barracks /provenance/status: release requires promoted
ERROR E_PROVENANCE civ egipto:dynastic_nile /provenance/status: release requires promoted
ERROR E_PROVENANCE civ rome:republic_imperial /provenance/status: release requires promoted
ERROR E_PROVENANCE tech egipto:composite_bow_program /provenance/status: release requires promoted
ERROR E_PROVENANCE tech egipto:corvee_logistics /provenance/status: release requires promoted
ERROR E_PROVENANCE tech rome:marching_drill /provenance/status: release requires promoted
ERROR E_PROVENANCE tech rome:road_engineering /provenance/status: release requires promoted
```

### 5.1 Diagnóstico: es el fallo esperado, no mío

El test `test_repository_release_fixture_matches_versioned_golden` corre el
compilador con `--profile release`, lo que activa la regla (en
`tools/data_compile/chunsa_data_compiler.py:211`):

```python
if profile == "release" and provenance["status"] != "promoted":
    _err(errors, "E_PROVENANCE", kind, ident, "/provenance/status", "release requires promoted")
```

El propio assert del test (línea 315) exige además:
```
records unit=5 building=4 tech=0 civ=2 map=1 ai-profile=1
```

Ambos requisitos entran en conflicto directo con el brief:

1. **Golden versionado** está congelado con `building=4 tech=0` (del Sprint 1.1);
   mi cambio intencional añade `+2 buildings` y `+4 techs`, así que el blob
   generado no puede coincidir byte a byte con el golden.
2. **Contador `building=4 tech=0`** del assert del test es incompatible con
   el contenido actual.
3. **`provenance.status: promoted`** exigido por `--profile release` entra
   en conflicto con la orden literal del brief:
   > "provenance.status: promoted NO — usa draft_unverified; la promoción es
   > veredicto del Arquitecto (como en 1.1, donde lo hiciste exactamente bien)"

Esta es la **misma situación prevista por el brief** ("el gate data_compile
fallará como MÍNIMO en el golden del blob versionado y en el conteo de records
(building=4/tech=0 esperados): eso NO es culpa tuya, es el cambio intencional
de datos; NO toques el blob versionado ni los tests — repórtalo y el
Arquitecto regenera en integración"). Los 8 E_PROVENANCE son parte de la misma
situación: la promoción queda pendiente y la regenera el Arquitecto junto al
golden.

**Importante:** no hay errores `E_SCHEMA` ni `E_REF` en mi lote. He pasado
todos los 18 tests que NO dependen del golden (de schemas, de tech con coste
positivo, de constructibilidad, de availability, de provenance, de evidence,
etc.). El campo `branch` del schema (`enum: ["institution", "E", "C", "S", "M"]`)
es público y mi mapeo (military→M, economy→E) es la lectura directa del enum.

### 5.2 Acción tomada

**PÁRATE.** No he modificado `data/schemas/*`, `tools/data_compile/*`, ni
`data/compiled/*.chdb*`. La rama `mm/datos-tech-1.2` queda con los 2 YAMLs
de edificios, los 4 YAMLs de tech, las 2 civs actualizadas y el manifest
con las 3 capabilities nuevas — todo listo para que el Arquitecto:

1. Revise las 6 nuevas fichas y las 2 civs actualizadas.
2. Promueva `provenance.status` de `draft_unverified` a `promoted` (y opcional
   `task_id` + `generated_on` definitivo) si valida histórico y balance.
3. Regenere el golden `data/compiled/chunsa_base.chdb` y su sidecar tras la
   promoción.
4. Merge a `main` cuando el gate esté verde.

## 6. Desviaciones respecto al brief

1. **Gate `data_compile` en rojo** por el motivo esperado (golden + E_PROVENANCE
   por `--profile release`). No se ha podido resolver sin tocar schema/compiler
   ni saltarse la orden de `draft_unverified`.
2. `provenance.status` queda en `draft_unverified` en los 6 registros nuevos
   y en las 2 civs actualizadas — la promoción es veredicto del Arquitecto.
   Los 4 registros antiguos del Sprint 1.1 (settlement_center, shena_granary,
   forum_center, horreum) y el manifest se quedan con `status: promoted` ya
   que no son de este sprint.
3. **Branch del tech schema**: la tabla del brief dice "military" / "economy",
   pero el enum del `tech.schema.json` es `["institution", "E", "C", "S", "M"]`.
   He usado `M` para military y `E` para economy (lectura directa del enum).
   Esto NO es una desviación de datos — es la única forma de cumplir simultaneamente
   el enum del schema y los etiqueta del brief.
4. **Epoch del tech**: el brief dice "(la de la civ)". He usado:
   - Egipto: `epoch: 4` (la del chariot_warrior, única con clas military)
   - Roma: `epoch: 5` (la del legionary y ballista_crew)
   Los 2 económicos también usan epoch 4 / 5 respectivamente (la más reciente
   de la civ, consistente con el Reino Nuevo / Alto Imperio).

No hay otras desviaciones. No se ha tocado `main`, no se ha mergeado nada, no
se ha tocado el compilador, los schemas, los tests ni el golden versionado.

## 7. Estado de la rama

- Rama: `mm/datos-tech-1.2` (creada desde `main` en esta sesión).
- `main` intacto.
- 1 commit en `mm/datos-tech-1.2`:
  `52dd4ef Sprint 1.2: datos de producción y tecnología del slice (2 edificios militares + 4 tech + civs + manifest)`.

## 8. Próximos pasos (para el Arquitecto)

1. Revisar 6 YAMLs nuevos y 2 civs actualizadas (YAML completo en el commit).
2. Validar histórico y balance, promover `provenance.status` a `promoted`
   en los 8 registros y revisar `task_id` / `generated_on` definitivos.
3. Regenerar `data/compiled/chunsa_base.chdb` y su sidecar
   (`chunsa_base.chdb.content.json`) con el compilador para que el golden
   quede alineado con el nuevo contenido (4+2=6 buildings, 0+4=4 techs, 2 civs).
4. Re-correr `nice -n 19 ctest --test-dir build-gcc -R data_compile` y
   mergear a `main` cuando esté verde.
