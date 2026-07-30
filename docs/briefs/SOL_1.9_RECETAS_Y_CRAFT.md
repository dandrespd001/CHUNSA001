# Brief 1.9 — Recetas, `CRAFT=14` y el primer recurso producido

**Modelo:** GPT-5.6 SOL · **Rama:** `arch/sprint-1.9-recetas`
**Normativo:** `SPEC-007` **§12** (íntegro, es el contrato) · `SPEC-004` §12.2
**Metodología:** `METODOLOGIA_TDD.md` — **obligatoria**

---

## 0. Reglas innegociables

1. Determinismo bit-exacto: cero float, cero reloj, cero entropía fuera de
   `RngStream`. Recorrido ascendente, desempate por índice bajo.
2. `Step()` sin heap ni STL.
3. `movement_v1` CONGELADO · `CommandType` **append-only**.
4. `GameState` en heap en los tests.
5. Nada de `assert()` para validar datos.

---

## 1. Qué es este sprint

El que hace que **cobre y estaño sirvan para algo**. Hasta ahora un recurso
producido era una fila en un catálogo; aquí se fabrica de verdad.

**La restricción de diseño es firme y no se re-litiga** (SPEC-007 §12.1):
**cadenas de un solo paso.** `cobre + estaño → bronce`, nunca `A → B → C → D`.
Salió de una crítica del panel —«se añade una capa de gestión de fábricas
típica de Factorio, ajena al ritmo de un RTS»— que se aceptó. Si al implementar
te parece que una cadena de dos pasos sería más elegante, **no la hagas**.

### Alcance: la cadena del bronce y nada más

Deliberadamente recortado, por la misma razón que el 1.8D:

- **SÍ**: el mecanismo completo (tabla, estado, comando, sistema, versionado) y
  **una** cadena real que lo demuestre: `cobre + estaño → bronce` (época 4).
- **NO**: carbón vegetal, hierro forjado ni cal viva. La cal viva es de **época
  8** y no hay nada de época 8 en el juego; añadirla ahora es peso muerto.
  Van a un 1.9B de datos cuando el mecanismo esté cerrado.

---

## 2. Lo que verifiqué antes de firmar esto

No lo des por supuesto; lo comprobé en el repo hoy:

- `GATHER = 13` es el último `CommandType`. **El 14 está libre** para `CRAFT`.
- `SAVE_FORMAT_VERSION` = **14** y `CHECKSUM_ALGO_VERSION` = **9**. Suben a
  **15** y **10** (§12.5): `craft_recipe` y `craft_progress` entran en
  serialización y checksum.
- `research_tech`/`research_progress` ya existen: **copia ese patrón**, no
  inventes otro (§12.3).
- El esquema de edificio **ya tiene `recipes: []`** en todos los YAML, sin usar.
- Hay **6 edificios** y **ninguno de conversión**.

### El bloqueo que casi se me escapa

**El estaño no está en el mapa.** El 1.8D dejó 12 depósitos con comida, madera,
piedra, cobre, oro, arcilla y sal — y registró plomo y estaño como pendientes.
Sin estaño, el bronce es **infabricable** y este sprint entregaría un mecanismo
que no se puede ejercitar jugando.

Por eso el alcance incluye datos de mapa. No es ampliación: es lo que hace que
lo demás signifique algo.

---

## 3. Qué implementar

### 3.1 Kernel (SPEC-007 §12.2–§12.4, es literal)

- `RecipeDefinitionV1` como **tabla tipada nueva** del catálogo, con el mismo
  patrón que `UnitDefinitionV1`/`TechDefinitionV1`. Referencia no resoluble ⇒
  **error de carga con código**, nunca índice basura.
- `BuildingDefinitionV1` gana `recipes[8]` / `recipe_count`, análogo a
  `researches`.
- Estado: `craft_recipe[ENTITY_HARD_CAP]` (centinela `INVALID_RECIPE_ID`) y
  `craft_progress[ENTITY_HARD_CAP]`.
- `CRAFT = 14`. **El orden de validación de §12.4 es contractual**: los ocho
  pasos, en ese orden. La prueba 8 comprueba justamente que un comando que
  viola varias reglas devuelve el código de la **primera**.
- **Deducción por adelantado**: al aceptar se restan **todos** los inputs de
  golpe. Es deliberado (§12.4) y elimina una clase entera de casos.
- `craft_system` en la **misma fase** que `production_system`.

### 3.2 Datos mínimos para que sea jugable

