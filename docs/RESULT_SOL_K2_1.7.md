# Resultado SOL K2 — Zona aliada de auto-recolección (Sprint 1.7)

Fecha: 2026-07-28

Rama: `arch/sprint-1.7-zona-aliada`

Contrato: `docs/specs/SPEC-004_SISTEMAS_PARTIDA.md` §23

Brief: `docs/briefs/SOL_K2_ZONA_ALIADA_1.7.md`

## Resultado

La auto-asignación económica ya no envía ciudadanos a depósitos remotos por
preferencia de recurso. Solo considera depósitos vivos a un máximo de 32 tiles
de algún edificio aliado completo; mantiene las dos pasadas deterministas
(recurso preferido y luego cualquier recurso) y deja al ciudadano `IDLE` cuando
la zona no contiene ningún candidato.

La orden `GATHER` del jugador conserva su agencia: un depósito explícitamente
asignado puede estar fuera de la zona y continúa siendo el objetivo hasta
agotarse. La reasignación posterior sí vuelve a la zona aliada.

No se modificaron `SAVE_FORMAT_VERSION` (13), `CHECKSUM_ALGO_VERSION` (8),
`CommandType`, `GameState`, `movement_v1` ni
`addons/chunsa_sim/gdextension/`.

## Implementación

### `economy.hpp`

- Añade `ECO_AUTO_GATHER_RADIUS_RAW = 32 * FX_ONE_RAW`, con el criterio de
  balance exigido por §23.4.
- Representa el conjunto elegible como una máscara `uint32_t`: el kernel ya
  limita `ECO_MAX_DEPOSITS` a 32 y un `static_assert` blinda esa relación.
- `eco_find_nearest_deposit` filtra la máscara tanto en la pasada del recurso
  preferido como en la pasada de cualquier recurso. Conserva recorrido
  ascendente, distancia entera y desempate por índice bajo.
- `eco_step_citizen` recibe la máscara solo para el camino de
  auto-reasignación. Un índice explícito todavía vivo no consulta la máscara.

### `step.hpp`

- Extrae `is_complete_owned_building`, criterio único compartido por dropoff y
  zona aliada: entidad viva, `entity_kind == 1`, mismo dueño, definición válida
  y `build_progress >= build_time_ticks`.
- `allied_auto_gather_deposit_masks` recorre una vez las entidades en orden
  ascendente y, por cada edificio completo, los depósitos en orden ascendente.
  Produce una máscara fija por emisor en la pila.
- `economy_system` precalcula esas máscaras una vez por fase y entrega la del
  dueño a `economy.hpp`.

Esta fue la decisión de separación de §2.2 del brief: `economy.hpp` sigue puro,
sin incluir ni conocer `GameState`, catálogo o edificios. El wiring que ya
resolvía el dropoff en `step.hpp` resuelve también la elegibilidad espacial.
No hay heap, STL, float, reloj ni RNG nuevos dentro de `Step()`.

### Escenarios y fixtures

`ai_skirmish_eco` usaba el patrón legacy con todos los depósitos a unos 90
tiles de su único centro. Con §23 ese mapa sintético no tenía zona económica y
dejaba de probar su invariante central, “economía real”. Se añadió init de
escenario fuera de `Step()` que sitúa su depósito A de base a 8 tiles, la misma
escala 8–20 del mapa real. No se debilitó ningún aserto: conserva stock
recolectado, victoria, determinismo, save/continuación y replay.

Los fixtures `economy`, `citizen_task` y `buildings` también se hicieron
explícitos:

- los escenarios que prueban auto-recolección incluyen un edificio completo;
- los estados fabricados directamente en `RETURN` fijan además
  `citizen_task=GATHER`, autoridad requerida desde K1.

## Pruebas obligatorias

`tests/unit/test_gather.cpp` cubre los siete casos del brief:

1. mismo recurso fuera de zona frente a otro recurso dentro: gana el local;
2. dos preferidos dentro: gana el más cercano y, en empate, el índice bajo;
3. ningún depósito dentro: `IDLE`, `ECO_NO_DEPOSIT` y velocidad cero;
4. `GATHER` explícito fuera: `ACCEPTED` y comienza la marcha;
5. agotamiento del objetivo explícito remoto: vuelve a un depósito local;
6. completar un edificio de expansión activa un depósito antes inelegible;
7. redirección con carga de recurso distinto sigue en `RETURN` y acredita el
   tipo original antes de recolectar el nuevo.

Todos los `GameState` nuevos o modificados de estas pruebas viven en heap con
`std::make_unique`; no se usa `assert()` para validar datos.

## Baselines pre/post

Los valores “pre” son los baselines V8 presentes en la rama al comenzar. Antes
de editar se ejecutó el conjunto canónico y pasó 7/7.

