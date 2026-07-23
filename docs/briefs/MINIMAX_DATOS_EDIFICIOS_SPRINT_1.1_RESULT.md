# RESULT — Datos de edificios del slice (Sprint 1.1 · MiniMax)

## Estado: BLOQUEADO por schema/compiler

He creado los 4 YAML y actualizado las 2 fichas de civilización tal y como pedía el brief.
**El gate `data_compile` NO pasa en verde.** El fallo es del schema (no de mis YAML):
ver §3. NO he tocado schema, compilador, ni golden fixture. NO he mergeado a `main`.

## 1. Archivos creados

- `data/buildings/egipto_settlement_center.yaml` (record_id `egipto:settlement_center`, kind `civic`, dropoff `[A, B, Me]`, hp 1500, footprint 3x3, build_time 1, sin coste)
- `data/buildings/egipto_shena_granary.yaml`     (record_id `egipto:shena_granary`, kind `dropoff`, dropoff `[A]`, hp 600, footprint 2x2, build_time 500, coste B:60)
- `data/buildings/rome_forum_center.yaml`        (record_id `rome:forum_center`, kind `civic`, dropoff `[A, B, Me]`, hp 1500, footprint 3x3, build_time 1, sin coste)
- `data/buildings/rome_horreum.yaml`             (record_id `rome:horreum`, kind `dropoff`, dropoff `[A]`, hp 600, footprint 2x2, build_time 500, coste B:60)

Cada YAML usa `schema_version: 1` (const del `building.schema.json`), cumple todos los
campos `required`, copia el patrón de `data/units/rome_legionary.yaml` (estructura
`provenance`, claves de localización por namespace), y declara `trains/researches/
required_capabilities/grants_capabilities/recipes` como listas vacías.

## 2. Fichas de civilización actualizadas

- `data/civilizations/egipto_dynastic_nile.yaml`: `building_ids` ahora
  `["egipto:settlement_center", "egipto:shena_granary"]`.
- `data/civilizations/rome_republic_imperial.yaml`: `building_ids` ahora
  `["rome:forum_center", "rome:horreum"]`.

`epoch_window` y `playable_period_ids` de cada edificio copian los de las unidades
existentes de su civ:
- Egipto: epoch `[3,4]`, periods `egipto:old_kingdom` + `egipto:new_kingdom`
- Roma:   epoch `[5,5]`, periods `rome:republican_expansion` + `rome:high_empire`

El manifest no enumera archivos/kinds (solo `owned_namespaces`, `declared_capabilities`
y `declared_behaviors`), por lo que no se ha tocado.

## 3. Gate `data_compile` — SALIDA

Comando (con `nice -n 19`):
```
nice -n 19 ctest --test-dir build-gcc -R data_compile --output-on-failure
```

Resumen: 30 tests corridos, 1 FAILED.
- `test_repository_release_fixture_matches_versioned_golden` — FAIL.
- El resto (incluidos `test_building_constructibility`, `test_minimal_fixtures_validate`,
  `test_extra_property_rejected`, `test_provenance_requirements`,
  `test_dropoff_resources_only_required_for_dropoff`, y demás contract tests del schema)
  pasan en verde.

El test que falla emite dos errores `E_SCHEMA`:

```
ERROR E_SCHEMA building egipto:settlement_center /: {...} is not valid under any of the given schemas
ERROR E_SCHEMA building rome:forum_center     /: {...} is not valid under any of the given schemas
```

(Los dos dropoff — `egipto:shena_granary` y `rome:horreum` — sí validan: su
`resource_costs.B = 60` cumple `positive_resource_costs`. Solo fallan los dos civic
centers.)

### 3.1 Diagnóstico: es del schema, no de los YAML

`data/schemas/building.schema.json` declara (en `allOf`):
```jsonc
{"if": {"properties": {"constructible": {"const": true}}},
 "then": {"properties": {"build_time_ticks": {"minimum": 1}},
          "anyOf": [
            {"properties": {"resource_costs": {"$ref": "common.schema.json#/$defs/positive_resource_costs"}}},
            {"required": ["material_costs"], "properties": {"material_costs": {"minItems": 1}}}
          ]}}
```

`positive_resource_costs` exige al menos una propiedad con valor ≥ 1. Mi brief pide
explícitamente para los centros:
> `(ninguno: omite el campo o todo 0)`

O sea, los centros son `constructible: true` con coste cero. Esa combinación está
**explícitamente prohibida por el schema actual**. No es algo que yo pueda arreglar
sin tocar el schema o el compilador, cosa que el brief me prohíbe.

Aparte, ese mismo test `test_repository_release_fixture_matches_versioned_golden`
afirma en su stdout esperado:
```
records unit=5 building=0 tech=0 civ=2 map=1 ai-profile=1
```
Es decir, asume `building=0` y compara contra `data/compiled/chunsa_base.chdb`. Aunque
mis YAML compilaran, ese assertion (y el `out.read_bytes() == golden.read_bytes()`)
necesitaría regenerar el golden — operación de schema/compiler/tooling, fuera de
mi scope.

