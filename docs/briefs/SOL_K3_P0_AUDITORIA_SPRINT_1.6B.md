# Brief K3 — Cierre de los bloqueantes de la auditoría multimodelo (Sprint 1.6B)

**Modelo asignado:** GPT-5.6 SOL (`codex -m gpt-5.6-sol`)
**Rama base:** `arch/sprint-1.6b-k2` (HEAD `3a2ccd2`)
**Rama de trabajo:** `arch/sprint-1.6b-k3`
**Autoridad del contrato:** Arquitecto Jefe. Este documento es normativo; donde
contradiga al informe de auditoría, manda este documento.

Tú mismo encontraste F-00 y reprodujiste F-01 en la auditoría del 2026-07-27
(`auditorias/CHUNSA001-2026-07-27/respuestas/gpt-5.6-sol-codex.md`). Este brief
te devuelve el trabajo con la semántica ya decidida por el Arquitecto, para que
no tengas que inferirla.

---

## 0. Reglas del proyecto que NO puedes violar

1. **Determinismo bit-exacto.** Cero `float`/`double`, cero reloj de pared,
   cero entropía fuera de `RngStream`. Iteración en índice ascendente,
   desempates por índice más bajo.
2. **`Step()` sin heap ni STL.** Los drivers (`driver.hpp`, `skirmish*.hpp`)
   NO son `Step()` — ahí `std::vector` ya se usa y es legítimo.
3. **`CommandType` es append-only.** No renumerar, no borrar.
4. **`GameState` va en el heap en los tests** (`std::make_unique`), nunca en la
   pila: bajo ctest la pila se desborda y segfaultea.
5. **No toques** `SAVE_FORMAT_VERSION` (12) ni `CHECKSUM_ALGO_VERSION` (7).
   Ver §4, es una decisión de arquitectura ya tomada.
6. **Nada de `assert()`** para validar entrada: desaparece en Release.

---

## 1. F-00 — Heap-buffer-overflow en la ruta de replay (BLOQUEANTE)

### Diagnóstico confirmado

`replay.hpp:185` define `MAX_PER_TICK = 4096u`: el loader acepta batches de
hasta 4096 comandos por tick. Los drivers dimensionan el buffer para el caso de
la IA (`AI_MAX_COMMANDS` = 64) y luego copian el batch del replay sin comprobar
capacidad.

Sitios afectados (los CUATRO, la corrección es sistémica):

| Fichero | Declaración | Escritura sin cota |
|---|---|---|
| `addons/chunsa_sim/core/include/chunsa/driver.hpp` | :141 | :169 |
| `addons/chunsa_sim/core/include/chunsa/skirmish.hpp` | :202 | (ruta de replay equivalente) |
| `addons/chunsa_sim/core/include/chunsa/skirmish_eco.hpp` | :245 | (ruta de replay equivalente) |
| `addons/chunsa_sim/core/include/chunsa/skirmish_apertura.hpp` | :190 | :215 |
| `tests/unit/test_ai_skirmish_apertura.cpp` | :191 | (mismo patrón, arréglalo también) |

### Decisión del Arquitecto: redimensionar, NO rechazar

Un batch de 4096 comandos es **legal**: el loader lo acepta. Rechazarlo
introduciría un modo de fallo nuevo que haría irreproducible un replay válido.
La corrección es que el consumidor honre lo que el loader admite.

En la rama de replay de cada driver, **antes del bucle de copia**:

```cpp
// Auditoría multimodelo 2026-07-27, F-00: el loader admite hasta
// MAX_PER_TICK (4096) comandos por tick (replay.hpp), muy por encima de la
// cota de la IA con la que se dimensiona el buffer. Sin esta línea un replay
// legal escribe fuera del heap. Amortizado O(1): los batches reales del
// escenario son < 72 y jamás redimensionan, así que ninguna trayectoria
// existente cambia.
if (b.size() > batch.size()) batch.resize(b.size());
```

Restricciones:
- El buffer de la IA sigue acotado por `AI_MAX_COMMANDS`; **no** relajes esa
  cota ni redimensiones en la ruta de la IA.
- `resize` ocurre fuera de `Step()`, no es entropía y no altera el estado del
  juego. El determinismo se mantiene.
