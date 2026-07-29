# Brief 1.8B — Reconciliación del esquema de recursos

**Modelo:** GPT-5.6 SOL · **Rama:** `arch/sprint-1.8b-esquema-recursos`
**Contratos:** `SPEC-007` **§18** (normativo) · `CONCORDANCIA_SPEC-007.md` C5–C7, H11
**Metodología:** `METODOLOGIA_TDD.md` — **obligatoria**

---

## 0. Reglas innegociables

1. Determinismo bit-exacto: cero float, cero reloj, cero entropía fuera de
   `RngStream`. Recorrido ascendente, desempate por índice bajo.
2. `Step()` sin heap ni STL.
3. `movement_v1` CONGELADO · `CommandType` append-only.
4. `GameState` en heap en los tests.
5. Nada de `assert()` para validar datos.

---

## 1. Qué es este sprint

Al preparar el 1.8B descubrí que `data/schemas/` **ya tenía** un modelo de
recursos, diseñado por delante del kernel y **distinto** del aprobado. Lee
**SPEC-007 §18 entero** antes de tocar nada.

Había dos vocabularios de coste en paralelo y un `El` (electricidad) como
recurso almacenable, que contradice §8.1.

**Este sprint reconcilia el vocabulario. NO añade recursos al juego.**

### Lo que SÍ

1. **Eliminar el enum de 8 letras** (`A, B, P, W, Me, F, I, El`) de
   `common.schema.json`.
2. **Recursos por `record_id` namespaced**: un recurso se declara en
   `data/resources/*.yaml` con `id: chunsa:food`. Schema nuevo
   `resource.schema.json`.
3. **El compilador asigna los índices numéricos**, no el autor del YAML. La
   asignación debe ser **determinista y reproducible**: dos compilaciones del
   mismo conjunto de ficheros producen **el mismo blob byte a byte**.
4. **Fusionar `material_costs` en `resource_costs`.** Coste de migración cero:
   no se usa en ningún fichero. Elimina también `material_cost` y
   `material_costs` de `common.schema.json`.
5. **`El` deja de existir** como recurso almacenable.
6. **Renombrar** los tres actuales a `chunsa:food`, `chunsa:wood` y
   `chunsa:stone` **conservando los índices 0, 1 y 2**.
7. Un coste que referencie un **id inexistente** es **error de carga** con
   código, nunca un índice basura (SPEC-008 §3.3).

### Lo que NO

- **No** añadir los 30 recursos: eso es el 1.8C. Aquí solo los tres actuales,
  renombrados.
- **No** tocar recetas más allá de renombrar `output_material_id` →
  `output_resource_id` en el esquema. Las recetas siguen vacías.
- **No** tocar `addons/chunsa_sim/gdextension/`.
- **No** cambiar `RESOURCE_COUNT` (ya es 32).

---

## 2. Criterio de éxito

**Las trayectorias no cambian.** Los tres recursos conservan sus índices, así
que el juego debe comportarse **exactamente igual**.

| Invariante | Debe seguir |
|---|---:|
| apertura `end_tick` | **9317 exacto** |
| eco `end_tick` | **1107 exacto** |
| G1 `alloc_delta` | 0 |
| golden | 1074 / 0 |

**Si un `end_tick` se mueve un tick, PARA y repórtalo.** No lo re-registres:
sería señal de que el renombrado cambió comportamiento, que es imposible si
está bien hecho.

`SAVE_FORMAT_VERSION` y `CHECKSUM_ALGO_VERSION`: **solo súbelos si el dominio
del checksum cambia de verdad**. Renombrar no lo cambia. Justifica la decisión
en el informe, sea cual sea.

---

## 3. Protocolo TDD — obligatorio

1. Escribe **primero** las pruebas de §4.
2. Compílalas y **ejecútalas antes de implementar**. Deben **fallar por
   aserción**, no por compilación. Si algo no existe todavía, declara un stub
   con valor incorrecto a propósito para que el fallo sea de aserción.
3. **Pega la salida roja en el informe.**
4. Implementa lo mínimo. Pega la salida verde.

**Un informe sin salida roja se rechaza sin revisar el código.** Si una prueba
pasa en fase roja, dilo: significa que no prueba nada.

---

## 4. Criterios de aceptación (= pruebas)

Son los 25–31 de SPEC-007 §18.5, más los de reproducibilidad:

1. `common.schema.json` **no** contiene el enum de 8 letras.
2. `material_costs` y `material_cost` **no existen** en ningún esquema.
3. `El` no aparece como recurso almacenable en ningún sitio.
4. Un recurso se declara con `record_id` namespaced y valida contra
   `resource.schema.json`.
5. Los tres recursos actuales quedan en los índices **0, 1 y 2**.
6. **Dos compilaciones del mismo YAML producen blobs byte a byte idénticos.**
7. **Compilar en orden de fichero distinto produce el mismo blob**: la
   asignación de índices **no** depende del orden de lectura del directorio.
8. Un coste que referencia un id inexistente ⇒ **error de carga con código**,
   no índice basura ni fallo silencioso.
9. Un recurso duplicado en dos ficheros ⇒ error de carga.
10. Los cuatro invariantes de §2 se mantienen.

La prueba **7 es la más importante**: el orden de `readdir` no es estable entre
sistemas de ficheros, y si el índice dependiera de él, el blob dejaría de ser
determinista entre máquinas. Es exactamente la clase de fallo que no aparece
hasta que alguien compila en otro equipo.

---

## 5. Definición de hecho

- [ ] Salida **roja** pegada, con fallos de aserción.
- [ ] Salida **verde** pegada.
- [ ] `cmake --build build-gcc -j8` limpio.
- [ ] `ctest --test-dir build-gcc -L fast` verde en **≤ 60 s**.
- [ ] `ctest --test-dir build-gcc` todo verde.
- [ ] Los cuatro invariantes de §2, **idénticos**.
- [ ] Pruebas nuevas **etiquetadas** `fast` o `slow` en `CMakeLists.txt`. Sin
      etiqueta se caen del ciclo rápido sin que nadie se entere.
- [ ] ASan y UBSan verdes sobre los binarios tocados; salida pegada.
- [ ] Informe en `docs/RESULT_SOL_1.8B.md`.

**Sin pipes que enmascaren el código de salida**: `cmd; echo $?`.

**No afirmes haber ejecutado nada que no hayas ejecutado.**

**No hagas merge a `main`.**
