# Resultado Sprint 1.8C-kernel — catálogo tipado de recursos

## Resultado

Se ejecutó `docs/briefs/SOL_1.8C_CATALOGO_RECURSOS.md` con el ciclo TDD de
`docs/METODOLOGIA_TDD.md`.

`DataCatalogV1` expone ahora una tabla `ResourceDefinitionV1` con:

- `index` compiler-owned (`uint8_t`);
- `display_name_key` con memoria propiedad de `DataCatalogStorageV1`;
- `family` como enum cerrado de siete valores;
- `appearance_epoch` validado en `1..15`;
- `nature` como enum cerrado `collected | produced`.

`catalog_find_resource()` sigue la forma de los demás `catalog_find_*`: busca
por `record_id` bytewise y devuelve un `ResourceId`. El `ResourceId` es la
posición de la definición en orden de `record_id`; el slot económico estable
se obtiene de `ResourceDefinitionV1::index`. Esta separación conserva
exactamente los índices compiler-owned aunque `food`, `wood` y `stone` no
ocupen sus posiciones alfabéticas.

No se tocó `addons/chunsa_sim/gdextension/`, `data/resources/`,
`RESOURCE_COUNT`, `movement_v1` ni `CommandType`.

## Evidencia TDD

### ROJO C++ — pruebas y stubs antes de implementar

Primero se añadió el target `resource_catalog`, etiquetado `fast`, junto con
la superficie API mínima y un stub deliberadamente incorrecto:
`catalog_find_resource()` devolvía siempre `INVALID_RESOURCE_ID` y el loader
dejaba la tabla vacía. No se había implementado todavía la reconstrucción.

Comandos, sin pipes:

```text
cmake -S . -B build-gcc -DCHUNSA_BUILD_GODOT=OFF; echo CMAKE_RC=$?
cmake --build build-gcc --target chunsa_test_resource_catalog -j8; echo BUILD_RC=$?
./build-gcc/chunsa_test_resource_catalog; echo RESOURCE_CATALOG_RED_RC=$?
```

Salida roja literal:

```text
CMAKE_RC=0
[100%] Built target chunsa_test_resource_catalog
BUILD_RC=0
CHECK_EQ resource_count: esperado=30 obtenido=0
CHECK_EQ copper_family: esperado=3 obtenido=0
CHECK_EQ chunsa:food: esperado=1 obtenido=0
CHECK_EQ chunsa:steel: esperado=12 obtenido=0
CHECK_EQ chunsa:uranium: esperado=14 obtenido=0
CHECK_EQ bronze_nature: esperado=2 obtenido=0
CHECK_EQ copper_nature: esperado=1 obtenido=0
CHECK_EQ resources_with_four_metadata_fields: esperado=30 obtenido=0
CHECK_EQ chunsa:food: esperado=0 obtenido=18446744073709551615
CHECK_EQ chunsa:wood: esperado=1 obtenido=18446744073709551615
CHECK_EQ chunsa:stone: esperado=2 obtenido=18446744073709551615
CHECK_EQ chunsa:aluminum: esperado=3 obtenido=18446744073709551615
CHECK_EQ chunsa:bauxite: esperado=4 obtenido=18446744073709551615
CHECK_EQ chunsa:bronze: esperado=5 obtenido=18446744073709551615
CHECK_EQ chunsa:cement: esperado=6 obtenido=18446744073709551615
CHECK_EQ chunsa:charcoal: esperado=7 obtenido=18446744073709551615
CHECK_EQ chunsa:clay: esperado=8 obtenido=18446744073709551615
CHECK_EQ chunsa:coal: esperado=9 obtenido=18446744073709551615
CHECK_EQ chunsa:coke: esperado=10 obtenido=18446744073709551615
CHECK_EQ chunsa:copper: esperado=11 obtenido=18446744073709551615
CHECK_EQ chunsa:gold: esperado=12 obtenido=18446744073709551615
CHECK_EQ chunsa:gunpowder: esperado=13 obtenido=18446744073709551615
CHECK_EQ chunsa:iron_ore: esperado=14 obtenido=18446744073709551615
CHECK_EQ chunsa:lead: esperado=15 obtenido=18446744073709551615
CHECK_EQ chunsa:limestone: esperado=16 obtenido=18446744073709551615
CHECK_EQ chunsa:nitre: esperado=17 obtenido=18446744073709551615
CHECK_EQ chunsa:nitrogen_fixed: esperado=18 obtenido=18446744073709551615
CHECK_EQ chunsa:oil: esperado=19 obtenido=18446744073709551615
CHECK_EQ chunsa:oil_products: esperado=20 obtenido=18446744073709551615
CHECK_EQ chunsa:quicklime: esperado=21 obtenido=18446744073709551615
CHECK_EQ chunsa:rare_earths: esperado=22 obtenido=18446744073709551615
CHECK_EQ chunsa:salt: esperado=23 obtenido=18446744073709551615
CHECK_EQ chunsa:silicon: esperado=24 obtenido=18446744073709551615
CHECK_EQ chunsa:steel: esperado=25 obtenido=18446744073709551615
CHECK_EQ chunsa:sulfur: esperado=26 obtenido=18446744073709551615
CHECK_EQ chunsa:tin: esperado=27 obtenido=18446744073709551615
CHECK_EQ chunsa:uranium: esperado=28 obtenido=18446744073709551615
CHECK_EQ chunsa:wrought_iron: esperado=29 obtenido=18446744073709551615
CHECK_EQ invalid_family_load_code: esperado=15 obtenido=0
CHECK_EQ appearance_epoch_zero_load_code: esperado=15 obtenido=0
CHECK_EQ appearance_epoch_above_15_load_code: esperado=15 obtenido=0
resource_catalog: FAIL (41 fallos)
RESOURCE_CATALOG_RED_RC=1
```

