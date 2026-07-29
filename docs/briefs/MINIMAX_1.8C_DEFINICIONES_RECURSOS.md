# Brief 1.8C-datos — Los 30 recursos como definiciones

**Modelo:** MiniMax-M3 (`claude-minimax`) · **Rama:** `arch/sprint-1.8c-recursos`
**Normativo:** `docs/specs/SPEC-007_RECURSOS_Y_EDADES.md` **§9.2** (tabla
autoritativa) y **§20.5** · familias en `SPEC-006` Parte III §13

---

## 1. Qué es este sprint, y qué NO

Escribir los **30 recursos** como definiciones de datos. Es **vocabulario
puro**.

**Lo que SÍ:**

- Un fichero por recurso en `data/resources/`, validando contra
  `data/schemas/resource.schema.json` (ya existe, del 1.8B).
- Cada uno con: `id` namespaced, clave de localización, **familia**, **edad de
  aparición**, y si es **recolectado** o **producido**.
- Compilar el blob y que el `content_hash` cambie de forma reproducible.

**Lo que NO, bajo ningún concepto:**

- **No** tocar `data/maps/`. Ningún depósito nuevo.
- **No** tocar los `resource_costs` de unidades, edificios ni tecnologías.
  Siguen costando solo `chunsa:food`, `chunsa:wood` y `chunsa:stone`.
- **No** tocar código: ni `addons/`, ni `tools/`.

**Criterio de éxito:** los `end_tick` de la apertura (**9317**) y del eco
(**1107**) quedan **idénticos**. Si se mueven, has tocado algo que no debías.

---

## 2. La lista — cópiala de SPEC-007 §9.2, no de aquí

La tabla autoritativa es §9.2. **Léela ahí.** Este brief no la repite para que
no existan dos versiones que puedan divergir.

Resumen de forma: 20 recolectados (índices 0–19) y 10 producidos (20–29).
Los tres primeros ya existen y **conservan sus índices**: `chunsa:food` = 0,
`chunsa:wood` = 1, `chunsa:stone` = 2.

## 3. Familias (SPEC-006 §13)

Cada recurso lleva **exactamente una**:

| Familia | Recursos |
|---|---|
| `subsistence` | comida |
| `construction` | madera, piedra, arcilla, cal viva, cemento |
| `base_metals` | cobre, estaño, oro, plomo, mena de hierro |
| `metallurgy` | bronce, hierro forjado, carbón vegetal, coque, acero, aluminio |
| `chemistry` | sal, caliza, salitre, azufre, pólvora, nitrógeno fijado, derivados del petróleo |
| `energy` | carbón, petróleo, uranio |
| `high_tech` | silicio, tierras raras, bauxita |

Si algún recurso de §9.2 no encaja en ninguna, **dilo en el informe** en vez de
forzarlo. Una familia mal asignada se nota en el HUD.

## 4. Procedencia — se verifica

El esquema exige `provenance`. **Este proyecto ya cazó una fabricación de
citas** en el Sprint 0.3 y desde entonces la procedencia se audita.

- Si un dato viene de una fuente real, cítala con precisión suficiente para
  comprobarla.
- Si es una **decisión de diseño** nuestra (la familia, la edad de aparición),
  dilo así: procedencia «diseño del proyecto, SPEC-007 §9.2». Es una respuesta
  legítima.
- **No inventes fuentes.** Es preferible «sin fuente externa» a una cita falsa.

Las fechas históricas ya están investigadas y verificadas en SPEC-007 §19 y
§20; cita esas secciones en lugar de buscar de nuevo.

## 5. Definición de hecho

- [ ] 30 ficheros en `data/resources/`, uno por recurso.
- [ ] Todos validan contra el esquema.
- [ ] `chunsa:food`, `chunsa:wood`, `chunsa:stone` en los índices **0, 1, 2**.
- [ ] El blob compila y **dos compilaciones son byte a byte idénticas**.
- [ ] `ctest --test-dir build-gcc` **todo verde**.
- [ ] Apertura **9317** y eco **1107**, idénticos.
- [ ] `git diff --stat main..HEAD -- data/maps/ addons/ tools/` → **vacío**.
- [ ] **Commitea en la rama. NO fusiones a `main`.**
- [ ] Informe en `docs/RESULT_MINIMAX_1.8C.md` con la tabla de los 30 y
      cualquier desviación.

**No afirmes haber ejecutado nada que no hayas ejecutado.** Si no puedes
compilar el blob, dilo: es información útil, inventarla destruye el informe.
