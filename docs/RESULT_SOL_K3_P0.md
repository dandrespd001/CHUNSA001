# RESULT — K3: cierre de bloqueantes de auditoría (Sprint 1.6B)

Fecha: 2026-07-28

Rama: `arch/sprint-1.6b-k3`
Contrato: `docs/briefs/SOL_K3_P0_AUDITORIA_SPRINT_1.6B.md`

## Resultado

Se cerraron F-00 y F-01 con la semántica fijada por el Arquitecto y se
endurecieron F-02, F-04 y F-06. No se modificaron
`SAVE_FORMAT_VERSION` (12), `CHECKSUM_ALGO_VERSION` (7) ni la numeración de
`CommandType`. No se hizo merge a `main`.

## Cambios

### F-00 — capacidad de los consumidores de replay

Antes de copiar un batch de replay se garantiza
`batch.size() >= replay_batch.size()` en:

- `addons/chunsa_sim/core/include/chunsa/driver.hpp`
- `addons/chunsa_sim/core/include/chunsa/skirmish.hpp`
- `addons/chunsa_sim/core/include/chunsa/skirmish_eco.hpp`
- `addons/chunsa_sim/core/include/chunsa/skirmish_apertura.hpp`

El `resize` existe solo en la rama de replay de los drivers, fuera de
`Step()`. La ruta de IA conserva la cota `AI_MAX_COMMANDS`; no se redimensiona
ni se relaja. La sonda manual de apertura mantiene su guard de capacidad y
ahora comprueba explícitamente las cotas `8` y `AI_MAX_COMMANDS`.

`tests/unit/test_replay_v3.cpp` escribe y vuelve a cargar replays v3 de un
tick con 72, 73 y 4096 comandos. Cada batch legal atraviesa tanto
`driver.hpp` como `skirmish_apertura.hpp`. El caso 4097 comprueba
`replay_load(...) == 1` y que el batch permanece vacío, por lo que ningún
driver llega a copiarlo.

### F-01 — conservación de carga al redirigir GATHER

En `addons/chunsa_sim/core/include/chunsa/step.hpp`, después de resolver y
validar el depósito:

- carga cero: asigna el depósito y entra en `SEEK`;
- carga del mismo recurso: asigna el depósito y entra en `SEEK`;
- carga de recurso distinto: asigna el depósito y entra en `RETURN`.

`build_target = BUILD_NO_TARGET` y el orden de rechazos permanecen intactos.
`RETURN` descarga usando `eco_carry_resource`, conserva el nuevo
`eco_assigned_deposit` y luego pasa a `SEEK`.

`tests/unit/test_gather.cpp` cubre:

1. carga parcial A + `GATHER(B)`: descarga exactamente A, sin acreditar B;
2. carga parcial A + `GATHER(A)`: continúa en `SEEK/HARVEST` y suma A;
3. carga completa A + `GATHER(B)`: descarga antes de buscar B;
4. `ai_scan_economy` elige un donante cargado, cuya orden diferida se aplica
   sin conversión;
5. save v12/load y replay v3 en `RETURN`: conservan carga, tipo, estado,
   depósito y checksum; las tres continuaciones descargan igual.

### F-02, F-04 y F-06 — evidencia del DoD

- F-02: el save se elige por estado observable de un ciudadano de Rome:
  `carry > 0` o estado `HARVEST/RETURN`, depósito vivo, `game_over == 0`.
  En la frontera observada se registran y, tras load, se comparan índice,
  estado, depósito, carga, tipo y checksum de continuación.
- F-04: el ganador exigido es `winner == 1u` (Rome), también en la prueba de
  doble corrida.
- F-06: `make_apertura_state` devuelve `nullptr` si no obtiene los 12
  depósitos reales. Un pre-flight dedicado se ejecuta antes de los subtests
  y aborta el binario si falla.

Frontera transitoria observada:

```text
apertura C save-boundary: tick=142 citizen=7 state=1 deposit=5 carry=0 resource=0 cont=5c76c006c8dd168f
```

`state=1` es `HARVEST`; por tanto el save ya no cae después de un dropoff
completo.

## Build y suite

```text
$ cmake --build build-gcc -j2
[100%] Built target chunsa_test_ai_skirmish_apertura
```

Build GCC limpio, sin avisos nuevos.

```text
$ ctest --test-dir build-gcc --output-on-failure
100% tests passed out of 25
Total Test time (real) = 306.14 sec
```

Las pruebas nuevas amplían los binarios `replay_v3` y `gather`; por eso el
conteo de ejecutables CTest sigue siendo 25.

Gates funcionales dirigidos:

```text
$ ./build-gcc/chunsa_test_replay_v3
replay_v3: OK

$ ./build-gcc/chunsa_test_gather
gather: OK

$ ./build-gcc/chunsa_test_ai_skirmish_apertura
apertura A: end_tick=12292 winner=1 ai_executions=615 p0_gather=1 p1_gather=1 p1_built=1 p1_trained=1 state=71774aaa9c166103 cont=b2294197e9964ba5
apertura C save-boundary: tick=142 citizen=7 state=1 deposit=5 carry=0 resource=0 cont=5c76c006c8dd168f
ai_skirmish_apertura: OK
```

