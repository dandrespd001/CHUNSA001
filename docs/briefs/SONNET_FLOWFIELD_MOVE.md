# BRIEF — Integración FlowField → MovementSystem (Sonnet 5)

Eres ingeniero C++ senior de sistemas deterministas en el repo CHUNSA001 (kernel de RTS). **Trabaja en la rama `sonnet/flowfield-move` (créala desde main). Jamás toques main directamente.** El contrato de abajo es CERRADO y decidido por el Arquitecto: **impleméntalo tal cual, NO rediseñes**. Si algo es literalmente imposible, documenta la desviación mínima y sigue.

## Objetivo
Que las unidades marchen hacia un objetivo **rodeando muros** siguiendo el campo de flujo (`chunsa/flow_field.hpp`, ya existe y está verificado), en vez del seek directo actual. Determinismo bit-exacto INTACTO (los gates G1/G3/G4/G5 deben seguir verdes).

## ⚠️ RECURSOS (el equipo del dueño se apaga por térmica): TODO build `nice -n 19 cmake --build <dir> -j2`. Una compilación a la vez. Nunca `-j` sin número.

## Invariantes que NO puedes violar (o G1 falla y se rechaza)
- Cero float en estado de simulación (usa `Fx`/`Wide128`/`normalize_v1` de `chunsa/fixed64.hpp` y `chunsa/vec2fx.hpp`).
- Cero UB de signed overflow. Cero heap en `step()` (todo pool/array en GameState).
- Orden de iteración fijo: índice de entidad ascendente.
- No toques el core header-only salvo lo que este contrato indica; no toques `flow_field.hpp` (úsalo tal cual: `struct FlowField`, `void ff_compute(FlowField&, const uint8_t* cost, uint32_t w, uint32_t h, uint32_t goal_x, uint32_t goal_y)`, constantes `FF_AXIS=256`, `FF_CELLS`, `FF_WALL=255`, `FF_UNREACHABLE`).

## Contrato exacto

### 1. `game_state.hpp` — nuevos campos de estado en `struct GameState` (tras `VisionGrid vision;`)
```cpp
    // Flujo de navegación (Sprint 0.2). cost_grid/flow_mode/flow_goal_cell/flow_has_goal
    // son ESTADO (serializados, checksummeados). `flow` es DERIVADA (excluida): se
    // recomputa cuando flow_dirty. Layout 256×256, stride FF_AXIS (== map fijo 256).
    uint8_t  cost_grid[FF_CELLS];        // 1..254 transitable, FF_WALL=255 muro
    uint8_t  flow_mode[ENTITY_HARD_CAP]; // 0 = seek directo (tgt); 1 = seguir flujo
    uint32_t flow_goal_cell;             // celda goal activa (ty*FF_AXIS+tx)
    uint8_t  flow_has_goal;              // 0/1
    uint8_t  flow_dirty;                 // 1 = recalcular flow al inicio del próximo movement
    FlowField flow;                      // DERIVADA — NO serializar, NO checksummear
```
Incluye `chunsa/flow_field.hpp` en game_state.hpp.

En `gs_init`: tras el `std::memset(&g,0,...)` existente y `vis_init`, inicializa el cost_grid con un **patrón determinista fijo** (función `gs_init_cost_grid(GameState&)` que escribes): todo `1`, salvo un **muro vertical en la columna de tile x=128 para y en [32,224)** puesto a `FF_WALL`, **con un hueco de 8 tiles en y∈[124,132)** (por donde las unidades deben colarse). `flow_has_goal=0`, `flow_dirty=0`. (memset ya dejó flow_mode/flow en cero; el índice del cost_grid es `ty*FF_AXIS+tx`.)

### 2. `commands.hpp` — nuevo comando
Añade a `enum class CommandType : uint16_t` (append-only, NO renumeres): `FLOW_MOVE = 4`.
Semántica del payload: `p.x_raw`, `p.y_raw` = goal en raw Q47.16 (se convierte a tile con `>>16`). `p.handle`/`p.speed_mtpt` sin uso.
En la deserialización de comandos de `serialize.hpp` que valida `type_raw < 1 || type_raw > 3` → cámbialo a `> 4`. En `chunsa_data_compiler` NO tocar (es de datos, no comandos).