- **Un edificio de conversión por civilización** (fundición), simétrico en
  coste y tiempo de obra, `epoch_window` que cubra la 4, `constructible: true`,
  con la receta del bronce en su lista.
- **La receta**: `cobre + estaño → bronce`, época 4.
- **Depósitos de estaño en el mapa**: par **espejado**
  (`x_der = 256000 - x_izq`, misma Y, mismo `amount`, mismo recurso), regla
  dura de `SPEC-004` §15.1. Un mapa asimétrico es injugable en competitivo y ya
  se corrigió un fallo de esto en el 1.6B.
- Ningún elemento por encima de **3 recursos** de coste (regla del 1.8D).

---

## 4. Lo que SÍ puedes tocar, y por qué lo digo explícitamente

**Puedes y debes tocar `tests/`.** En el 1.8D escribí un brief que prohibía
`tests/` y a la vez mandaba cambiar datos, lo cual invalida por construcción el
`kExpectedHash` de `test_data_blob.cpp`. El implementador hizo lo correcto
—reportarlo y no parchear— pero el contrato era imposible. **Era mi error, y no
lo repito.**

Concretamente estás autorizado a:

- Actualizar `kExpectedHash` en `tests/unit/test_data_blob.cpp` y cualquier
  aserción golden de catálogo que cambie por los datos nuevos. El valor sale de
  `data/compiled/chunsa_base.chdb.content.json`, que es la fuente autoritativa.
- **Re-registrar los baselines** de `tests/determinism/baselines.hpp`. Añadir
  depósitos y un edificio **mueve las trayectorias**, y eso es correcto aquí.

**Lo que NO**: `addons/chunsa_sim/gdextension/` (el adaptador es otra rama) y
`movement_v1`.

---

## 5. Criterio de éxito

Las trayectorias **se mueven a propósito**. Lo que no puede moverse:

| Invariante | Debe seguir |
|---|---|
| `winner` de la apertura | **1** |
| apertura termina | **< 36000 ticks** |
| las cuatro fases de la apertura | **observadas** |
| G1 `alloc_delta` | **0** |
| vectores dorados | **1074 / 0 fallos** |
| blob | **dos compilaciones byte a byte idénticas** |

**Si la apertura deja de terminar, o gana el bando equivocado, PARA.** Eso no
es un baseline a re-registrar: es contenido mal balanceado.

---

## 6. Protocolo TDD — obligatorio

1. Escribe **primero** las 11 pruebas de `SPEC-007` §12.6.
2. Ejecútalas **antes de implementar**. Deben fallar **por aserción**, no por
   compilación: declara stubs con valores incorrectos a propósito si hace falta.
3. **Pega la salida roja en el informe.**
4. Implementa lo mínimo. Pega la verde.

**Sin salida roja, el entregable se rechaza sin revisar el código.**

Las dos que más importan: la **2** (un input insuficiente ⇒ rechazo y **no
deduce nada** — una deducción parcial es un robo silencioso al jugador) y la
**11** (destruir el edificio a mitad de producción **no** acredita la salida ni
devuelve los inputs).

El checksum no se puede hacer con TDD: escribe primero la invariante de
pertenencia al dominio (prueba 10) y registra el valor después como cierre de
regresión.

---

## 7. Definición de hecho

- [ ] Salida **roja** pegada, con fallos de aserción.
- [ ] Salida **verde** pegada.
- [ ] Las 11 pruebas de §12.6, **etiquetadas** `fast` o `slow` en
      `CMakeLists.txt`. Sin etiqueta se caen del ciclo rápido sin que nadie se
      entere.
- [ ] `SAVE_FORMAT_VERSION` 14→15 y `CHECKSUM_ALGO_VERSION` 9→10, justificados.
- [ ] Se fabrica bronce **jugando**: hay estaño en el mapa y una fundición que
      lo convierte.
- [ ] Mapa **simétrico**, verificado.
- [ ] `ctest -L fast` verde en **≤ 60 s** · `ctest` completo verde.
- [ ] Invariantes de §5 · baselines nuevos **justificados** en el commit.
- [ ] ASan y UBSan verdes; salida pegada.
- [ ] `git diff --stat -- addons/chunsa_sim/gdextension/` → **vacío**.
- [ ] **Commitea en la rama. NO fusiones a `main`.**
- [ ] Informe en `docs/RESULT_SOL_1.9.md`.

**Sin pipes que enmascaren el código de salida**: `cmd; echo $?`.

**No afirmes haber ejecutado nada que no hayas ejecutado.**