## Sanitizers

Configuración ASan:

```text
$ cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug '-DCMAKE_CXX_FLAGS=-fsanitize=address -fno-omit-frame-pointer' '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address'
-- Build files have been written to: .../build-asan

$ cmake --build build-asan --target chunsa_test_replay_v3 chunsa_test_gather -j2
[100%] Built target chunsa_test_replay_v3
[100%] Built target chunsa_test_gather

$ ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 ./build-asan/chunsa_test_replay_v3
dump checkpoint tick=10 sha256=b55f3622adf2d205c3e7142a8a9334c85d3d882c5496bded2b40e9482b523298 len=141737
dump checkpoint tick=60 sha256=1173b1ea5de2910eb6b4aacea754f717cbb545a4a3a4a58b0a28ae263084ca10 len=142857
dump checkpoint tick=150 sha256=df2ef899b63d6bd936d61ae74fc7bd688a4fe7f442e475e4954c703c8a31fca9 len=144537
replay_v3: OK

$ ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 ./build-asan/chunsa_test_gather
gather: OK
```

Configuración UBSan:

```text
$ cmake -S . -B build-ubsan -DCMAKE_BUILD_TYPE=Debug '-DCMAKE_CXX_FLAGS=-fsanitize=undefined -fno-omit-frame-pointer' '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=undefined'
-- Build files have been written to: .../build-ubsan

$ cmake --build build-ubsan --target chunsa_test_replay_v3 chunsa_test_gather -j2
[100%] Built target chunsa_test_replay_v3
[100%] Built target chunsa_test_gather

$ UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ./build-ubsan/chunsa_test_replay_v3
dump checkpoint tick=10 sha256=b55f3622adf2d205c3e7142a8a9334c85d3d882c5496bded2b40e9482b523298 len=141737
dump checkpoint tick=60 sha256=1173b1ea5de2910eb6b4aacea754f717cbb545a4a3a4a58b0a28ae263084ca10 len=142857
dump checkpoint tick=150 sha256=df2ef899b63d6bd936d61ae74fc7bd688a4fe7f442e475e4954c703c8a31fca9 len=144537
replay_v3: OK

$ UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ./build-ubsan/chunsa_test_gather
gather: OK
```

ASan no reportó accesos inválidos y UBSan no reportó comportamiento
indefinido. LeakSanitizer no puede ejecutarse bajo el `ptrace` del entorno:

```text
LeakSanitizer has encountered a fatal error.
LeakSanitizer does not work under ptrace (strace, gdb, etc)
```

Por ello se desactivó solo la detección de fugas (`detect_leaks=0`); la
instrumentación AddressSanitizer que reproduce F-00 permaneció activa.

## Hashes pre/post

| Gate | Pre (contrato) | Post | Resultado |
|---|---|---|---|
| G1 state | `fefa48125dd35736` | `fefa48125dd35736` | bit-idéntico |
| G4 state | `774316057e5667fb` | `774316057e5667fb` | bit-idéntico |
| G4 continuation | `d52ac0019700684f` | `d52ac0019700684f` | bit-idéntico |
| skirmish state | `3f64d3223b74d477` | `3f64d3223b74d477` | bit-idéntico |
| skirmish continuation | `92ec9aa95374a429` | `92ec9aa95374a429` | bit-idéntico |
| apertura state | `cf57ea3ca2266627` | `71774aaa9c166103` | cambio esperado |
| apertura continuation | `5b69fbcea73bb432` | `b2294197e9964ba5` | cambio esperado |

Salidas:

```text
G1 selftest: alloc_delta=0 OK checksum=fefa48125dd35736
G4 savetest(save@200): OK state=774316057e5667fb cont=d52ac0019700684f ai_executions=30
skirmish: OK end_tick=1226 game_over=1 winner=1 ai_executions=61 state=3f64d3223b74d477 cont=92ec9aa95374a429 fatal=NONE alloc_delta_total=2
GOLDEN backend=int128 casos=1074 fallos=0  [OK]
```

La apertura cambia de `end_tick=12480`, `ai_executions=624` a
`end_tick=12292`, `ai_executions=615`. La causa es la redirección adaptativa
de `ai_scan_economy`: cuando toma un donante que aún transporta un recurso y
lo dirige a un depósito de otro, el nuevo contrato hace primero `RETURN`.
Eso evita la conversión de carga, cambia stocks/timing de las decisiones
posteriores y, por tanto, la trayectoria. El resultado contractual se
mantiene: `winner=1`, fin antes de 36000 ticks y las cuatro fases observadas.

## Desviaciones y notas

- No hay desviaciones funcionales del contrato.
- Limitación del entorno: LeakSanitizer no funciona bajo `ptrace`; se
  ejecutó ASan con detección de fugas desactivada, manteniendo activa la
  detección de overflows/use-after-free que cubre F-00.
- El directorio ajeno `.claude/`, ya presente y sin seguimiento al comenzar,
  se preservó y no forma parte de los commits.
