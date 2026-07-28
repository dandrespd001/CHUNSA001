# Brief 1.8A — Ampliación estructural del vector de recursos

**Modelo:** GPT-5.6 SOL (`codex -m gpt-5.6-sol`)
**Rama base:** `main` @ `45c8fb4` · **Rama de trabajo:** `arch/sprint-1.8a-resource-count`
**Contratos:** `docs/specs/SPEC-007_RECURSOS_Y_EDADES.md` §9.3 y §11 ·
`docs/specs/CONCORDANCIA_SPEC-007.md` filas **C1, C2, C3, C4, C8**

---

## 0. Reglas innegociables

1. Determinismo bit-exacto: cero float/double, cero reloj, cero entropía fuera
   de `RngStream`. Recorrido ascendente, desempate por índice bajo.
2. `Step()` sin heap ni STL.
3. `movement_v1` CONGELADO.
4. `CommandType` append-only.
5. `GameState` en heap en los tests (`std::make_unique`).
6. Nada de `assert()` para validar datos.

---

## 1. Qué es este sprint, y sobre todo qué NO es

Es **puramente estructural**. Amplía la capacidad del modelo de recursos sin
usarla todavía.

**Lo que SÍ:**

- `RESOURCE_COUNT = 32` como constante con nombre.
- `player_stock[MAX_EMITTERS][3]` → `[RESOURCE_COUNT]`.
- Los vectores de coste de `BuildingDefinitionV1`, `UnitDefinitionV1` y
  `TechDefinitionV1` pasan de `cost_a/cost_b/cost_me` a un vector de longitud
  `RESOURCE_COUNT` (concordancia C1, C3, C4).
- `dropoff_mask` de `uint8_t` a `uint32_t` (32 bits, alineado con
  `RESOURCE_COUNT` — no es casualidad, ver SPEC-007 §9.3).
- Toda comparación `< 3u` sobre índices de recurso pasa a `< RESOURCE_COUNT`.
- `SAVE_FORMAT_VERSION` 13 → 14; `CHECKSUM_ALGO_VERSION` 8 → 9, etiqueta
  `CHUNSA_STATE_V9`. **Universal, sin condicionales por contenido** — ese error
  ya se cometió y se corrigió en el K1-B; no se repite.

**Lo que NO, bajo ningún concepto:**

- **No** renombrar `A`/`B`/`Me`. Los índices 0, 1 y 2 siguen significando
  exactamente lo mismo; 3..31 quedan a **cero**.
- **No** tocar ningún fichero de `data/`.
- **No** añadir recursos al mapa.
- **No** tocar recetas, energía, upkeep, granjas ni recuperación. Eso es 1.9
  en adelante.
- **No** tocar `addons/chunsa_sim/gdextension/`.

---

## 2. Criterio de éxito, y es exigente

**El juego debe comportarse EXACTAMENTE igual.** Lo único que puede cambiar en
todo el repositorio son los **valores de checksum**, y cambian por el bump de
algoritmo, no por comportamiento.

| Invariante | Antes | Debe seguir |
|---|---|---|
| G1 `alloc_delta` | 0 | 0 |
| apertura `winner` | 1 | 1 |
| apertura `end_tick` | 9317 | **9317 exacto** |
| `ai_skirmish_eco` `end_tick` | 1107 | **1107 exacto** |
| Vectores dorados | 1074 / 0 fallos | idéntico |

**Si un `end_tick` se mueve un solo tick, PARA y repórtalo.** Significaría que
la ampliación cambió comportamiento, que es justo lo que este sprint promete no
hacer. No lo "arregles" re-registrando el baseline.

Los checksums sí cambian (bump V8→V9): re-regístralos **todos** y di en el
commit que el cambio se debe al bump y no a la trayectoria — con los
`end_tick` idénticos como prueba.

---

## 3. Protocolo TDD — obligatorio

`docs/METODOLOGIA_TDD.md` es vigente desde este sprint. Léelo.

**Este sprint tiene una particularidad** (§8 de esa metodología): al ser una
migración estructural sin comportamiento nuevo, no hay una prueba de
funcionalidad que escribir en rojo. Lo que **sí** se escribe primero es la
prueba de que **la capacidad nueva existe y es correcta**:

1. **Escribe primero** `tests/unit/test_resource_count.cpp` con, como mínimo:
   - `RESOURCE_COUNT == 32`.
   - Escribir y leer el índice **31** de `player_stock` conserva el valor.
   - `dropoff_mask` acepta y distingue el bit **31**.
   - Un coste declarado en el índice 31 se deduce correctamente del stock.
   - Save/load conserva los 32 índices, incluidos los que valen cero.
   - Mutar el índice 31 **cambia** el checksum (prueba de que entra al dominio,
     igual que hiciste con `citizen_task` en el K1 — esa prueba estuvo bien).
2. **Compila y ejecuta ANTES de implementar.** Deben fallar por **aserción**,
   no por compilación. Si no compilan porque `RESOURCE_COUNT` no existe,
   declara la constante con un valor **incorrecto a propósito** (por ejemplo 3)
   para que el fallo sea de aserción y puedas enseñar el rojo.
3. **Pega la salida roja en el informe.**
4. Implementa. Pega la salida verde.

**Un informe sin salida roja se rechaza sin revisar el código.** Si alguna
prueba pasa en fase roja, dilo: significa que no prueba nada y hay que
rehacerla. Decirlo es exactamente lo que se espera.

---

## 4. Definición de hecho

- [ ] Salida **roja** pegada, con fallos de aserción.
- [ ] Salida **verde** pegada.
- [ ] `cmake --build build-gcc -j8` limpio, sin avisos nuevos.
- [ ] `ctest --test-dir build-gcc --output-on-failure` todo verde.
- [ ] Los cuatro `end_tick`/invariantes de §2 **idénticos**.
- [ ] Checksums re-registrados con justificación de que es el bump.
- [ ] `grep -rn "cost_a\|cost_b\|cost_me" addons/` sin resultados en el kernel.
- [ ] `git diff --stat main..HEAD -- data/` **vacío**.
- [ ] ASan y UBSan verdes sobre los binarios tocados; salida pegada.
- [ ] Informe en `docs/RESULT_SOL_1.8A.md`.

**Sin pipes que enmascaren el código de salida**: `cmd; echo $?`.

**No afirmes haber ejecutado nada que no hayas ejecutado.**

**No hagas merge a `main`.**