- Si en algún driver la ruta de replay usa un índice distinto de `b.size()`,
  adapta la comprobación pero conserva la semántica: **capacidad ≥ lo que se
  va a escribir, siempre**.

### Pruebas obligatorias

Fichero nuevo o ampliación de `tests/unit/test_replay_v3.cpp`. Batches de
**72, 73, 4096 y 4097** comandos en un tick, alimentados por la ruta de replay
de al menos `driver.hpp` y `skirmish_apertura.hpp`:

- 72 y 73 → ambos deben ejecutarse sin fallo (73 es el que hoy desborda).
- 4096 → debe ejecutarse sin fallo.
- 4097 → el **loader** debe rechazarlo limpiamente (comprueba el código de
  error que ya devuelve `replay.hpp`); el driver no debe llegar a copiarlo.

Ejecuta esas pruebas bajo **ASan y UBSan** y pega la salida en el informe.

---

## 2. F-01 — Conversión de recurso al redirigir carga parcial (BLOQUEANTE)

### Diagnóstico confirmado

`step.hpp` (manejador de `GATHER`, ~:683-688) cambia `eco_assigned_deposit` y
fuerza `EcoState::SEEK` **sin tocar** `eco_carry` ni `eco_carry_resource_idx`.
Luego `economy.hpp` (`HARVEST`, ~:218-229) suma al `carry` existente y
**sobrescribe** `carry_resource_idx` con el recurso del depósito actual.

Resultado reproducido por tu sonda: `before carry=10 resource=0` →
`after carry=15 resource=1 harvested=5`. Diez unidades de A se acreditan como B
en el dropoff. Rompe conservación de recursos. Lo dispara tanto una orden
humana como la reasignación automática de `ai_scan_economy`.

### Contrato nuevo (enmienda del Arquitecto a SPEC-004 §18)

El manejador de `GATHER`, tras resolver `found` (el índice de depósito por
radio) y **antes** de escribir el estado, distingue tres casos:

| Condición | `eco_assigned_deposit` | `eco_state` |
|---|---|---|
| `eco_carry[ci] == 0` | `found` | `SEEK` |
| `carry > 0` y `deposits[found].resource_idx == eco_carry_resource_idx[ci]` | `found` | `SEEK` |
| `carry > 0` y `resource_idx != carry_resource_idx` | `found` | **`RETURN`** |

En los tres casos `g.build_target[ci] = BUILD_NO_TARGET` (sin cambio) y el
orden contractual de rechazos se conserva **exactamente** como está hoy: esta
lógica va DESPUÉS de todas las validaciones, en el punto donde hoy se escribe
`SEEK`.

### Por qué esto no necesita campos nuevos (verificado por el Arquitecto)

En `economy.hpp`, `RETURN` al llegar al dropoff hace `carry = 0`,
`did_dropoff = true` y `state = SEEK`, **conservando `assigned_deposit`**.
En el `SEEK` siguiente, `need_reassign` es falso si el depósito nuevo es
válido y `remaining > 0` — así que el aldeano marcha directo al depósito que
pidió el jugador. **`SAVE_FORMAT_VERSION` 12 queda intacto.**

El caso "mismo recurso" mantiene `SEEK` a propósito: la carga sigue siendo
homogénea, no hay corrupción posible, y forzar un viaje de vuelta sería una
regresión de jugabilidad injustificada.

### Pruebas obligatorias (`tests/unit/test_gather.cpp`)

1. Carga parcial de A + `GATHER(B)` → **no** convierte A en B: tras el
   siguiente dropoff, `player_stock` del recurso A sube exactamente la carga
   previa y el de B no la incluye.
2. Carga parcial de A + `GATHER(A)` → sigue en `SEEK` y sigue llenando la
   misma clase de carga.
3. Carga completa (`carry == ECO_CARRY_CAP`) + redirección a otro recurso →
   deposita antes de buscar el nuevo.
4. La capa de IA (`ai_scan_economy`) redirige un donante con `carry > 0` sin
   corrupción — el mismo aserto de conservación.
5. `save_game`/`load_game` **y** replay durante esa transición conservan
   cantidad y tipo (`eco_carry`, `eco_carry_resource_idx`, `eco_state`,
   `eco_assigned_deposit`).

