# Brief cerrado — schemas JSON de SPEC-002 v1.0

## Alcance

Genera exclusivamente los ocho schemas de `data/schemas/` y `tools/data_compile/test_schemas.py`. No modifiques compilador, CMake, datos, kernel, docs ni dependencias.

Todos son JSON Schema Draft 2020-12, `$id` local `chunsa/<nombre>.schema.json`, objetos cerrados con `additionalProperties:false`, rangos literales y `$ref` relativos solo a `common.schema.json`. No uses red ni `format` que requiera plugins.

```text
common.schema.json  manifest.schema.json  unit.schema.json
building.schema.json  tech.schema.json  civ.schema.json
map.schema.json  ai-profile.schema.json
```

## Tipos comunes

- `record_id`: `^[a-z][a-z0-9_]{0,31}:[a-z][a-z0-9_]{0,63}$`.
- `package_id`: `^[a-z][a-z0-9_.-]{0,63}$`.
- `localization_key`: `^[a-z][a-z0-9_]{0,31}:[a-z][a-z0-9_.]{0,127}$`.
- `snake_tag`: `^[a-z][a-z0-9_]{0,63}$`.
- `iso_date`: string `^\d{4}-\d{2}-\d{2}$`.
- epoch: integer 1..15; epoch_window: array exactamente 2 epochs.
- year: integer distinto de 0.
- resource: enum `A,B,P,W,Me,F,I,El`.
- resource_costs: objeto cerrado con esas ocho propiedades opcionales, valores integer 0..1000000, maxProperties 8. Permite vacío; cada caller fija minProperties.
- material_cost: objeto cerrado requerido `material_id` record_id y `amount` integer 1..1000000. material_costs: array uniqueItems.
- Todo set usa `uniqueItems:true`.

Disponibilidad son propiedades top-level de unit/building/tech: `playable_period_ids` set no vacío de record_id; `availability_mode` historical|counterfactual; `counterfactual_label_key` prohibido en historical y obligatorio/localization_key en counterfactual.

`CommonProvenanceV1` es objeto cerrado requerido:

```text
status draft_unverified|verified|promoted
generator string 1..64; task_id string 1..128; generated_on iso_date
reviewed_by set no vacío string 1..64
historical_claims objeto cerrado requerido evidence,verification_reports,sources
balance_design objeto cerrado requerido author string 1..64,
  rationale string 1..4096, reviewed_by set no vacío string 1..64
```

`historical_claims.evidence` es H|C|P|X|N. `verification_reports` es set de strings no vacías. `sources` es set de objetos cerrados: citation requerida 1..2048; locator opcional 1..512; url opcional `^https://` max2048; accessed_on iso_date obligatorio si url y prohibido sin url. H/C exige sources e informes minItems1 y prohíbe evidence_label_key. P/X exige ambos minItems1 y `evidence_label_key` localization. N exige ambos maxItems0 y prohíbe label.

## Manifest v1

Objeto cerrado, todos requeridos: schema_version const1; package_id; package_version SemVer sin build metadata pattern `^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-[0-9A-Za-z.-]+)?$`; package_kind base|mod; owned_namespaces set no vacío de `^[a-z][a-z0-9_]{0,31}$`; declared_capabilities/declared_behaviors/declared_variant_groups sets record_id; declared_materials set de objetos cerrados requeridos `{id:record_id,name_key:localization_key,kind:deposit|intermediate,resource,strategic:bool}`; dependencies set de objetos cerrados requeridos `{package_id,version_constraint:string 1..128}`; load_after set package_id; provenance. Si kind=base, dependencies/load_after maxItems0.

## Unit v2

Objeto cerrado. Requeridos salvo material_costs, bonus_vs_bp y label contrafactual: schema_version const2; id record_id; display_name_key/description_key localization; civ_id record_id; epoch_window; class `infantry|cavalry|artillery|citizen|siege|naval_light`; tags set de `can_take_cover|formation_capable|suppression_resist_low|suppression_resist_high|drop_off_carrier`; resource_costs minProperties1; material_costs; disponibilidad; provenance.

Stats es objeto cerrado con todos requeridos: hp 1..1000000; attack 0..1000000; range_millitiles 0..100000; speed_millitile_tick 1..100000; morale 0..100; build_time_ticks 1..1000000. bonus_vs_bp es objeto cerrado con las seis classes como claves opcionales y valores -10000..10000. Citizen exige drop_off_carrier, attack=0 y range=0; las otras clases attack>=1. Los tags suppression low/high son mutuamente excluyentes.

## Building v1