### 3. `step.hpp` — aplicación del comando y movimiento
**3a.** En `apply_command`, nuevo `case CommandType::FLOW_MOVE:`:
- Valida goal en cota de mundo (`world_contains({Fx{c.p.x_raw},Fx{c.p.y_raw}})`), si no → `MALFORMED`.
- `tx = c.p.x_raw >> 16; ty = c.p.y_raw >> 16;` (ambos ya ≥0 y <256 por la cota). `g.flow_goal_cell = ty*FF_AXIS+tx; g.flow_has_goal=1; g.flow_dirty=1;`
- Marca en modo flujo a **todas las unidades vivas cuyo `owner==c.emitter`**: recorrido índice ascendente, `if alive && owner==emitter → flow_mode[i]=1;`
- return `ACCEPTED`.

**3b.** En `detail::movement_v1(g)`, AL PRINCIPIO (antes del bucle de unidades):
```cpp
    if (g.flow_dirty && g.flow_has_goal) {
        ff_compute(g.flow, g.cost_grid, VIS_AXIS_DUMMY? ...);   // usa 256,256 y goal
        g.flow_dirty = 0;
    }
```
Concretamente: `ff_compute(g.flow, g.cost_grid, 256u, 256u, g.flow_goal_cell % FF_AXIS, g.flow_goal_cell / FF_AXIS);`

**3c.** En el bucle de `movement_v1`, para cada unidad viva, ANTES de la lógica de seek directo actual:
```cpp
        if (g.flow_mode[i] == 1u && g.flow_has_goal) {
            const uint32_t tx = static_cast<uint32_t>(g.pos_x[i] >> 16);
            const uint32_t ty = static_cast<uint32_t>(g.pos_y[i] >> 16);
            const uint32_t cell = ty * FF_AXIS + tx;   // tx,ty <256 por cota de mundo
            const int8_t dx = g.flow.dir_x[cell];
            const int8_t dy = g.flow.dir_y[cell];
            if (dx == 0 && dy == 0) {           // goal o inalcanzable → detener
                g.vel_x[i] = 0; g.vel_y[i] = 0;
                continue;
            }
            const int64_t step_fx = (static_cast<int64_t>(g.speed_mtpt[i]) * FX_ONE_RAW) / 1000;
            const Vec2Fx dir = normalize_v1(Vec2Fx{Fx{static_cast<int64_t>(dx) * FX_ONE_RAW},
                                                   Fx{static_cast<int64_t>(dy) * FX_ONE_RAW}}, g.fatal);
            const Fx vx = fx_mul(dir.x, Fx{step_fx}, g.fatal);
            const Fx vy = fx_mul(dir.y, Fx{step_fx}, g.fatal);
            g.vel_x[i] = vx.raw; g.vel_y[i] = vy.raw;
            g.pos_x[i] = fx_add(Fx{g.pos_x[i]}, vx, g.fatal).raw;
            g.pos_y[i] = fx_add(Fx{g.pos_y[i]}, vy, g.fatal).raw;
            // Clamp defensivo a cota de mundo [0, WORLD_RAW_MAX) para no salir del grid.
            if (g.pos_x[i] < 0) g.pos_x[i] = 0;
            if (g.pos_y[i] < 0) g.pos_y[i] = 0;
            if (g.pos_x[i] >= WORLD_RAW_MAX) g.pos_x[i] = WORLD_RAW_MAX - 1;
            if (g.pos_y[i] >= WORLD_RAW_MAX) g.pos_y[i] = WORLD_RAW_MAX - 1;
            continue;
        }
```
El seek directo actual (tgt) queda IGUAL para unidades con flow_mode==0.

### 4. `checksum.hpp` — `state_checksum_v1`
Antes del `return h.digest()`, añade (tras el bloque de visión): cost_grid (los `FF_CELLS` bytes con `h.u8`), luego `flow_mode[0..capacity-1]` (`h.u8`), luego `h.u32(g.flow_goal_cell)` y `h.u8(g.flow_has_goal)`. **NO incluyas `g.flow`** (derivada). NO incluyas flow_dirty (transitorio de presentación de cómputo).