El target compiló y los fallos fueron exclusivamente de aserción. Las últimas
tres aserciones demuestran además que el loader anterior aceptaba blobs con
familia desconocida o época fuera de rango porque descartaba esos campos.

### Validación YAML preexistente y prueba de mutación

Las dos pruebas nuevas de YAML inválido nacieron verdes antes de implementar:
el schema creado por el sprint de datos ya rechazaba esos valores con
`E_SCHEMA`. No se presenta ese verde como fase roja.

Para demostrar el poder de esas pruebas se aplicó §4 de
`METODOLOGIA_TDD.md`: temporalmente se permitió `invalid_family` y el rango
`0..16` en `resource.schema.json`, se ejecutaron las pruebas y luego se
restauró el schema. La mutación no está en el diff final.

Comando:

```text
PYTHONDONTWRITEBYTECODE=1 /usr/bin/python3 -m unittest \
  tools.data_compile.test_data_compiler.ResourceReconciliationTests.test_unknown_resource_family_is_coded_load_error \
  tools.data_compile.test_data_compiler.ResourceReconciliationTests.test_resource_appearance_epoch_out_of_range_is_coded_load_error \
  -v; echo YAML_MUTATION_RED_RC=$?
```

Salida roja literal:

```text
test_unknown_resource_family_is_coded_load_error (...) ... FAIL
test_resource_appearance_epoch_out_of_range_is_coded_load_error (...) ...
  ... (appearance_epoch=0) ... FAIL
  ... (appearance_epoch=16) ... FAIL

======================================================================
FAIL: test_unknown_resource_family_is_coded_load_error (...)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/home/adquiod/Imágenes/Project/CHUNSA001/tools/data_compile/test_data_compiler.py", line 717, in test_unknown_resource_family_is_coded_load_error
    self.assertEqual(1, rc)
AssertionError: 1 != 0

======================================================================
FAIL: test_resource_appearance_epoch_out_of_range_is_coded_load_error (...) (appearance_epoch=0)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/home/adquiod/Imágenes/Project/CHUNSA001/tools/data_compile/test_data_compiler.py", line 735, in test_resource_appearance_epoch_out_of_range_is_coded_load_error
    self.assertEqual(1, rc)
AssertionError: 1 != 0

======================================================================
FAIL: test_resource_appearance_epoch_out_of_range_is_coded_load_error (...) (appearance_epoch=16)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/home/adquiod/Imágenes/Project/CHUNSA001/tools/data_compile/test_data_compiler.py", line 735, in test_resource_appearance_epoch_out_of_range_is_coded_load_error
    self.assertEqual(1, rc)
AssertionError: 1 != 0

----------------------------------------------------------------------
Ran 2 tests in 0.149s

FAILED (failures=3)
YAML_MUTATION_RED_RC=1
```

### VERDE focal

Después de implementar:

```text
$ ./build-gcc/chunsa_test_resource_catalog; echo RESOURCE_CATALOG_GREEN_RC=$?
resource_catalog: OK (0 fallos)
RESOURCE_CATALOG_GREEN_RC=0

$ PYTHONDONTWRITEBYTECODE=1 /usr/bin/python3 -m unittest \
  tools.data_compile.test_data_compiler.ResourceReconciliationTests.test_unknown_resource_family_is_coded_load_error \
  tools.data_compile.test_data_compiler.ResourceReconciliationTests.test_resource_appearance_epoch_out_of_range_is_coded_load_error \
  -v; echo YAML_VALIDATION_GREEN_RC=$?
test_unknown_resource_family_is_coded_load_error (...) ... ok
test_resource_appearance_epoch_out_of_range_is_coded_load_error (...) ... ok

----------------------------------------------------------------------
Ran 2 tests in 0.143s

OK
YAML_VALIDATION_GREEN_RC=0
```