Objeto cerrado. Requeridos: schema_version const1; id/civ_id record_id; display_name_key/description_key localization; epoch_window; kind `economic|military|civic|infrastructure|dropoff|research|housing|wonder`; footprint cerrado width_cells,height_cells 1..32 y blocks_movement bool; stats cerrado hp 1..10000000; constructible bool; resource_costs; build_time_ticks; dropoff_resources set resource; trains/researches/required_capabilities/grants_capabilities sets record_id; recipes; disponibilidad; provenance. material_costs opcional.

Recipe es objeto cerrado requerido id, input_resource_costs, input_material_costs, output_material_id, output_amount 1..1000000, duration_ticks 1..10000000. Si dropoff, dropoff_resources minItems1. Si constructible=true: build time 1..10000000 y al menos un resource/material cost; si false: build time 0 y ambos costes vacíos.

## Tech v1

Objeto cerrado. Requeridos salvo material_costs, regional_variant_group y label contrafactual: schema_version const1; id; display/description; available_to set no vacío record_id; epoch; branch `institution|E|C|S|M`; evidence H|C|P|X; resource_costs (minProperties1 salvo institution); material_costs maxItems2; required_capabilities; research_time_ticks 1..10000000; prerequisites/required_buildings/mutually_exclusive_with sets record_id; grants cerrado requerido units/buildings/capabilities sets record_id; regional_variant_group record_id; disponibilidad; provenance. Con `if/then`, top-level evidence debe igualar historical_claims.evidence.

## Civ v1

Objeto cerrado, todos requeridos: schema_version const1; id; historical_window cerrado start_year/end_year; epoch_window; region_key/display_name_key/description_key localization; gameplay_verb cerrado id record_id,name_key/description_key localization; playable_periods array no vacío de objetos cerrados id,start_year,end_year,label_key; institutions array objetos cerrados id,name_key,mechanic_description_key,cost_or_tension_key,required_capability_ids set record_id; social_tensions array objetos cerrados id,name_key,description_key; unit_ids/building_ids/tech_ids sets record_id; art_rule_keys set no vacío localization; review_roles set no vacío string 1..64; provenance. Orden/solape/IDs son semánticos.

## Map v1

Objeto cerrado, todos requeridos: schema_version const1; id; display_name_key; width_tiles/height_tiles 1..8192; biome `plain|desert|arctic|tropical|island|archipelago|continental|basin|canyon|jungle|tundra`; terrain_rle no vacío de objetos cerrados terrain `plain|water|forest|hill|mountain|desert|swamp|jungle|snow|city`, run 1..4294967295; cost_rle igual con cost 1..255; resource_spawns array objetos cerrados kind resource|material,id string,x/y 0..2147483647,amount 1..1000000; starting_positions max8 objetos cerrados slot0..7,x/y mismo rango; recommended_epoch_window; provenance. Sumas RLE, tipo de id, bounds y orden son semánticos.

## AI profile v1

Objeto cerrado, todos requeridos: schema_version const1; id; display_name_key; personality `trader|warrior|technologist|balanced`; difficulty `apprentice|normal|hard|brutal`; strategic_weights_bp cerrado con exactamente economy_focus_bp,military_focus_bp,tech_focus_bp,expansion_aggressiveness_bp,risk_tolerance_bp,diplomacy_openness_bp enteros 0..10000; utility_curves array de objetos cerrados consideration snake_tag y points array 2..16 de objetos cerrados input_bp/output_bp 0..10000; tactical_behaviors array de objetos cerrados group_type `melee_line|ranged_line|siege_group|cavalry_wing|naval_fleet|air_wing|cyber_cell`, behavior_id record_id, seek_cover bool, suppression_response `hold|retreat|flank`, formation_preference `line|column|wedge|dispersed`, dos thresholds bp 0..10000; difficulty_params cerrado con decision/reaction/counter delays 1..1000000 y tres bp 0..10000; performance_lod cerrado min_decision_period_ticks/cache_ttl_ticks 1..1000000; provenance.

## Pruebas

`test_schemas.py` usa unittest + `jsonschema.Draft202012Validator`, sin red/subprocess/escritura. Carga los ocho schemas, `check_schema`, registry/resolver estrictamente local y fixtures mínimos válidos para manifest + seis records. Negativos: extra property; float; citizen sin tag/con attack; suppression tags juntos; historical con label; counterfactual sin label; P sin label; URL sin accessed_on; building no construible con coste; tech evidence mismatch; map run0; AI con un punto.

No implementes semantic pass global, no agregues paquetes, no cambies nombres/enums. Devuelve unified diff o `NO_CHANGE`.
