# RESULT — K1: Identidad de civilización + depósitos desde el mapa (Sprint 1.6B)

Autor: sonnet-5. Brief: `docs/briefs/SONNET_K1_CIV_DEPOSITOS_SPRINT_1.6B.md`.
Spec: `docs/specs/SPEC-004_SISTEMAS_PARTIDA.md` Parte III §15.3/§16/§17/§20.

## Rama y commits

Rama `sonnet/k1-civ-depositos`, creada desde `main` en el commit
`18f8ab260dd393cdb4c91b4bb652ac48a9670963` (el mismo que HEAD al recibir el
encargo). **`main` no se tocó** en ningún momento.

4 commits atómicos:

1. `4e7b111` — `feat(catalog): tipar civ_id (unit/building/tech) y
   resource_spawns del mapa` (§15.3/§16 del loader, + ajuste del fixture de
   `test_data_blob.cpp`).
2. `e4286d1` — `feat(kernel): player_civ por jugador + gates de
   civilización` (§16/§17 en `game_state.hpp`/`step.hpp`).
3. `520f3f9` — `feat(persist): save v12 + checksum v7 (player_civ)` (§20).
4. `855ea01` — `test(civ-deposits): cobertura K1 — civ_id, resource_spawns,
   gates, save v12` (test nuevo + registro en CMake).

