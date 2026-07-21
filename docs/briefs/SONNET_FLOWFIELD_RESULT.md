# RESULT — Integración FlowField → MovementSystem (Sonnet 5)

Rama: `sonnet/flowfield-move` (desde `main`). Contrato: `docs/briefs/SONNET_FLOWFIELD_MOVE.md`, implementado tal cual, sin rediseño. Cero desviaciones.

## Qué se tocó

- `addons/chunsa_sim/core/include/chunsa/game_state.hpp`: include de `flow_field.hpp`; nuevos campos de `GameState` (`cost_grid`, `flow_mode`, `flow_goal_cell`, `flow_has_goal`, `flow_dirty`, `flow` derivada) tras `VisionGrid vision;`; función `gs_init_cost_grid` (muro vertical x=128, y∈[32,224) con hueco y∈[124,132)); llamada desde `gs_init`.
- `addons/chunsa_sim/core/include/chunsa/commands.hpp`: `CommandType::FLOW_MOVE = 4` (append-only).
- `addons/chunsa_sim/core/include/chunsa/step.hpp`: `apply_command` — nuevo `case FLOW_MOVE` (valida cota de mundo, fija goal/dirty, marca `flow_mode[i]=1` en unidades vivas del emisor, orden ascendente). `movement_v1` — recompute de `flow` al inicio si `flow_dirty`; por unidad, si `flow_mode==1` sigue `flow.dir_x/dir_y` de su celda (normalize_v1 + fx_mul + fx_add, clamp defensivo a `[0, WORLD_RAW_MAX)`); el seek directo (tgt) queda intacto para `flow_mode==0`.
- `addons/chunsa_sim/core/include/chunsa/checksum.hpp`: `state_checksum_v1` — añade `cost_grid` (FF_CELLS), `flow_mode[0..capacity)`, `flow_goal_cell`, `flow_has_goal`. Excluye `flow` (derivada) y `flow_dirty` (transitorio).
- `addons/chunsa_sim/core/include/chunsa/serialize.hpp`: `gs_serialize`/`gs_deserialize` — mismo orden que el checksum, al final del stream. En deserialize, `flow_dirty = flow_has_goal`. Bound de `type_raw` en `pending` ampliado de `> 3` a `> 4`.
- `addons/chunsa_sim/core/include/chunsa/save_io.hpp`: `SAVE_FORMAT_VERSION` 2 → 3.
- `tests/unit/test_flow_move.cpp` (nuevo): 200 unidades nacen a la izquierda (tick 0), `FLOW_MOVE` del emisor 0 al lado derecho (tick 1, tras el muro con hueco), 600 ticks. Verifica `fatal==NONE`, cero unidades sobre tile-muro, ≥70% cruzadas (real: 181/200 = 90.5%), y checksum bit-idéntico en dos corridas frescas independientes.
- `CMakeLists.txt`: target `chunsa_test_flow_move` + `add_test(flow_move)`.

## Verificación (todo en `nice -n 19`, `-j2`, una compilación a la vez)

**Build** — `g++`, `-Wall -Wextra -Wshadow -Werror`: limpio, 0 warnings.

**ctest** (6/6 verdes):
```
1/6 props ...... Passed
2/6 golden ...... Passed
3/6 state ....... Passed
4/6 ring ........ Passed
5/6 flow_field .. Passed
6/6 flow_move ... Passed   (nuevo: "flow_move: alive=200 crossed=181 checksum=c60105b7d87a6f34 / OK")
100% tests passed out of 6
```

**Gates de regresión** (checksums cambiaron por los campos nuevos de estado — esperado; lo que importa es save/load/replay bit-consistentes consigo mismos):

| Gate | Resultado |
|---|---|
| `golden --vectors tests/determinism/golden` | `GOLDEN backend=int128 casos=1074 fallos=0 [OK]` |
| `run --selftest-g1` | `G1 selftest: alloc_delta=0 OK checksum=6566b4b3a46b2397` |
| `savetest --units 200 --save-at 150 --resume-to 400` | `G3 savetest(save@150): OK` |
| `savetest --ai --units 200 --save-at 9 --resume-to 60 --hold-dispatched` | `G4 savetest(save@9,hold): OK` |
| `record --units 200 --ticks 300` + `verify --replay` | `record: OK` → `G5 verify: OK ai_executions=0` |

`alloc_delta=0`: los arrays nuevos (`cost_grid`, `flow_mode`, `flow`) viven en `GameState` preasignado; ninguno se aloca dentro de `step()`.

## Desviaciones del contrato

Ninguna. El pseudocódigo del §3b (`ff_compute(g.flow, g.cost_grid, VIS_AXIS_DUMMY? ...)`) se implementó literalmente con la forma concreta que el propio contrato aclara a continuación: `ff_compute(g.flow, g.cost_grid, 256u, 256u, g.flow_goal_cell % FF_AXIS, g.flow_goal_cell / FF_AXIS);`.
