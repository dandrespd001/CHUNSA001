# Resultado Sprint 1.8B — reconciliación del esquema de recursos

## Resultado

Se ejecutó el brief `SOL_1.8B_ESQUEMA_RECURSOS.md` contra el contrato
normativo de `SPEC-007 §18`:

- el enum almacenable de ocho letras desaparece;
- `material_cost` y `material_costs` desaparecen de todos los schemas;
- los recursos authored usan `record_id` namespaced;
- el compilador, no el YAML, asigna índices;
- `chunsa:food`, `chunsa:wood` y `chunsa:stone` conservan exactamente los
  índices `0`, `1` y `2`;
- una referencia inexistente y un recurso duplicado producen errores con
  código;
- el blob es reproducible aunque cambie el orden de descubrimiento de los
  ficheros.

No se añadió ningún recurso jugable. Las trayectorias permanecen idénticas:
apertura termina en `9317` y eco en `1107`.

## Evidencia TDD

### ROJO — pruebas escritas y ejecutadas antes de implementar

El único cambio presente al ejecutar esta fase era la nueva clase
`ResourceReconciliationTests`. El schema, los datos, el compilador y el loader
seguían en su estado anterior.

Comando:

```text
PYTHONDONTWRITEBYTECODE=1 /usr/bin/python3 -m unittest tools.data_compile.test_data_compiler.ResourceReconciliationTests -v
```

Salida roja literal capturada (`exit_code=1`):

