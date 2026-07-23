# BRIEF K2 — Producción, tecnología y épocas (Sonnet · Sprint 1.2, pieza 2)

Implementa **SPEC-004 §11 + §12 + §13** (`docs/specs/SPEC-004_SISTEMAS_PARTIDA.md`,
Parte II) sobre el `main` actual, que YA incluye K1 (replay v3, save v9, checksum v4
`CHUNSA_STATE_V4`) y los datos reales (blob `building=6 tech=4`: cuarteles con `trains`
y 4 techs con capacidades). Lee §11–§13 enteros antes de tocar nada.

## Rama y alcance
- Rama `sonnet/k2-produccion-tech` desde `main` (HEAD). Jamás toques `main`.
- Archivos esperados: `data_catalog.hpp` (§11.1: costes/build_time tipados en
  UnitDefinitionV1; trains/researches resueltos en BuildingDefinitionV1; tabla
  TechDefinitionV1 + tabla de capacidades del blob), `game_state.hpp` (§11.2/§12.2),
  `commands.hpp` (TRAIN_UNIT=9/SET_RALLY=10/RESEARCH_TECH=11/EPOCH_UP=12, append-only),
  `step.hpp` (validaciones + production_system/research_system + gating §12.4),
  `serialize/checksum/save_io` (§13: save v10, dominio → `CHUNSA_STATE_V5` — la V4 ya
  la consumió K1), tests nuevos.

## Decisiones ya tomadas (no re-litigar)
- Techs = paquetes de capacidad, SIN efectos de stats en Parte II.
- Sin CANCEL de cola ni de research (desviación pre-aceptada; documenta).
- `POP_CAP_V1 = 200` constante; pop se reserva al encolar y se libera al morir la
  unidad o al morir el edificio con ítems sin entrenar (§11.4).
- Época inicial de cada jugador: la mínima común de los datos del slice (derívala del
  catálogo de forma determinista y documenta la fórmula exacta en el RESULT).
- El blob YA contiene los records tech (kind=3 en el directorio, hoy solo validados
  estructuralmente) — el trabajo §11.1/§12.1 es tipificarlos en el loader con el MISMO
  patrón endurecido de unidades/edificios (CveValue acotado, unique_ptr, reserve
  exacto, rechazo del catálogo entero, orden bytewise en find). Opus auditará: espejo
  fiel del patrón o lo rechazamos.
- Los `grants` de los datos referencian capacidades del manifest: la tabla de
  capacidades sale del blob (si el blob no la incluye como sección utilizable,
  deriva los CapabilityId de los propios grants en orden canónico bytewise y
  documenta la desviación — NO toques el compilador Python).

## Tests obligatorios (§13)
TRAIN feliz + CADA rechazo del §11.3 en orden · cola llena · pop llena · producción
multi-ítem con pool de entidades exhausto a mitad (espera sin perder progreso) · rally
(la unidad entrenada camina al rally) · RESEARCH feliz + prereq incumplido + mutex +
época + edificio ocupado · EPOCH_UP: falla por edificios, falla por tiempo, pasa con
ambos gates · gating de época en TRAIN y PLACE_BUILDING (§12.4) · save v10 round-trip
con cola no vacía y research en curso · catálogo golden real: los 2 cuarteles resuelven
sus `trains` y las 4 techs sus `grants` (ids del blob del repo) · determinismo del
escenario completo (dos corridas + save intermedio + replay v3, patrón de
test_buildings/test_replay_v3). RECUERDA: GameState SIEMPRE en heap en los tests
(make_unique) — un GameState en pila segfaultea bajo ctest (lección K1).

## Reglas duras
Las de siempre: iteración ascendente, cero float/heap en Step, append-only, trayectoria
golden previa bit-idéntica (dump pre/post), térmica `nice -n 19 -j2` un build a la vez,
conservador ante huecos + desviación numerada en RESULT. La demo headless debe seguir
verde (no toques el adaptador; la UI la hará otro agente).

## Entrega
Commits atómicos + `docs/briefs/SONNET_K2_PRODUCCION_TECH_RESULT.md` (desviaciones,
gates completos golden/G1/G3/G4/G5/ctest, checksums nuevos, fórmula de época inicial).
NO merges; el Arquitecto revisa e integra.
