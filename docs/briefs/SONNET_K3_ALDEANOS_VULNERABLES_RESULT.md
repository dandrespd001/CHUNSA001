# RESULT K3 — Aldeanos vulnerables + partida con economía (Sprint 1.4, cierre de kernel)

Rama: `sonnet/k3-aldeanos-vulnerables`, creada desde `main` @ `4e12b2c` (K1+K2 de
la IA de 3 capas ya integradas). Sin merges a `main`.

## Commits

Atómicos, en orden:
1. `step.hpp` — los 2 guards de targeting (`combat_system` ~L857,
   `aggro_system` ~L945) + la rama RPS `rps_mult_vs_citizen_bp` + comentarios
   actualizados donde documentaban la invariante antigua.
2. `skirmish_eco.hpp` (nuevo) + `test_ai_skirmish_eco.cpp` (nuevo) +
   `CMakeLists.txt` (registro del test) — el escenario de partida con
   economía real que termina por conquista.
3. `test_combat.cpp` — 3 tests nuevos de comportamiento (objetivo válido,
   guard de atacante intacto, aggro re-adquiere aldeano).
4. `test_economy.cpp` — comentario corregido (ya no afirma una invariante
   global que K3 invalida; el test en sí no cambia de resultado).
5. Este RESULT.

## Los cambios exactos (§7.1)

- **`combat_system` targeting** (línea ~857 antes del cambio): guard
  `unit_class[j] > 2 && entity_kind[j] != 1u` → `unit_class[j] > 2 &&
  unit_class[j] != 3u && entity_kind[j] != 1u`. Un aldeano enemigo
  (`unit_class==3`, `entity_kind==0`) ya es objetivo válido; los edificios
  (`entity_kind==1`) lo siguen siendo; los propios (mismo `owner`) siguen
  excluidos por el guard `owner[j]==owner[i]` (sin tocar).
- **`aggro_system` targeting** (línea ~945 antes del cambio): mismo cambio,
  mismo patrón.
- **Guards de ATACANTE intactos**: verificados en `movement_v1` (L676),
  `combat_system` (L823→834) y `aggro_system` (L909→925) —
  `unit_class[i] > 2` sin tocar en ninguno de los tres. Los aldeanos siguen
  sin atacar, perseguir ni desplazarse por seek/flujo.
- **RPS contra aldeano defensor**: `rps_mult_vs_citizen_bp(atk_class)` nueva,
  mismo patrón que `rps_mult_vs_building_bp` — devuelve 10000 bp (×1.0)
  siempre, documentado como balance v1 ajustable. `combat_system` la invoca
  cuando `unit_class[best]==3`, ANTES de caer en `rps_mult_bp` (tabla 3×3),
  evitando el OOB. La tabla 3×3 existente no se tocó.

## Desviaciones numeradas

1. **Guard implementado como `> 2 && != 3` en vez de `> 3`.** Ambas formas
   son equivalentes con los valores de `unit_class` que el kernel asigna hoy
   (0/1/2 combate, 3 aldeano, 255 edificio — nunca 4, `SPAWN_UNIT` todavía no
   admite `Siege`). Elegí la forma más explícita (`!= 3`) para no dejar una
   mina de OOB latente: si `Siege` (`unit_class==4`) se vuelve spawneable en
   un sprint futuro, `> 3` lo admitiría como objetivo sin que
   `rps_mult_vs_citizen_bp`/`rps_mult_bp` lo contemplen (`rps_mult_bp` leería
   `TABLE[..][4]`, OOB); `> 2 && != 3` lo sigue excluyendo, igual que el
   guard original excluía cualquier `unit_class` fuera de 0..2 que no fuera
   edificio. Conservador ante huecos, por la regla dura del brief.
2. **Comentarios corregidos, no solo el guard.** Tres sitios documentaban
   literalmente la invariante "unit_class=3 excluido de combat_system" como
   si fuera universal (`init_citizen_from_catalog`, el handler
   `SPAWN_CITIZEN` en ambos caminos data-driven/debug). Los actualicé para
   reflejar la nueva realidad (excluido como ATACANTE, vulnerable como
   OBJETIVO) — cambio de documentación puro, cero efecto en código/checksum.
   Igual en `test_economy.cpp`: el comentario afirmaba "no hay mecanismo de
   muerte para citizens en v1", ya falso en general (aunque el resultado de
   ESE test no cambia, porque nunca spawnea al owner 1).