## Implementación

El loader hace dos trabajos separados:

1. el prepass existente conserva `record_id → index` para resolver costes y
   depósitos mientras se leen secciones anteriores;
2. el pase canónico de `kind=8` construye `ResourceDefinitionV1`, valida enums
   y rangos y verifica que `id/index` coincidan con el prepass.

Los `record_id` y `display_name_key` viven en vectores propiedad del `Impl`.
Se reserva el tamaño exacto antes de llenarlos y los punteros públicos se
instalan solo después de terminar las inserciones. No se usa `assert()` para
validar datos; todo dato inválido termina en `CatalogLoadCode`.

Los CHDB 1.0 legados conservan el fallback interno A/B/Me para resolver
economía, pero exponen `resource_count=0` porque ese formato no contiene los
metadatos necesarios. No se fabrican familias ni épocas por defecto.

El compilador Python ya escribía en los records CVE completos los cuatro
campos creados por el sprint 1.8C-datos. El hueco real estaba en el loader C++,
que preleía únicamente `id/index`. Por eso no fue necesario cambiar
`chunsa_data_compiler.py` ni regenerar el golden para este commit.

## Índices y reproducibilidad

La prueba C++ verifica los 30 pares exactos, incluidos:

```text
food=0 wood=1 stone=2 aluminum=3 ... wrought_iron=29
```

Dos compilaciones completas del repositorio:

```text
$ PYTHONDONTWRITEBYTECODE=1 /usr/bin/python3 tools/data_compile/chunsa_data_compiler.py compile data --out /tmp/chunsa-1.8c-verify/first.chdb --profile release --print-hash
content_hash=sha256-v1:0be89a04375244969dc2cf827f4b7780675501142ba4fbd52da2a9d870197722
records unit=5 building=6 tech=4 civ=2 map=1 ai-profile=1 resource=30
COMPILE_FIRST_RC=0

$ PYTHONDONTWRITEBYTECODE=1 /usr/bin/python3 tools/data_compile/chunsa_data_compiler.py compile data --out /tmp/chunsa-1.8c-verify/second.chdb --profile release --print-hash
content_hash=sha256-v1:0be89a04375244969dc2cf827f4b7780675501142ba4fbd52da2a9d870197722
records unit=5 building=6 tech=4 civ=2 map=1 ai-profile=1 resource=30
COMPILE_SECOND_RC=0

$ cmp /tmp/chunsa-1.8c-verify/first.chdb /tmp/chunsa-1.8c-verify/second.chdb
CMP_RC=0
```

El hash `0be89a…7722` ya era el golden de entrada de esta rama: cambió en el
sprint 1.8C-datos cuando los 30 YAML entraron al blob. Este commit solo expone
campos ya presentes y no vuelve a cambiar los bytes.

## Versionado

`SAVE_FORMAT_VERSION` permanece en `14` y `CHECKSUM_ALGO_VERSION` en `9`.

La tabla descriptiva vive en el catálogo offline y no se serializa en
`GameState`, no cambia el orden de ningún campo de save, no cambia el dominio
`CHUNSA_STATE_V9` y no participa en el checksum de estado. El content hash del
catálogo sí liga los bytes del blob, pero su actualización correspondiente ya
ocurrió con el golden de datos de entrada. Subir save/checksum aquí declararía
una incompatibilidad de estado inexistente.

## Invariantes de trayectoria

Se midieron antes de editar:

```text
apertura A: end_tick=9317 winner=1 ... state=fb9f9d45c3430ba4 cont=738854e75ae38cae
APERTURA_RC=0
skirmish_eco A: end_tick=1107 winner=1 ... state=e268dbc0346607ed cont=d07629db4b491e7a
ECO_RC=0
```

Y después de implementar:

```text
apertura A: end_tick=9317 winner=1 ai_executions=466 p0_gather=1 p1_gather=1 p1_built=1 p1_trained=1 state=fb9f9d45c3430ba4 cont=738854e75ae38cae
apertura C save-boundary: tick=143 citizen=7 state=1 deposit=5 carry=5 resource=0 cont=d958e4b1a898f4c7
ai_skirmish_apertura: OK
APERTURA_POST_RC=0
skirmish_eco A: end_tick=1107 winner=1 ai_executions=55 state=e268dbc0346607ed cont=d07629db4b491e7a
skirmish_eco A2: stock_defensor A=500 B=0 Me=0
ai_skirmish_eco: OK
ECO_POST_RC=0
```

