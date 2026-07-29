# Resultado Sprint 1.8A — ampliación estructural del vector de recursos

## Evidencia TDD

### ROJO — antes de implementar

Para que la prueba compilase y fallase por aserción, se añadió el stub
deliberadamente incorrecto `RESOURCE_COUNT = 3`, tal como permite el brief.
No se había ampliado `GameState`, los costes, el mask, save ni checksum.

Comando:

```text
./build-gcc/chunsa_test_resource_count
```

Salida exacta (`exit_code=1`):

```text
CHECK_EQ L61: esperado=32 obtenido=3 (RESOURCE_COUNT)
CHECK_EQ L62: esperado=14 obtenido=13 (SAVE_FORMAT_VERSION)
CHECK_EQ L63: esperado=9 obtenido=8 (CHECKSUM_ALGO_VERSION)
CHECK_EQ L67: esperado=32 obtenido=3 (PLAYER_STOCK_RESOURCE_COUNT)
CHECK_EQ L79: esperado=4 obtenido=1 (sizeof(Mask))
CHECK_EQ L98: esperado=1 obtenido=0 (HasResourceCostVector<Definition>)
CHECK_EQ L98: esperado=1 obtenido=0 (HasResourceCostVector<Definition>)
CHECK_EQ L98: esperado=1 obtenido=0 (HasResourceCostVector<Definition>)
CHECK_EQ L113: esperado=1 obtenido=0 (HasResourceCostVector<Definition>)
CHECK_EQ L114: esperado=32 obtenido=3 (PLAYER_STOCK_RESOURCE_COUNT)
CHECK_EQ L158: esperado=32 obtenido=3 (PLAYER_STOCK_RESOURCE_COUNT)
CHECK_EQ L188: esperado=32 obtenido=3 (PLAYER_STOCK_RESOURCE_COUNT)
resource_count: 12 fallos
```

El target compiló y enlazó antes de esta ejecución. Los doce fallos son de
aserción y ninguna exigencia nueva pasó durante la fase roja.

### VERDE — después de implementar

Comando:

```text
./build-gcc/chunsa_test_resource_count
```

Salida exacta (`exit_code=0`):

```text
resource_count: OK
```

## Cambio estructural

- `RESOURCE_COUNT = 32`; A/B/Me conservan 0/1/2 y 3..31 se inicializan a
  cero.
- `player_stock`, los tres tipos de definición y todos los recorridos de
  coste usan `RESOURCE_COUNT`.
- `dropoff_mask` es `uint32_t`.
- Save v14 serializa los 32 stocks; checksum v9 usa universalmente
  `CHUNSA_STATE_V9` y cubre los 32 índices.

## Invariantes y baselines

Se midió primero `main`, luego se ejecutaron eco y apertura con la
implementación V9 pero conservando todavía los baselines V8. En ambas
ejecuciones los únicos fallos fueron los dos hashes esperados:

```text
BASELINE ai_skirmish_eco.state: esperado=f2d313552c5c23aa obtenido=e268dbc0346607ed
BASELINE ai_skirmish_eco.continuation: esperado=9e817f78a66239db obtenido=d07629db4b491e7a
skirmish_eco A: end_tick=1107 winner=1 ai_executions=55 state=e268dbc0346607ed cont=d07629db4b491e7a
skirmish_eco A2: stock_defensor A=500 B=0 Me=0
ai_skirmish_eco: 2 fallos
```

```text
BASELINE ai_skirmish_apertura.state: esperado=e99b6ca32cf0b78d obtenido=fb9f9d45c3430ba4
BASELINE ai_skirmish_apertura.continuation: esperado=9670e9e87ba25b50 obtenido=738854e75ae38cae
apertura A: end_tick=9317 winner=1 ai_executions=466 p0_gather=1 p1_gather=1 p1_built=1 p1_trained=1 state=fb9f9d45c3430ba4 cont=738854e75ae38cae
apertura C save-boundary: tick=143 citizen=7 state=1 deposit=5 carry=5 resource=0 cont=d958e4b1a898f4c7
ai_skirmish_apertura: 2 fallos
```

Por tanto, se registraron los once hashes nuevos únicamente después de
comprobar los `end_tick` exactos:

