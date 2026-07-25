# BRIEF K2 — GATHER + la IA juega la apertura (Sonnet · Sprint 1.6B, pieza 2)

Implementa **SPEC-004 §18, §19 y el DoD de §20** (`docs/specs/SPEC-004_SISTEMAS_PARTIDA.md`,
Parte III). Es la pieza que cierra el sprint: el jugador puede **dirigir** la recolección, la IA
juega una apertura económica de verdad, y existe un escenario que va de `centro + 3 aldeanos`
hasta `ejército entrenado` **sin nada inyectado**. Lee §18, §19 y §20 enteros antes de empezar.

## Rama y alcance
- Rama `sonnet/k2-gather-apertura` desde `main` (HEAD, con K1 ya integrado: `civ_id` tipado,
  `ResourceSpawnV1`, `player_civ`, `gs_init_economy_from_catalog`, save v12/checksum v7).
- Archivos esperados: `commands.hpp` (`GATHER = 13`, append-only), `step.hpp` (validación +
  efecto), `economy.hpp` (endurecer la reasignación por preferencia de recurso — sigue
  autocontenido, sin conocer GameState), `ai_stub.hpp` (§19, la capa económica), un escenario
  nuevo (`skirmish_apertura.hpp` o similar, NO toques `skirmish.hpp`/`skirmish_eco.hpp`), tests.

## Estado que ya tienes de K1 (úsalo, no lo reimplementes)
- `gs_init_economy_from_catalog(g)` puebla los depósitos desde el mapa: **es opt-in y nadie la
  llama todavía**. Tu escenario DEBE llamarla (es el punto de la apertura: 12 depósitos reales
  simétricos en vez de los 6 fijos legacy).
- `player_civ[]` + `gs_set_player_civ` + gates de civilización ya existen: tu escenario asigna la
  civ de cada jugador (egipto slot 0 / roma slot 1) y así los gates hacen su trabajo.
- Los centros ya entrenan al aldeano de su civ (dato del blob real, `trains` poblado).

## §18 — GATHER
- **`GATHER = 13`**: `p.handle` = ciudadano propio; `p.x_raw/p.y_raw` = punto raw del depósito.
  Validación EN ORDEN (es contrato, testéalo): handle vivo/propio (**INVALID_ENTITY**/
  **NOT_OWNER**) · `unit_class == 3` (**ILLEGAL_STATE**) · resolver depósito = el de índice más
  bajo con `remaining > 0` a distancia ≤ `GATHER_PICK_RADIUS_RAW` (1 tile) del punto; ninguno ⇒
  **INVALID_ENTITY**. Efecto: `eco_assigned_deposit`, `eco_state = SEEK`, y
  `build_target = BUILD_NO_TARGET` (recolectar cancela construir — decisión explícita del
  contrato).
- **Agotamiento/reasignación determinista**: al agotarse un depósito, el aldeano reasigna al
  **vivo más cercano del MISMO recurso** (dist_sq entera, empate por menor índice); si no queda
  ninguno de ese recurso, al más cercano de cualquiera; si no queda ninguno, ocioso
  (`ECO_NO_DEPOSIT`). Hoy `eco_find_nearest_deposit` no respeta la preferencia de recurso:
  endurécelo manteniendo `economy.hpp` autocontenido (pásale la preferencia como parámetro).

## §19 — La IA juega la apertura (extiende `ai_execute`)
**La regla de oro de SPEC-005 §0 sigue siendo el criterio de rechazo #1** (lo auditará Opus):
función pura de `(g, source_tick, runtime_before)`, cero `g.tick`/reloj/float/heap/RNG fuera de
`AI_TIEBREAK`, orden de emisión canónico ascendente, presupuesto `AI_MAX_COMMANDS` respetado.
- Cuenta aldeanos por recurso asignado y el stock A/B/Me; si el stock de un recurso que su
  intención necesita está bajo el umbral, emite **GATHER** redirigiendo aldeanos ociosos o
  excedentes a ese recurso (demanda adaptativa, con umbrales enteros derivados de los pesos
  `_bp` del perfil como ya hacen las otras capas).
- **Repone aldeanos**: si `citizens < AI_ECON_TARGET_CITIZENS` y su centro puede entrenar,
  `TRAIN_UNIT` del aldeano de SU civilización (ahora posible: `trains` poblado + `civ_id`).
- Mantén las capas existentes (construir/militarizar/tech/reactiva/táctica) intactas en su orden.

## El escenario del DoD (§20) — lo que demuestra el sprint
Un escenario nuevo que arranca con **solo centro + 3 aldeanos por jugador** (cero ejército, cero
edificios militares, cero depósitos hardcodeados: llama a `gs_init_economy_from_catalog`) y donde
**la IA recorre sola el camino completo**: recolectar → construir edificio militar → entrenar
ejército → atacar → `game_over` con `winner != 0xFF`, en < 36000 ticks. Sin mutaciones
privilegiadas: TODO por comandos (el setup en la ventana del tick 0, el resto con delay real).
Verifica y documenta: tick de fin, ganador, y que el defensor/atacante pasaron por las fases
(recursos recolectados > 0, edificio militar construido, unidades entrenadas).

## Tests obligatorios
GATHER: camino feliz + CADA rechazo en orden · GATHER cancela `build_target` · agotamiento →
reasignación al mismo recurso → a cualquiera → ocioso · la IA emite GATHER cuando falta un
recurso y TRAIN_UNIT cuando faltan aldeanos · **determinismo de `ai_execute`** (dos llamadas con
el mismo estado congelado → `result[]` byte-idéntico) · el escenario del DoD: termina, dos
corridas idénticas (mismo winner y tick), save/load a **mitad de recolección** + continuar ==
corrida continua, replay bit-exacto · GameState SIEMPRE en heap (`make_unique`).

## Reglas duras
Append-only; iteración ascendente; cero float/heap en `step` y en `ai_execute`; térmica
`nice -n 19 -j2` un build a la vez; **regresión**: `skirmish` militar, `skirmish_eco` y los
escenarios sintéticos deben quedar bit-idénticos (dump pre/post) — si el checksum cambia debe ser
solo por un bump de dominio que TÚ declares y justifiques; conservador ante huecos + desviación
numerada. NO toques el adaptador Godot. NO merges a main.

## Entrega
Commits atómicos + `docs/briefs/SONNET_K2_GATHER_APERTURA_RESULT.md`: desviaciones numeradas,
gates completos (golden, G1/G3/G4/G5, ctest N/N, regresión de los 2 skirmish previos), el tick de
fin/ganador/fases del escenario del DoD, checksums, y la nota de cómo garantizaste la regla de oro
en la capa económica nueva. El Arquitecto revisa (+ Opus audita el determinismo) e integra.
