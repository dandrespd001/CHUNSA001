# Revisión del Arquitecto — K2: auto-recolección acotada a zona aliada (Sprint 1.7)

Fecha: 2026-07-28
Rama: `arch/sprint-1.7-zona-aliada`
Implementación: GPT-5.6 SOL · Contrato: SPEC-004 §23 + `SOL_K2_ZONA_ALIADA_1.7.md`
Informe: `docs/RESULT_SOL_K2_1.7.md`

## Veredicto

**Aprobado e integrado.** Corrige el defecto que el Director reprodujo jugando.

## 1. Verificación independiente

No se aceptaron las cifras del informe; se midieron:

```text
BUILD=0     cmake --build build-gcc -j8    · 0 avisos
CTEST=0     100% tests passed out of 29
```

Como los gates de determinismo **aseveran** los baselines desde el K1
(Parte C), un `ctest` verde **prueba por sí solo** que G1, G3, G4, skirmish y
los vectores dorados siguen bit-idénticos. Antes de ese blindaje habría hecho
falta comparar a mano.

Invariantes funcionales:

| Gate | Antes | Después | |
|---|---|---|---|
| apertura `end_tick` | 12292 | **9317** | mejora esperada, `< 36000` |
| apertura `winner` | 1 | 1 | contractual |
| `ai_skirmish_eco` | — | `winner=1`, `end_tick=1107` | |

La apertura **acelera un 24%**: los aldeanos dejan de cruzar el mapa y
producen antes. Es el efecto buscado, no un accidente.

Baselines re-registrados (ambos escenarios tienen ciudadanos):
`AI_SKIRMISH_ECO_STATE`, `AI_SKIRMISH_APERTURA_STATE` y sus continuaciones.
`SAVE_FORMAT_VERSION` 13 y `CHECKSUM_ALGO_VERSION` 8 **sin tocar**, correcto:
§23 no añade estado persistido.

## 2. Prueba de mutación — las pruebas tienen poder

Este trabajo es anterior a la directriz TDD, así que no hay fase roja. Se
verificó el poder de las pruebas por **mutación** (`METODOLOGIA_TDD.md` §4):

Mutante aplicado: radio de zona aliada multiplicado por 10⁶, lo que **reproduce
exactamente el bug original** (auto-asignación sin cota de distancia).

```text
CHECK L703: g->eco_assigned_deposit[1] == 2u
CHECK L749: g->eco_assigned_deposit[1] == ECO_NO_DEPOSIT
CHECK L751: g->citizen_task[1] == CITIZEN_TASK_IDLE
CHECK L752: g->vel_x[1] == 0 && g->vel_y[1] == 0
CHECK L815: (detail::allied_auto_gather_deposit_mask(*g, 0u) & 1u) == 0u
gather: 5 fallos
```

Cinco asertos caen, incluido el que comprueba que el aldeano elige el depósito
**cercano** frente al lejano del mismo recurso. Restaurado el original, verde.

Esto es lo que faltó en los cuatro entregables anteriores y lo que la
metodología TDD (§2) pasa a exigir por contrato desde el Sprint 1.8A.

## 3. Diseño

`allied_auto_gather_deposit_masks` calcula una **máscara de bits por jugador**
(bit *i* = `deposits[i]` elegible) y se la pasa a `eco_find_nearest_deposit`.

Es mejor de lo que pedía mi brief. Yo dejé abierto cómo daría la búsqueda
acceso a los edificios; SOL eligió **no dárselo**: precalcula en `step.hpp` y
`economy.hpp` sigue siendo puro, sin dependencias de `GameState`. Se conserva
la separación que el kernel ya tenía, y de paso la máscara se calcula una vez
por tick en lugar de una por ciudadano.

`is_complete_owned_building` se extrae como helper compartido, así que el
criterio de «edificio válido» vive en un único sitio, como pedía el contrato.

## 4. Lo que esto arregla para el Director

Los aldeanos ya no peregrinan. Con el binario correcto, la cadena que dejaba la
partida muerta —sin recolección de `A` → sin stock → todo rechazado con
`ILLEGAL_STATE`— queda rota en su primer eslabón.

`GATHER` por clic derecho **sigue sin acotar** (§23.3): puedes mandar aldeanos
a cualquier depósito del mapa, incluidos los neutrales disputados. Es agencia
del jugador, patrón AoE2, y hay prueba dedicada que lo verifica.

## 5. Deuda que queda viva

El `.so` de Godot sigue construido contra un kernel anterior. Tras integrar
esta rama y la del HUD hay que **regenerar `demo/bin/libchunsa_godot.so`**
contra `main`. Es la misma desalineación que ya costó una sesión de pruebas del
Director.