| Gate | Pre | Post | Resultado |
|---|---|---|---|
| Golden Fixed64/normalize | 1074 / 0 fallos | 1074 / 0 fallos | bit-idéntico |
| G1 estado | `770b83a7cf97bd12` | `770b83a7cf97bd12` | bit-idéntico |
| G3 estado / continuación | `4083889b6a9f9a14` / `ead0dc41779bdc9e` | iguales | bit-idéntico |
| G4 estado / continuación | `6d2552c57b2b4f7e` / `8f39cd2b72df2871` | iguales | bit-idéntico |
| `ai_skirmish` estado / continuación | `5d7603757c533e97` / `4cdfd0b15dc12daa` | iguales | bit-idéntico; sin ciudadanos |
| `ai_skirmish_eco` estado / continuación | `68ae70ca41c3834b` / `bbcd6fba69413eee` | `f2d313552c5c23aa` / `9e817f78a66239db` | cambio esperado de trayectoria + depósito base del fixture |
| `ai_skirmish_apertura` estado / continuación | `c7b04caea8c32e64` / `4ae3ddd1ad5ab4f9` | `e99b6ca32cf0b78d` / `9670e9e87ba25b50` | cambio esperado de trayectoria |
| Apertura `end_tick` | 12292 | 9317 | mejora esperada; `< 36000` |

Invariantes funcionales observados:

- `ai_skirmish_eco`: `winner=1`, `end_tick=1107`, stock defensor
  `A=500 B=0 Me=0`;
- `ai_skirmish_apertura`: `winner=1`, `end_tick=9317`,
  `p0_gather=1`, `p1_gather=1`, `p1_built=1`, `p1_trained=1`.

## Validación GCC

```text
$ cmake --build build-gcc -j8
[100%] Built target chunsa_test_ai_skirmish_apertura
```

Compilación GCC 16.1.1 limpia con `-Wall -Wextra -Wshadow -Werror`, sin avisos
nuevos.

```text
$ ctest --test-dir build-gcc --output-on-failure
29/29 Test #29: data_compile ..................... Passed
100% tests passed, 0 tests failed out of 29
Total Test time (real) = 243.50 sec
```

Los cinco gates que debían permanecer bit-idénticos se ejecutaron además como
grupo y pasaron 5/5. Los dos baselines económicos re-registrados pasaron juntos
2/2 después de revisar primero sus invariantes y la salida obtenida.

## Sanitizers

Se recompilaron y ejecutaron los seis binarios directamente afectados:
`economy`, `buildings`, `gather`, `citizen_task`, `ai_skirmish_eco` y
`ai_skirmish_apertura`.

### AddressSanitizer

Configuración verificada en `build-asan`:
`-fsanitize=address -fno-omit-frame-pointer` y linker
`-fsanitize=address`.

```text
$ ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 ctest --test-dir build-asan -V \
    -R '^(economy|buildings|gather|citizen_task|ai_skirmish_eco|ai_skirmish_apertura)$'
skirmish_eco A: end_tick=1107 winner=1 ai_executions=55
state=f2d313552c5c23aa cont=9e817f78a66239db
skirmish_eco A2: stock_defensor A=500 B=0 Me=0
apertura A: end_tick=9317 winner=1 ai_executions=466
p0_gather=1 p1_gather=1 p1_built=1 p1_trained=1
state=e99b6ca32cf0b78d cont=9670e9e87ba25b50
The following tests passed:
  economy
  buildings
  ai_skirmish_eco
  gather
  citizen_task
  ai_skirmish_apertura
100% tests passed, 0 tests failed out of 6
Total Test time (real) = 785.60 sec
```

Sin diagnósticos de AddressSanitizer.

### UndefinedBehaviorSanitizer

Configuración verificada en `build-ubsan`:
`-fsanitize=undefined -fno-omit-frame-pointer` y linker
`-fsanitize=undefined`.

```text
$ UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build-ubsan -V \
    -R '^(economy|buildings|gather|citizen_task|ai_skirmish_eco|ai_skirmish_apertura)$'
skirmish_eco A: end_tick=1107 winner=1 ai_executions=55
state=f2d313552c5c23aa cont=9e817f78a66239db
skirmish_eco A2: stock_defensor A=500 B=0 Me=0
apertura A: end_tick=9317 winner=1 ai_executions=466
p0_gather=1 p1_gather=1 p1_built=1 p1_trained=1
state=e99b6ca32cf0b78d cont=9670e9e87ba25b50
The following tests passed:
  economy
  buildings
  ai_skirmish_eco
  gather
  citizen_task
  ai_skirmish_apertura
100% tests passed, 0 tests failed out of 6
Total Test time (real) = 302.84 sec
```

Sin diagnósticos de UndefinedBehaviorSanitizer.

## Comprobaciones estáticas

```text
$ git diff --check
(sin salida; exit 0)

$ git diff --name-only -- addons/chunsa_sim/gdextension
(sin salida; exit 0)
```

La búsqueda de `new`, `delete`, allocadores, STL, `float`, `double`, reloj y
entropía sobre `economy.hpp`/`step.hpp` no encontró código ejecutable nuevo
(solo comentarios históricos sobre los gates). El diff no contiene cambios de
serialización/checksum y conserva literalmente `SAVE_FORMAT_VERSION = 13` y
`CHECKSUM_ALGO_VERSION = 8`.

## Desviaciones

No hay desviaciones del contrato normativo. La recolocación del depósito del
fixture `ai_skirmish_eco` es una adaptación de escenario necesaria para que su
mapa sintético satisfaga la nueva precondición espacial y mantenga el propósito
del gate; está fuera de `Step()` y queda registrada por sus nuevos baselines.
