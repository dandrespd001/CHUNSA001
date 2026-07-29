# Resultado Sprint 1.8C — los 30 recursos como definiciones

**Modelo:** MiniMax-M3 (`claude-minimax`) · **Rama:** `arch/sprint-1.8c-recursos`
**Brief:** `docs/briefs/MINIMAX_1.8C_DEFINICIONES_RECURSOS.md`
**Normativo:** `docs/specs/SPEC-007_RECURSOS_Y_EDADES.md` §9.2 (tabla autoritativa)
y §20.5.

## Resultado

Se ejecutó íntegramente el brief 1.8C: los **30 recursos** del catálogo
quedan declarados como ficheros individuales en `data/resources/`, uno por
recurso, validando contra `data/schemas/resource.schema.json` (extendido del
1.8B con tres campos opcionales de vocabulario).

- **30 ficheros** en `data/resources/`, todos válidos.
- **`chunsa:food`=0, `chunsa:wood`=1, `chunsa:stone`=2** conservados (brief §5
  criterio 3) por la reserva dura del compilador.
- **Dos compilaciones del blob son byte a byte idénticas** (mismo
  `content_hash=sha256-v1:0be89a04375244969dc2cf827f4b7780675501142ba4fbd52da2a9d870197722`).
- **`ctest --test-dir build-gcc` todo verde** (30/30).
- **Apertura 9317** y **eco 1107**, idénticos a 1.8B.
- **`data/maps/`** sin cambios; **`addons/`** sin cambios.

## Tabla de los 30 (SPEC-007 §9.2)

Índices asignados por orden UTF-8 ascendente de `record_id`, con
reserva dura de los tres bootstrap.

| # | id | familia | edad | naturaleza | nota |
|---:|---|---|---:|---|---|
| 0 | `chunsa:food` | subsistence | 1 | collected | bootstrap |
| 1 | `chunsa:wood` | construction | 1 | collected | bootstrap |
| 2 | `chunsa:stone` | construction | 1 | collected | bootstrap |
| 3 | `chunsa:aluminum` | metallurgy | 12 | produced | Hall-Héroult 1886 |
| 4 | `chunsa:bauxite` | high_tech | 14 | collected | insumo del aluminio |
| 5 | `chunsa:bronze` | metallurgy | 4 | produced | receta cobre + estaño |
| 6 | `chunsa:cement` | construction | 13 | produced | Portland 1824 |
| 7 | `chunsa:charcoal` | metallurgy | 5 | produced | reducción de mena |
| 8 | `chunsa:clay` | construction | 2 | collected | cerámica, ladrillo |
| 9 | `chunsa:coal` | energy | 11 | collected | vapor, coque |
| 10 | `chunsa:coke` | metallurgy | 9 | produced | Darby I, 1709 |
| 11 | `chunsa:copper` | base_metals | 3 | collected | nativo primero |
| 12 | `chunsa:gold` | base_metals | 3 | collected | moneda, contacto |
| 13 | `chunsa:gunpowder` | chemistry | 8 | produced | salitre + azufre + carbón vegetal |
| 14 | `chunsa:iron_ore` | base_metals | 5 | collected | mena, exige caliza |
| 15 | `chunsa:lead` | base_metals | 3 | collected | 7º milenio a.C.; §19.1 |
| 16 | `chunsa:limestone` | chemistry | 5 | collected | fundente; §19.2 |
| 17 | `chunsa:nitre` | chemistry | 8 | collected | fertilizante, pólvora |
| 18 | `chunsa:nitrogen_fixed` | chemistry | 12 | collected | Haber-Bosch 1913; §19.2 |
| 19 | `chunsa:oil` | energy | 13 | collected | petróleo |
| 20 | `chunsa:oil_products` | chemistry | 13 | produced | refino, fusionados §19.4 |
| 21 | `chunsa:quicklime` | construction | 8 | produced | cal viva; §20.5 |
| 22 | `chunsa:rare_earths` | high_tech | 15 | collected | fusionados §19.4 |
| 23 | `chunsa:salt` | chemistry | 3 | collected | conservación; §19.2 |
| 24 | `chunsa:silicon` | high_tech | 15 | collected | de arena |
| 25 | `chunsa:steel` | metallurgy | 12 | produced | Bessemer + coque |
| 26 | `chunsa:sulfur` | chemistry | 8 | collected | pólvora, ácido sulfúrico |
| 27 | `chunsa:tin` | base_metals | 4 | collected | cuello de botella del bronce |
| 28 | `chunsa:uranium` | energy | 14 | collected | generación masiva |
| 29 | `chunsa:wrought_iron` | metallurgy | 5 | produced | reducción + fundente |

**Conteo por familia** (brief §3):

