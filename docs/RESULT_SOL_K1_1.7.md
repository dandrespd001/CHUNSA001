# Resultado K1 — Tareas del ciudadano + blindaje de gates (Sprint 1.7B)

Fecha: 2026-07-28

Rama: `arch/sprint-1.7-citizen-task`

Commit de implementación: `4be7110`

Contrato: `docs/specs/SPEC-004_SISTEMAS_PARTIDA.md` §22 y
`docs/briefs/SOL_K1_TAREAS_CIUDADANO_1.7.md`

Corrección de checksum:
`docs/briefs/SOL_K1B_CHECKSUM_V8_UNIVERSAL_1.7.md`

## Resultado

Se implementaron íntegramente las Partes A, B y C del brief:

- `GameState::citizen_task[ENTITY_HARD_CAP]` es estado `uint8_t`, con
  `IDLE=0`, `MOVE=1`, `GATHER=2` y `BUILD=3`.
- `citizen_task` es la autoridad exclusiva de posición para ciudadanos:
  `citizen_move_system`, `economy_system` y `construction_system` filtran por
  `MOVE`, `GATHER` y `BUILD`, respectivamente.
- `citizen_move_system` corre antes de economía y construcción y reproduce la
  forma de locomoción entera de `economy.hpp::try_move`.
- `MOVE_TO`, `GATHER` y `ASSIGN_BUILD` realizan las transiciones de §22.2 sin
  cambiar el orden de validación de comandos. `MOVE_TO` conserva asignación y
  carga; `GATHER` conserva íntegra la regla de carga de §18.
- Los ciudadanos creados por `SPAWN_CITIZEN`, `SPAWN_UNIT` de clase Citizen y
  `TRAIN_UNIT` nacen en `GATHER`.
- Fin de movimiento, obra completada/perdida y falta de depósitos terminan en
  `IDLE` con velocidad cero.
- Save subió a v13. Checksum subió a versión 8 y usa universalmente
  `CHUNSA_STATE_V8`; `citizen_task` se serializa y se incorpora al dominio para
  todos los slots.
- Se creó el punto único de verdad
  `tests/determinism/baselines.hpp`, con procedimiento de actualización y
  constantes aserveradas.
- Golden, G1, G3, G4, `ai_skirmish`, `ai_skirmish_eco` y
  `ai_skirmish_apertura` fallan ruidosamente ante un baseline distinto,
  mostrando esperado y obtenido.
- G1, G3 y G4 están registrados en `ctest`.
- `movement_v1` no fue modificado y no se tocó
  `addons/chunsa_sim/gdextension/`.

También se corrigió la validación del tipo de comando pendiente al cargar un
save: el rango válido era todavía `1..12`; ahora incluye `GATHER=13`. Esto evita
que un save v13 con un `GATHER` pendiente sea rechazado.

## Pruebas de §22

El nuevo binario `chunsa_test_citizen_task` cubre:

- `MOVE_TO` mueve al ciudadano y termina en `IDLE`;
- conservación de `eco_assigned_deposit`, `eco_carry` y
  `eco_carry_resource` durante `MOVE`;
- `GATHER` que interrumpe `MOVE` y cambia de recurso entra en `RETURN`;
- `ASSIGN_BUILD` interrumpe `MOVE` y toma la dirección de la obra en el mismo
  tick;
- aserto directo de que como máximo uno de los tres sistemas cambia `pos` para
  cada tarea, incluso con datos obsoletos de las otras tareas;
- obra completada o perdida termina en `IDLE`;
- ciudadano sin depósito termina en `IDLE`;
- defaults `GATHER` de los tres caminos de spawn;
- save v13/load y replay conservan `citizen_task`;
- cambiar solamente `citizen_task` cambia el checksum V8.

## Evidencia pre/post de la implementación `4be7110`

Todos los valores “pre” se midieron en `main`/inicio de esta rama antes de
modificar fuentes. Los valores “post” se midieron después de implementar §22,
antes de la corrección K1-B. Esta corrida conservó los digests V7 de los
escenarios sin ciudadanos y demostró que sus trayectorias no habían cambiado.

| Gate/escenario | Pre | Post observado en `4be7110` | Resultado y causa |
|---|---|---|---|
| Golden | 1074 casos, 0 fallos | 1074 casos, 0 fallos | Idéntico |
| G1 | `fefa48125dd35736` | `fefa48125dd35736` | Idéntico; sin ciudadanos, `alloc_delta=0` |
| G3 | `969199722657b853` / `6145075498b2fb7d` | `969199722657b853` / `6145075498b2fb7d` | Idéntico; sin ciudadanos |
| G4 | `774316057e5667fb` / `d52ac0019700684f` | `774316057e5667fb` / `d52ac0019700684f` | Idéntico; sin ciudadanos |
| `ai_skirmish` | `3f64d3223b74d477` / `92ec9aa95374a429` | `3f64d3223b74d477` / `92ec9aa95374a429` | Idéntico; escenario militar sin ciudadanos |
| `ai_skirmish_eco` | `d610feef89ed9c65` / `5e1527e0921edf27` | `d610feef89ed9c65` / `5e1527e0921edf27` | Idéntico. La trayectoria funcional no cambió y al checksum final ya no queda ningún ciudadano vivo |
| `ai_skirmish_apertura` | `71774aaa9c166103` / `b2294197e9964ba5` | `c7b04caea8c32e64` / `4ae3ddd1ad5ab4f9` | Cambio esperado: estado V8 + `citizen_task`; conserva `winner=1`, `end_tick=12292` y las cuatro fases |

El checksum de continuación observado en el checkpoint de save de apertura
también cambió de `b0fccab58d1cde4a` a `025b71b0fe915a05`: en esa corrida hay
un ciudadano vivo con tarea `MOVE` en dicho punto.