```text
test_common_schema_has_no_legacy_resource_enum (tools.data_compile.test_data_compiler.ResourceReconciliationTests.test_common_schema_has_no_legacy_resource_enum) ... FAIL
test_duplicate_resource_id_is_coded_load_error (tools.data_compile.test_data_compiler.ResourceReconciliationTests.test_duplicate_resource_id_is_coded_load_error) ... FAIL
test_material_cost_vocabulary_is_absent_from_every_schema (tools.data_compile.test_data_compiler.ResourceReconciliationTests.test_material_cost_vocabulary_is_absent_from_every_schema) ... FAIL
test_namespaced_resource_validates_against_resource_schema (tools.data_compile.test_data_compiler.ResourceReconciliationTests.test_namespaced_resource_validates_against_resource_schema) ... FAIL
test_repository_declares_three_resources_at_indices_zero_one_two (tools.data_compile.test_data_compiler.ResourceReconciliationTests.test_repository_declares_three_resources_at_indices_zero_one_two) ... FAIL
test_repository_resource_references_are_namespaced (tools.data_compile.test_data_compiler.ResourceReconciliationTests.test_repository_resource_references_are_namespaced) ... FAIL
test_resource_indices_do_not_depend_on_file_order (tools.data_compile.test_data_compiler.ResourceReconciliationTests.test_resource_indices_do_not_depend_on_file_order) ... FAIL
test_two_compilations_are_byte_identical (tools.data_compile.test_data_compiler.ResourceReconciliationTests.test_two_compilations_are_byte_identical) ... FAIL
test_unknown_resource_reference_is_coded_load_error (tools.data_compile.test_data_compiler.ResourceReconciliationTests.test_unknown_resource_reference_is_coded_load_error) ... FAIL

======================================================================
FAIL: test_common_schema_has_no_legacy_resource_enum (tools.data_compile.test_data_compiler.ResourceReconciliationTests.test_common_schema_has_no_legacy_resource_enum)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/home/adquiod/Imágenes/Project/CHUNSA001/tools/data_compile/test_data_compiler.py", line 506, in test_common_schema_has_no_legacy_resource_enum
    self.assertNotIn(
    ~~~~~~~~~~~~~~~~^
        "resource",
        ^^^^^^^^^^^
        common["$defs"],
        ^^^^^^^^^^^^^^^^
        "the eight-letter storable-resource enum must be removed",
        ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    )
    ^
AssertionError: 'resource' unexpectedly found in {'record_id': {'type': 'string', 'pattern': '^[a-z][a-z0-9_]{0,31}:[a-z][a-z0-9_]{0,63}$'}, 'package_id': {'type': 'string', 'pattern': '^[a-z][a-z0-9_.-]{0,63}$'}, 'namespace': {'type': 'string', 'pattern': '^[a-z][a-z0-9_]{0,31}$'}, 'localization_key': {'type': 'string', 'pattern': '^[a-z][a-z0-9_]{0,31}:[a-z][a-z0-9_.]{0,127}$'}, 'snake_tag': {'type': 'string', 'pattern': '^[a-z][a-z0-9_]{0,63}$'}, 'iso_date': {'type': 'string', 'pattern': '^\\d{4}-\\d{2}-\\d{2}$'}, 'epoch': {'type': 'integer', 'minimum': 1, 'maximum': 15}, 'epoch_window': {'type': 'array', 'minItems': 2, 'maxItems': 2, 'items': {'$ref': '#/$defs/epoch'}}, 'year': {'type': 'integer', 'not': {'const': 0}}, 'resource': {'enum': ['A', 'B', 'P', 'W', 'Me', 'F', 'I', 'El']}, 'resource_costs': {'type': 'object', 'additionalProperties': False, 'maxProperties': 8, 'properties': {'A': {'type': 'integer', 'minimum': 0, 'maximum': 1000000}, 'B': {'type': 'integer', 'minimum': 0, 'maximum': 1000000}, 'P': {'type': 'integer', 'minimum': 0, 'maximum': 1000000}, 'W': {'type': 'integer', 'minimum': 0, 'maximum': 1000000}, 'Me': {'type': 'integer', 'minimum': 0, 'maximum': 1000000}, 'F': {'type': 'integer', 'minimum': 0, 'maximum': 1000000}, 'I': {'type': 'integer', 'minimum': 0, 'maximum': 1000000}, 'El': {'type': 'integer', 'minimum': 0, 'maximum': 1000000}}}, 'positive_resource_costs': {'allOf': [{'$ref': '#/$defs/resource_costs'}, {'minProperties': 1}, {'anyOf': [{'required': ['A'], 'properties': {'A': {'minimum': 1}}}, {'required': ['B'], 'properties': {'B': {'minimum': 1}}}, {'required': ['P'], 'properties': {'P': {'minimum': 1}}}, {'required': ['W'], 'properties': {'W': {'minimum': 1}}}, {'required': ['Me'], 'properties': {'Me': {'minimum': 1}}}, {'required': ['F'], 'properties': {'F': {'minimum': 1}}}, {'required': ['I'], 'properties': {'I': {'minimum': 1}}}, {'required': ['El'], 'properties': {'El': {'minimum': 1}}}]}]}, 'material_cost': {'type': 'object', 'additionalProperties': False, 'required': ['material_id', 'amount'], 'properties': {'material_id': {'$ref': '#/$defs/record_id'}, 'amount': {'type': 'integer', 'minimum': 1, 'maximum': 1000000}}}, 'material_costs': {'type': 'array', 'uniqueItems': True, 'items': {'$ref': '#/$defs/material_cost'}}, 'record_id_set': {'type': 'array', 'uniqueItems': True, 'items': {'$ref': '#/$defs/record_id'}}, 'resource_set': {'type': 'array', 'uniqueItems': True, 'items': {'$ref': '#/$defs/resource'}}, 'availability': {'type': 'object', 'additionalProperties': False, 'required': ['playable_period_ids', 'availability_mode'], 'properties': {'playable_period_ids': {'type': 'array', 'minItems': 1, 'uniqueItems': True, 'items': {'$ref': '#/$defs/record_id'}}, 'availability_mode': {'enum': ['historical', 'counterfactual']}, 'counterfactual_label_key': {'$ref': '#/$defs/localization_key'}}, 'allOf': [{'if': {'properties': {'availability_mode': {'const': 'historical'}}}, 'then': {'not': {'required': ['counterfactual_label_key']}}}, {'if': {'properties': {'availability_mode': {'const': 'counterfactual'}}}, 'then': {'required': ['counterfactual_label_key']}}]}, 'provenance': {'type': 'object', 'additionalProperties': False, 'required': ['status', 'generator', 'task_id', 'generated_on', 'reviewed_by', 'historical_claims', 'balance_design'], 'properties': {'status': {'enum': ['draft_unverified', 'verified', 'promoted']}, 'generator': {'type': 'string', 'minLength': 1, 'maxLength': 64}, 'task_id': {'type': 'string', 'minLength': 1, 'maxLength': 128}, 'generated_on': {'$ref': '#/$defs/iso_date'}, 'reviewed_by': {'type': 'array', 'minItems': 1, 'uniqueItems': True, 'items': {'type': 'string', 'minLength': 1, 'maxLength': 64}}, 'historical_claims': {'$ref': '#/$defs/historical_claims'}, 'balance_design': {'$ref': '#/$defs/balance_design'}}}, 'historical_claims': {'type': 'object', 'additionalProperties': False, 'required': ['evidence', 'verification_reports', 'sources'], 'properties': {'evidence': {'enum': ['H', 'C', 'P', 'X', 'N']}, 'evidence_label_key': {'$ref': '#/$defs/localization_key'}, 'verification_reports': {'type': 'array', 'uniqueItems': True, 'items': {'type': 'string', 'minLength': 1}}, 'sources': {'type': 'array', 'uniqueItems': True, 'items': {'$ref': '#/$defs/source'}}}, 'allOf': [{'if': {'properties': {'evidence': {'enum': ['H', 'C']}}}, 'then': {'properties': {'verification_reports': {'minItems': 1}, 'sources': {'minItems': 1}}, 'not': {'required': ['evidence_label_key']}}}, {'if': {'properties': {'evidence': {'enum': ['P', 'X']}}}, 'then': {'required': ['evidence_label_key'], 'properties': {'verification_reports': {'minItems': 1}, 'sources': {'minItems': 1}}}}, {'if': {'properties': {'evidence': {'const': 'N'}}}, 'then': {'properties': {'verification_reports': {'maxItems': 0}, 'sources': {'maxItems': 0}}, 'not': {'required': ['evidence_label_key']}}}]}, 'source': {'type': 'object', 'additionalProperties': False, 'required': ['citation'], 'properties': {'citation': {'type': 'string', 'minLength': 1, 'maxLength': 2048}, 'locator': {'type': 'string', 'minLength': 1, 'maxLength': 512}, 'url': {'type': 'string', 'pattern': '^https://', 'maxLength': 2048}, 'accessed_on': {'$ref': '#/$defs/iso_date'}}, 'allOf': [{'if': {'required': ['url']}, 'then': {'required': ['accessed_on']}, 'else': {'not': {'required': ['accessed_on']}}}]}, 'balance_design': {'type': 'object', 'additionalProperties': False, 'required': ['author', 'rationale', 'reviewed_by'], 'properties': {'author': {'type': 'string', 'minLength': 1, 'maxLength': 64}, 'rationale': {'type': 'string', 'minLength': 1, 'maxLength': 4096}, 'reviewed_by': {'type': 'array', 'minItems': 1, 'uniqueItems': True, 'items': {'type': 'string', 'minLength': 1, 'maxLength': 64}}}}} : the eight-letter storable-resource enum must be removed

======================================================================
FAIL: test_duplicate_resource_id_is_coded_load_error (tools.data_compile.test_data_compiler.ResourceReconciliationTests.test_duplicate_resource_id_is_coded_load_error)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/home/adquiod/Imágenes/Project/CHUNSA001/tools/data_compile/test_data_compiler.py", line 662, in test_duplicate_resource_id_is_coded_load_error
    self.assertIn("ERROR E_DUPLICATE_ID resource chunsa:food", stderr)
    ~~~~~~~~~~~~~^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: 'ERROR E_DUPLICATE_ID resource chunsa:food' not found in "ERROR E_SCHEMA building rome:forum /: {'schema_version': 1, 'id': 'rome:forum', 'civ_id': 'rome:module', 'display_name_key': 'rome:building.forum', 'description_key': 'rome:building.forum_desc', 'epoch_window': [5, 5], 'kind': 'civic', 'footprint': {'width_cells': 2, 'height_cells': 2, 'blocks_movement': True}, 'stats': {'hp': 1000}, 'constructible': True, 'resource_costs': {'chunsa:stone': 50}, 'build_time_ticks': 100, 'dropoff_resources': [], 'trains': [], 'researches': [], 'required_capabilities': [], 'grants_capabilities': [], 'recipes': [], 'playable_period_ids': ['rome:late_republic'], 'availability_mode': 'historical', 'provenance': {'status': 'promoted', 'generator': 'human', 'task_id': 'schema-tests', 'generated_on': '2026-07-22', 'reviewed_by': ['architect'], 'historical_claims': {'evidence': 'N', 'verification_reports': [], 'sources': []}, 'balance_design': {'author': 'architect', 'rationale': 'Gameplay value.', 'reviewed_by': ['architect']}}} is not valid under any of the given schemas\nERROR E_SCHEMA building rome:forum /resource_costs: Additional properties are not allowed ('chunsa:stone' was unexpected)\nERROR E_SCHEMA tech rome:roads /resource_costs: Additional properties are not allowed ('chunsa:stone' was unexpected)\nERROR E_SCHEMA tech rome:roads /resource_costs: Additional properties are not allowed ('chunsa:stone' was unexpected)\nERROR E_SCHEMA tech rome:roads /resource_costs: {'chunsa:stone': 20} is not valid under any of the given schemas\nERROR E_SCHEMA unit rome:legionary /resource_costs: Additional properties are not allowed ('chunsa:food' was unexpected)\nERROR E_SCHEMA unit rome:legionary /resource_costs: {'chunsa:food': 10} is not valid under any of the given schemas\n"

======================================================================
FAIL: test_material_cost_vocabulary_is_absent_from_every_schema (tools.data_compile.test_data_compiler.ResourceReconciliationTests.test_material_cost_vocabulary_is_absent_from_every_schema)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/home/adquiod/Imágenes/Project/CHUNSA001/tools/data_compile/test_data_compiler.py", line 524, in test_material_cost_vocabulary_is_absent_from_every_schema
    self.assertEqual([], offenders)
    ~~~~~~~~~~~~~~~~^^^^^^^^^^^^^^^
AssertionError: Lists differ: [] != ['building.schema.json:"material_costs"', [145 chars]ts"']

Second list contains 5 additional elements.
First extra element 0:
'building.schema.json:"material_costs"'

- []
+ ['building.schema.json:"material_costs"',
+  'common.schema.json:"material_cost"',
+  'common.schema.json:"material_costs"',
+  'tech.schema.json:"material_costs"',
+  'unit.schema.json:"material_costs"']

======================================================================
FAIL: test_namespaced_resource_validates_against_resource_schema (tools.data_compile.test_data_compiler.ResourceReconciliationTests.test_namespaced_resource_validates_against_resource_schema)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/home/adquiod/Imágenes/Project/CHUNSA001/tools/data_compile/test_data_compiler.py", line 529, in test_namespaced_resource_validates_against_resource_schema
    self.assertTrue(resource_path.is_file(), "resource.schema.json is required")
    ~~~~~~~~~~~~~~~^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: False is not true : resource.schema.json is required

======================================================================
FAIL: test_repository_declares_three_resources_at_indices_zero_one_two (tools.data_compile.test_data_compiler.ResourceReconciliationTests.test_repository_declares_three_resources_at_indices_zero_one_two)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/home/adquiod/Imágenes/Project/CHUNSA001/tools/data_compile/test_data_compiler.py", line 581, in test_repository_declares_three_resources_at_indices_zero_one_two
    self.assertEqual(sorted(self.RESOURCE_IDS), authored_ids)
    ~~~~~~~~~~~~~~~~^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: Lists differ: ['chunsa:food', 'chunsa:stone', 'chunsa:wood'] != []

First list contains 3 additional elements.
First extra element 0:
'chunsa:food'

- ['chunsa:food', 'chunsa:stone', 'chunsa:wood']
+ []

======================================================================
FAIL: test_repository_resource_references_are_namespaced (tools.data_compile.test_data_compiler.ResourceReconciliationTests.test_repository_resource_references_are_namespaced)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/home/adquiod/Imágenes/Project/CHUNSA001/tools/data_compile/test_data_compiler.py", line 573, in test_repository_resource_references_are_namespaced
    self.assertEqual([], legacy)
    ~~~~~~~~~~~~~~~~^^^^^^^^^^^^
AssertionError: Lists differ: [] != ['egipto_chariot_warrior.yaml:A', 'egipto_[1218 chars]:Me']

Second list contains 40 additional elements.
First extra element 0:
'egipto_chariot_warrior.yaml:A'

Diff is 1390 characters long. Set self.maxDiff to None to see it.

======================================================================
FAIL: test_resource_indices_do_not_depend_on_file_order (tools.data_compile.test_data_compiler.ResourceReconciliationTests.test_resource_indices_do_not_depend_on_file_order)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/home/adquiod/Imágenes/Project/CHUNSA001/tools/data_compile/test_data_compiler.py", line 628, in test_resource_indices_do_not_depend_on_file_order
    self.assertEqual(
    ~~~~~~~~~~~~~~~~^
        (0, "", 0, ""),
        ^^^^^^^^^^^^^^^
        (first[0], first[2], second[0], second[2]),
        ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    )
    ^
AssertionError: Tuples differ: (0, '', 0, '') != (1, "ERROR E_SCHEMA building rome:forum /:[3423 chars]s\n")

First differing element 0:
0
1

Diff is 3811 characters long. Set self.maxDiff to None to see it.

======================================================================
FAIL: test_two_compilations_are_byte_identical (tools.data_compile.test_data_compiler.ResourceReconciliationTests.test_two_compilations_are_byte_identical)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/home/adquiod/Imágenes/Project/CHUNSA001/tools/data_compile/test_data_compiler.py", line 606, in test_two_compilations_are_byte_identical
    self.assertEqual(
    ~~~~~~~~~~~~~~~~^
        (0, "", 0, ""),
        ^^^^^^^^^^^^^^^
        (first[0], first[2], second[0], second[2]),
        ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    )
    ^
AssertionError: Tuples differ: (0, '', 0, '') != (1, "ERROR E_SCHEMA building rome:forum /:[3423 chars]s\n")

First differing element 0:
0
1

Diff is 3811 characters long. Set self.maxDiff to None to see it.

======================================================================
FAIL: test_unknown_resource_reference_is_coded_load_error (tools.data_compile.test_data_compiler.ResourceReconciliationTests.test_unknown_resource_reference_is_coded_load_error)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/home/adquiod/Imágenes/Project/CHUNSA001/tools/data_compile/test_data_compiler.py", line 646, in test_unknown_resource_reference_is_coded_load_error
    self.assertIn("ERROR E_REFERENCE unit", stderr)
    ~~~~~~~~~~~~~^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
AssertionError: 'ERROR E_REFERENCE unit' not found in "ERROR E_SCHEMA building rome:forum /: {'schema_version': 1, 'id': 'rome:forum', 'civ_id': 'rome:module', 'display_name_key': 'rome:building.forum', 'description_key': 'rome:building.forum_desc', 'epoch_window': [5, 5], 'kind': 'civic', 'footprint': {'width_cells': 2, 'height_cells': 2, 'blocks_movement': True}, 'stats': {'hp': 1000}, 'constructible': True, 'resource_costs': {'chunsa:stone': 50}, 'build_time_ticks': 100, 'dropoff_resources': [], 'trains': [], 'researches': [], 'required_capabilities': [], 'grants_capabilities': [], 'recipes': [], 'playable_period_ids': ['rome:late_republic'], 'availability_mode': 'historical', 'provenance': {'status': 'promoted', 'generator': 'human', 'task_id': 'schema-tests', 'generated_on': '2026-07-22', 'reviewed_by': ['architect'], 'historical_claims': {'evidence': 'N', 'verification_reports': [], 'sources': []}, 'balance_design': {'author': 'architect', 'rationale': 'Gameplay value.', 'reviewed_by': ['architect']}}} is not valid under any of the given schemas\nERROR E_SCHEMA building rome:forum /resource_costs: Additional properties are not allowed ('chunsa:stone' was unexpected)\nERROR E_SCHEMA tech rome:roads /resource_costs: Additional properties are not allowed ('chunsa:stone' was unexpected)\nERROR E_SCHEMA tech rome:roads /resource_costs: Additional properties are not allowed ('chunsa:stone' was unexpected)\nERROR E_SCHEMA tech rome:roads /resource_costs: {'chunsa:stone': 20} is not valid under any of the given schemas\nERROR E_SCHEMA unit rome:legionary /resource_costs: Additional properties are not allowed ('chunsa:missing' was unexpected)\nERROR E_SCHEMA unit rome:legionary /resource_costs: {'chunsa:missing': 1} is not valid under any of the given schemas\n"

----------------------------------------------------------------------
Ran 9 tests in 0.291s

FAILED (failures=9)
```

