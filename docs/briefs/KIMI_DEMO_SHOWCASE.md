# BRIEF — Demo showcase Sprint 0.3: combate + moral + economía visibles (Kimi K3)

Eres ingeniero Godot/C++ senior en CHUNSA001. **Rama `kimi/demo-showcase` desde main. Jamás toques main.** Reemplaza el escenario actual de la demo (FlowField, unidades cruzando un muro) por uno que muestre TODO lo construido en el Sprint 0.3: dos ejércitos chocando (combate RPS + pánico/moral visibles) y unos ciudadanos recolectando en paralelo (economía).

## ⚠️ RECURSOS (el equipo se apaga por térmica): builds `nice -n 19 ... -j2`, runs de Godot `nice -n 19`, UNO a la vez.

## Archivos a tocar (SOLO estos): `addons/chunsa_sim/gdextension/chunsa_sim_node.h` y `.cpp`. NO toques el core (`addons/chunsa_sim/core/`).

## API EXACTA del kernel (cópiala literal — NO adivines nombres de campos)

`GameState` (en `chunsa/game_state.hpp`) expone estos arrays PLANOS indexados por slot de entidad (SIN prefijo `entities.`):
```cpp
uint8_t  owner[ENTITY_HARD_CAP];        // 0..7
uint8_t  unit_class[ENTITY_HARD_CAP];   // 0=infantry 1=cavalry 2=artillery 3=citizen
uint8_t  fleeing[ENTITY_HARD_CAP];      // 1 = en pánico
int64_t  pos_x[ENTITY_HARD_CAP], pos_y[ENTITY_HARD_CAP];  // raw Q47.16
int64_t  player_stock[MAX_EMITTERS][3]; // 0=A 1=B 2=Me, por emisor
struct EntityTable { uint32_t capacity; uint8_t alive[ENTITY_HARD_CAP]; /*...*/ } entities;
uint32_t tick;
```
Comandos (`chunsa/commands.hpp`):
```cpp
enum class CommandType : uint16_t { SPAWN_DEBUG=1, MOVE_TO=2, DESTROY_DEBUG=3, FLOW_MOVE=4, SPAWN_UNIT=5, SPAWN_CITIZEN=6 };
struct CmdPayload { EntityHandle handle; int64_t x_raw,y_raw; int32_t speed_mtpt;
                    int32_t hp; int32_t attack; int32_t range_mt; uint8_t unit_class; };
struct RawCommand { uint32_t target_tick; uint16_t emitter; CommandType type; uint64_t sequence; CmdPayload p; };
struct EntityHandle { uint32_t index; uint32_t generation; };
```
`rng_range(seed, stream_id, tick, entity_index, stable_slot, lo, hi, FatalReason&)` en `chunsa/rng.hpp` (ya usado en el adaptador actual — mira `build_flow_batch` como referencia de estilo, tiene el mismo patrón que necesitas).
`chunsa::step(GameState&, const RawCommand*, uint32_t n) -> StepResult` ya se llama en `sim_loop`.

## Contrato del snapshot (amplía `DemoSnapshot`, layout por-slot igual que ahora)

Añade estos 3 arrays al struct `DemoSnapshot` existente (mismo patrón que `x`/`y`/`alive`, tamaño 1024):
```cpp
uint8_t owner[1024];
uint8_t unit_class[1024];
uint8_t fleeing[1024];
```
En `sim_loop`, en el mismo bucle que ya llena `s->x[i]/s->y[i]/s->alive[i]`, añade `s->owner[i]=gs->owner[i]; s->unit_class[i]=gs->unit_class[i]; s->fleeing[i]=gs->fleeing[i];` (mismo índice `i`, sin condicional extra).

## Escenario nuevo: `build_showcase_batch(RawCommand* batch, uint32_t t) -> uint32_t`

Reemplaza la llamada a `build_flow_batch` en `sim_loop` por esta nueva función (puedes dejar `build_flow_batch` sin usar o borrarla, tu elección). Lógica EXACTA:

