# Brief K2 — Auto-recolección acotada a zona aliada (Sprint 1.7)

**Modelo:** GPT-5.6 SOL (`codex -m gpt-5.6-sol`)
**Rama base:** `main` (con K1 ya integrado)
**Rama de trabajo:** `arch/sprint-1.7-zona-aliada`
**Contrato normativo:** `docs/specs/SPEC-004_SISTEMAS_PARTIDA.md` **§23**.
Léelo entero. Este brief no lo resume: añade el cómo.

Trabajo de **kernel** y **pruebas**. **No toques `addons/chunsa_sim/gdextension/`**
(hay otra rama en paralelo con el adaptador).

---

## 0. Reglas innegociables

1. Determinismo bit-exacto: cero float/double, cero reloj, cero entropía fuera
   de `RngStream`. Recorrido ascendente por índice, desempate por índice bajo.
2. `Step()` sin heap ni STL.
3. `movement_v1` CONGELADO.
4. `CommandType` append-only.
5. `GameState` en heap en los tests (`std::make_unique`).
6. Nada de `assert()` para validar datos.

---

## 1. Contexto: por qué existe este trabajo

El Director jugó una partida real y no pudo construir, entrenar ni investigar
**nada**. Instrumenté el kernel y la causa es la regla de preferencia de recurso
de §18, que especifiqué **sin cota de distancia**: los cuatro aldeanos agotan el
depósito contiguo y se van andando 100+ tiles al siguiente depósito del mismo
recurso, ignorando dos depósitos de otro recurso a 8–20 tiles. Como todo cuesta
`A` y nadie recoge `A`, cada comando muere en `ILLEGAL_STATE`.

Evidencia reproducida (posiciones marchando sin parar hacia el este):

```text
CIT idx=10 state=SEEK dep=7 dep_rem=750 dep_res=2 carry=0 pos=45,132
CIT idx=11 state=SEEK dep=7 dep_rem=750 dep_res=2 carry=0 pos=46,133
```

Es un defecto **de mi especificación**, no de la implementación anterior.

---

## 2. Qué implementar

### 2.1 Predicado de zona aliada

Un depósito está en zona aliada de `p` si su distancia a **algún edificio
COMPLETO propiedad de `p`** es `<= ECO_AUTO_GATHER_RADIUS_RAW`.

Reutiliza el recorrido de edificios que ya hace
`detail::find_building_dropoff` (`step.hpp`, §6): `entity_kind == 1`, vivo,
`owner == p`, `build_progress >= build_time_ticks`. **No dupliques el criterio
de "edificio completo"**; si hace falta, extrae un helper compartido para que
exista un solo sitio donde se decide qué es un edificio válido.

Distancias con `dist_sq_raw` y comparación al cuadrado, como el resto del
kernel. Sin raíces.

### 2.2 `eco_find_nearest_deposit` acotada

La búsqueda automática pasa a filtrar por el predicado anterior. Mantén las dos
pasadas que ya existen (recurso preferido primero, luego cualquiera), pero
**ambas dentro de la zona aliada**. Si ninguna encuentra nada, el ciudadano va a
`IDLE` (§22.2) — ese camino ya lo implementaste en K1, no lo dupliques.

Cuidado: la función hoy no conoce al `owner` ni ve los edificios. Necesitará
acceso a lo que corresponda; elige la forma menos invasiva que no rompa la
separación actual de `economy.hpp` (que es puro y sin dependencias de
`GameState`). Si la vía limpia es que el **caller** en `step.hpp` precalcule el
conjunto de depósitos elegibles y se lo pase, hazlo así y explica la decisión en
el informe.

### 2.3 La orden del jugador NO se toca

`GATHER` emitido por el jugador asigna el depósito **directamente** y **puede
apuntar a cualquier depósito del mapa** (§23.3). No añadas ninguna validación de
zona a ese camino. Verifica explícitamente que sigue siendo así.

### 2.4 Constante

`ECO_AUTO_GATHER_RADIUS_RAW` en `economy.hpp`, valor **32 tiles** en raw,
con comentario que explique el criterio (base 8–20 tiles, neutrales 100+).

---

## 3. Baselines

| Gate | Expectativa |
|---|---|
| G1, G3, G4, `ai_skirmish` | **bit-idénticos** — sin ciudadanos. Si cambian, PARA y repórtalo |
| Vectores dorados | 1074 / 0 fallos |
| `ai_skirmish_eco` | cambia; re-registra y justifica |
| `ai_skirmish_apertura` | cambia; re-registra y justifica |

Invariantes funcionales que **deben** mantenerse en la apertura: `winner == 1`,
final por debajo de 36000 ticks, las cuatro fases observadas. Si alguna se
rompe, es regresión: para y repórtalo.

`SAVE_FORMAT_VERSION` y `CHECKSUM_ALGO_VERSION` **no cambian** (§23.5).

---

## 4. Pruebas obligatorias

1. Depósito del mismo recurso **fuera** de zona aliada y otro de recurso
   distinto **dentro**: la auto-asignación elige el de dentro. *(Es el caso que
   reprodujo el Director.)*
2. Dos depósitos del recurso preferido dentro de zona: gana el más cercano;
   ante empate exacto, el índice más bajo.
3. Sin ningún depósito en zona aliada: el ciudadano acaba en `IDLE`, no en
   `SEEK` perpetuo.
4. `GATHER` del jugador a un depósito **fuera** de zona aliada: **aceptado**, y
   el ciudadano va allí.
5. Tras agotarse un depósito asignado por el jugador fuera de zona, la
   reasignación automática lo trae de vuelta a la zona aliada.
6. Un edificio que se **completa** amplía la zona: un depósito antes inelegible
   pasa a serlo.
7. La regla de carga de §18 (recurso distinto ⇒ `RETURN` antes) sigue intacta.

---

## Definición de hecho

- [ ] `cmake --build build-gcc -j8` limpio, sin avisos nuevos.
- [ ] `ctest --test-dir build-gcc --output-on-failure` todo verde.
- [ ] Gates bit-idénticos de §3 confirmados; los que cambian, re-registrados y
      justificados en el commit.
- [ ] Las 7 pruebas de §4 en verde.
- [ ] ASan y UBSan verdes sobre los binarios tocados; pega la salida.
- [ ] Informe en `docs/RESULT_SOL_K2_1.7.md`: qué cambiaste y dónde, la decisión
      de diseño de §2.2 (cómo accede la búsqueda a los edificios), tabla de
      baselines pre/post, salida de sanitizers, y cualquier desviación con su
      razón.

**Sin pipes que enmascaren el código de salida**: `cmd; echo $?`.

**No afirmes haber ejecutado nada que no hayas ejecutado.**

**No hagas merge a `main`.**