### 5. `serialize.hpp` — `gs_serialize` / `gs_deserialize`
Serializa cost_grid (FF_CELLS bytes), flow_mode[0..capacity), flow_goal_cell (u32), flow_has_goal (u8). En deserialize, léelos en el MISMO orden y **pon `g.flow_dirty = g.flow_has_goal`** (fuerza recomputar el campo derivado al cargar). NO serializar `g.flow`. Sube `SAVE_FORMAT_VERSION` a 3 en `save_io.hpp`.

### 6. Escenario y test — `tests/unit/test_flow_move.cpp` (nuevo)
Usa el kernel directamente (incluye `chunsa/driver.hpp` o step directo). Define `uint64_t g_chunsa_allocs=0;` si enlaza driver. Escenario:
- `GameState` en heap, `gs_init` con cfg `{256+16, 1, 1, 20, 20, 256, 256, 99}`.
- Tick 0: batch de SPAWN_DEBUG, N=200 unidades del emisor 0 en el lado izquierdo: posiciones deterministas en tiles x∈[8,40], y∈[112,144] (raw = tile*65536 + 32768 centro), speed_mtpt=200, sequence creciente, handle {i,1}.
- Tick 1: un comando FLOW_MOVE del emisor 0 con goal en el lado derecho (tile x=220, y=128 → raw). sequence siguiente.
- Corre 600 ticks. Verifica (CHECK):
  1. `g.fatal == NONE`.
  2. **Rodeo**: en ningún tick una unidad ocupa un tile-muro (comprueba al final: para cada unidad viva, `cost_grid[cell] != FF_WALL`).
  3. **Progreso**: al menos el 70% de las unidades terminan con `pos_x > 128*65536` (cruzaron al lado del goal — solo posible por el hueco).
  4. **Determinismo**: segunda corrida idéntica (dos GameState frescos, mismos comandos) → `state_checksum_v1` iguales al final.
- Imprime "flow_move: OK" / "N fallos".
Añade el target a CMakeLists.txt (`chunsa_test_flow_move`, link `chunsa_sim_core`, `add_test`).

## Verificación OBLIGATORIA antes de commitear (en la rama)
1. `nice -n 19 cmake -B build-gcc -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ && nice -n 19 cmake --build build-gcc -j2` limpio (`-Werror`, 0 warnings).
2. `ctest --test-dir build-gcc --output-on-failure` — TODOS verdes, incluido tu `flow_move`.
3. **Gates de regresión** (los checksums CAMBIARÁN por los campos nuevos — es esperado; lo que importa es que PASEN, es decir que save/load/replay sigan siendo bit-consistentes consigo mismos):
   `./build-gcc/chunsa_sim_cli golden --vectors tests/determinism/golden` (1074/1074) ·
   `./build-gcc/chunsa_sim_cli run --selftest-g1` (OK, alloc_delta=0) ·
   `./build-gcc/chunsa_sim_cli savetest --units 200 --save-at 150 --resume-to 400` (OK) ·
   `./build-gcc/chunsa_sim_cli savetest --ai --units 200 --save-at 9 --resume-to 60 --hold-dispatched` (OK) ·
   `./build-gcc/chunsa_sim_cli record --units 200 --ticks 300 --out /tmp/g5.curp && ./build-gcc/chunsa_sim_cli verify --replay /tmp/g5.curp` (OK, ai_executions=0).
   Si alguno falla → tu cambio rompió el determinismo; arréglalo, no lo ocultes.
4. `git add` de los archivos tocados; commit en la rama: "Sprint 0.2: FlowField→MovementSystem (goal, cost grid, modo flujo) — generado: sonnet-5, pendiente revisión Arquitecto".
5. Escribe `docs/briefs/SONNET_FLOWFIELD_RESULT.md`: qué tocaste, salida de la verificación 2 y 3 (recorta), y cualquier desviación del contrato con su justificación. Inclúyelo en el commit.

## alloc_delta
Todos los arrays nuevos viven en GameState (preasignado). Verifica que el selftest-g1 sigue reportando `alloc_delta=0` — si no, alocaste dentro de step (prohibido).
