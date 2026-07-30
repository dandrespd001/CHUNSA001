# Brief 1.8D — Contenido: depósitos en el mapa y costes reales

**Modelo:** MiniMax-M3 · **Rama:** `arch/sprint-1.8d-contenido`
**Normativo:** `SPEC-007` §9.2 (tabla autoritativa) y §9.4 · `SPEC-004` §15.1
(simetría de mapas)

---

## 0. Este sprint es el peligroso, y conviene decirlo

El panel multimodelo avisó de que este trabajo puede desbordarse:

> `[I]` «Requiere diseñar y balancear los costes de todas las unidades,
> edificios y tecnologías existentes → el diseño de contenido absorberá meses,
> no un sprint, y el juego será injugable hasta que se complete.»

Por eso el alcance está **deliberadamente recortado**. No es el sprint que
completa el balance de las 15 edades: es el que hace **jugable la época 3–4**.

**Si en algún momento ves que el alcance se te va, PARA y repórtalo.** Entregar
menos con la partida jugable es mejor que entregar todo con la partida rota.

---

## 1. Alcance

### 1.1 Depósitos en el mapa (`data/maps/base_demo_desert_basin.yaml`)

Hoy hay 12 depósitos de tres recursos. Deben cubrir los que **existen en las
épocas 1–4**: comida, madera, piedra, arcilla, cobre, oro, plomo, sal, estaño.

**Reglas duras, no negociables:**

- **Simetría** (SPEC-004 §15.1): conteo PAR ⇒ pares espejados
  (`x_der = 256000 - x_izq`, misma Y, mismo `amount`, **mismo recurso**).
  Conteo IMPAR ⇒ **sobre el eje central** (`x = 128000`, `y ∈ [124000, 132000)`).
  Un mapa asimétrico es injugable en competitivo y **ya se corrigió un fallo de
  esto** en el Sprint 1.6B.
- **Ningún depósito en el muro**: `x_millitiles ∉ [127500, 128500)`.
- Los de base, a **8–20 tiles** de cada centro. Los neutrales disputados, en la
  franja central.
- **Máximo `ECO_MAX_DEPOSITS` = 64.** Si necesitas más, el diseño está mal, no
  el límite.

**Cantidades**: el Director pidió depósitos «con mucho más recursos». Hoy son
500 (base) y 800 (neutral). Súbelos de forma que una partida de época 3–4
**no agote la zona propia**, y **justifica el número** en el informe.

### 1.2 Costes reales (`data/units/`, `data/buildings/`)

**Solo de lo que existe hoy**: 5 unidades y 6 edificios.

Regla de diseño tomada de la investigación:

> `[V] [Rise of Nations]` la mayoría de unidades cuesta **solo 2 recursos**.

**Ningún elemento debe costar más de 3 recursos.** Es lo que mantiene la carga
mental baja aunque existan 30 recursos.

Reparto orientativo: unidades militares comida + un metal · edificios madera +
piedra · el establo de carros, algo de bronce cuando exista (edad 4).

**No inventes recursos de edades futuras en cosas de la época 3.** Un cuartel de
la edad 3 no puede costar acero.

### 1.3 Lo que NO

- **No** tocar `addons/`, `tests/` ni `tools/`.
- **No** tocar `data/resources/`: las definiciones están cerradas.
- **No** añadir edificios ni unidades nuevas.
- **No** tocar recetas: siguen vacías hasta el 1.9.

---

## 2. Criterio de éxito — y aquí SÍ cambian las trayectorias

**Este es el primer sprint del bloque que cambia el juego a propósito.** Los
`end_tick` de la apertura y del eco **van a moverse**, y eso es correcto.

Lo que se exige:

| Invariante | Debe seguir |
|---|---|
| `winner` de la apertura | **1** |
| apertura termina | **< 36000 ticks** |
| las cuatro fases de la apertura | **observadas** |
| G1 `alloc_delta` | **0** |
| vectores dorados | **1074 / 0 fallos** |
| G1, G3, G4, `ai_skirmish` (sin ciudadanos) | **bit-idénticos** |

Los baselines de `ai_skirmish_eco` y `ai_skirmish_apertura` **se re-registran**,
con el valor nuevo **justificado** en el commit.

**Si la apertura deja de terminar, o gana el bando equivocado, PARA.** Eso no es
un baseline a actualizar: es contenido mal balanceado.

---

## 3. Definición de hecho

- [ ] Depósitos de las épocas 1–4 en el mapa, **simétricos**, ≤ 64.
- [ ] `x_millitiles ∉ [127500, 128500)` en todos.
- [ ] Costes reales en las 5 unidades y 6 edificios, **≤ 3 recursos cada uno**.
- [ ] El blob compila y **dos compilaciones son byte a byte idénticas**.
- [ ] `ctest -L fast` verde · `ctest` completo verde.
- [ ] Las invariantes de §2 se cumplen; los baselines nuevos, justificados.
- [ ] `git diff --stat -- addons/ tests/ tools/ data/resources/` → **vacío**.
- [ ] **Commitea en la rama. NO fusiones a `main`.**
- [ ] Informe en `docs/RESULT_MINIMAX_1.8D.md` con: tabla de depósitos, tabla
      de costes, justificación de las cantidades y los baselines nuevos.

**No afirmes haber ejecutado nada que no hayas ejecutado.** Si no puedes correr
`ctest`, dilo.