Nota informativa (no accionable por esta pieza): durante la sesión, `main`
avanzó a `3867f6...` con el merge de los datos de MiniMax
(`mm/datos-apertura-1.6b`, 12 `resource_spawns` + `trains` de aldeano). Por
instrucción explícita del brief ("NO dependas de esa rama... el Arquitecto
integra ambos") esta pieza se desarrolló y verificó enteramente sobre la
base `18f8ab2`, con fixtures propios — el Arquitecto reconcilia ambas ramas
al integrar.

## Los 4 bloques — qué se implementó

### 1. `civ_id` tipado (§15.3) — `data_catalog.hpp`

Patrón endurecido **espejo exacto** de tech/ai-profile: `CveValue` acotado
(caps ya existentes, sin cambios), `unique_ptr` de `Impl` durante toda la
construcción, `reserve` exacto de `civ_ids` antes de llenar, resolución
DIFERIDA de `civ_id`/`available_to` (la sección `civ`, kind=5, va después de
unit/building/tech en el blob — mismo motivo que `building.researches`
espera a la sección tech), rechazo del catálogo entero si no resuelve, y
`catalog_find_civ` por búsqueda binaria bytewise (idéntico a
`catalog_find_unit`/`catalog_find_building`/`catalog_find_tech`/
`catalog_find_ai_profile`).

`CivNameIndexV1` es una tabla **mínima** (solo record_id + índice, sin
reconstrucción de ningún otro campo del record civ) — mismo criterio que
`CapabilityNameIndexV1`: el kernel v1 no consume historical_window/
epoch_window/institutions/etc. de civ, esa validación semántica sigue
siendo responsabilidad de `chunsa_data_compiler.py`.

`UnitDefinitionV1`/`BuildingDefinitionV1` ganan `CivId civ_id` resuelto
contra esa tabla. `TechDefinitionV1` también, con una desviación documentada
(**D1**, ver abajo).

### 2. Depósitos desde el mapa (§16) — `data_catalog.hpp` + `game_state.hpp`

`ResourceSpawnV1{resource_idx, x_raw, y_raw, amount}` tipado del **primer**
record `map` del catálogo (desviación **D2**, ver abajo), con:
- conversión exacta en enteros `raw = mt * FX_ONE_RAW / 1000`;
- `id` textual A/B/Me → `resource_idx` 0/1/2; **cualquier otro valor** (id
  fuera de A/B/Me, o `kind != "resource"`) rechaza el catálogo entero
  (`CatalogLoadCode::InvalidMap`, código nuevo, append-only);
- si el mapa trae más de `ECO_MAX_DEPOSITS` (32) spawns, rechazo del
  catálogo entero (no se truncan datos en silencio) — verificado con control
  positivo en el límite exacto (32 sí carga, 33 no).

`gs_init_economy_from_catalog(GameState&)` (función NUEVA, no invocada desde
`gs_init`) puebla `deposits[]`/`n_deposits` desde `cat.map_resource_spawns`
cuando hay catálogo enlazado con spawns; si no hay catálogo o
`map_resource_spawn_count==0`, es **NO-OP** — el patrón fijo de 6 depósitos
que `gs_init_economy` (sin tocar) ya dejó en `gs_init` queda intacto. Los
slots `[n, ECO_MAX_DEPOSITS)` se limpian a 0 explícitamente para que
`checksum.hpp` (que recorre los 32 slots fijos, no solo `n_deposits`) sea
determinista incluso cuando el mapa trae menos de 6 depósitos.

### 3. Civ y época por jugador (§17) — `game_state.hpp` + `step.hpp`

- `GameState::player_civ[MAX_EMITTERS]` (ESTADO: serializado + checksummeado,
  escalar del partido como `game_over`/`winner`/`pop_used`); `gs_init` lo
  fija a `INVALID_CIV_ID` explícitamente (mismo motivo que `unit_id`/
  `building_id`: el memset a 0 significaría "civ 0 asignada").
- **Asignación**: `gs_set_player_civ(GameState&, uint32_t player, CivId civ)`
  — init explícito del host, análogo a `gs_bind_catalog`/
  `gs_init_epoch_from_catalog`. Ver "cómo se asigna player_civ" más abajo.
- **Época por jugador**: `gs_init_epoch_from_catalog_per_player(GameState&)`,
  función NUEVA que coexiste con `gs_init_epoch_from_catalog` (sin tocar,
  sin fusionar). Para cada jugador: si tiene civ asignada, mínimo de
  `epoch_min`/`epoch` de SOLO los datos de esa civ; si `INVALID_CIV_ID`, cae
  a la misma fórmula catálogo-ancha que la función original.
- **Gates de civilización**: `PLACE_BUILDING`/`TRAIN_UNIT`/`RESEARCH_TECH`
  rechazan con `ILLEGAL_STATE` si `def.civ_id != player_civ[emitter]`, salvo
  (a) `player_civ[emitter] == INVALID_CIV_ID` (gate no aplica) y (b), solo en
  `PLACE_BUILDING`, la ventana de setup del tick 0 (`scenario_exempt`, ya
  existente — el gate de civ vive DENTRO de ese mismo bloque `if
  (!scenario_exempt)`).

### 4. Persistencia (§20) — `checksum.hpp` / `serialize.hpp` / `save_io.hpp`

- `SAVE_FORMAT_VERSION` 11 → 12: `player_civ[MAX_EMITTERS]` (u32 cada uno)
  al final del stream, append-only, sin migración (precedente D7 — un save
  v11 real falla el check de versión del envelope, no hay ruta v11→v12).
- `CHECKSUM_ALGO_VERSION` 6 → 7, dominio `"CHUNSA_STATE_V7"`: `player_civ`
  al final del dominio hasheado, mismo orden que el save.

## Cómo se asigna `player_civ` en el setup

Se eligió **init explícito del host** (`gs_set_player_civ`), no un comando
nuevo. Motivo: `gs_bind_catalog` y `gs_init_epoch_from_catalog` ya son el
precedente establecido de "setters de configuración de partida, llamados por
el host FUERA de `Step()`, nunca comandos que viajen por el replay/agenda".
La identidad de civilización de un jugador es fija para TODA la partida (no
una acción repetible como `MOVE_TO`/`GATHER`) — encajaba con ese patrón, no
con el sistema de comandos. Secuencia de setup recomendada (documentada en
los comentarios de cada función y ejercitada en los tests):

```cpp
gs_init(g, cfg);
gs_bind_catalog(g, catalog);
gs_set_player_civ(g, 0, civ_a);
gs_set_player_civ(g, 1, civ_b);
gs_init_epoch_from_catalog_per_player(g);   // LEE player_civ: después de fijarlo
gs_init_economy_from_catalog(g);            // opcional: solo si el mapa trae spawns
```

## Confirmación de bit-identidad del fallback legacy

`gs_init_economy` (Sprint 0.3) **no se tocó**: sigue siendo la única función
que corre dentro de `gs_init`, y en ese punto `g.catalog` es SIEMPRE
`nullptr` (se enlaza después, aparte). `gs_init_economy_from_catalog` es una
función nueva y separada, nunca invocada automáticamente — cualquier
escenario/test existente que no la llame explícitamente obtiene el patrón
fijo de 6 depósitos exacto de siempre. Verificado en
`test_civ_deposits.cpp::test_legacy_fallback_bit_identical` byte a byte
(posiciones/resource_idx/remaining/dropoff de los 6 depósitos y 16
dropoffs), más control positivo de que `gs_init_economy_from_catalog` SÍ
sobreescribe cuando se invoca con un catálogo con spawns.

## Gates completos

- **Golden**: `1074/1074`, `0 fallos` (vectores Fixed64 puros, sin tocar).
- **G1** (`run --selftest-g1`): `alloc_delta=0 OK`.
- **G3** (`savetest`, sin IA): `OK`.
- **G4** (`savetest --ai`): `OK`.
- **G5** (`record` + `verify --replay`): `OK ai_executions=0
  schedule_mismatches=0 replay_v=3`.
- **ctest**: `22/22` (los 21 targets previos + `civ_deposits` nuevo),
  `-Werror` limpio en los 3 sanity-compiles aislados de cada header tocado
  y en el build completo (`-Wall -Wextra -Wshadow -Werror`).
- Build con `nice -n 19`, `cmake --build build -j2` (un build a la vez).

## Skirmish militar y económico — regresión, con checksums

Se comparó `main` (`18f8ab2`, worktree detached temporal, eliminado tras la
comparación) contra esta rama, ejecutando **el mismo binario de test** en
ambos:

| Escenario | end_tick | winner | ai_executions | checksum `main` (v6) | checksum K1 (v7) |
|---|---|---|---|---|---|
| `ai_skirmish` | 1226 | 1 | 61 | `ef844270bd4d5aca` | `3f64d3223b74d477` |
| `ai_skirmish_eco` | 1824 | 1 | 91 | `5c8be20083cf490e` | `d610feef89ed9c65` |

`end_tick`/`winner`/`ai_executions` (y, en `ai_skirmish_eco`, el stock final
del defensor `A=0 B=300 Me=0`) son **idénticos** — evidencia fuerte de
trayectoria bit-idéntica (cualquier divergencia de estado en un solo tick,
en una simulación determinista tan sensible como esta, cambiaría casi con
certeza el tick/ganador de fin). El **valor del checksum SÍ difiere**, y es
el comportamiento **esperado e intencional**: el dominio del checksum
cambió (v6→v7, `player_civ` añadido al final) exactamente con el mismo
precedente que los bumps v1→v6 documentados en `checksum.hpp` — no existe
en este repo un "checksum golden" persistido contra el que comparar; los
tests comparan corridas en vivo entre sí (y así lo hacen `ai_skirmish`/
`ai_skirmish_eco`/`production_tech`, que pasan con sus propias aserciones de
igualdad interna, sin cambios). `player_civ` permanece en `INVALID_CIV_ID`
durante toda la corrida de ambos escenarios (ninguno llama a
`gs_set_player_civ`), así que el único campo nuevo del dominio aporta un
valor constante — el cambio de checksum es puramente el bump de dominio, no
un cambio de trayectoria.

Adicionalmente: `test_buildings.cpp`, `test_production_tech.cpp`,
`test_economy.cpp`, `test_ai_layers.cpp`, `test_replay_v3.cpp`,
`test_victory_ai_profile.cpp` y `test_data_blob.cpp` (con el fixture
ajustado) pasan **sin ningún cambio en sus aserciones de valor exacto**
(posiciones/hp/stock/tick/checksums-entre-dos-corridas-vivas) — refuerza que
ninguna trayectoria existente cambió.

## Checksums nuevos de referencia (esta rama, dominio v7)

- `G1 selftest`: `fefa48125dd35736`.
- `skirmish` (CLI): `3f64d3223b74d477` / continuación `92ec9aa95374a429`.
- `savetest` sin IA (G3): `969199722657b853` / continuación
  `6145075498b2fb7d`.
- `savetest --ai` (G4): `774316057e5667fb` / continuación
  `d52ac0019700684f`.

## Desviaciones numeradas

**D1 — `TechDefinitionV1.civ_id` derivado de `available_to`, no un campo
escalar.** El literal de SPEC-004 §15.3 contrata `CivId civ_id` también para
tech, pero `tech.schema.json` **no declara** `civ_id` — declara
`available_to` (`record_id_set` de civs, potencialmente múltiples; la civ
misma valida en `chunsa_data_compiler.py` que la referencia inversa
`civ.tech_ids` sea consistente). Para conciliar el contrato con el schema
real sin inventar semántica no especificada: el kernel v1 **solo soporta
techs de una civ** — `civ_id` es el único elemento de `available_to` si
`available_to.size()==1`; 0 o >1 elementos rechaza el catálogo entero
(`InvalidTech`). Las 4 techs reales del repo ya tienen exactamente 1
elemento, así que el golden actual no se ve afectado (verificado en
`test_civ_id_real_golden`). Una tech compartida entre 2+ civs necesitará
revisar este contrato en un sprint futuro (fuera de alcance de K1).

**D2 — "mapa activo" = el primer record `map` del catálogo, sin mecanismo de
selección multi-mapa.** El schema/blob permiten hasta 1024 records `map` por
catálogo, pero el slice real trae exactamente 1
(`data/maps/base_demo_desert_basin.yaml`) y el brief no especifica ningún
criterio de selección para cuando hubiera más de uno. El loader tipa
`resource_spawns` únicamente del primer record `map` (record_id ascendente,
ya garantizado); registros `map` adicionales se ignoran a efectos de
economía. Si un sprint futuro necesita múltiples mapas seleccionables,
hará falta un mecanismo explícito (p.ej. nombre de mapa en `MatchConfig01A`)
— no inventado aquí para no comprometerse con una API no contratada.

**D3 — asignación de `player_civ` vía init explícito del host, no un
comando.** Ver la sección dedicada arriba; el brief ofrecía ambas opciones
y pedía elegir y documentar.

**D4 — `gs_init_economy_from_catalog`/`gs_init_epoch_from_catalog_per_player`
son funciones NUEVAS, no fusionadas con `gs_init_economy`/
`gs_init_epoch_from_catalog`.** Mismo razonamiento que ya documenta
`gs_init_epoch_from_catalog` sobre `gs_bind_catalog`: fusionar habría
cambiado en silencio el comportamiento de todos los call sites existentes
que llaman a las funciones originales sin esperar el efecto nuevo. Ambos
pares de funciones coexisten; el host explícitamente opta por la variante
consciente de civ/mapa cuando la necesita.

**D5 — checksum domain bump (v6→v7) cambia el VALOR del checksum de
escenarios sin civ, aunque la trayectoria es idéntica.** Documentado en
detalle en la sección "Skirmish militar y económico" arriba; es el mismo
patrón que todos los bumps anteriores (v1→v6) de este archivo, no una
desviación nueva de comportamiento sino una consecuencia esperada de añadir
un campo nuevo al dominio hasheado.

## Archivos tocados

- `addons/chunsa_sim/core/include/chunsa/data_catalog.hpp`
- `addons/chunsa_sim/core/include/chunsa/game_state.hpp`
- `addons/chunsa_sim/core/include/chunsa/step.hpp`
- `addons/chunsa_sim/core/include/chunsa/checksum.hpp`
- `addons/chunsa_sim/core/include/chunsa/serialize.hpp`
- `addons/chunsa_sim/core/include/chunsa/save_io.hpp`
- `tests/unit/test_data_blob.cpp` (fixture ajustado, sin cambiar aserciones)
- `tests/unit/test_civ_deposits.cpp` (nuevo)
- `CMakeLists.txt` (registro del target nuevo)

No se tocó el adaptador Godot. No hay merge a `main`.
