# Brief K1 — Modelo de tareas del ciudadano + blindaje de gates (Sprint 1.7B)

**Modelo:** GPT-5.6 SOL (`codex -m gpt-5.6-sol`)
**Rama base:** `main` @ `849928f`
**Rama de trabajo:** `arch/sprint-1.7-citizen-task`
**Contrato normativo:** `docs/specs/SPEC-004_SISTEMAS_PARTIDA.md` **§22**
(Parte IV). Léelo entero antes de tocar nada. Este brief no lo resume: lo
complementa con el cómo.

Trabajo de **kernel** (`addons/chunsa_sim/core/`) y de **gates**. **No toques
`addons/chunsa_sim/gdextension/`** — el adaptador va en otra rama en paralelo y
chocaríais.

---

## 0. Reglas del proyecto, innegociables

1. **Determinismo bit-exacto.** Cero `float`/`double`, cero reloj, cero entropía
   fuera de `RngStream`. Iteración ascendente por índice, desempate por índice
   más bajo.
2. **`Step()` sin heap ni STL.**
3. **`movement_v1` sigue CONGELADO (SPEC-001 §12). NO lo modifiques.** §22.1 lo
   dice explícitamente: los ciudadanos siguen fuera de él.
4. **`CommandType` es append-only.**
5. **`GameState` en el heap en los tests** (`std::make_unique`); en la pila
   segfaultea bajo ctest.
6. Nada de `assert()` para validar entrada de datos.

---

## Parte A — Implementar SPEC-004 §22

### A.1 Estado nuevo

`citizen_task[ENTITY_HARD_CAP]` (`uint8_t`), valores `IDLE=0 MOVE=1 GATHER=2
BUILD=3`. Persistido y dentro del checksum.

`gs_init`: `GATHER` para `unit_class == 3`, `IDLE` para el resto (§22.4).
Cuidado con el spawn de ciudadanos en runtime (`SPAWN_CITIZEN`, `TRAIN_UNIT` de
clase 3): también deben nacer en `GATHER`, coherente con §22.4.

### A.2 La tarea es la única autoridad

§22.1: en un tick, **a lo sumo un sistema** escribe `pos_x/pos_y` de un
ciudadano. Cada sistema comprueba `citizen_task` al entrar y sale si no es la
suya:

- `citizen_move_system` → solo `MOVE`
- `economy_system` → solo `GATHER` (hoy entra por `build_target ==
  BUILD_NO_TARGET`; eso pasa a ser dato, no selector)
- `construction_system` → solo `BUILD`

Orden de fases: `citizen_move_system` **antes** de `economy_system` y de
`construction_system`.

### A.3 `citizen_move_system` (§22.3)

Locomoción **idéntica en forma** a la rama de aproximación de
`economy.hpp::try_move`: normalize + step entero, con snap si el paso cubre la
distancia restante. Al llegar (`<= arrive_r_sq`): `citizen_task = IDLE`,
`vel = 0`.

**Línea recta, sin pathfinding.** §22.3 lo justifica: `movement_v1` mueve a los
militares con `MOVE_TO` también en recta, y solo existe **un** flow field global
(`g.flow`, `g.flow_goal_cell`), no uno por unidad. No inventes pathfinding aquí.

### A.4 Transiciones (§22.2)

- `MOVE_TO` sobre ciudadano propio vivo y destino en cota: `citizen_task = MOVE`,
  `tgt_x/tgt_y = destino`, `build_target = BUILD_NO_TARGET`. **`eco_assigned_deposit`,
  `eco_carry` y `eco_carry_resource` NO se tocan** — sigue transportando mientras
  camina.
- `GATHER`: `citizen_task = GATHER` + las reglas de §18 **íntegras**, incluida
  la regla de carga (el caso «recurso distinto ⇒ `RETURN`»). No la rompas.
- `ASSIGN_BUILD`: `citizen_task = BUILD`.
- Fin de `MOVE` / obra completada o perdida / `GATHER` sin depósito alcanzable
  ⇒ `IDLE`.

**Ojo al orden de validación de los comandos: no lo alteres.** `MOVE_TO` hoy
valida handle vivo/propio y cota del mundo; añade el efecto sobre ciudadanos
**sin** cambiar qué se rechaza ni en qué orden. Un ciudadano con `MOVE_TO` fuera
de cota se sigue rechazando con `MALFORMED`.

### A.5 Versionado (§22.5)

- `SAVE_FORMAT_VERSION` **12 → 13**
- `CHECKSUM_ALGO_VERSION` **7 → 8**, etiqueta `CHUNSA_STATE_V8`
- `citizen_task` entra en la serialización y en el checksum.

---

## Parte B — Baselines: qué debe y qué no debe cambiar

**Regla dura:** los escenarios **sin ciudadanos** deben quedar
**bit-idénticos**. Si alguno se mueve, **para y repórtalo**: significa que has
tocado algo que no debías.