Los nueve criterios fallaron por `AssertionError`; no hubo error de
compilación, importación o sintaxis, y ninguna prueba nació verde.

### VERDE — después de implementar

Mismo comando:

```text
test_common_schema_has_no_legacy_resource_enum ... ok
test_duplicate_resource_id_is_coded_load_error ... ok
test_material_cost_vocabulary_is_absent_from_every_schema ... ok
test_namespaced_resource_validates_against_resource_schema ... ok
test_repository_declares_three_resources_at_indices_zero_one_two ... ok
test_repository_resource_references_are_namespaced ... ok
test_resource_indices_do_not_depend_on_file_order ... ok
test_two_compilations_are_byte_identical ... ok
test_unknown_resource_reference_is_coded_load_error ... ok

----------------------------------------------------------------------
Ran 9 tests in 0.546s

OK
```

Suite Python completa de schemas y compilador:

```text
$ PYTHONDONTWRITEBYTECODE=1 /usr/bin/python3 -m unittest discover -s tools/data_compile -v
Ran 39 tests in 4.731s

OK
```

Las pruebas nuevas viven dentro del test CTest `data_compile`, ya registrado
con `LABELS "fast"` en `CMakeLists.txt`; no fue necesario crear otro target ni
modificar su etiqueta.

## Implementación