---

## 3. P1 — Cerrar el DoD con evidencia fuerte (mismo pase)

**F-04.** En `tests/unit/test_ai_skirmish_apertura.cpp`, sustituir
`CHECK(out.winner != 0xFF)` por `CHECK(out.winner == 1u)`. El escenario
documenta a Rome/owner 1 como la IA atacante; hoy una regresión que declarase
ganador a Egipto dejaría la aserción en verde.

**F-06.** El `CHECK(g->n_deposits == 12u)` que añadí en la revisión sigue sin
confirmar en git y es blando (el helper sigue devolviendo un estado incorrecto).
Conviértelo en **pre-flight dedicado**: que el helper devuelva `nullptr` ante
fallo y que el test aborte ahí, sin ruido derivado en los subtests.

**F-02.** El subtest de "save a mitad de recolección"
(`test_ai_skirmish_apertura.cpp:171-221`) busca el primer
`player_stock[1][*] > 0`, que es un dropoff **ya completado** — puede quedar
verde guardando en una frontera limpia entre ciclos, sin cubrir los campos
transitorios que el DoD pretende blindar. Elige el tick de guardado por
condición **observable**:

- algún ciudadano de Rome con `eco_carry > 0`, **o** `eco_state` en
  `HARVEST`/`RETURN`; y
- `game_over == 0` y recolección aún activa.

Antes de guardar, registra índice, estado, depósito, carga y tipo. Tras cargar,
compara esos campos **y** el checksum de continuación.

---

## 4. Versionado: decisión ya tomada, no la re-abras

La corrección de F-01 **cambia trayectorias** en cualquier escenario donde un
aldeano con carga parcial sea redirigido a otro recurso — incluida la apertura,
porque la capa económica de la IA lo hace.

**No se sube `SAVE_FORMAT_VERSION` ni `CHECKSUM_ALGO_VERSION`.** El formato
serializado y el algoritmo de checksum son idénticos; lo que cambia es el
comportamiento del kernel, que es exactamente lo que la versión de checksum
**no** cubre. Además `GATHER` no ha llegado nunca a `main`: no existe ningún
replay publicado que se rompa. Un replay que divergiera fallaría de forma
ruidosa (replay v3 liga contra el checksum de estado), nunca en silencio.

Lo que **sí** debes hacer es **documentar los hashes pre/post** en el informe:

- **Deben quedar bit-idénticos** (no tocan `GATHER`): G1 `fefa48125dd35736`,
  G4 `774316057e5667fb` / `d52ac0019700684f`, skirmish `3f64d3223b74d477`.
  Si alguno cambia, **para y repórtalo**: es una regresión, no un efecto
  esperado.
- **Puede cambiar**: la apertura, hoy `end_tick=12480 winner=1
  ai_executions=624 state=cf57ea3ca2266627 cont=5b69fbcea73bb432`. Si cambia,
  registra el valor nuevo y explica qué redirección lo provocó.

---

## 5. Definición de hecho

- [ ] `cmake --build build-gcc -j2` limpio, sin avisos nuevos.
- [ ] `ctest --test-dir build-gcc --output-on-failure` → **todas** en verde
      (25 actuales + las nuevas).
- [ ] Pruebas 72/73/4096/4097 en verde bajo **ASan y UBSan**, salida pegada.
- [ ] Las 5 pruebas de carga/redirección de §2 en verde.
- [ ] G1, G4 y skirmish bit-idénticos a los hashes de §4.
- [ ] Hashes de la apertura registrados (pre y post).
- [ ] El escenario del DoD sigue concluyendo con `winner == 1` y las 4 fases.
- [ ] Informe en `docs/RESULT_SOL_K3_P0.md` con: qué cambiaste y dónde, la
      salida de los sanitizers, la tabla de hashes pre/post, y **cualquier
      desviación de este contrato con su justificación**.

**Verifica sin pipes que enmascaren el código de salida.** Usa
`cmd; echo $?`, nunca `cmd | tail`.

**No hagas merge a `main`.** Deja la rama lista y el informe escrito; la
integración la hace el Arquitecto.