### 3.2 Acción tomada

**PÁRATE.** No he modificado `data/schemas/*`, ni `tools/data_compile/*`, ni
`data/compiled/*.chdb*`. La rama `mm/datos-edificios-1.1` queda con los YAMLs y las
civs listas, pero el gate en rojo a la espera de que el Arquitecto decida:

  (a) relajar el `anyOf` del schema para permitir `constructible: true` con coste
      cero (e.g., permitir `resource_costs` vacío cuando el edificio es `civic`
      y/o satisface alguna otra condición), o
  (b) cambiar el brief para que los centros tengan un coste mínimo (lo que
      contradice la tabla dada), o
  (c) marcar `constructible: false` en los centros y servirlos como edificios
      iniciales pre-colocados (también contradice "constructible: true" del brief).

## 4. Procedencia — fuentes usadas (todas abiertas en esta sesión)

URL real + accessed_on `2026-07-23`. Citas textuales recogidas con WebFetch y leídas
íntegras antes de incluirlas:

- **https://en.wikipedia.org/wiki/Horreum** — base de la identidad histórica del
  horreum romano (definición, primer horreum público por C. Graco en 123 a.C.,
  almacenamiento estatal de grano y aceite). Cita exacta leída:
  > "A horreum (plural: horrea) was a type of public warehouse used during the
  > ancient Roman period. Although the Latin term is often used to refer to granaries,
  > by the end of the imperial period, the city of Rome had nearly 300 horrea to
  > supply its demands."

- **https://en.wikipedia.org/wiki/Roman_Forum** — base de la identidad del foro
  romano (centro cívico-comercial-judicial). Cita exacta leída:
  > "a rectangular forum (plaza) surrounded by the ruins of several important
  > ancient government buildings at the centre of the city of Rome. Citizens of the
  > ancient city referred to this space, originally a marketplace, as the Forum
  > Magnum, or simply the Forum."

- **https://en.wikipedia.org/wiki/Economy_of_ancient_Egypt** — base de la identidad
  del granero/šnwt egipcio y de la función del centro cívico (templo y complejo
  estatal como nodo de acopio). Cita exacta leída:
  > "The temples formed the backbone of the economy. Not only were they places of
  > worship, but were also responsible for collecting and storing the kingdom's
  > wealth in a system of granaries and treasuries administered by overseers, who
  > redistributed grain and goods."

Intento fallido: Met Museum `model granary from tomb of Meketre` (HTTP 429 por
rate-limit); UCL Digital Egypt `/administration/` (la página existe pero el contenido
recogido por WebFetch no menciona explícitamente "granary" o "šnwt", por lo que no la
he citado). Britannica (HTTP 403). Todos los intentos están documentados; nada
citado ha sido fabricado.

`verification_reports` añadidos (paths relativos a `data/`, ambos existen):
- Egipto → `game_data/research/verificacion/REVISION_ARQUITECTO_SPRINT_0.4.md`
           + `game_data/research/verificacion/VERIF_36a.md`
- Roma   → `game_data/research/verificacion/REVISION_ARQUITECTO_SPRINT_0.4.md`
           + `game_data/research/verificacion/VERIF_36b.md`

`evidence: H` en los 4: el **tipo de edificio** existió en esa civilización y época
(šnwt, horreum, Foro Romano, nodo cívico templo/Estado). Todos los números (HP,
costes, build_time, footprint, dropoff) van como `balance_design` con rationale que
lo declara explícitamente.

## 5. Desviaciones respecto al brief

1. **Gate `data_compile` en rojo** por inconsistencia brief↔schema (ver §3).
   No se ha podido resolver sin tocar schema/compiler, cosa prohibida.
2. `provenance.status` queda en `draft_unverified` (no `promoted`), porque el
   Arquitecto aún no ha revisado este lote — consistente con el resto de la
   metodología ADR-014 del repo.

No hay otras desviaciones. No se ha tocado `main`, no se ha mergeado nada, el
`manifest.yaml` no necesitaba cambios y no se han modificado.

## 6. Estado de la rama

Rama: `mm/datos-edificios-1.1` (creada desde `main` en esta sesión).
`main` intacto. Commit en la rama: pendiente (ver nota abajo).

> **Nota sobre el commit:** El brief pide commit en `mm/datos-edificios-1.1` con
> "Sprint 1.1: datos de edificios del slice (4 YAML + civs)". Hago el commit de
> todos modos para que el Arquitecto pueda revisar los YAML y las fuentes; pero
> el gate queda rojo, lo cual queda explícito en este RESULT y en el mensaje del
> commit. Si el Arquitecto prefiere revertir y reabrir Sprint 1.1 como tarea de
> schema/compiler, puede hacerlo sin pérdida de trabajo.

## 7. Próximos pasos (para el Arquitecto)

1. Decidir entre las opciones (a)/(b)/(c) de §3.2.
2. Si se modifica el schema: regenerar golden y re-correr `data_compile`.
3. Cambiar `provenance.status` a `promoted` y `task_id` definitivo tras revisar.
4. Merge a `main` cuando el gate esté verde.