### Schema y datos authored

- `resource.schema.json` define un recurso por `record_id`; `index` es un
  campo `readOnly` generado únicamente dentro del blob.
- `resource_costs`, `dropoff_resources`, recetas y spawns referencian ids
  namespaced.
- Las recetas conservan su forma y solo unifican
  `input_resource_costs`/`output_resource_id`; siguen vacías.
- `data/resources/` contiene solo comida, madera y piedra.
- Todos los costes, dropoffs y spawns del catálogo actual fueron migrados sin
  cambiar cantidades ni posiciones.

### Compilador y blob

La asignación es independiente de `readdir`:

1. orden bytewise ascendente de `record_id`;
2. reserva dura de `chunsa:food=0`, `chunsa:wood=1`,
   `chunsa:stone=2`;
3. cualquier recurso futuro recibe el menor índice libre.

El lector hostil vuelve a comprobar ids únicos, índices únicos, rango
`0..31` y la misma política determinista. Las referencias de costes, dropoffs,
recetas y mapas se validan contra el catálogo de recursos; un id inexistente
produce `E_REFERENCE` durante compilación y un código tipado al cargar el
blob.

El formato CHDB pasa de `1.0`/schema-set `1`/7 secciones a
`1.1`/schema-set `2`/8 secciones, con `kind=8 resource` append-only. El loader
mantiene lectura compatible del formato legado `1.0`.