- **`t == 0`**: spawns iniciales. Usa `demo_units` como total repartido así: 40% owner 0 CABALLERÍA (unit_class=1), 40% owner 1 ARTILLERÍA (unit_class=2), 20% owner 0 CIUDADANOS (SPAWN_CITIZEN). Con `demo_units=600` por defecto: 240 caballería, 240 artillería, 120 ciudadanos.
  - Caballería (owner 0): `SPAWN_UNIT`, posiciones dispersas en tile x∈[60,90], y∈[100,156] (usa `rng_range` con el patrón de `build_flow_batch` para dispersar, seed=`DEMO_SEED`, stream `RngStream::BENCH`, slots 1/2 para x/y como ya hace el código existente). `hp=100, attack=20, range_mt=1500, unit_class=1, speed_mtpt=150`.
  - Artillería (owner 1): `SPAWN_UNIT`, tile x∈[166,196], y∈[100,156]. `hp=100, attack=20, range_mt=1500, unit_class=2, speed_mtpt=80` (más lenta, sabor "artillería pesada").
  - Ciudadanos (owner 0): `SPAWN_CITIZEN`, tile x∈[36,44], y∈[36,44] (cerca del depósito de Alimentos en (40,40) — ver `gs_init_economy` en game_state.hpp si quieres confirmarlo). `speed_mtpt=200`.
  - `sequence` por emisor estrictamente creciente empezando en 1 (mismo patrón que `build_flow_batch`).
- **`t == 1`**: UN `MOVE_TO` por cada unidad de COMBATE (caballería y artillería, NO ciudadanos) hacia el centro del "campo de batalla": caballería → tile (128,128); artillería → tile (128,128) también (convergen y se enzarzan). Ciudadanos no reciben orden (su economía es automática).
- Resto de ticks: batch vacío (los ciudadanos y el combate son autónomos vía `economy_system`/`combat_system`/`morale_system`, ya en el kernel).

## Render: color por bando + pánico (en `render_interpolated`, mismo patrón de lerp ya existente)

Además del lerp de posición ya implementado, calcula el COLOR por instancia cada frame según el snapshot actual (`snap_curr`, sin interpolar color, solo posición):
```cpp
godot::Color c;
if (snap_curr.unit_class[i] == 3)      c = godot::Color(0.9, 0.85, 0.2);   // ciudadano: amarillo
else if (snap_curr.fleeing[i] != 0)    c = godot::Color(0.9, 0.9, 0.95);   // pánico: casi blanco
else if (snap_curr.owner[i] == 0)      c = godot::Color(0.2, 0.6, 0.95);   // owner 0: azul
else                                    c = godot::Color(0.9, 0.3, 0.2);   // owner 1: rojo/naranja
mm->set_instance_color(k, c);
```
(`set_use_colors(true)` ya debería estar activo en el `MultiMesh` de unidades desde el spike; si no lo está, actívalo).

## Diagnóstico en consola

Cada 100 ticks (mismo patrón que el print existente de `tick=... units=...`), añade: `godot::UtilityFunctions::print("CHUNSA cav=", n_cav_alive, " art=", n_art_alive, " citizens=", n_cit_alive, " stock_A=", gs->player_stock[0][0]);` — cuenta vivos por `unit_class` recorriendo `gs->entities.capacity` (esto lee del hilo de simulación, que es donde ya vive el print existente en `sim_loop`, no en `_process`).

## Verificación OBLIGATORIA antes de commitear
1. `nice -n 19 cmake -B build-godot -DCMAKE_BUILD_TYPE=Debug -DCHUNSA_BUILD_GODOT=ON && nice -n 19 cmake --build build-godot -j2 --target chunsa_godot` — limpio, `demo/bin/libchunsa_godot.so` existe.
2. `nice -n 19 ./third_party_build/Godot_v4.7.1-stable_linux.x86_64 --headless --path demo --quit-after 2000` — SIN crash; en la salida deben aparecer líneas `CHUNSA cav=... art=... citizens=... stock_A=...` con `cav`/`art` bajando con el tiempo (combate real) y `stock_A` subiendo (economía real).
3. Kernel intacto: `nice -n 19 cmake --build build-gcc -j2 && ./build-gcc/chunsa_sim_cli golden --vectors tests/determinism/golden` (no debiste tocar `addons/chunsa_sim/core/`).
4. `git add` de los 2 archivos del adaptador + `demo/bin/libchunsa_godot.so`; commit en la rama: "Sprint 0.3: demo showcase — combate+moral+economía visibles (generado: kimi-k3, pendiente revisión Arquitecto)".
5. Escribe `docs/briefs/KIMI_DEMO_SHOWCASE_RESULT.md`: qué hiciste, la salida de la verificación 2 (8-10 líneas mostrando la progresión), desviaciones. Inclúyelo en el commit.

Si algo del contrato es imposible tal cual (p. ej. una unidad no encuentra rival y el combate no arranca), ajusta posiciones/velocidades con el mínimo cambio y documenta la desviación — no rediseñes el kernel.
