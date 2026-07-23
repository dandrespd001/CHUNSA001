# BRIEF — Datos de producción y tecnología del slice (MiniMax M3 · Sprint 1.2)

Tarea ACOTADA de datos: 2 edificios militares (con `trains`), 4 tecnologías (2 por civ)
y las actualizaciones de civs/manifest. NO toques código C++, schemas ni el compilador.
Si algo no valida y parece culpa del compilador/schema, PÁRATE y repórtalo en el RESULT.

## Rama
`mm/datos-tech-1.2` desde `main`. ⚠️ Térmica: `nice -n 19`, uno a la vez.

## Método (idéntico al Sprint 1.1, que hiciste bien)
Plantillas de estilo: `data/buildings/egipto_shena_granary.yaml` (edificio con coste) y
`data/units/rome_legionary.yaml` (provenance). Lee los schemas `building`/`tech` y usa
sus campos required. Los números de la tabla son balance de diseño: van tal cual.

## Edificios (data/buildings/)
| record_id | civ | kind | trains | resource_costs | build_time_ticks | footprint | hp | dropoff |
|---|---|---|---|---|---|---|---|---|
| `egipto:chariotry_stable` | egipto:dynastic_nile | military | `[egipto:chariot_warrior]` | B: 80 | 600 | 3×2 | 800 | (sin campo) |
| `rome:castra_barracks` | rome:republic_imperial | military | `[rome:legionary, rome:ballista_crew]` | B: 80 | 600 | 3×2 | 900 | (sin campo) |
Ambos `constructible: true`, `blocks_movement: true`, `availability_mode: historical`,
`researches` = las techs de su civ de abajo, resto de listas vacías, epoch_window y
playable_period_ids como los de las unidades existentes de su civ.
Identidad histórica REAL a citar (fuente web abierta por ti HOY, mismo estándar de julio):
los establos/cuerpo de carros del Reino Nuevo egipcio; los castra/barracks romanos.

## Tecnologías (data/tech/ — crea el directorio si no existe)
| record_id | civ (available_to) | branch | grants (capacidad) | resource_costs | research_time_ticks | epoch |
|---|---|---|---|---|---|---|
| `egipto:composite_bow_program` | egipto:dynastic_nile | military | `egipto:composite_bow_craft` | A:60, Me:40 | 800 | (la de la civ) |
| `egipto:corvee_logistics` | egipto:dynastic_nile | economy | `egipto:corvee_levy` | A:80 | 700 | (la de la civ) |
| `rome:marching_drill` | rome:republic_imperial | military | `rome:legion_drill` | A:60, Me:40 | 800 | (la de la civ) |
| `rome:road_engineering` | rome:republic_imperial | economy | `rome:maintain_public_infrastructure` (YA declarada en el manifest) | A:80 | 700 | (la de la civ) |
- `required_buildings`: el edificio militar de su civ (los 2 primeros) o vacío (los 2 de
  economía). `prerequisites`/`mutually_exclusive_with`: vacíos. `evidence`: el nivel que
  la fuente soporte de verdad.
- Las capacidades NUEVAS (`egipto:composite_bow_craft`, `egipto:corvee_levy`,
  `rome:legion_drill`) DEBEN añadirse a `declared_capabilities` del `data/manifest.yaml`
  (la 4ª ya está declarada). Si el schema del tech exige campos que esta tabla no fija,
  usa el valor mínimo/neutro del schema y anótalo en el RESULT.
- Identidad histórica real: arco compuesto del Reino Nuevo · corvea egipcia (ya
  documentada en VERIF_36a) · instrucción/marcha legionaria · ingeniería de calzadas.
  Cita solo lo que abras y leas HOY (una cita fabricada invalida el lote, ADR-014).

## Actualizaciones
- `building_ids` y `tech_ids` de las 2 fichas de civilización (cada civ los suyos).
- `verification_reports`: mismos del 1.1 (VERIF_36a/b + REVISION_ARQUITECTO_SPRINT_0.4,
  rutas relativas a `data/`).
- `provenance.status: promoted` NO — usa `draft_unverified`; la promoción es veredicto
  del Arquitecto (como en 1.1, donde lo hiciste exactamente bien).

## Verificación OBLIGATORIA
`nice -n 19 ctest --test-dir build-gcc -R data_compile --output-on-failure` — el gate
fallará como MÍNIMO en el golden del blob versionado y en el conteo de records
(`building=4`/`tech=0` esperados): eso NO es culpa tuya, es el cambio intencional de
datos; NO toques el blob versionado ni los tests — repórtalo y el Arquitecto regenera en
integración (procedimiento establecido). Cualquier OTRO error (E_SCHEMA/E_PROVENANCE/
E_REF de TUS yaml) sí es tuyo: corrígelo.

## Entrega
Commit en `mm/datos-tech-1.2` + `docs/briefs/MINIMAX_DATOS_TECH_SPRINT_1.2_RESULT.md`
(archivos, fuentes con URL, salida del gate, desviaciones). NO merges.