Blob reproducido:

```text
content_hash=sha256-v1:58984b2c5756b04c47267301546f7ca49acdc9a04c7cb6f14ee2a34e9bab2edf
records unit=5 building=6 tech=4 civ=2 map=1 ai-profile=1 resource=3
```

### Versiones de save y checksum

`SAVE_FORMAT_VERSION` queda en `14` y `CHECKSUM_ALGO_VERSION` en `9`.

La decisión es deliberada: los ids namespaced existen en el catálogo offline
y se resuelven a los mismos slots `0/1/2` antes de entrar al estado de juego.
No cambió ningún campo serializado, su orden, el dominio
`CHUNSA_STATE_V9` ni el algoritmo de checksum. Sí cambió el formato del blob,
por eso el versionado que sube es CHDB/schema-set y su content hash, no el
save ni el checksum.

## Invariantes y regresión

Comparación pre/post:

| Invariante | Antes | Después |
|---|---:|---:|
| apertura `end_tick` | 9317 | **9317** |
| eco `end_tick` | 1107 | **1107** |
| G1 `alloc_delta` | 0 | **0** |
| golden | 1074 / 0 | **1074 / 0** |

Salida funcional:

```text
G1 selftest: alloc_delta=0 OK checksum=6d66e42f0109605a
GOLDEN backend=int128 casos=1074 fallos=0 [OK]
skirmish_eco A: end_tick=1107 winner=1 ai_executions=55 state=e268dbc0346607ed cont=d07629db4b491e7a
skirmish_eco A2: stock_defensor A=500 B=0 Me=0
ai_skirmish_eco: OK
apertura A: end_tick=9317 winner=1 ai_executions=466 p0_gather=1 p1_gather=1 p1_built=1 p1_trained=1 state=fb9f9d45c3430ba4 cont=738854e75ae38cae
apertura C save-boundary: tick=143 citizen=7 state=1 deposit=5 carry=5 resource=0 cont=d958e4b1a898f4c7
ai_skirmish_apertura: OK
```