Los ticks y los hashes de estado/continuación son idénticos.

Los otros dos invariantes:

```text
$ ./build-gcc/chunsa_sim_cli golden --vectors tests/determinism/golden
GOLDEN backend=int128 casos=1074 fallos=0  [OK]
GOLDEN_RC=0

$ ./build-gcc/chunsa_sim_cli run --selftest-g1
G1 selftest: alloc_delta=0 OK checksum=6d66e42f0109605a
G1_RC=0
```

## Gates

### Build y suites

```text
$ cmake --build build-gcc -j8
[100%] Built target chunsa_test_ai_skirmish_apertura
BUILD_GCC_RC=0
```

```text
$ ctest --test-dir build-gcc -L fast --output-on-failure
28/28 Test #31: data_compile .....................   Passed    5.12 sec

100% tests passed out of 28
Label Time Summary:
fast    =  26.34 sec*proc (28 tests)
Total Test time (real) =  26.34 sec
CTEST_FAST_RC=0
```

```text
$ ctest --test-dir build-gcc --output-on-failure
31/31 Test #31: data_compile .....................   Passed    5.12 sec

100% tests passed out of 31
Label Time Summary:
fast    =  26.75 sec*proc (28 tests)
slow    = 211.80 sec*proc (3 tests)
Total Test time (real) = 238.56 sec
CTEST_FULL_RC=0
```

### AddressSanitizer

Configuración comprobada:

```text
CMAKE_CXX_FLAGS=-fsanitize=address -fno-omit-frame-pointer
CMAKE_EXE_LINKER_FLAGS=-fsanitize=address
```

Se compilaron y ejecutaron los cuatro binarios afectados con
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`:

```text
resource_catalog: OK (0 fallos)
ASAN_RESOURCE_CATALOG_RC=0
data_blob: OK
ASAN_DATA_BLOB_RC=0
skirmish_eco A: end_tick=1107 winner=1 ai_executions=55 state=e268dbc0346607ed cont=d07629db4b491e7a
skirmish_eco A2: stock_defensor A=500 B=0 Me=0
ai_skirmish_eco: OK
ASAN_ECO_RC=0
apertura A: end_tick=9317 winner=1 ai_executions=466 p0_gather=1 p1_gather=1 p1_built=1 p1_trained=1 state=fb9f9d45c3430ba4 cont=738854e75ae38cae
apertura C save-boundary: tick=143 citizen=7 state=1 deposit=5 carry=5 resource=0 cont=d958e4b1a898f4c7
ai_skirmish_apertura: OK
ASAN_APERTURA_RC=0
```

No hubo diagnóstico de AddressSanitizer. `detect_leaks=0` es la configuración
documentada del proyecto para este entorno bajo `ptrace`.

### UndefinedBehaviorSanitizer

Configuración comprobada:

```text
CMAKE_CXX_FLAGS=-fsanitize=undefined -fno-omit-frame-pointer
CMAKE_EXE_LINKER_FLAGS=-fsanitize=undefined
```

Se ejecutaron los mismos binarios con
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`:

```text
resource_catalog: OK (0 fallos)
UBSAN_RESOURCE_CATALOG_RC=0
data_blob: OK
UBSAN_DATA_BLOB_RC=0
skirmish_eco A: end_tick=1107 winner=1 ai_executions=55 state=e268dbc0346607ed cont=d07629db4b491e7a
skirmish_eco A2: stock_defensor A=500 B=0 Me=0
ai_skirmish_eco: OK
UBSAN_ECO_RC=0
apertura A: end_tick=9317 winner=1 ai_executions=466 p0_gather=1 p1_gather=1 p1_built=1 p1_trained=1 state=fb9f9d45c3430ba4 cont=738854e75ae38cae
apertura C save-boundary: tick=143 citizen=7 state=1 deposit=5 carry=5 resource=0 cont=d958e4b1a898f4c7
ai_skirmish_apertura: OK
UBSAN_APERTURA_RC=0
```

No hubo diagnóstico de UndefinedBehaviorSanitizer.

## Cerrojazos de alcance

Antes del commit:

```text
$ git diff --check
DIFF_CHECK_RC=0

$ git diff --stat -- addons/chunsa_sim/gdextension/ data/resources/
# sin salida
PROHIBITED_WORKTREE_DIFF_RC=0
```

La rama de trabajo es `arch/sprint-1.8c-catalogo`. No se fusionó a `main`.
