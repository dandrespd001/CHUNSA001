# RESULT — Sprint 1.3: UI/HUD v1

Fecha: 2026-07-24  ·  Rama: `gpt/ui-hud-1.3` desde `main`

## Entrega

Implementación completa del frontend Godot en `addons/chunsa_sim/gdextension/`:

- Cámara ortográfica movible: pan con WASD/flechas y arrastre con botón central,
  clamp a `1024 × 1024` px, zoom `[300, 1200]` anclado al cursor.
- Minimapa v1 dibujado con `_draw`: mapa completo, muro del escenario, entidades
  vivas por owner/clase, edificios por footprint, viewport y salto/arrastre por clic.
  No hay fog of war en este sprint; queda para la exposición de visión posterior.
- Panel de selección única/múltiple con tipo, owner, HP agregado/individual,
  barra de vida, progreso de construcción y estado de cola/research.
- Barras de vida sobre las entidades seleccionadas.
- Grupos `Ctrl+1..9` / `1..9`, almacenando slot y `generation`; al recuperar se
  exige `alive` y generación coincidente. Doble pulsación centra la cámara.
- TRAIN/TECH migrados de las teclas numéricas a botones contextuales del panel;
  tabs TRAIN/TECH y botón dedicado `E: SUBIR ÉPOCA`. Los números quedan libres
  para grupos. `T`, `E`, `B`, `N`, `R` y clic derecho conservan sus funciones de
  letra/modo existentes.
- Marcadores breves para MOVE_TO/SET_RALLY y rally persistente leído del snapshot.
- `DemoSnapshot` expone/copia `hp[]` y `max_hp[]`. También se completó el binding
  mecánico de `rally_x[]`, `rally_y[]`, `rally_set[]`, que el contrato de UI ya
  declaraba disponible aunque no estuviera presente en el adaptador base.

La UI sigue leyendo el snapshot publicado y encolando `RawCommand` por el mailbox;
no se añadió lógica de juego ni se modificó el kernel.

## Verificación

- Build del adaptador con `nice -n 19 cmake --build build-godot -j2`: OK,
  `-Wall -Wextra -Werror` sin warnings.
- Gate obligatorio, ejecutado fuera del sandbox para permitir `user://`:

  ```text
  nice -n 19 ./third_party_build/Godot_v4.7.1-stable_linux.x86_64 \
    --headless --path demo --quit-after 1500
  ```

  `exit_code=0`; salida con `buildings=4`; sin `CHUNSA ERROR`.
- `ctest --test-dir build-godot --output-on-failure -j2`: **16/16**.
- `git diff --check`: OK.
- `git diff -- addons/chunsa_sim/core`: vacío.

El intento del gate dentro del sandbox falló antes de arrancar por la incapacidad
ambiental de abrir `user://logs/...` y el binario terminó con SIGSEGV; la repetición
autorizada fuera del sandbox pasó. La captura opcional tampoco pudo producir imagen:
el renderer dummy devolvió una textura vacía (`CHUNSA SHOT: imagen vacía`), sin
afectar al gate de lógica.

## Alcance y deuda

No se detectó ninguna carencia del kernel que requiriera improvisación. El binario
`demo/bin/libchunsa_godot.so` fue regenerado junto con el adaptador. El cambio ajeno
no seguido `.claude/` se dejó intacto.