No se re-registró ningún `end_tick` ni baseline de estado/continuación: además
de los ticks, los hashes de trayectoria ya vigentes permanecen iguales.

## Verificación

### GCC

```text
$ cmake --build build-gcc -j8
exit_code=0
```

```text
$ ctest --test-dir build-gcc -L fast --output-on-failure
100% tests passed, 0 tests failed out of 27
Total Test time (real) = 26.57 sec
```

El ciclo rápido queda por debajo del límite de 60 s.

```text
$ ctest --test-dir build-gcc --output-on-failure
100% tests passed, 0 tests failed out of 30
Label Time Summary:
fast = 26.26 sec
slow = 219.75 sec
Total Test time (real) = 246.02 sec
```

### AddressSanitizer

Se compilaron y ejecutaron con ASan los tres binarios afectados por el cambio
de catálogo/recursos:

```text
$ cmake --build build-asan --target chunsa_test_data_blob chunsa_test_ai_skirmish_eco chunsa_test_ai_skirmish_apertura -j8
exit_code=0

$ ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 ./build-asan/chunsa_test_data_blob
data_blob: OK

$ ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 ./build-asan/chunsa_test_ai_skirmish_eco
skirmish_eco A: end_tick=1107 winner=1 ai_executions=55 state=e268dbc0346607ed cont=d07629db4b491e7a
skirmish_eco A2: stock_defensor A=500 B=0 Me=0
ai_skirmish_eco: OK

$ ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 ./build-asan/chunsa_test_ai_skirmish_apertura
apertura A: end_tick=9317 winner=1 ai_executions=466 p0_gather=1 p1_gather=1 p1_built=1 p1_trained=1 state=fb9f9d45c3430ba4 cont=738854e75ae38cae
apertura C save-boundary: tick=143 citizen=7 state=1 deposit=5 carry=5 resource=0 cont=d958e4b1a898f4c7
ai_skirmish_apertura: OK
```

