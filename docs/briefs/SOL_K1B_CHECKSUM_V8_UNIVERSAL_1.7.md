# Corrección K1-B — El checksum V8 debe ser universal (Sprint 1.7)

**Modelo:** GPT-5.6 SOL (`codex -m gpt-5.6-sol`)
**Rama:** `arch/sprint-1.7-citizen-task` (continúa sobre `8b458d7`)
**Alcance:** quirúrgico. No toques §22, ni las pruebas, ni los sistemas.

---

## 0. El error fue mío, no tuyo

Mi brief exigía dos cosas incompatibles: subir `CHECKSUM_ALGO_VERSION` a 8 **y**
conservar bit-idénticos los digests de G1/G3/G4/skirmish. Un bump de versión de
checksum **invalida por definición todos los digests anteriores**. No había
forma de cumplir ambas.

Detectaste la tensión y la documentaste con honestidad en vez de esconderla —
eso está bien hecho. La resolución que elegiste, sin embargo, no puedo
aceptarla. Abajo el porqué y el arreglo.

---

## 1. Por qué se rechaza el checksum condicional

`checksum.hpp:129-142` selecciona el stream según el contenido del estado:

```cpp
bool has_live_citizen = /* escanea unit_class == 3 */;
if (has_live_citizen) { h.bytes("CHUNSA_STATE_V8", 15); h.u32(8); }
else                  { h.bytes("CHUNSA_STATE_V7", 15); h.u32(7); }
```

Cuatro razones, en orden de gravedad:

**1.1 — Ya está produciendo el fallo, no es hipotético.** Tu propio informe,
línea 77:

> `ai_skirmish_eco` … Idéntico. La trayectoria funcional no cambió y **al
> checksum final ya no queda ningún ciudadano vivo**

El escenario cuyo tema es *la economía y los ciudadanos vulnerables* se hashea
como **V7**. El algoritmo pasó a depender del desenlace de la partida.

**1.2 — La transición es alcanzable en juego normal.** Por decisión del
Director en el Sprint 1.4, los aldeanos son atacables. Una partida puede pasar
de «tiene ciudadanos» a «no tiene» al morir el último. En ese tick exacto el
dominio del checksum **cambia de algoritmo a mitad de partida**.

**1.3 — El campo `CHECKSUM_ALGO_VERSION = 8` miente.** Un save que anuncia
versión 8 puede llevar un digest calculado con el stream V7. Cualquier
herramienta o gate que confíe en ese número queda engañado.

**1.4 — Rompe el invariante que da sentido al versionado.** Una versión de
algoritmo debe identificar **exactamente un** algoritmo. Con la rama, «V8» son
dos funciones distintas seleccionadas por un predicado sobre el estado. Además
sienta un precedente: el próximo campo que se añada heredará la misma excusa.

---

## 2. Qué quería decir realmente la Parte B

La exigencia de bit-identidad **no era una afirmación sobre digests**, era una
afirmación sobre **trayectorias**: demostrar que §22 no alteró el
comportamiento de los escenarios sin ciudadanos.

Y esa demostración **ya la produjiste**. Al conservarse los digests bajo el
stream V7, quedó probado que las trayectorias de G1/G3/G4/skirmish son
idénticas. Esa evidencia está en tu informe y es exactamente lo que hacía falta.

Por tanto ahora podemos bumpear limpio: la evidencia de no-regresión ya existe,
y los digests nuevos son consecuencia del cambio de algoritmo, no de un cambio
de comportamiento.

---

## 3. El arreglo

**3.1** En `addons/chunsa_sim/core/include/chunsa/checksum.hpp`: elimina la rama
y el escaneo `has_live_citizen`. El stream es **siempre** V8:

```cpp
h.bytes("CHUNSA_STATE_V8", 15);
h.u32(CHECKSUM_ALGO_VERSION);   // 8, sin condicionales
```

`citizen_task` entra en el stream **siempre**, para todos los slots, igual que
cualquier otro componente. Sin excepciones por contenido.

Actualiza el comentario de cabecera (líneas ~94-99): debe decir que V8 es
universal y que el bump invalidó todos los baselines previos por diseño.

**3.2** Re-registra **todos** los baselines en `tests/determinism/baselines.hpp`
con sus valores V8 medidos: G1, G3, G4, `ai_skirmish`, `ai_skirmish_eco` y
`ai_skirmish_apertura` (estado y continuación de cada uno).

**3.3** En el comentario de cada constante que cambie, y en el mensaje del
commit, deja constancia de que **el cambio de digest se debe al bump V7→V8 y no
a un cambio de trayectoria**, y que la no-regresión de comportamiento quedó
probada en la corrida previa (commit `4be7110`), donde esos mismos escenarios
conservaron sus digests V7 bit-exactos.

**3.4** Verifica que se mantienen invariantes **funcionales**, que son las que
de verdad importan ahora:

- G1: `alloc_delta == 0`.
- Apertura: `winner == 1`, `end_tick == 12292`, las cuatro fases.
- `ai_skirmish`: `winner == 1`, mismo `end_tick` que antes de tu cambio.
- Vectores dorados: 1074 casos, 0 fallos.

Si alguna de estas cambia, **para y repórtalo**: eso sí sería regresión.

---

## Definición de hecho

- [ ] `grep -rn "CHUNSA_STATE_V7" addons/` no devuelve nada.
- [ ] No queda ningún condicional por contenido en la selección del stream.
- [ ] `cmake --build build-gcc -j2` limpio.
- [ ] `ctest --test-dir build-gcc --output-on-failure` → 29/29.
- [ ] Baselines re-registrados con los valores V8 y justificados.
- [ ] Invariantes funcionales de §3.4 verificadas.
- [ ] Informe: añade una sección **«Corrección K1-B»** a
      `docs/RESULT_SOL_K1_1.7.md` con la tabla de baselines V7→V8 y la
      confirmación de las invariantes funcionales.

**Sin pipes que enmascaren el código de salida**: `cmd; echo $?`.

**No afirmes haber ejecutado nada que no hayas ejecutado.**

**No hagas merge a `main`.**
