# BRIEF — Datos de la apertura económica (MiniMax M3 · Sprint 1.6B)

Tarea ACOTADA de datos: poblar los **depósitos de recursos del mapa** y hacer que los **centros
entrenen aldeanos**. NO toques código C++, schemas ni el compilador. Si algo no valida y parece
culpa del compilador o los schemas, PÁRATE y repórtalo en el RESULT.

## Rama
`mm/datos-apertura-1.6b` desde `main`. ⚠️ Térmica: `nice -n 19`, uno a la vez.

## Contexto (una frase)
Hoy la economía arranca con 6 depósitos hardcodeados en el kernel y los centros no producen
aldeanos, así que no existe "apertura económica"; estos datos la habilitan (el kernel los
consumirá en paralelo — no dependes de él).

## TAREA 1 — `resource_spawns` del mapa (hoy `resource_spawns: []`)
Archivo: `data/maps/base_demo_desert_basin.yaml`. El schema (`data/schemas/map.schema.json`)
ya exige el campo con esta forma EXACTA por entrada:
```yaml
resource_spawns:
  - kind: resource        # enum: resource | material  → usa SIEMPRE "resource"
    id: A                 # A = alimento, B = madera/construcción, Me = metal
    x_millitiles: 24500   # posición en MILI-TILES (1 tile = 1000)
    y_millitiles: 120500
    amount: 500           # cantidad recolectable
```
**Diseño requerido (la simetría es requisito de balance, no estético)**: el mapa tiene dos
`starting_positions` ya definidas — slot 0 en `x=20500, y=128500`; slot 1 en `x=235500,
y=128500` (mili-tiles). Coloca **12 depósitos**:
- **4 por jugador, cerca de su base** (radio ~8–20 tiles de su starting position), espejados
  exactamente entre los dos lados: 2 de `A`, 1 de `B`, 1 de `Me` por jugador. La coordenada X de
  los del slot 1 debe ser el espejo de los del slot 0 respecto al centro del mapa (x=128000):
  `x_slot1 = 256000 - x_slot0`. Misma Y. Mismo `amount`.
- **4 neutrales en la franja central** (x entre 110000 y 146000), también simétricos por parejas
  respecto a x=128000: 2 de `A`, 1 de `B`, 1 de `Me`.
- `amount`: 500 para los de base, 800 para los neutrales (premio por disputarlos).
- **Evita el muro**: el escenario tiene un muro vertical en el tile x=128 (y entre 32 y 224, con
  hueco en y∈[124,132)). NO pongas ningún depósito en x_millitiles entre 127500 y 128500 salvo
  que la Y caiga en el hueco. Verifica cada coordenada contra esa regla.

## TAREA 2 — Los centros entrenan aldeanos
`data/buildings/egipto_settlement_center.yaml` y `data/buildings/rome_forum_center.yaml` tienen
`trains: []`. Pon en cada uno el aldeano de SU civilización:
- `egipto:settlement_center` → `trains: [egipto:work_crew]`
- `rome:forum_center` → `trains: [rome:camp_work_crew]`
(Verifica que esos record_id existen en `data/units/` antes de escribirlos; si el nombre no
coincide, usa el que exista y dilo en el RESULT.) Es exactamente el mismo tipo de edición que
hiciste con `researches` en el Sprint 1.2 y salió bien.

## Procedencia (ADR-014)
Los depósitos y el `trains` son **diseño de escenario/balance**, no afirmaciones históricas: NO
necesitas fuentes nuevas ni tocar `historical_claims`. Si el schema te obliga a tocar
`provenance` del mapa, actualiza solo `generated_on` y añade una línea a
`balance_design.rationale` explicando la simetría. NO pongas `status: promoted` en nada que
cambies — déjalo como esté o en `draft_unverified`; la promoción es veredicto del Arquitecto.

## Verificación OBLIGATORIA
```bash
nice -n 19 ctest --test-dir build-gcc -R data_compile --output-on-failure
```
Fallará como MÍNIMO en el golden del blob versionado (`test_repository_release_fixture_matches_
versioned_golden`) porque cambian los datos: **eso NO es culpa tuya**, lo regenera el Arquitecto
en integración. Cualquier OTRO error (E_SCHEMA / E_REF / E_PROVENANCE de TUS ediciones) sí es
tuyo: corrígelo. Comprueba además, con python o a ojo, que cada par de depósitos cumple la
simetría `x_slot1 = 256000 - x_slot0` y que ninguno cae en el muro.

## Entrega
Commit en `mm/datos-apertura-1.6b` + `docs/briefs/MINIMAX_DATOS_APERTURA_SPRINT_1.6B_RESULT.md`:
la tabla exacta de los 12 depósitos que colocaste (id, x, y, amount, a qué jugador sirven), la
verificación de simetría y de muro, la salida del gate, y cualquier desviación. NO merges.
