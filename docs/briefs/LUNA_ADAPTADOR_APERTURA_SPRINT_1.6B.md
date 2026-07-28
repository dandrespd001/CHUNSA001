# Brief — Adaptador Godot de la apertura económica (Sprint 1.6B, cierre)

**Modelo asignado:** GPT-5.6 Luna Max (`codex -m gpt-5.6-luna`)
**Rama base:** `main` @ `ac904fe` (K3 ya integrado)
**Rama de trabajo:** `gpt/apertura-economica-1.6b`
**Autoridad:** Arquitecto Jefe. Este documento es normativo.

Objetivo: que un humano pueda **jugar la apertura económica** — seleccionar
aldeanos, mandarlos a recolectar sobre un depósito con clic derecho, y ver en el
HUD cuántos aldeanos trabajan cada recurso.

Todo el demo vive en un único nodo C++ (`ChunsaSimNode`,
`addons/chunsa_sim/gdextension/chunsa_sim_node.cpp`, ~2400 líneas). **No hay
GDScript y no vamos a introducirlo**: el trabajo es C++ dentro de ese nodo y su
cabecera `chunsa_sim_node.h`.

---

## 0. Restricciones del kernel que NO puedes violar

1. **El kernel es intocable en esta tarea.** No modifiques nada bajo
   `addons/chunsa_sim/core/`. Si crees que necesitas un cambio de kernel, para y
   repórtalo — no lo hagas.
2. **`movement_v1` está CONGELADA (SPEC-001 §12) y excluye incondicionalmente
   `unit_class > 2`.** Los ciudadanos (`unit_class == 3`) **no se mueven con
   `MOVE_TO`**: el comando sería aceptado por `apply_command` pero
   `movement_v1` lo ignora y `economy_system` reescribe `pos_x/pos_y` cada tick
   desde la máquina de estados económica. **Está prohibido** añadir aldeanos al
   bucle de `MOVE_TO` del clic derecho: produciría un control que parece
   funcionar y no hace nada. El control de aldeano en esta build es exactamente
   dos cosas: **asignación de construcción** y **GATHER**.
3. El kernel resuelve el depósito **él mismo**, por radio
   (`GATHER_PICK_RADIUS_RAW` = 1 tile) desde el punto raw del comando. Envía
   siempre las **coordenadas exactas del depósito elegido**, no las del clic:
   así la distancia de validación es cero y nunca hay rechazo por borde. Es el
   mismo patrón que ya usa la IA.
4. Orden de validación de `GATHER` en el kernel: handle vivo/propio ·
   `unit_class == 3` (**ILLEGAL_STATE** si no) · depósito resoluble por radio
   (**INVALID_ENTITY** si ninguno). No intentes replicar la validación: filtra
   en el adaptador para no emitir comandos que sabes que se van a rechazar, pero
   la autoridad es el kernel.
5. Un `GATHER` cancela la orden de construir (`build_target = BUILD_NO_TARGET`).
   Es contractual (SPEC-004 §18), no lo compenses en la UI.

Constantes útiles: `ECO_MAX_DEPOSITS = 32`, `ECO_CARRY_CAP = 50`
(`economy.hpp`), `GATHER_PICK_RADIUS_RAW = FX_ONE_RAW` (`step.hpp:36`).

---

## 1. Ampliar `DemoSnapshot` con el estado económico

`DemoSnapshot` (`chunsa_sim_node.h:45`) hoy **no lleva ningún dato económico**:
ni depósitos, ni carga, ni estado de recolección. Añádelos siguiendo
exactamente el patrón de los campos ya existentes, y cópialos en `sim_loop()`
en el mismo bloque donde se copian `prod_queue`/`rally_*` (~línea 302), bajo el
mismo lock.

Campos nuevos:

```cpp
// Depósitos del mapa (SPEC-004 §15, 12 en el mapa base).
uint32_t n_deposits;
int64_t  dep_x_raw[chunsa::ECO_MAX_DEPOSITS];
int64_t  dep_y_raw[chunsa::ECO_MAX_DEPOSITS];
int32_t  dep_remaining[chunsa::ECO_MAX_DEPOSITS];
uint8_t  dep_resource_idx[chunsa::ECO_MAX_DEPOSITS];

// Estado económico por entidad.
int32_t  eco_carry[1024];
uint8_t  eco_carry_resource[1024];
uint8_t  eco_state[1024];            // 0=SEEK 1=HARVEST 2=RETURN
uint32_t eco_assigned_deposit[1024]; // ECO_NO_DEPOSIT si ocioso
```

Verifica los nombres reales en `game_state.hpp`/`economy.hpp` antes de escribir;
si alguno difiere, manda el del kernel.

---

## 2. Clic derecho: emitir `GATHER`

En `_input`, rama `MOUSE_BUTTON_RIGHT && is_pressed()`
(`chunsa_sim_node.cpp:~746`). El orden de resolución actual es:

1. `enqueue_build_assignments(tile)` → si devuelve > 0, `return`.
2. si no, `MOVE_TO` para lo militar (`unit_class <= 2`).

Orden **nuevo**, con GATHER intercalado:

1. `enqueue_build_assignments(tile)` — **sin cambios**, sigue primero.
2. **NUEVO — `enqueue_gather_orders(...)`**: resolver el depósito **vivo**
   (`remaining > 0`) más cercano al punto del clic dentro de
   `GATHER_PICK_RADIUS_RAW`, con **desempate por índice más bajo** (misma regla
   que el kernel). Si hay uno:
   - por cada slot seleccionado con `selected_slot_is_current(i)`,
     `owner == 0`, `entity_kind == 0` y `unit_class == 3`, encolar un
     `RawCommand` de tipo `CommandType::GATHER` con
     `p.handle = {i, generation[i]}` y `p.x_raw/p.y_raw` = **las del depósito**;
   - si se emitió alguno: `add_order_marker(...)` sobre el depósito y `return`.
3. `MOVE_TO` para lo militar — **sin cambios, y sin añadir aldeanos** (§0.2).

Sigue el patrón exacto de construcción de `RawCommand` que ya usa la rama de
`MOVE_TO`: `std::memset` a cero, `target_tick = 0`, `emitter = 0`,
`sequence = next_player_sequence++`, `push_back` en `pending_player_commands`
bajo `input_mutex`.

**Comportamiento si el clic derecho cae sobre un depósito pero la selección no
tiene aldeanos**: no emitas nada y **deja caer el flujo al paso 3** (lo militar
se mueve hacia allí). No inventes un mensaje de error.

---

## 3. HUD: aldeanos por recurso

Dos piezas, ambas en el overlay ya existente (`draw_world_overlay` /
`draw_selection_panel`), respetando el estilo y la tipografía actuales:

**3.1 Contador por recurso.** Junto a la lectura de stocks (`stock_a`,
`stock_b`, `stock_me`), añade cuántos aldeanos de owner 0 trabajan cada
recurso. Criterio de imputación, en este orden:

- si `eco_assigned_deposit != ECO_NO_DEPOSIT` y el índice es válido →
  el recurso de **ese depósito**;
- si no, y `eco_carry > 0` → `eco_carry_resource` (va de vuelta con carga y su
  depósito se agotó);
- si no → cuenta como **ocioso**.

Los aldeanos con `build_target != BUILD_NO_TARGET` cuentan aparte, como
**construyendo** (el kernel los saca del pipeline económico).

Formato sugerido, una línea compacta:
`Aldeanos — A:3  B:2  Me:0  constr:1  ocioso:0`

**3.2 Panel de selección.** Cuando la selección sea de un solo aldeano, muestra
su estado legible: `SEEK`/`HARVEST`/`RETURN`, depósito asignado, y carga
`carry/ECO_CARRY_CAP` con el nombre del recurso. Usa `catalog_name` si aplica;
si no hay nombre en catálogo, `A`/`B`/`Me` por índice.

---

## 4. Dibujar los depósitos

Hoy los depósitos son invisibles: no se puede clicar lo que no se ve.

- Dibuja cada depósito con `remaining > 0` en el mundo, en su posición raw,
  con una forma distinguible por recurso y el `remaining` como texto.
- **Respeta la niebla**: usa `presentation_tile_visible(x_raw, y_raw)` igual que
  el resto del overlay. Un depósito en niebla no se dibuja.
- Añádelos también al minimapa (`draw_minimap`) como puntos, con el mismo filtro
  de niebla.

---

## 5. Definición de hecho

- [ ] `cmake --build build-godot -j2` limpio (es el build del `.so` del demo).
- [ ] `cmake --build build-gcc -j2 && ctest --test-dir build-gcc` → **25/25**.
      El kernel no se toca, así que **cualquier** cambio aquí es una regresión:
      para y repórtalo.
- [ ] Los checksums de los gates siguen bit-idénticos:
      G1 `./build-gcc/chunsa_sim_cli run --selftest-g1` → `fefa48125dd35736`;
      G4 `./build-gcc/chunsa_sim_cli savetest --ai` → `774316057e5667fb` /
      `d52ac0019700684f`.
- [ ] El demo arranca y es jugable: seleccionar aldeanos, clic derecho sobre un
      depósito, verlos ir, cosechar y depositar, y el contador del HUD moverse
      en consecuencia.
- [ ] Informe en `docs/RESULT_LUNA_APERTURA_1.6B.md`: qué cambiaste y dónde,
      cómo verificaste la jugabilidad, y **cualquier desviación del contrato con
      su justificación**.

**Verifica sin pipes que enmascaren el código de salida**: `cmd; echo $?`, nunca
`cmd | tail`.

**No hagas merge a `main`.** Deja la rama y el informe; integra el Arquitecto.