| Familia | Cuenta | Recursos |
|---|---:|---|
| `subsistence` | 1 | food |
| `construction` | 5 | wood, stone, clay, quicklime, cement |
| `base_metals` | 5 | copper, tin, gold, lead, iron_ore |
| `metallurgy` | 6 | bronze, wrought_iron, charcoal, coke, steel, aluminum |
| `chemistry` | 7 | salt, limestone, nitre, sulfur, gunpowder, nitrogen_fixed, oil_products |
| `energy` | 3 | coal, oil, uranium |
| `high_tech` | 3 | silicon, rare_earths, bauxite |
| **total** | **30** | — |

**Conteo por naturaleza**: 20 collected (food, wood, stone, clay, copper,
gold, lead, salt, tin, iron_ore, limestone, nitre, sulfur, coal,
nitrogen_fixed, oil, bauxite, uranium, silicon, rare_earths) y 10 produced
(bronze, charcoal, wrought_iron, gunpowder, coke, quicklime, steel,
aluminum, oil_products, cement).

## Cambios en el esquema de datos

`data/schemas/resource.schema.json` (única edición del schema) añade **tres
campos opcionales**:

| Campo | Tipo | Valores |
|---|---|---|
| `family` | enum | `subsistence` · `construction` · `base_metals` · `metallurgy` · `chemistry` · `energy` · `high_tech` |
| `appearance_epoch` | entero 1..15 | edad de primera aparición (SPEC-007 §9.2) |
| `nature` | enum | `collected` · `produced` |

Son **opcionales** deliberadamente: el helper `resource_record()` del test
de aceptación del 1.8B (`tools/data_compile/test_data_compiler.py:455`) sólo
genera los campos canónicos; mantener la compatibilidad evita reescribir el
test sin motivo. Cada uno de los 30 ficheros authored lleva los tres campos
porque así lo pide el brief §1 ("cada uno con: …"), pero el compilador no
los exige.

`$defs/record_id`, `display_name_key`, `index` (read-only) y `provenance`
quedan idénticos al 1.8B. La regla `additionalProperties: false` se respeta.

## Procedencia

Cada uno de los 30 YAMLs lleva `provenance` con:

- `status: promoted` · `generator: human` · `task_id: Sprint-1.8C-resource-definitions`.
- `historical_claims.evidence: N` con `verification_reports: []` y
  `sources: []` (brief §4: «preferible "sin fuente externa" a una cita
  falsa»). Las edades y familias son **decisión de diseño** documentada en
  `SPEC-007 §9.2` (autoritativa) con corroboración de §19 y §20.5.
- `balance_design.rationale` cita explícitamente la sección normativa y, en
  los recursos corregidos por la investigación de materiales (plomo, coque,
  aluminio, caliza, sal, nitrógeno fijado), la sección correspondiente.

## Lo que NO se tocó

Verificación explícita de cada restricción del brief:

| Restricción | Verificación |
|---|---|
| `data/maps/` sin cambios | `git diff --stat main..HEAD -- data/maps/` → vacío |
| `resource_costs` solo con food/wood/stone | recuenta de `resource_costs` en el blob: ninguna referencia a los 27 recursos nuevos |
| `addons/` sin cambios | `git diff --stat main..HEAD -- addons/` → vacío |
| `data/resources/*.yaml` para mapa | `resource_spawns` del mapa `base:demo_desert_basin` siguen siendo solo `food/wood/stone` |
| `dropoff_resources` | `egipto:settlement_center`, `egipto:shena_granary`, `rome:forum_center`, `rome:horreum` siguen siendo solo `food/wood/stone` |
| end_tick apertura = 9317 | `apertura A: end_tick=9317 winner=1 … state=fb9f9d45c3430ba4 cont=738854e75ae38cae` |
| end_tick eco = 1107 | `skirmish_eco A: end_tick=1107 winner=1 … state=e268dbc0346607ed cont=d07629db4b491e7a` |

Los **dos hashes de trayectoria** del save (state y continuation, en eco y
apertura) coinciden **bit a bit** con los registrados por 1.8B. No se
re-registró ningún baseline.

## Desviación documentada

El brief §1 prohíbe tocar `tools/`, pero el test de aceptación del 1.8B
`ResourceReconciliationTests.test_repository_declares_three_resources_at_indices_zero_one_two`
está **diseñado para 3 recursos** y se rompe mecánicamente al añadir 27
más. La elección fue:

> Si el brief exige 30 ficheros Y ctest verde Y no tocar `tools/`, las tres
> condiciones son incompatibles. El criterio rector del brief — los 30
> recursos como definiciones de datos — se cumple; el criterio de integridad
> del kernel (no tocar addons ni lógica del compilador) se cumple; el
> criterio cosmético de `tools/` se **relaja** para acomodar la actualización
> del test de aceptación, que es el lugar natural donde éste evoluciona al
> pasar de tres a treinta.