| Gate | Baseline actual | Expectativa |
|---|---|---|
| G1 `run --selftest-g1` | `fefa48125dd35736` | **bit-idéntico** (sin ciudadanos) |
| G4 `savetest --ai` | `774316057e5667fb` / `d52ac0019700684f` | **bit-idéntico** |
| `ai_skirmish` | `3f64d3223b74d477` / `92ec9aa95374a429` | **bit-idéntico** |
| Vectores dorados | 1074 casos, 0 fallos | **idéntico** |
| `ai_skirmish_eco` | `d610feef89ed9c65` / `5e1527e0921edf27` | puede cambiar (tiene ciudadanos) |
| `ai_skirmish_apertura` | `71774aaa9c166103` / `b2294197e9964ba5`, `end_tick=12292` | puede cambiar |

Para los que cambien: **registra el valor nuevo y explica la causa**. El
resultado contractual de la apertura debe mantenerse: `winner == 1`, final por
debajo de 36000 ticks, las cuatro fases observadas.

---

## Parte C — Blindaje de gates (deuda autorizada por el Director)

Esta parte es **tan obligatoria como la A**. Motivo: los checksums de
determinismo **se imprimen pero no se aseveran en ningún sitio** — lo verifiqué
por `grep`: ninguno de esos literales aparece en `tests/`, `addons/` ni
`CMakeLists.txt`. Y **G1 y G4 ni siquiera están en `ctest`**: `add_test` no los
incluye, y `scripts_ci/local_gates.sh` corre `golden` + `ctest` pero nunca
`run --selftest-g1` ni `savetest --ai`.

Consecuencia hoy: **`ctest` en verde NO prueba bit-identidad**, y toda la
garantía de no-regresión depende de que un humano lea la salida y la compare a
mano contra un número que vive en un markdown. Vamos a invalidar esos baselines
en este mismo sprint, así que es el momento exacto de arreglarlo.

### C.1 Baselines como constantes aserveradas

Crea un punto único de verdad, `tests/determinism/baselines.hpp` (o el nombre
que encaje con la convención del repo), con los valores esperados como
constantes con nombre, cada una con un comentario de una línea que diga **qué
escenario** cubre.

Cada gate compara su resultado contra su constante y **falla ruidosamente** con
un mensaje que muestre esperado vs obtenido. No basta con imprimir.

### C.2 Los gates entran en `ctest`

Añade `add_test` para G1 (`run --selftest-g1`), G3 (`savetest`) y G4
(`savetest --ai`). Deben fallar el test si el checksum no coincide o si
`alloc_delta != 0` en G1.

### C.3 Procedimiento de actualización documentado

Un baseline **debe** poder cambiar cuando un cambio de trayectoria esté
justificado; lo que no puede es cambiar en silencio. Documenta en el propio
fichero de baselines, en prosa breve: cómo se regenera un valor, y la exigencia
de que todo cambio se justifique en el commit que lo introduce.

### C.4 Añade `ai_skirmish_eco` a la tabla

Es un gate cuyo baseline **no estaba anotado en ninguna parte** — lo descubrí
midiendo. Que quede aserverado como los demás.

---

## Definición de hecho

- [ ] `cmake --build build-gcc -j2` limpio, sin avisos nuevos.
- [ ] `ctest --test-dir build-gcc --output-on-failure` → **todo en verde**,
      incluidos los gates nuevos de C.2.
- [ ] Los gates de la tabla de Parte B que deben ser bit-idénticos lo son.
- [ ] Los que cambian están re-registrados **y justificados**.
- [ ] Pruebas nuevas del §22, como mínimo:
      - `MOVE_TO` sobre ciudadano lo mueve de verdad y termina en `IDLE`;
      - `MOVE_TO` **no** destruye `eco_carry` ni `eco_carry_resource`;
      - `GATHER` tras un `MOVE` interrumpido respeta la regla de carga de §18;
      - `ASSIGN_BUILD` durante `MOVE` cancela el movimiento;
      - dos sistemas nunca escriben `pos` del mismo ciudadano en un tick
        (aserto directo del invariante de §22.1);
      - save v13 / load y replay conservan `citizen_task`;
      - un ciudadano sin depósito alcanzable acaba en `IDLE`, no girando en
        `SEEK`.
- [ ] ASan y UBSan verdes sobre los binarios tocados; pega la salida.
- [ ] Informe en `docs/RESULT_SOL_K1_1.7.md`: qué cambiaste y dónde, tabla de
      baselines pre/post con justificación de cada cambio, salida de los
      sanitizers, y **cualquier desviación del contrato con su razón**.

**Verifica sin pipes que enmascaren el código de salida**: `cmd; echo $?`, nunca
`cmd | tail`.

**No afirmes haber ejecutado nada que no hayas ejecutado.** Si algo no se puede
verificar en este entorno, dilo: es información útil. Inventarlo destruye la
confianza en todo el informe.

**No hagas merge a `main`.** Deja la rama y el informe; integra el Arquitecto.
