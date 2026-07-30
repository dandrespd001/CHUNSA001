# Brief 1.8E — Costes visibles y construcción con constructores

**Modelo:** GPT-5.6 Luna Max · **Rama:** `gpt/ui-costes-construccion-1.8e`
**Normativo:** `SPEC-006` **Parte IV** (§17–§21)

---

## 0. De dónde sale este sprint

De un test de juego completo del Director. Tres quejas, y la conclusión que
importa:

> **El kernel es más capaz de lo que la interfaz expone.** Ninguna de las tres
> necesita tocar la simulación.

1. «No se muestra el coste para construir, investigar o avanzar de edad hasta
   realizar la acción.» El catálogo tiene todos los costes; solo hay que
   enseñarlos **antes**.
2. «Los edificios aparecen por arte de magia.» Falso, y al revés de lo que
   parece: `construction_system` **ya exige proximidad** y hace caminar al
   constructor. Lo que pasa es que colocar un edificio **no asigna a nadie**,
   así que la obra se queda quieta para siempre y nadie explica por qué.
3. «No están implementados diversos edificios.» Hay **cuatro** `constructible:
   true` en el catálogo y el adaptador ofrece **uno** (`buildable_id=2`).

**Criterio de diseño rector, de SPEC-006 §18:** *el jugador no debe aprender el
juego por rechazos.* Un rechazo es una red de seguridad, no un canal de
información.

---

## 1. Qué puedes y qué no puedes tocar

**SÍ:** `addons/chunsa_sim/gdextension/`, `demo/`, el binario
`demo/bin/libchunsa_godot.so` (hay que regenerarlo) y `tests/unit/` **para
añadir pruebas** de la política pura (ver §3).

**NO:** `addons/chunsa_sim/core/` · `data/` · `tools/`. El kernel y los datos
están cerrados en este sprint. **Si crees que necesitas tocarlos, PARA y
repórtalo**: significa que el contrato está mal escrito, y en este proyecto eso
ha pasado varias veces por mi culpa, no por la del implementador.

---

## 2. El estado del que partes (verificado contra el repo, 2026-07-30)

No des nada de esto por supuesto: lo comprobé antes de firmar el brief.

- `construction_system` **ya comprueba la distancia** del constructor a la obra
  y lo hace caminar. No lo reimplementes.
- `ASSIGN_BUILD` **ya existe** como comando y funciona: en el log del Director
  se ve `ASSIGN_BUILD enqueued citizens=2` aceptado. Lo que falta es que
  **colocar** un edificio lo dispare solo.
- Los aldeanos **ya aceptan `MOVE_TO`** desde el commit `ba3c229`.
- El adaptador **ya sabe explicar un rechazo** por recurso y cantidad
  (`Faltan 60 de Madera`). Esa función es la de mejor relación valor/coste del
  proyecto: **reutilízala hacia el momento anterior a la decisión**, no la
  dupliques.
- `player_civ` es `INVALID_CIV_ID` en la demo (nadie llama a
  `gs_set_player_civ`), y `append_civilization_detail` ya trata ese caso.

### Los cuatro edificios NO coexisten

Me equivoqué en la primera versión del SPEC y lo corregí; que no te pase a ti:

| Edificio | Civ | `epoch_window` | Obra |
|---|---|---|---|
| `egipto:chariotry_stable` | egipto | **[3, 4]** | 600 |
| `egipto:shena_granary` | egipto | **[3, 4]** | 500 |
| `rome:castra_barracks` | rome | **[5, 5]** | 600 |
| `rome:horreum` | rome | **[5, 5]** | 500 |

En la época 3–4 salen **dos**; los romanos aparecen en la **época 5**. Una
lista fija de uno **y** una lista fija de cuatro son igual de incorrectas.

---

## 3. La deuda que este sprint empieza a pagar

**El adaptador ha causado tres desincronizaciones con el kernel porque no tenía
pruebas.** Ya hay patrón para arreglarlo, y lo usarás:

