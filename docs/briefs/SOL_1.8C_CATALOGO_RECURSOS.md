# Brief 1.8C-kernel — El catálogo expone los metadatos de recurso

**Modelo:** GPT-5.6 SOL · **Rama:** `arch/sprint-1.8c-catalogo`
**Normativo:** `SPEC-006` Parte III §13–§14 · `SPEC-007` §9.2
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

## 1. El hueco

El Sprint 1.8C-datos escribió los 30 recursos con `family`, `appearance_epoch`,
`nature` y `display_name_key`. El compilador **los valida contra el esquema**
pero **no los mete en el blob**: la tabla de recursos solo lleva `id` e índice,
que era lo único que hacía falta para resolver costes.

Consecuencia: `SPEC-006` Parte III §13 dice «el HUD **lee** las familias del
catálogo» y hoy **es imposible**. Verificado antes de escribir este contrato.

**Sin esto, el HUD tendría que cablear la lista de 30 recursos y sus familias en
C++**, y entonces añadir un recurso por datos ya no bastaría — que es
exactamente lo que §16.1 de SPEC-007 promete.

## 2. Qué implementar

Una **tabla tipada nueva** en el blob y en `DataCatalogV1`, siguiendo el patrón
que ya usan `UnitDefinitionV1`, `BuildingDefinitionV1` y `TechDefinitionV1`. No
inventes una estructura distinta.

```
ResourceDefinitionV1
  index              uint8   (el que ya asigna el compilador)
  display_name_key   record_id / clave de localización
  family             enum     subsistence construction base_metals
                              metallurgy chemistry energy high_tech
  appearance_epoch   uint8   1..15
  nature             enum     collected | produced
```

Y el acceso correspondiente en `data_catalog.hpp`, con la misma forma que los
`catalog_find_*` existentes.

**El compilador**: emitir la tabla en el blob. La asignación de índices **no
cambia** (alfabética con `food`/`wood`/`stone` fijados en 0/1/2); solo se añaden
los campos que ya están en el YAML y se descartaban.

### Lo que NO

- **No** tocar `addons/chunsa_sim/gdextension/`: el HUD es otra rama, en
  paralelo.
- **No** cambiar los índices de recurso.
- **No** tocar `data/resources/*.yaml`: los datos ya están completos y correctos.
- **No** cambiar `RESOURCE_COUNT`.

## 3. Criterio de éxito

**Las trayectorias no cambian.** Añadir metadatos descriptivos al catálogo no
altera ninguna decisión de simulación.

| Invariante | Debe seguir |
|---|---:|
| apertura `end_tick` | **9317 exacto** |
| eco `end_tick` | **1107 exacto** |
| G1 `alloc_delta` | 0 |
| golden | 1074 / 0 |

Si un `end_tick` se mueve, **PARA y repórtalo**.

**Sobre el versionado**: el `content_hash` del blob **sí** cambia (lleva datos
nuevos). Decide si `SAVE_FORMAT_VERSION`/`CHECKSUM_ALGO_VERSION` deben subir y
**justifícalo**. Pista: el catálogo no está en el dominio del checksum de
estado; en el 1.8B razonaste bien un caso parecido.

## 4. Protocolo TDD — obligatorio

1. Escribe **primero** las pruebas de §5.
2. Compílalas y **ejecútalas antes de implementar**. Deben **fallar por
   aserción**, no por compilación. Declara stubs con valores incorrectos a
   propósito si hace falta.
3. **Pega la salida roja en el informe.**
4. Implementa lo mínimo. Pega la salida verde.

**Sin salida roja, el entregable se rechaza sin revisar el código.**

## 5. Criterios de aceptación (= pruebas)

1. El catálogo devuelve la `family` correcta de `chunsa:copper` (`base_metals`).
2. Devuelve `appearance_epoch` correcta de tres recursos de edades distintas.
3. Devuelve `nature` = `produced` para `chunsa:bronze` y `collected` para
   `chunsa:copper`.
4. Los 30 recursos tienen los cuatro campos poblados: **ninguno vacío ni cero
   por defecto**.
5. `family` fuera del enum en el YAML ⇒ **error de carga con código**.
6. `appearance_epoch` = 0 o > 15 ⇒ **error de carga con código**.
7. Los índices siguen siendo los mismos que antes del sprint: `food`=0,
   `wood`=1, `stone`=2, `aluminum`=3 … `wrought_iron`=29.
8. **Dos compilaciones producen blobs byte a byte idénticos.**
9. Los cuatro invariantes de §3, idénticos.

La prueba **4 es la que importa**: un campo que se lee como cero por defecto en
vez de fallar es la clase de error que aparece en el HUD como «todos los
recursos son de la familia 0» y cuesta una tarde encontrar.

## 6. Definición de hecho

- [ ] Salida **roja** pegada, con fallos de aserción.
- [ ] Salida **verde** pegada.
- [ ] `cmake --build build-gcc -j8` limpio.
- [ ] `ctest -L fast` verde en **≤ 60 s** · `ctest` completo verde.
- [ ] Pruebas nuevas **etiquetadas** `fast` o `slow`.
- [ ] Los cuatro invariantes de §3, idénticos.
- [ ] `git diff --stat main..HEAD -- addons/chunsa_sim/gdextension/ data/resources/` → **vacío**.
- [ ] ASan y UBSan verdes; salida pegada.
- [ ] **Commitea en la rama. NO fusiones a `main`.**
- [ ] Informe en `docs/RESULT_SOL_1.8C_CATALOGO.md`.

**Sin pipes que enmascaren el código de salida**: `cmd; echo $?`.

**No afirmes haber ejecutado nada que no hayas ejecutado.**