No hubo diagnóstico de AddressSanitizer. Una primera ejecución con
`detect_leaks=1` alcanzó el límite conocido del entorno instrumentado:

```text
LeakSanitizer has encountered a fatal error.
HINT: LeakSanitizer does not work under ptrace
```

Por ello se repitió con `detect_leaks=0`, como ya está documentado para este
entorno; las comprobaciones de memoria de ASan sí finalizaron limpias.

### UndefinedBehaviorSanitizer

```text
$ cmake --build build-ubsan --target chunsa_test_data_blob chunsa_test_ai_skirmish_eco chunsa_test_ai_skirmish_apertura -j8
exit_code=0

$ UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ./build-ubsan/chunsa_test_data_blob
data_blob: OK

$ UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ./build-ubsan/chunsa_test_ai_skirmish_eco
skirmish_eco A: end_tick=1107 winner=1 ai_executions=55 state=e268dbc0346607ed cont=d07629db4b491e7a
skirmish_eco A2: stock_defensor A=500 B=0 Me=0
ai_skirmish_eco: OK

$ UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ./build-ubsan/chunsa_test_ai_skirmish_apertura
apertura A: end_tick=9317 winner=1 ai_executions=466 p0_gather=1 p1_gather=1 p1_built=1 p1_trained=1 state=fb9f9d45c3430ba4 cont=738854e75ae38cae
apertura C save-boundary: tick=143 citizen=7 state=1 deposit=5 carry=5 resource=0 cont=d958e4b1a898f4c7
ai_skirmish_apertura: OK
```

