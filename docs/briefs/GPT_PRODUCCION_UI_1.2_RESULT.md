# RESULT — UI de producción, tecnología y época (GPT/Luna · Sprint 1.2)

Fecha: 2026-07-24
Rama: `gpt/produccion-ui-1.2`, creada desde `main` (incluye merge `9b59965`)
Alcance: adaptador Godot + showcase + HUD. `addons/chunsa_sim/core/` no fue tocado.

## Implementación

- `DemoSnapshot` conserva el layout por slot y expone `prod_queue[i][k]`,
  `prod_count[i]`, `prod_progress[i]`, `research_tech[i]` y
  `research_progress[i]`; la UI usa la cabeza `prod_queue[i][0]`.
- El snapshot copia para el jugador 0 `stock_a`, `stock_b`, `stock_me`,
  `player_epoch` y `pop_used`, además del último receipt del mailbox para
  feedback visual.
- HUD 2D mínimo en la esquina: recursos A/B/Me, época, población `/200`,
  ayuda de controles, feedback de receipt y acciones del edificio seleccionado.
- Controles contextuales:
  - `1..8`: `TRAIN_UNIT` en modo TRAIN; `T` cambia a modo TECH y las mismas
    teclas emiten `RESEARCH_TECH` para `researches[]`.
  - `R` + clic: `SET_RALLY`.
  - `E`: `EPOCH_UP`.
  - La cola y el progreso de producción/research se dibujan sobre el edificio.
- Los cuatro comandos copian el patrón existente de `enqueue_place_building`:
  `memset`, `target_tick=0`, `emitter=0`, secuencia creciente y push protegido
  por `input_mutex`. `SET_RALLY` usa raw Q47.16 (`tile*65536+32768`);
  `EPOCH_UP` deja todos los campos del payload en cero.
- El showcase pre-coloca en `build_showcase_batch(t==0)` los centros y los dos
  cuarteles completos: `egipto:chariotry_stable` para owner 0 y
  `rome:castra_barracks` para owner 1. El host no drena input humano durante el
  primer `Step(t==0)`, preservando la exención exclusiva de escenario.

No se detectó una carencia no mecánica del kernel ni se improvisó ninguna API;
la validación autoritativa de owner, época, stock, cola, población, prerequisitos
y receipts sigue en el kernel.

## Verificación

Build térmico, uno a la vez:

```text
nice -n 19 cmake -B build-godot -DCMAKE_BUILD_TYPE=Debug -DCHUNSA_BUILD_GODOT=ON
nice -n 19 cmake --build build-godot -j2 --target chunsa_godot
```

Resultado: `Built target chunsa_godot`, 0 warnings/errores observados y
`demo/bin/libchunsa_godot.so` regenerada.

Headless equivalente reproducible (se añadió solo `--log-file` para evitar el
fallo de logger `user://` del entorno):

```text
nice -n 19 ./third_party_build/Godot_v4.7.1-stable_linux.x86_64 \
  --headless --path demo --quit-after 1500 \
  --log-file /tmp/chunsa-sprint-1.2-godot-final.log
```

Resultado: exit `0`; evidencia:

```text
CHUNSA catálogo OK: ... building_count=6 ... stable_id=0 barracks_id=3 ...
CHUNSA cav=240 art=240 citizens=120 buildings=4 stock_A=0
CHUNSA cav=238 art=240 citizens=120 buildings=4 stock_A=0
CHUNSA cav=226 art=228 citizens=120 buildings=4 stock_A=0
```

`rg` sobre el log final no encontró ninguna línea `CHUNSA ERROR`.

El comando exacto pedido, sin `--log-file`, abortó antes de iniciar el proyecto
(exit `134`) porque Godot no pudo abrir `user://logs/...` en este entorno y cayó
en SIGSEGV. El binario y la demo sí pasan con el log absoluto temporal indicado;
no es una desviación del adaptador.

No se ejecutó `ctest`: la copia de trabajo no contiene un directorio `build/`
configurado y este sprint no modificó el core ni sus tests.

## Archivos entregados

- `addons/chunsa_sim/gdextension/chunsa_sim_node.h`
- `addons/chunsa_sim/gdextension/chunsa_sim_node.cpp`
- `demo/bin/libchunsa_godot.so` (recompilada)
- este RESULT