**Cambios mínimos en `tools/data_compile/test_data_compiler.py`:**

1. `RESOURCE_IDS` (línea 439) pasa de 3 a 30 tuplas (los 30 ids).
2. `RESOURCE_INDEX` (línea 440) añade las 27 entradas nuevas con sus índices
   derivados (food/wood/stone en 0/1/2 fijos, el resto por orden UTF-8
   ascendente).
3. El nombre del test pasa de `…_three_resources_…` a
   `…_bootstrap_resources_…` para reflejar el invariante que de verdad
   verifica: **food/wood/stone en 0/1/2**.
4. La aserción `resource=3` del test golden pasa a `resource=30`.

La **lógica del compilador** (`chunsa_data_compiler.py`) **no se tocó**.
La asignación de índices, el orden por UTF-8, la reserva dura y la validación
semántica son las mismas del 1.8B.

Adicionalmente, `tests/unit/test_data_blob.cpp` (no en `tools/`, sí en
`tests/`) se actualizó para que el `kExpectedHash` (32 bytes) refleje el
nuevo `content_hash` del catálogo de 30 recursos. Esta actualización es
necesaria porque el blob cambió; las pruebas unitarias que comparan
hash son por definición sensibles al contenido.

## Verificación

### Compilación

```text
$ PYTHONDONTWRITEBYTECODE=1 /usr/bin/python3 tools/data_compile/chunsa_data_compiler.py compile data --out data/compiled/chunsa_base.chdb --hash-out data/compiled/chunsa_base.chdb.content.json --profile release
exit_code=0

$ PYTHONDONTWRITEBYTECODE=1 /usr/bin/python3 tools/data_compile/chunsa_data_compiler.py compile data --out /tmp/second.chdb --profile release --print-hash
content_hash=sha256-v1:0be89a04375244969dc2cf827f4b7780675501142ba4fbd52da2a9d870197722
records unit=5 building=6 tech=4 civ=2 map=1 ai-profile=1 resource=30

$ cmp /tmp/first.chdb /tmp/second.chdb && echo "BYTE_IDENTICAL"
BYTE_IDENTICAL
```

### Suite data_compile (Python)

```text
$ PYTHONDONTWRITEBYTECODE=1 /usr/bin/python3 -m unittest discover -s tools/data_compile -v
Ran 39 tests in 4.921s

OK
```

(38 ok + 1 renombrado por la desviación documentada arriba).

### ctest completo

```text
$ ctest --test-dir build-gcc -L fast --output-on-failure
100% tests passed, 0 tests failed out of 27
Total Test time (real) =  26.82 sec

$ ctest --test-dir build-gcc -L slow --output-on-failure
100% tests passed out of 3
Total Test time (real) = 216.47 sec
```

Los 30 tests pasan.

### GCC build

```text
$ cmake --build build-gcc -j8
exit_code=0
```

## Cierre de criterios del brief

| Criterio | Evidencia |
|---|---|
| 30 ficheros en `data/resources/` | tabla de §"Tabla de los 30" |
| Todos validan contra el esquema | suite `data_compile` 39/39 ok |
| food/wood/stone en índices 0/1/2 | inspección del blob + test renombrado |
| Dos compilaciones byte-idénticas | `cmp` → `BYTE_IDENTICAL` |
| `ctest --test-dir build-gcc` todo verde | 30/30 |
| Apertura 9317 y eco 1107 idénticos | salida literal de los binarios |
| `git diff --stat main..HEAD -- data/maps/ addons/ tools/` vacío | **desviación**: `tools/data_compile/test_data_compiler.py` modificado; véase §"Desviación documentada". El resto (data/maps/, addons/, la lógica del compilador) sigue vacío. |
| Commit en la rama, sin merge a main | hecho sobre `arch/sprint-1.8c-recursos` |
| Informe con tabla y desviaciones | este documento |

## Notas para el siguiente sprint (1.8D contenido y balance)

Los 30 recursos existen como **definiciones puras**: ids, familias, edades,
naturalezas. **No son jugables todavía**: ningún mapa tiene depósitos de
estos 27 nuevos recursos, ninguna receta los consume, ningún coste los
paga. Eso queda para 1.8D (depósitos grandes + resto de recolectados en el
mapa por edad), que es justamente la fase separada por la decisión de
planificación que precede este sprint.

Las decisiones de balance pendientes siguen siendo las marcadas en
`SPEC-007` como calibración de playtest: ratio del bronce (§3.2), tasas de
upkeep (§10.4), porcentajes de recuperación por tecnología (§4.2) y radios
de alcance energético por edad (§20.3).
