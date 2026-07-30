# Brief 1.8C-UI — HUD de recursos por familias

**Modelo:** GPT-5.6 Luna Max · **Rama:** `gpt/hud-familias-1.8c`
**Normativo:** `SPEC-006` **Parte III** (§12–§16) · `SPEC-007` §9.2

---

## 0. Qué puedes y qué no puedes tocar

**SÍ:** `addons/chunsa_sim/gdextension/`, `demo/`, y el binario
`demo/bin/libchunsa_godot.so` (hay que regenerarlo).

**NO:** `addons/chunsa_sim/core/` ni `tests/` ni `data/resources/` ni `tools/`.
El kernel está cerrado para este sprint.

---

## 1. El estado del que partes (verificado, 2026-07-30)

Tres cosas que necesitas saber y que ya comprobé por ti:

1. **El catálogo ya expone los metadatos.** Del Sprint 1.8C-kernel tienes, por
   índice de recurso: `display_name_key`, `family`, `appearance_epoch` y
   `nature`. Úsalos. **No cablees la lista de recursos en C++.**
2. **`resource_label()` en `chunsa_sim_node.cpp:130` está OBSOLETO.** Devuelve
   `"A"`, `"B"`, `"Me"` por índice, y esos nombres ya no existen: los recursos
   se renombraron a `chunsa:food`, `chunsa:wood`, `chunsa:stone` y hay 30.
   **Hay que sustituirlo por lectura del catálogo.**
3. **`data/localization/` está vacío.** No hay ninguna traducción en el
   proyecto todavía.

## 2. Localización: en Godot, no en el kernel

El catálogo da la **clave** (`chunsa:resource.copper`); la **traducción** es
presentación y va en el proyecto Godot.

Crea una traducción de Godot en `demo/` (CSV importado como `Translation`, o el
mecanismo nativo que prefieras) con las claves de los **30 recursos** y las
**7 familias**, en español. Resuelve con `tr()`.

Si una clave no tiene traducción, **muestra el identificador** (`copper`) en vez
de una cadena vacía. Un hueco visible se arregla; uno invisible se queda años.

**Recuerda `String::utf8`**: los acentos pasan por el helper `U()` del Sprint
1.7A o salen como mojibake. Ya nos costó una sesión de pruebas del Director.

## 3. Qué construir (SPEC-006 §13–§14)

1. **Seis familias**, leídas del catálogo:
   `subsistence` · `construction` · `base_metals` · `metallurgy` ·
   `chemistry` · `energy` · `high_tech`.
2. **Solo los recursos de la edad actual del jugador**: si
   `appearance_epoch > player_epoch`, no se muestra. La interfaz **crece con la
   partida**.
3. **Familia colapsada = su recurso más escaso**, no la suma. Sumar comida y
   piedra no significa nada.
4. **Expandible**: al abrir una familia se ven sus recursos individuales.
5. **Los rechazos siguen nombrando el recurso y la cantidad.** Hoy dice
   `Faltan 200 B`; debe decir `Faltan 200 de piedra`. **Esta funcionalidad es
   la de mejor relación valor/coste del proyecto** —fue lo que permitió al
   Director entender por qué no podía subir de época—; no la rompas al ampliar
   a 30 recursos.

**Lo que NO toca todavía**: la electricidad como producción/consumo (§14.4).
No existe en el kernel hasta el Sprint 1.10. Deja el hueco preparado si quieres,
pero **no inventes el dato**.

## 4. Realidad del contenido, para que no te alarmes

Hoy **solo `chunsa:food`, `chunsa:wood` y `chunsa:stone` tienen valores
distintos de cero**: nada cuesta los otros 27 todavía (eso es el Sprint 1.8D).

Es lo esperado. El HUD debe verse correcto **con casi todo a cero**, y ése es
justamente el caso que hay que comprobar: que no muestre 27 contadores vacíos
ocupando la pantalla.

## 5. Verificación — ejecución real, no afirmación

**El Director descubrió una vez que un informe afirmaba una ejecución en Godot
que no había ocurrido.** No repitas eso.

- Ejecuta `godot --path demo` de verdad.
- **Pega la salida de consola** en el informe.
- **Confirma en pantalla** que no queda mojibake y que las familias se ven.
- Si no puedes ejecutar Godot, **dilo**. Es información útil; afirmarlo sin
  hacerlo destruye la confianza en todo el informe.

## 6. Definición de hecho

- [ ] El HUD lee familias, edades y nombres **del catálogo**; cero listas de
      recursos cableadas en C++.
- [ ] `resource_label()` obsoleto **eliminado** o reescrito sobre el catálogo.
- [ ] Traducciones de los 30 recursos y las 7 familias, en español, en `demo/`.
- [ ] En la edad 3 **no** aparecen recursos de edades posteriores.
- [ ] Familia colapsada muestra su recurso **más escaso**.
- [ ] Un rechazo por stock nombra el recurso y la cantidad.
- [ ] **Sin mojibake**, verificado en ejecución.
- [ ] No desborda a 1920×1080 con todos los recursos activos.
- [ ] `demo/bin/libchunsa_godot.so` **regenerado y commiteado**.
- [ ] `git diff --stat -- addons/chunsa_sim/core/ tests/ data/ tools/` → **vacío**.
- [ ] **Commitea en la rama. NO fusiones a `main`.**
- [ ] Informe en `docs/RESULT_LUNA_1.8C_HUD.md` con la salida de consola real.
