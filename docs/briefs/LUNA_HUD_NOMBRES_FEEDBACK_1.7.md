# Brief — HUD: codificación, nombres y feedback de rechazos (Sprint 1.7A)

**Modelo:** GPT-5.6 Luna Max (`codex -m gpt-5.6-luna`)
**Rama base:** `gpt/apertura-economica-1.6b` @ `7d1dbd8`
**Rama de trabajo:** `gpt/hud-nombres-1.7`
**Autoridad:** Arquitecto Jefe. Normativo.

Trabajo **exclusivamente de adaptador**: `addons/chunsa_sim/gdextension/`.
**Prohibido tocar `addons/chunsa_sim/core/`.** Si crees que necesitas un cambio
de kernel, para y repórtalo — va en otro sprint que ya está en diseño.

---

## 1. Mojibake: todo el HUD está mal codificado

**Causa raíz, ya diagnosticada — no la re-investigues:** `godot::String`
construido desde un `const char*` interpreta los bytes como **Latin-1**, no
UTF-8. El fichero está lleno de literales con acentos que se convierten
implícitamente (`"Época "`, `"caballería"`, `"SELECCIÓN"`, `"·"`, `"—"`), y por
eso en pantalla salen `ÃPOCA`, `caballerÃa`, `SELECCIÃN`, `Â·`, `â`.

Hoy hay **una sola** llamada a `String::utf8` en las ~2400 líneas del fichero
(`catalog_name`); esa es la correcta y es el patrón a extender.

**Corrección exigida:** añade un helper de fichero e imponlo en **todo** literal
que contenga cualquier byte no ASCII:

```cpp
// Godot interpreta const char* como Latin-1. Todo literal con caracteres no
// ASCII DEBE pasar por aquí o sale mojibake en pantalla.
static inline godot::String U(const char* s) { return godot::String::utf8(s); }
```

Barre el fichero entero: cabecera del HUD, panel de selección, minimapa,
etiquetas de depósito, mensajes de `print`, nombres de clase, textos de ayuda y
los mensajes de rechazo. Incluye los `UtilityFunctions::print` — la salida de
terminal también sale con `catÃ¡logo` y `Ã©poca`.

**Criterio de hecho:** `grep` sobre el fichero no debe encontrar ningún literal
con bytes no ASCII que no esté envuelto en `U(...)` o `String::utf8(...)`.

---

## 2. Nombres legibles

**Estado actual, verificado por el Arquitecto:**

- El catálogo solo expone `record_id_utf8` (el ID técnico:
  `egipto:settlement_center`). No hay nombres de presentación.
- `data/localization/` está **vacío**. `display_name_key` existe en el esquema
  YAML pero no se compila al blob ni hay tabla de traducciones.
- `slot_display_name()` devuelve para edificios el **record_id crudo**, y para
  unidades una clase genérica hardcodeada (`"infantería"`, `"caballería"`…), así
  que un carro de guerra egipcio y un legionario romano se llaman igual.

**Alcance de ESTE sprint — solución provisional y explícita.** La solución
definitiva es poblar `data/localization/` y llevar `display_name_key` hasta el
blob, y eso es un cambio versionado del formato CHDB que **no** entra aquí.

Implementa una tabla de presentación en el adaptador, `record_id → nombre`,
para los registros que existen hoy (5 unidades, 6 edificios, 4 tecnologías,
2 civilizaciones). Requisitos:

- **Marca la tabla como provisional** con un comentario que diga explícitamente
  que desaparece cuando exista localización real, y por qué está aquí.
- **Respaldo obligatorio**: si un `record_id` no está en la tabla, deriva un
  nombre legible del propio ID (quita el prefijo de civilización, sustituye
  `_` por espacios, mayúscula inicial). **Nunca** muestres el ID crudo y
  **nunca** muestres una cadena vacía.
- Úsala en el panel de selección, en la lista de acciones de un edificio
  (entrenar/investigar), en la cola de producción y en las etiquetas del mundo.

**Además**: cuando la selección sea una sola unidad, muestra su nombre concreto
del catálogo, no solo la clase. La clase puede ir como calificador secundario.

---

## 3. Feedback de rechazos: hoy miente por omisión

`RejectReason::ILLEGAL_STATE` se muestra como
`"estado ilegal (época/stock/cola)"`, que mete tres causas muy distintas en el
mismo saco. El Director estuvo una partida entera sin poder producir nada y sin
saber por qué. **Esto es el fallo de usabilidad más grave del demo.**

El kernel devuelve un único `ILLEGAL_STATE` y **no vamos a cambiarlo aquí**. La
solución es que el adaptador, que ya tiene el catálogo y el stock en el
snapshot, **explique la causa concreta antes o después de emitir el comando**.

Implementa una comprobación de presentación que produzca un mensaje accionable.
Como mínimo debe distinguir:

- **Época**: `"Requiere época 4 (estás en la 3)"` — compara
  `epoch_min/epoch_max` de la definición contra `snap_curr.player_epoch`.
- **Stock**: `"Faltan 60 B"` (o la lista de los que falten, con la cantidad
  exacta) — compara `resource_costs` contra `stock_a/stock_b/stock_me`.
- **Cola llena**: `"Cola de producción llena"` si `prod_count` alcanza
  `PROD_QUEUE_CAP`.

Reglas:

- El mensaje debe nombrar el recurso y la cantidad **que falta**, no el coste
  total.
- Si concurren varias causas, muéstralas todas: el jugador necesita saber todo
  lo que le bloquea, no solo lo primero.
- **La autoridad sigue siendo el kernel**: esto es explicación, no validación.
  No suprimas el envío del comando por tu comprobación — emítelo igual y deja
  que el kernel decida, salvo que ya hoy se filtre por otro motivo. Si el kernel
  acepta algo que tu comprobación creía ilegal, es un **bug de tu comprobación**
  y quiero saberlo.
- Muestra el mensaje donde ya se muestra `Último comando #N: aceptado`.

---

## 4. Definición de hecho

- [ ] `cmake --build build-godot -j2` limpio.
- [ ] `cmake --build build-gcc -j2 && ctest --test-dir build-gcc` → **25/25**.
      No tocas kernel, así que cualquier movimiento aquí es regresión: para y
      repórtalo.
- [ ] Gates bit-idénticos: `./build-gcc/chunsa_sim_cli run --selftest-g1` →
      `fefa48125dd35736`; `./build-gcc/chunsa_sim_cli savetest --ai` →
      `774316057e5667fb` / `d52ac0019700684f`.
- [ ] **Evidencia visual obligatoria**: ejecuta
      `CHUNSA_SHOT=/tmp/hud17 godot --path demo --quit-after 900` y **mira el
      PNG resultante**. Hay Godot 4.7.1 instalado y sesión gráfica: no hay
      excusa para no verificar. Confirma en la imagen que no queda mojibake en
      ninguna cadena visible. Reporta la ruta del PNG.
- [ ] Informe en `docs/RESULT_LUNA_HUD_1.7.md`: qué cambiaste, la tabla de
      nombres provisional, ejemplos reales de los mensajes de rechazo, y
      cualquier desviación con su justificación.

**No afirmes haber ejecutado nada que no hayas ejecutado.** En la entrega
anterior el informe describía una verificación headless de Godot que era
imposible (no había binario instalado). Se detectó. Si no puedes verificar algo,
dilo — es información útil; inventarlo destruye la confianza en todo el informe.

**Verifica sin pipes que enmascaren el código de salida**: `cmd; echo $?`.

**No hagas merge.** Deja la rama y el informe.