- `fog_view.hpp` y `outcome_view.hpp` (Sprint 1.8F) son **cabeceras de política
  pura**, sin tipos de Godot, incluidas por el adaptador y **cubiertas por
  pruebas** en `tests/unit/` (`chunsa_test_fog_view`, `chunsa_test_outcome_view`).
  El cableado en `CMakeLists.txt` es de cinco líneas; cópialo.

**Obligatorio**: la lógica de **asequibilidad** —dado un coste y un stock, qué
falta y cuánto— va en una cabecera así, con pruebas, **no** enterrada en
`draw_*`. Es una función pura de dos entradas: no hay excusa para no probarla.

Casos que la prueba debe cubrir, como mínimo: alcanza justo; falta de un solo
recurso; falta de varios; coste vacío; recurso con stock 0.

---

## 4. Qué construir (SPEC-006 §18–§20)

Lee la Parte IV entera. Resumen de intención:

1. **Costes antes de comprometerse**, por recurso y **con nombre real**
   («100 de madera, 50 de piedra»), en edificios, tecnologías, entrenamiento y
   subida de época.
2. **Asequibilidad a simple vista**, y **cuánto falta** si no llega.
3. **Requisitos no económicos también antes**: «Requiere época 5».
4. **Colocar un edificio asigna constructores**: los ciudadanos seleccionados,
   o los ociosos más cercanos si no hay selección.
5. **Una obra sin constructores lo dice en pantalla.** Es el origen real de la
   sensación de «magia»: la obra aparece y no pasa nada.
6. **Progreso de obra y número de constructores** visibles.
7. **Lista de edificios desde el catálogo**, filtrada por civ y época.

**Lo que NO toca**: la electricidad (§14.4) sigue sin existir en el kernel; las
recetas y `CRAFT` son del 1.9. No inventes datos que no están.

---

## 5. Verificación — ejecución real

**Dos veces se ha descubierto en este proyecto un informe que afirmaba una
ejecución que no ocurrió, y una vez un artefacto obsoleto que solo se vio
mirando una captura.** Por eso:

- Ejecuta `godot --path demo` **de verdad** y pega la consola.
- **Captura de pantalla real** (no headless) con el panel de costes abierto:
  hay que ver que no desborda a 1920×1080. `CHUNSA_SHOT=prefix` guarda un PNG
  del viewport en el frame 600.
- **Míra la captura.** No basta con generarla.
- **Sin mojibake**: todo literal con acentos pasa por el helper `U()`, o sale
  roto en pantalla y en la terminal.
- Si no puedes ejecutar Godot, **dilo**. Es información útil; afirmarlo sin
  hacerlo destruye la confianza en todo el informe.

---

## 6. Definición de hecho

- [ ] Los diez criterios de `SPEC-006` §21, uno por uno, con evidencia.
- [ ] Asequibilidad en cabecera de política pura **con pruebas**, etiquetadas
      `fast` en `CMakeLists.txt`. Sin etiqueta se caen del ciclo rápido.
- [ ] Salida **roja** de esas pruebas antes de implementarlas, pegada
      (`METODOLOGIA_TDD.md`: fallo por **aserción**, no por compilación).
- [ ] Salida **verde** pegada.
- [ ] `ctest -L fast` verde · `ctest` completo verde.
- [ ] Apertura y eco **sin cambiar**: este sprint no toca simulación ni datos.
      Si un `end_tick` se mueve, **PARA**: has tocado algo que no debías.
- [ ] `demo/bin/libchunsa_godot.so` **regenerado y commiteado**.
- [ ] `git diff --stat -- addons/chunsa_sim/core/ data/ tools/` → **vacío**.
- [ ] Captura de pantalla real, mirada, adjunta o descrita.
- [ ] **Commitea en la rama. NO fusiones a `main`.**
- [ ] Informe en `docs/RESULT_LUNA_1.8E.md`.

**Sin pipes que enmascaren el código de salida**: `cmd; echo $?`.

**No afirmes haber ejecutado nada que no hayas ejecutado.**
