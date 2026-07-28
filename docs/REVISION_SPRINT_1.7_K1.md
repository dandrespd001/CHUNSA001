# Revisión del Arquitecto — K1: modelo de tareas del ciudadano + blindaje de gates (Sprint 1.7)

Fecha: 2026-07-28
Rama: `arch/sprint-1.7-citizen-task` (base `main` @ `a215bea`)
Implementación: GPT-5.6 SOL (`codex -m gpt-5.6-sol`)
Contratos: `docs/briefs/SOL_K1_TAREAS_CIUDADANO_1.7.md` +
`docs/briefs/SOL_K1B_CHECKSUM_V8_UNIVERSAL_1.7.md`
Informe del implementador: `docs/RESULT_SOL_K1_1.7.md`

## Veredicto

**Aprobado e integrado**, tras una ronda de corrección sobre el checksum.

## 1. Lo entregado

Parte A (SPEC-004 §22): `citizen_task` como autoridad única sobre quién escribe
la posición de un ciudadano; `citizen_move_system` como fase propia entre
`movement_v1` y economía/construcción; `movement_v1` **intacto** (sigue
congelado, SPEC-001 §12); transiciones de `MOVE_TO`/`GATHER`/`ASSIGN_BUILD`;
save v13 y checksum v8.

Parte C (deuda de gates): `tests/determinism/baselines.hpp` como punto único de
verdad, y G1/G3/G4 dentro de `ctest` bajo la etiqueta `determinism_gate`.

## 2. El hallazgo: checksum condicional por contenido

La entrega inicial (`4be7110`) resolvía una contradicción **de mi propio brief**
—que exigía simultáneamente subir `CHECKSUM_ALGO_VERSION` a 8 y conservar
bit-idénticos los digests previos— seleccionando el stream según el estado:

```cpp
bool has_live_citizen = /* escanea unit_class == 3 */;
if (has_live_citizen) { h.bytes("CHUNSA_STATE_V8", 15); h.u32(8); }
else                  { h.bytes("CHUNSA_STATE_V7", 15); h.u32(7); }
```

**Rechazado.** Razones, en orden de gravedad:

1. **Ya producía el fallo.** El propio informe documentaba que
   `ai_skirmish_eco` —el escenario cuyo tema es la economía y los aldeanos
   vulnerables— conservaba su digest V7 «porque al checksum final ya no queda
   ningún ciudadano vivo». El algoritmo pasó a depender del desenlace.
2. **La transición es alcanzable en juego normal.** Los aldeanos son atacables
   por decisión del Director (Sprint 1.4). Al morir el último, el dominio del
   checksum cambia de algoritmo a mitad de partida.
3. **`CHECKSUM_ALGO_VERSION = 8` mentía**: un save que anuncia versión 8 podía
   llevar un digest calculado con el stream V7.
4. **Rompe el invariante del versionado**: una versión debe identificar
   exactamente un algoritmo, y sentaba precedente para el próximo campo.

Corregido en `99bc7d5`: V8 universal, sin condicionales, `citizen_task` en el
stream siempre y para todos los slots.

**La contradicción era mía.** Un bump de versión invalida por definición todos
los digests anteriores. Lo que la Parte B quería exigir no era igualdad de
digests sino **igualdad de trayectorias** — y esa quedó probada precisamente en
la corrida `4be7110`, donde los escenarios sin ciudadanos conservaron sus
digests V7 bit-exactos bajo el stream antiguo. Con esa evidencia en mano, el
bump se pudo hacer limpio y re-registrar todo.

Nota de proceso a favor del implementador: SOL **detectó la tensión y la
documentó como desviación** en vez de esconderla. Eso es exactamente la
conducta que el contrato pide.

## 3. Verificación independiente del Arquitecto

No se aceptaron las cifras del informe; se midieron:

```text
BUILD_EXIT=0        (cmake --build build-gcc -j8, 0 avisos)
CTEST_EXIT=0        100% tests passed out of 29
determinism_gate    3 tests
ai_skirmish_apertura  232.73 s  Passed
citizen_task          Passed
```

Comprobaciones dirigidas:

- `grep -rn "CHUNSA_STATE_V7" addons/` → sin resultados.
- `grep -n "has_live_citizen" checksum.hpp` → sin resultados.
- Los gates **aseveran** contra las constantes, no solo imprimen: verificado que
  `test_ai_skirmish{,_eco,_apertura}.cpp` y `cli/main.cpp` referencian
  `determinism_baselines::*`.

Baselines re-registrados: cambiaron **todos** (G1, G3, G4, skirmish, eco), como
corresponde a un bump de algoritmo. La apertura conserva
`c7b04caea8c32e64`/`4ae3ddd1ad5ab4f9` porque ya tenía ciudadanos y por tanto ya
usaba el stream V8 en la versión condicional — internamente consistente.

Invariantes funcionales intactas: apertura `winner=1`, `end_tick=12292`, cuatro
fases; vectores dorados 1074/0.

## 4. Lo que este sprint arregla de fondo

Antes de la Parte C, **`ctest` en verde no probaba bit-identidad**: los
checksums se imprimían, ningún test los comparaba, y G1/G4 ni siquiera estaban
registrados como pruebas. Toda la garantía de no-regresión dependía de que un
humano leyera la salida y la cotejara a mano contra un número guardado en un
markdown. Ahora un cambio de trayectoria rompe la suite sola.

## 5. Deuda que este sprint deja abierta

- **El `.so` de Godot queda obsoleto respecto al kernel.** K1 sube
  `SAVE_FORMAT_VERSION` a 13 y `CHECKSUM_ALGO_VERSION` a 8. El adaptador de la
  rama `gpt/hud-nombres-1.7` se está reconstruyendo **contra el kernel
  anterior**. Al integrar esa rama habrá que **regenerar `demo/bin/libchunsa_godot.so`
  contra `main` ya con K1 dentro**, o la demo cargará un binario desalineado.
- LSan sigue sin poder ejecutarse en este entorno (`ptrace`); ASan y UBSan sí.

## 6. Lección de proceso

Cuarto entregable delegado consecutivo en el que la verificación encuentra algo
que el informe no presentaba como problema. Aquí, además, el defecto nació de
una **contradicción en el contrato que yo escribí**. Dos conclusiones:

1. Mantener la política de no aceptar informes al pie de la letra.
2. Revisar los contratos propios buscando exigencias mutuamente incompatibles
   **antes** de delegarlos. Un brief contradictorio obliga al implementador a
   inventar una resolución, y la que invente será deuda.
