# RESULT — Moral y pánico v1 (Sonnet 5) · Sprint 0.3

Rama `sonnet/morale` desde `main`. Contrato implementado tal cual, sin rediseños.

## Qué toqué

- `addons/chunsa_sim/core/include/chunsa/game_state.hpp`
  - `int32_t morale[ENTITY_HARD_CAP]` y `uint8_t fleeing[ENTITY_HARD_CAP]` tras los componentes de combate.
  - `zero_components`: limpia `morale`/`fleeing` al reciclar un índice.
- `addons/chunsa_sim/core/include/chunsa/step.hpp`
  - Constantes `MORALE_MAX/PANIC/RALLY/DROP/REGEN/RADIUS_CELLS`.
  - `SPAWN_UNIT` en `apply_command`: `morale[i] = MORALE_MAX; fleeing[i] = 0;`.
  - `combat_system`: unidad en pánico no ataca (enfría `atk_cd` si >0, `continue`).
  - `detail::morale_system(g)` (nuevo): cuenta aliados/enemigos en celda+8 vecinas (mismo patrón de `combat_system`), aplica drop/regen con clamp `[0,MORALE_MAX]`, e histéresis pánico/rally sobre `fleeing`.
  - `movement_v1`: rama de huida al inicio del bucle (prioridad máxima sobre flujo/seek) — busca el enemigo vivo más cercano en celda+8 vecinas (mismo patrón/desempate por índice que combate) y se mueve en dirección opuesta con `normalize_v1`; clamp a cota de mundo igual que la rama de flujo.
  - `step()`: `detail::morale_system(g)` se llama entre `combat_system` y el bloque DESTROY (fase 5c).
- `addons/chunsa_sim/core/include/chunsa/checksum.hpp`: `morale[]`/`fleeing[]` (i32/u8, todos los slots, índice ascendente) añadidos tras el bloque de combate en `state_checksum_v1`.
- `addons/chunsa_sim/core/include/chunsa/serialize.hpp`: mismo bloque, mismo orden, en `gs_serialize`/`gs_deserialize` (bloque `(j)` tras combate `(i)`).
- `addons/chunsa_sim/core/include/chunsa/save_io.hpp`: `SAVE_FORMAT_VERSION` 4→5.
- `tests/unit/test_morale.cpp` (nuevo): escenario de desbandada (10 infantería owner 0 cercadas por 80 infantería owner 1 en rejilla densa de 1 tile de paso, dentro de `range_mt=1500`). 300 ticks, 2 corridas frescas.
- `CMakeLists.txt`: target `chunsa_test_morale` + `add_test(NAME morale ...)`.

## Desviación (documentada, no es rediseño)

El contrato permite que el CHECK 3 ("huida efectiva") acepte **cualquiera** de dos ramas: alejamiento medio del centro del enjambre, **o** que casi todas las unidades del bando en desventaja mueran. `SPAWN_UNIT` (código ya existente en `main`, congelado, no tocado) fija `speed_mtpt[i] = 0` siempre — no hay comando en la API que asigne velocidad a unidades de combate. Por tanto, para unidades nacidas vía `SPAWN_UNIT` la rama de huida en `movement_v1` calcula `step_fx = 0` y nunca desplaza, aunque la lógica de dirección/huida esté correctamente implementada (se verá en unidades con `speed_mtpt>0`, p. ej. si se combinaran con `FLOW_MOVE`/`MOVE_TO` sobre unidades tipo `SPAWN_DEBUG`). El test usa la rama "casi todas murieron" (`alive0 <= 2` de 10), que es la que efectivamente ocurre en este escenario, y lo documenta en un comentario junto al `CHECK`. El pánico (CHECK 2) sí se observa con normalidad (`fleeing==1` durante la corrida).

## Verificación

### Build (`nice -n 19`, `-j2`)
0 warnings, `-Werror` limpio.

### `ctest --test-dir build-gcc --output-on-failure`
```
100% tests passed out of 8 (props, golden, state, ring, flow_field, flow_move, combat, morale)
```
`chunsa_test_morale`: `morale: o0_vivos=0 panicked=1 checksum=b5f9d8eeeca77209` → `morale: OK`.

### Gates obligatorios (checksums cambiaron por el nuevo estado; todos PASAN)
```
GOLDEN backend=int128 casos=1074 fallos=0  [OK]
G1 selftest: alloc_delta=0 OK checksum=bf1193fc58c738cf
G3 savetest(save@150): OK state=c426aa4781d8ccc1 cont=465cf4504b4f3b78
G4 savetest(save@9,hold): OK state=dac65d89e1d2da90 cont=bc8b2c0d5e72c17e
G5 verify: OK ai_executions=0 checksum=85b32d763f3c9f7c
```

Determinismo bit-exacto intacto: cero float en estado, cero UB de overflow, cero heap en `step()`, orden de iteración ascendente, desempates por índice menor — sin tocar `flow_field`/`vision`/`sha256`.
