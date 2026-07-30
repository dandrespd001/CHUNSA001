# RESULT — Sprint 1.8C: HUD de recursos por familias

Fecha: 2026-07-30 · Rama: `gpt/hud-familias-1.8c`

## Implementación

- `DemoSnapshot` publica el stock completo de `RESOURCE_COUNT` slots.
- El HUD itera `DataCatalogV1::resources[]`; no contiene una lista de recursos
  cableada. Usa `index`, `family`, `appearance_epoch`, `nature` y
  `display_name_key_utf8`.
- Solo se muestran recursos cuya `appearance_epoch` ya alcanzó el jugador.
  Las familias sin recursos disponibles desaparecen del panel.
- Cada familia empieza colapsada y muestra su recurso de menor stock; al
  pulsarla se despliegan sus recursos individuales y su naturaleza.
- `resource_label()` fue eliminado. Los rechazos recorren el vector completo de
  costes y producen mensajes como `Faltan 200 de piedra`.
- La localización fuente está en `demo/localization/es.csv`; el proyecto carga
  la Translation española compilada por Godot. Los 30 recursos y las 7
  familias tienen traducción.
- Todos los literales C++ con acentos pasan por `U()`.
- No se inventó electricidad ni upkeep: el snapshot/kernel de este sprint no
  publica esos flujos. El hueco queda fuera del panel hasta el sprint que los
  exponga.

## Verificación

Build del adaptador:

```text
cmake --build build-godot -j2 --target chunsa_godot
[100%] Linking CXX shared library .../demo/bin/libchunsa_godot.so
[100%] Built target chunsa_godot
```

La ejecución exacta sin `--log-file` sí se intentó y no llegó a abrir el
proyecto en este entorno:

```text
godot --path demo --headless --language es --quit-after 1500
ERROR: Failed to open 'user://logs/godot2026-07-30T08.29.53.log'.
exit_code=134
```

Con un log absoluto, la ejecución real del mismo proyecto terminó correctamente:

```text
godot --path demo --headless --language es --quit-after 1500 --log-file /tmp/chunsa-luna-1.8c-final.log
CHUNSA render=prod(c/3d+interp) units=600
CHUNSA catálogo OK: cav_id=0 cit_id=1 art_id=2 building_count=6 settlement_id=1 forum_id=4 stable_id=0 barracks_id=3 buildable_id=2
CHUNSA cav=8 art=8 citizens=8 buildings=4 stock0=0 ai_seq=14 ai_last_seq=14 ai_x=76
CHUNSA tick=0 units=28 fog_visible_blocks=0 fog_explored_blocks=0 fog_unexplored_blocks=1024 fog_enemy_presented=0
CHUNSA cav=8 art=8 citizens=8 buildings=4 stock0=0 ai_seq=58 ai_last_seq=58 ai_x=72
CHUNSA tick=100 units=28 fog_visible_blocks=13 fog_explored_blocks=0 fog_unexplored_blocks=1011 fog_enemy_presented=0
CHUNSA cav=8 art=8 citizens=8 buildings=4 stock0=0 ai_seq=98 ai_last_seq=98 ai_x=69
CHUNSA tick=200 units=28 fog_visible_blocks=12 fog_explored_blocks=1 fog_unexplored_blocks=1011 fog_enemy_presented=0
exit_code=0
```

La localización también se verificó dentro del runtime de Godot:

```text
TR food=Comida
TR family=Construcción
```

No fue posible confirmar la HUD en una ventana: el entorno no tiene display
X11 ni Wayland. El intento devolvió `X11 Display is not available`; el
renderer dummy headless tampoco produce una imagen (`CHUNSA SHOT: imagen
vacía`). Por tanto, no se afirma una comprobación visual de pantalla.

`git diff --stat -- addons/chunsa_sim/core/ tests/ data/ tools/` queda vacío.

La suite CTest disponible en `build-godot` también se lanzó sin modificar sus
fuentes: 23/31 pasaron. Fallaron `data_blob`, `production_tech`,
`victory_ai_profile`, `civ_deposits` y `ai_skirmish_apertura` por carga inválida
del catálogo, y `citizen_task`, `resource_count` y `resource_catalog` no tenían
ejecutable construido. No se tocaron esas rutas porque están fuera del alcance
del sprint.
