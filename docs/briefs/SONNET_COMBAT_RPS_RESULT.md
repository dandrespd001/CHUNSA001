# RESULT — Combate RPS v1 (Sonnet 5)

Rama: `sonnet/combat-rps` (desde `main`). Contrato: `docs/briefs/SONNET_COMBAT_RPS.md`, implementado tal cual, sin rediseño. Cero desviaciones.

## Qué se tocó

- `addons/chunsa_sim/core/include/chunsa/game_state.hpp`: nuevos campos de combate en `GameState` (`hp`, `max_hp`, `attack`, `range_mt`, `unit_class`, `atk_cd`), tras los componentes de flujo. `zero_components` los limpia al reciclar un slot.
- `addons/chunsa_sim/core/include/chunsa/commands.hpp`: `CommandType::SPAWN_UNIT = 5` (append-only). `CmdPayload` gana `hp`, `attack`, `range_mt`, `unit_class` al final.
- `addons/chunsa_sim/core/include/chunsa/step.hpp`: `ATK_COOLDOWN_TICKS = 10`. `apply_command` — nuevo `case SPAWN_UNIT` (como `SPAWN_DEBUG` + valida `hp>0 && attack>=0 && range_mt>=0 && unit_class<=2`, si no `MALFORMED`). `rps_mult_bp(atk_class, tgt_class)` — tabla congelada en basis points. `combat_system(g)` (en `namespace detail`, junto a `movement_v1`) — por unidad viva en orden ascendente: cooldown decreciente, búsqueda de enemigo más cercano en rango (celda propia + 8 vecinas del spatial hash, desempate por menor índice), daño entero inmediato, marca muerte + destroy_batch si `hp<=0`. Llamada `detail::combat_system(g)` insertada en `step()` después del bloque de visión y antes de DESTROY.
- `addons/chunsa_sim/core/include/chunsa/checksum.hpp`: `state_checksum_v1` — añade `hp/max_hp/attack/range_mt` (i32), `unit_class` (u8), `atk_cd` (u16), por índice ascendente sobre todos los slots, tras el bloque de flujo.
- `addons/chunsa_sim/core/include/chunsa/serialize.hpp`: `gs_serialize`/`gs_deserialize` — mismo orden que el checksum, tras el bloque de flujo. En `pending` (`ScheduledCommand`), los 4 campos nuevos de `CmdPayload` se serializan tras `speed_mtpt`. Bound de `type_raw` ampliado de `> 4` a `> 5` (`CommandType ∈ {1..5}`).
- `addons/chunsa_sim/core/include/chunsa/save_io.hpp`: `SAVE_FORMAT_VERSION` 3 → 4.
- `tests/unit/test_combat.cpp` (nuevo): 60 caballería (owner 0, class=1) vs 60 artillería (owner 1, class=2), hp=100, attack=20, range_mt=1500, agrupadas con solape en x∈[126,130]. 400 ticks. Verifica `fatal==NONE`, ventaja RPS de la caballería (52 vivos vs 33), combate real (85 < 120 vivos), y checksum bit-idéntico en dos corridas frescas independientes.
- `CMakeLists.txt`: target `chunsa_test_combat` + `add_test(combat)`.

## Verificación (todo en `nice -n 19`, `-j2`, una compilación a la vez)

**Build** — `g++`, `-Wall -Wextra -Wshadow -Werror`: limpio, 0 warnings.

**ctest** (7/7 verdes):
```
1/7 props ...... Passed
2/7 golden ..... Passed
3/7 state ...... Passed
4/7 ring ....... Passed
5/7 flow_field . Passed
6/7 flow_move .. Passed
7/7 combat ..... Passed   (nuevo: "combat: owner0=52 owner1=33 checksum=59dfb1a7ce61099f / OK")
100% tests passed out of 7
```

**Gates de regresión** (checksums cambiaron por los campos nuevos de estado — esperado; lo que importa es que PASEN, save/load/replay bit-consistentes consigo mismos):

| Gate | Resultado |
|---|---|
| `golden --vectors tests/determinism/golden` | `GOLDEN backend=int128 casos=1074 fallos=0 [OK]` |
| `run --selftest-g1` | `G1 selftest: alloc_delta=0 OK checksum=1c849cd6428cef22` |
| `savetest --units 200 --save-at 150 --resume-to 400` | `G3 savetest(save@150): OK` |
| `savetest --ai --units 200 --save-at 9 --resume-to 60 --hold-dispatched` | `G4 savetest(save@9,hold): OK` |
| `record --units 200 --ticks 300` + `verify --replay` | `record: OK` → `G5 verify: OK ai_executions=0` |

`alloc_delta=0`: los 6 arrays nuevos viven en `GameState` preasignado; ninguno se aloca dentro de `step()`. Ninguno de los gates obligatorios ejercita `SPAWN_UNIT` (usan solo `SPAWN_DEBUG`/`MOVE_TO`), así que el combate solo se ejercita en `test_combat`.

## Desviaciones del contrato

Ninguna. Notas de implementación (decisiones de forma, no de fondo, dejadas abiertas por el contrato):
- `rps_mult_bp` y `combat_system` se colocaron en `namespace detail` (como `apply_command`/`movement_v1`), llamado como `detail::combat_system(g)` — mismas firmas que pide el contrato.
- El contrato no pide tocar el checksum/serialize de los 4 campos nuevos de `CmdPayload` dentro del *checksum* de `pending` (§4 solo habla del bloque de componentes); se dejó tal cual — no afecta a los gates porque ninguno agenda `SPAWN_UNIT`.
- La distribución exacta de las 60+60 unidades dentro de los rangos de tiles que da el contrato (x∈[120,130]/[126,136], y∈[120,136)) quedó a discreción del test: se usó una rejilla `tile_x = base + i%11`, `tile_y = 120 + i/11` para tener solape denso y determinismo trivial de leer.

---

## Revisión del Arquitecto (2026-07-21)

**Veredicto: ACEPTADO sin cambios.** Sonnet 5 implementó el contrato con cero desviaciones y calidad alta. Auditoría de determinismo (el punto crítico de un sistema de combate): sin floats · desempate de objetivo por menor `dist_sq` y luego menor índice (determinista) · `int64` antes de multiplicar en `range_raw` y en el daño (sin overflow) · muerte sin doble-marca (el filtro `hp<=0` en la búsqueda + el check `alive[best]` lo garantizan) · orden de fase correcto (combate tras `sh_rebuild`, antes de DESTROY, usando el spatial hash del tick actual). Re-verifiqué yo mismo: build 0-warnings, ctest 7/7, golden 1074/1074, G1 (`1c849cd6`, alloc_delta=0), G3/G4/G5. Test de combate: caballería 52 > artillería 33 (ventaja RPS +30% confirmada), 35 bajas, determinismo bit-exacto (`59dfb1a7`).

**Reparto:** segundo encargo de kernel para Sonnet 5, de nuevo impecable — a diferencia del flowfield (donde el bug fue de mi contrato), aquí contrato e implementación estaban correctos. Confirma su nicho: sistemas de kernel con juicio, determinismo respetado.