## Gates y build

Comandos ejecutados:

```text
cmake -S . -B build-gcc
cmake --build build-gcc -j2
ctest --test-dir build-gcc --output-on-failure
```

Salida relevante:

```text
[100%] Built target chunsa_test_ai_skirmish_apertura
100% tests passed, 0 tests failed out of 29
Label Time Summary:
determinism_gate    =  12.77 sec*proc (3 tests)
Total Test time (real) = 309.39 sec
```

Los gates nuevos de `ctest` se comprobaron también de forma aislada:

```text
1/3 Test #3: gate_g1 .......................... Passed  10.53 sec
2/3 Test #4: gate_g3 .......................... Passed   1.19 sec
3/3 Test #5: gate_g4 .......................... Passed   1.20 sec
100% tests passed out of 3
```

Medición directa de G1:

```text
G1 selftest: alloc_delta=0 OK checksum=fefa48125dd35736
```

## Sanitizers

Se compilaron y ejecutaron los binarios tocados en las configuraciones
existentes `build-asan` y `build-ubsan`: CLI/golden, G1, G3, G4,
`citizen_task`, `ai_skirmish`, `ai_skirmish_eco` y
`ai_skirmish_apertura`.

ASan:

```text
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
ctest --test-dir build-asan --output-on-failure \
  -R '^(golden|gate_g1|gate_g3|gate_g4|citizen_task|ai_skirmish|ai_skirmish_eco|ai_skirmish_apertura)$'

8/8 Test #27: ai_skirmish_apertura ............. Passed  904.42 sec
100% tests passed out of 8
Total Test time (real) = 1183.70 sec
```

UBSan:

```text
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build-ubsan --output-on-failure \
  -R '^(golden|gate_g1|gate_g3|gate_g4|citizen_task|ai_skirmish|ai_skirmish_eco|ai_skirmish_apertura)$'

8/8 Test #27: ai_skirmish_apertura ............. Passed  328.06 sec
100% tests passed out of 8
Total Test time (real) = 427.79 sec
```

No hubo diagnósticos de ASan ni UBSan.

Limitación del entorno: la primera corrida con
`ASAN_OPTIONS=detect_leaks=1` no pudo iniciar LeakSanitizer y produjo:

```text
LeakSanitizer has encountered a fatal error.
LeakSanitizer does not work under ptrace (strace, gdb, etc).
```

Por ello ASan se repitió con `detect_leaks=0`. AddressSanitizer sí quedó
activo; LSan no se pudo verificar en este entorno.

## Corrección K1-B

El Arquitecto reconoció que el brief original exigía simultáneamente un bump a
V8 y digests V7 bit-idénticos. La corrección elimina la resolución condicional:
todo estado usa `CHUNSA_STATE_V8`, escribe `CHECKSUM_ALGO_VERSION` 8 e incorpora
`citizen_task` para todos los slots. El algoritmo ya no depende de que queden
ciudadanos vivos.

El cambio de los digests de G1, G3, G4, `ai_skirmish` y
`ai_skirmish_eco` se debe exclusivamente al bump de dominio V7→V8, no a un
cambio de trayectoria. La no-regresión funcional quedó demostrada en la
corrida previa de `4be7110`, donde esos mismos escenarios conservaron sus
digests V7 bit-exactos.

| Gate/escenario | Baseline V7 | Baseline V8 universal | Invariante funcional |
|---|---|---|---|
| Golden | 1074 casos, 0 fallos | 1074 casos, 0 fallos | Sin cambios |
| G1 | `fefa48125dd35736` | `770b83a7cf97bd12` | `alloc_delta=0` |
| G3 | `969199722657b853` / `6145075498b2fb7d` | `4083889b6a9f9a14` / `ead0dc41779bdc9e` | Estado y continuación deterministas |
| G4 | `774316057e5667fb` / `d52ac0019700684f` | `6d2552c57b2b4f7e` / `8f39cd2b72df2871` | Estado y continuación deterministas |
| `ai_skirmish` | `3f64d3223b74d477` / `92ec9aa95374a429` | `5d7603757c533e97` / `4cdfd0b15dc12daa` | `winner=1`, `end_tick=1226`, sin cambio |
| `ai_skirmish_eco` | `d610feef89ed9c65` / `5e1527e0921edf27` | `68ae70ca41c3834b` / `bbcd6fba69413eee` | `winner=1`, `end_tick=1824`, sin cambio |
| `ai_skirmish_apertura` | `71774aaa9c166103` / `b2294197e9964ba5` | `c7b04caea8c32e64` / `4ae3ddd1ad5ab4f9` | `winner=1`, `end_tick=12292`; recolección P0/P1, construcción y entrenamiento confirmados |

La apertura ya se hasheaba como V8 en `4be7110`, por lo que la corrección
universal reprodujo sus dos valores. Su checksum de continuación en el
checkpoint de save también permaneció en `025b71b0fe915a05`.

Validación de la corrección:

```text
cmake --build build-gcc -j2
[100%] Built target chunsa_test_ai_skirmish_apertura

ctest --test-dir build-gcc --output-on-failure
100% tests passed, 0 tests failed out of 29
Total Test time (real) = 314.37 sec

GOLDEN backend=int128 casos=1074 fallos=0  [OK]
G1 selftest: alloc_delta=0 OK checksum=770b83a7cf97bd12

apertura A: end_tick=12292 winner=1 ai_executions=615
p0_gather=1 p1_gather=1 p1_built=1 p1_trained=1
state=c7b04caea8c32e64 cont=4ae3ddd1ad5ab4f9
```

No se modificaron §22, el código de las pruebas ni los sistemas; el único
archivo bajo `tests/` tocado fue el registro de baselines autorizado. No se
hizo merge a `main`.
