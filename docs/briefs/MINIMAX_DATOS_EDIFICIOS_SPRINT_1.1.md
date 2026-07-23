# BRIEF — Datos de edificios del slice (MiniMax M3 · Sprint 1.1)

Tarea ACOTADA de datos: escribir **4 archivos YAML de edificios** (2 por civilización),
actualizar las 2 fichas de civilización y el manifest, y verificar que el compilador pasa.
NO toques código C++ ni schemas ni el compilador. Si algo no valida y no sabes por qué,
PÁRATE y repórtalo — no "arregles" schemas ni el compilador.

## Rama
`mm/datos-edificios-1.1` desde `main`. Jamás toques `main` directamente.
⚠️ Térmica: cualquier comando pesado con `nice -n 19`, uno a la vez.

## Los 4 edificios (record_id EXACTOS — el kernel y el adaptador los buscarán por estos nombres)
| record_id | civ | kind | rol jugable | dropoff_resources | resource_costs | build_time_ticks | footprint | hp |
|---|---|---|---|---|---|---|---|---|
| `egipto:settlement_center` | egipto:dynastic_nile | civic | centro inicial (dropoff universal) | `[A, B, Me]` | (ninguno: omite el campo o todo 0) | 1 | 3×3 | 1500 |
| `egipto:shena_granary` | egipto:dynastic_nile | dropoff | granero (dropoff de alimento) | `[A]` | B: 60 | 500 | 2×2 | 600 |
| `rome:forum_center` | rome:republic_imperial | civic | centro inicial (dropoff universal) | `[A, B, Me]` | (ninguno) | 1 | 3×3 | 1500 |
| `rome:horreum` | rome:republic_imperial | dropoff | horreum (dropoff de alimento) | `[A]` | B: 60 | 500 | 2×2 | 600 |

Notas fijas para TODOS: `constructible: true`, `footprint.blocks_movement: true`,
`availability_mode: historical`, `epoch_window` igual al de las unidades existentes de su
civ, `trains: []`, `researches: []`, `recipes: []`, `required_capabilities: []`,
`grants_capabilities: []`, `playable_period_ids` = los mismos que usan las unidades
existentes de esa civ. Los números de hp/costes/tiempos de la tabla son **balance de
diseño** (van tal cual; no los cambies).

## Método (copia el patrón existente, no inventes estructura)
1. Lee `data/schemas/building.schema.json` y `data/schemas/common.schema.json` (los campos
   required son obligatorios; usa el `schema_version` const que declare el schema).
2. Usa `data/units/rome_legionary.yaml` como plantilla de estilo para `provenance`,
   claves de localización (`egipto:building.settlement_center`, etc.) y formato general.
3. Crea `data/buildings/egipto_settlement_center.yaml`, `data/buildings/egipto_shena_granary.yaml`,
   `data/buildings/rome_forum_center.yaml`, `data/buildings/rome_horreum.yaml`.
4. Añade los 4 record_id a `building_ids` de `data/civilizations/egipto_dynastic_nile.yaml`
   y `rome_republic_imperial.yaml` (cada civ los suyos).
5. Si el manifest (`data/manifest.yaml`) declara listas de archivos/kinds, actualízalo
   igual que están declarados los units; si el compilador descubre por glob, no hace falta.

## Procedencia (ADR-014 — la parte con juicio)
- `verification_reports` (rutas relativas al dir `data/`, DEBEN existir):
  Egipto → `game_data/research/verificacion/VERIF_36a.md`; Roma → `VERIF_36b.md`;
  ambos + `game_data/research/verificacion/REVISION_ARQUITECTO_SPRINT_0.4.md`.
- La identidad histórica es real y verificable: el **šnwt** (granero estatal egipcio) y el
  **horreum** romano existieron; los "centros" son abstracción DISEÑO de asentamiento.
  En `sources` pon 1–2 fuentes REALES que puedas abrir por web AHORA (museo, enciclopedia
  académica, JSTOR/UCL/Oxford…), con `url` y `accessed_on` de hoy. **PROHIBIDO citar nada
  que no hayas abierto y leído en esta sesión** — una cita fabricada invalida todo el lote.
  Si no encuentras fuente sólida para un claim: usa evidencia más débil y dilo en
  `balance_design.rationale`, no inventes.
- `evidence: H` SOLO para la identidad (que el tipo de edificio existió en esa civilización
  y época); todo número de la tabla es `balance_design` (rationale: "identidad histórica;
  hp/costes/tiempos/footprint son balance").

## Verificación OBLIGATORIA antes de commitear
```bash
nice -n 19 ctest --test-dir build-gcc -R data_compile --output-on-failure
```
Debe pasar (el gate recompila el blob y valida schemas + procedencia E_PROVENANCE).
Si falla, lee el error: si es de TUS yaml, corrígelo; si parece del compilador/schema,
PÁRATE y repórtalo.

## Entrega
Commit en `mm/datos-edificios-1.1`: "Sprint 1.1: datos de edificios del slice (4 YAML + civs)".
Escribe `docs/briefs/MINIMAX_DATOS_EDIFICIOS_SPRINT_1.1_RESULT.md`: archivos creados,
salida del gate, fuentes usadas (con URL) y cualquier desviación. Inclúyelo en el commit.
NO merges tú; el Arquitecto revisa e integra.