3. **Escenario económico como archivo NUEVO separado (`skirmish_eco.hpp`),
   no una variante dentro de `skirmish.hpp`.** El brief permitía ambas
   opciones. Elegí la separada para que `skirmish.hpp` quede exactamente
   append-only/intacto (cero riesgo de regresión en el gate de K2, que sigue
   pasando bit-idéntico — ver checksums abajo).
4. **Tests con manipulación white-box de `GameState`.**
   `test_citizen_attacker_guard_intact()` y `test_aggro_targets_citizen()`
   (en `test_combat.cpp`) fuerzan directamente `g->attack[i]`/
   `g->range_mt[i]`/`g->speed_mtpt[i]` en un aldeano después del spawn —
   estados inalcanzables vía comandos reales (`SPAWN_CITIZEN` fuerza
   `attack=0` siempre). Documentado inline: sirven para probar el guard de
   ATACANTE en aislamiento, no un camino de producción.
5. **No existe un artefacto de "checksums golden de escenarios" separado en
   este repo del kernel.** El gate "Golden" de `tests/determinism/golden/`
   (1074 casos) es aritmético puro (`fixed64`/`normalize_v1`), sin entidades
   ni combate — estructuralmente no puede verse afectado por este cambio (ni
   antes ni después hay `unit_class` alguno involucrado). Confirmado
   igual en ambas ramas (ver checksums). La "regeneración de checksums de
   escenarios con aldeanos al alcance" que pide el brief se satisface con el
   test NUEVO (`test_ai_skirmish_eco.cpp`): sus checksums SON el golden
   regenerado de la trayectoria recién habilitada. El showcase de Godot
   (`docs/briefs/KIMI_DEMO_SHOWCASE.md`, "120 aldeanos amarillos") vive fuera
   de este repo de kernel (capa GDExtension/UI) — no lo toqué ni lo verifiqué
   aquí; queda señalado para el Arquitecto/equipo de esa capa, tal como
   anticipa el propio brief ("espera que el showcase cambie... documéntalo").

## Gates

### Build
`cmake --build build -j2` (nice -n 19): **limpio, `-Werror`, 0 warnings.**
Cero float/heap nuevos en las rutas tocadas de `step.hpp` (solo enteros:
comparaciones `uint8_t`, una función `constexpr`-friendly que devuelve
`10000`).

### Golden (aritmético, NO debe cambiar)
```
GOLDEN backend=int128 casos=1074 fallos=0  [OK]
```
Idéntico en `main` (pre-K3) y en esta rama (post-K3) — 1074/1074 en ambas.

### G1 (alloc_delta=0)
```
G1 selftest: alloc_delta=0 OK checksum=2defd6416796e3d8
```
Checksum **idéntico** a `main` (`2defd6416796e3d8` en ambas ramas):
escenario `synthetic_movement` sin ciudadanos, trayectoria intacta.

### G3 (save/continuar sin IA)
```
G3 savetest(save@200): OK state=794d43a2dd8333a8 cont=8b3a30f0b0eb11f6 ai_executions=0
```
Idéntico a `main` (mismo `state`/`cont`).

### G4 (save/continuar con IA real)
```
G4 savetest(save@200): OK state=2681ad5f3eb161ad cont=486c9601301ff753 ai_executions=30
```
Idéntico a `main` (mismo `state`/`cont`/`ai_executions`).

### G5 (record + replay feed-mode)
```
record: 400 ticks → checksum=2681ad5f3eb161ad
G5 verify: OK ai_executions=0 schedule_mismatches=0 replay_v=3 checksum=2681ad5f3eb161ad
```
Idéntico a `main`.

