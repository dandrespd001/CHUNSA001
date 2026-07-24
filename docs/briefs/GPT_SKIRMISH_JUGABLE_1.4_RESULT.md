# RESULT — Skirmish jugable humano contra IA · Sprint 1.4

Fecha: 2026-07-24  
Rama: `gpt/skirmish-jugable-1.4`, creada desde `main` en `afbe66f`  
Rol: desarrollador gráfico/Godot (Luna)

## Estado

Implementado el modo jugable owner 0 contra IA owner 1 en el adaptador Godot.
No se modificó `addons/chunsa_sim/core/`; el kernel existente conserva sus
contratos y el adaptador copia el ciclo de IA del driver.

## Implementación

- `ChunsaSimNode` posee `AiJobBox ai_box` y `AiRuntimeV1 ai_rt{0,0}`;
  `_ready()` inicializa la caja para owner 1 después de `gs_init`.
- `sim_loop` ejecuta, antes de `step`, el patrón literal
  `ai_should_dispatch` → `ai_dispatch` → `ai_execute` → `ai_stalled` /
  `ai_execute` → copia en batch cuando `ai_due` → `ai_commit`.
- Después del setup de tick 0, el runtime de IA continúa desde
  `gs->last_seq[1]`; esto evita que las secuencias de los comandos iniciales
  choquen con la secuencia del emisor 1 validada por el kernel.
- El nuevo escenario usa el blob real: owner 0 recibe
  `egipto:settlement_center`, `egipto:chariotry_stable`,
  `egipto:chariot_warrior` y `egipto:work_crew`; owner 1 recibe
  `rome:forum_center`, `rome:castra_barracks`, `rome:ballista_crew` y
  `egipto:work_crew`. Ambos arrancan con centro, cuartel, ocho unidades de
  ejército y cuatro aldeanos vulnerables.
- Las bases se separan para que la capa reactiva de la IA no quede atrapada en
  defensa; el dropoff del owner 1 se alinea con su base de demo.
- `DemoSnapshot` expone `game_over` y `winner`. `_draw` muestra `VICTORIA`,
  `DERROTA` o `EMPATE`; el hilo de simulación congela el avance al terminar y
  emite `CHUNSA game_over winner=N tick=T`.

## Verificación

Build del adaptador:

```text
nice -n 19 cmake --build build-godot -j2 --target chunsa_godot
```

Resultado: `Built target chunsa_godot`, sin warnings/errores observados.

Regresión del repositorio:

```text
nice -n 19 cmake --build build-gcc -j2
ctest --test-dir build-gcc --output-on-failure
```

Resultado: `100% tests passed out of 20`, incluido golden, IA, victoria,
skirmish económico y `data_compile`.

Headless obligatorio, usando un directorio de datos temporal para evitar el
fallo del logger `user://logs` del entorno:

```text
env XDG_DATA_HOME=/tmp/chunsa-godot-data nice -n 19 \
  ./third_party_build/Godot_v4.7.1-stable_linux.x86_64 \
  --headless --path demo --quit-after 4000
```

Resultado: exit `0`, catálogo cargado, sin `CHUNSA ERROR`; el run alcanza
aproximadamente el tick 500 y confirma el bombeo de IA (`ai_seq` creciente).

Corrida extendida para comprobar la condición de fin:

```text
env XDG_DATA_HOME=/tmp/chunsa-godot-data nice -n 19 \
  ./third_party_build/Godot_v4.7.1-stable_linux.x86_64 \
  --headless --path demo --quit-after 20000
```

Resultado: exit `0`; la IA avanza desde `ai_x=76` hacia el frente y la partida
termina con:

```text
CHUNSA game_over winner=1 tick=2554
```

El binario cargado por la demo quedó regenerado en
`demo/bin/libchunsa_godot.so`.

## Archivos entregados

- `addons/chunsa_sim/gdextension/chunsa_sim_node.h`
- `addons/chunsa_sim/gdextension/chunsa_sim_node.cpp`
- `demo/bin/libchunsa_godot.so`
- este brief