No hubo diagnóstico de UndefinedBehaviorSanitizer.

## Cierre de criterios

| Criterio del brief | Evidencia |
|---|---|
| Sin enum de ocho letras | prueba de schema verde |
| Sin vocabulario `material_cost*` | búsqueda exhaustiva de schemas + prueba verde |
| Sin `El` almacenable | búsqueda exhaustiva de datos/schemas sin coincidencias |
| Recurso namespaced válido | schema nuevo + prueba verde |
| Índices 0/1/2 | inspección del blob + prueba verde |
| Dos compilaciones byte-idénticas | prueba verde |
| Independencia del orden de fichero | prueba verde con nombres permutados |
| Id de recurso inexistente | `E_REFERENCE` probado |
| Recurso duplicado | `E_DUPLICATE_ID` probado |
| Cuatro invariantes | 9317, 1107, 0 y 1074/0 |

Cerrojazos finales:

- `git diff --check`: limpio.
- `movement_v1` y `CommandType`: sin cambios.
- `Step()`: sin heap/STL nuevo y sin float/reloj/entropía.
- no se añadió `assert()` para validar datos.
- `RESOURCE_COUNT` permanece en `32`.
- `addons/chunsa_sim/gdextension/`: sin cambios.
- el directorio ajeno `.claude/` se preservó sin tocar.
- trabajo realizado en `arch/sprint-1.8b-esquema-recursos`.
- no se hizo merge a `main`.