**Conclusión del dump pre/post**: los 4 escenarios de gate (golden, G1, G3,
G4/G5) no contienen ningún `unit_class==3` — por construcción, el guard
modificado (`unit_class[j] > 2 && unit_class[j] != 3 && ...`) es
EQUIVALENTE lógicamente al guard original en ausencia de aldeanos, y la
comprobación empírica (checksums bit a bit idénticos entre `main` y esta
rama, misma build, mismos comandos) lo confirma sin margen de duda.

### ctest completo
```
100% tests passed, 20/20 tests, Total Test time (real) = 8.40 sec
```
Incluye los 2 tests nuevos (`ai_skirmish_eco`, y los 3 casos nuevos dentro de
`combat`) y los existentes sin regresión.

### Skirmish militar de K2 (regresión, SIN aldeanos)
```
skirmish A: end_tick=1236 winner=1 ai_executions=62 state=fe34b80541a40007 cont=ca0b366d3cc88711
```
**Bit-idéntico** a la misma corrida en `main` (mismo `end_tick`, `winner`,
`ai_executions`, `state`, `cont`) — `skirmish.hpp` no se tocó (append-only) y
su trayectoria no cambió, tal como exige el guard conservador.

### Skirmish CON ECONOMÍA (K3, nuevo — el valor central de este cierre)
```
skirmish_eco A: end_tick=1827 winner=1 ai_executions=91 state=8ea5bc60ca1a48cd cont=5536e17acab4555a
skirmish_eco A2: stock_defensor A=0 B=300 Me=0
```
- **Tick de fin: 1827** (< 36000, con margen amplio ×19).
- **Ganador: emisor 1** (el atacante, IA real) — asimetría deliberada
  (mismo patrón que K2: el atacante arranca con más soldados y refuerza
  continuamente via `TRAIN_UNIT`; el defensor es humano-scripted).
- **Economía real verificada**: el defensor (owner 0) acumuló 300 unidades
  del recurso B (sus 3 aldeanos, spawneados junto a su centro, recolectaron
  del depósito de B más cercano y entregaron en su dropoff fijo — sin un
  solo comando adicional tras el setup de t==0, puro `economy_system`
  autónomo).
- **La enmienda §7.1 en acción**: al final de la partida el defensor tiene
  **0 edificios vivos Y 0 aldeanos vivos** (ambos verificados por test) — la
  derrota v1 de SPEC-005 §6 solo pudo declararse porque los aldeanos, ahora
  vulnerables, terminaron muertos junto con el centro. Antes de K3 esto era
  estructuralmente imposible (un aldeano intocable habría mantenido al
  defensor "no derrotado" para siempre).
- **Geometría del diseño** (no casualidad): la base del defensor se ancla
  exactamente en el tile de su propio `dropoff_x/y` fijo
  (`gs_init_economy`), así que cada ciclo `RETURN` de sus aldeanos termina,
  por construcción, a menos de 1 tile del centro — garantiza que el último
  aldeano vivo pase por la zona de combate en, como mucho, un ciclo
  económico completo tras la caída del centro (documentado en el header).
- **Determinismo**: dos corridas independientes → mismo `winner`, `end_tick`,
  `final_checksum`, `continuation_checksum` (test B).
- **Save/continuar**: guardar a mitad de camino y continuar reproduce
  exactamente la corrida continua (test C).
- **Replay bit-exacto**: grabar y reproducir en feed-mode da el mismo
  checksum, `ai_executions==0` en feed-mode, `schedule_mismatches==0`
  (test D).

## Archivos tocados/nuevos

- `addons/chunsa_sim/core/include/chunsa/step.hpp` (modificado: 2 guards +
  1 función RPS + comentarios).
- `addons/chunsa_sim/core/include/chunsa/skirmish_eco.hpp` (nuevo).
- `tests/unit/test_ai_skirmish_eco.cpp` (nuevo).
- `tests/unit/test_combat.cpp` (modificado: +3 tests de comportamiento).
- `tests/unit/test_economy.cpp` (modificado: solo comentario).
- `CMakeLists.txt` (modificado: registro del test nuevo).

## Entrega

Lista para revisión del Arquitecto (+ auditoría de Opus, el cambio toca
combate — el propio brief lo anticipa). Sin merges a `main`.