| Baseline | V8 | V9 |
|---|---:|---:|
| G1 state | `770b83a7cf97bd12` | `6d66e42f0109605a` |
| G3 state | `4083889b6a9f9a14` | `5a20ac5093ec9708` |
| G3 continuation | `ead0dc41779bdc9e` | `f19faf596c019cb4` |
| G4 state | `6d2552c57b2b4f7e` | `d3dbc590712d0b7b` |
| G4 continuation | `8f39cd2b72df2871` | `958ca2dcbbd8e4e3` |
| AI skirmish state | `5d7603757c533e97` | `1bb2d03ff34709d3` |
| AI skirmish continuation | `4cdfd0b15dc12daa` | `d7410b71f1c526b1` |
| AI eco state | `f2d313552c5c23aa` | `e268dbc0346607ed` |
| AI eco continuation | `9e817f78a66239db` | `d07629db4b491e7a` |
| Apertura state | `e99b6ca32cf0b78d` | `fb9f9d45c3430ba4` |
| Apertura continuation | `9670e9e87ba25b50` | `738854e75ae38cae` |

Comparación funcional:

| Invariante | Antes | Después |
|---|---:|---:|
| G1 `alloc_delta` | 0 | 0 |
| eco `winner` | 1 | 1 |
| eco `end_tick` | 1107 | **1107** |
| apertura `winner` | 1 | 1 |
| apertura `end_tick` | 9317 | **9317** |
| golden | 1074 / 0 fallos | 1074 / 0 fallos |

No se re-registró ningún `end_tick`; ambos están aserverados en la suite.

## Verificación

### GCC y suite completa

```text
$ cmake --build build-gcc -j8
[100%] Built target chunsa_test_ai_skirmish_apertura
exit_code=0
```

Sin warnings nuevos.

```text
$ ctest --test-dir build-gcc --output-on-failure
28/30 Test #28: ai_skirmish_apertura .............   Passed  180.97 sec
29/30 Test #29: fog_view .........................   Passed    0.00 sec
30/30 Test #30: data_compile .....................   Passed    4.52 sec

100% tests passed out of 30
Total Test time (real) = 254.31 sec
```

El gate `golden` incluido en esta corrida produjo 1074 casos y cero fallos.

### AddressSanitizer

Configuración verificada: `-fsanitize=address -fno-omit-frame-pointer`,
linker `-fsanitize=address`.

```text
$ ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 ctest --test-dir build-asan --output-on-failure -R '^(golden|gate_g1|gate_g3|gate_g4|data_blob|buildings|production_tech|ai_layers|ai_skirmish|ai_skirmish_eco|gather|citizen_task|resource_count|ai_skirmish_apertura)$'
14/14 Test #28: ai_skirmish_apertura .............   Passed  744.44 sec

100% tests passed out of 14
Total Test time (real) = 986.97 sec
```

Sin diagnósticos de AddressSanitizer. Se mantuvo `detect_leaks=0`, la
configuración ya documentada para este entorno bajo `ptrace`.

### UndefinedBehaviorSanitizer

Configuración verificada: `-fsanitize=undefined -fno-omit-frame-pointer`,
linker `-fsanitize=undefined`.

```text
$ UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build-ubsan --output-on-failure -R '^(golden|gate_g1|gate_g3|gate_g4|data_blob|buildings|production_tech|ai_layers|ai_skirmish|ai_skirmish_eco|gather|citizen_task|resource_count|ai_skirmish_apertura)$'
14/14 Test #28: ai_skirmish_apertura .............   Passed  251.77 sec

100% tests passed out of 14
Total Test time (real) = 337.69 sec
```

Sin diagnósticos de UndefinedBehaviorSanitizer.

### Cerrojazos estructurales

```text
$ grep -rn "cost_a\|cost_b\|cost_me" addons/chunsa_sim/core
# sin salida; exit_code=1

$ git diff --stat main -- data/ addons/chunsa_sim/gdextension/
# sin salida; exit_code=0

$ git diff --name-only main -- data/ addons/chunsa_sim/gdextension/
# sin salida; exit_code=0
```

- No cambió ningún fichero de `data/`.
- No cambió ningún fichero de `addons/chunsa_sim/gdextension/`.
- `movement_v1` permanece intacto.
- `CommandType` permanece intacto.
- Los `GameState` de la prueba nueva están siempre en heap.
- No se añadió `assert()` para validar datos.
- El directorio ajeno `.claude/`, ya no rastreado al comenzar, se preservó.
- Trabajo realizado en `arch/sprint-1.8a-resource-count`; no se hizo merge a
  `main`.

Nota operativa: una medición preliminar de apertura se lanzó accidentalmente
en paralelo con otra instancia y ambas compartían nombres fijos de archivos
temporales; esa corrida terminó con señal 136 y se descartó. Todas las
evidencias aceptadas arriba se repitieron de forma serial y terminaron
limpias.
