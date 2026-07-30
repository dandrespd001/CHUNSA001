# Revisión del Sprint 1.8E — Costes visibles y construcción

**Implementación:** GPT-5.6 Luna Max · **Revisión:** Claude Opus 5 (Arquitecto)
**Contrato:** `SPEC-006` Parte IV §17–§21 · brief
`docs/briefs/LUNA_1.8E_UI_COSTES_Y_CONSTRUCCION.md`
**Veredicto:** **aceptado con dos salvedades**, una corregida aquí y otra
registrada como deuda.

---

## 1. Cómo terminó la entrega

Luna **no llegó a cerrar el sprint**: la sesión se cortó con
`Selected model is at capacity` tras **677 237 tokens**, sin comitear y sin
escribir `docs/RESULT_LUNA_1.8E.md`.

El código estaba **íntegro en el árbol de trabajo** y compilaba. Se comiteó
(`124c6fe`) antes de tocar nada más: 464 líneas sin comitear con un modelo
caído es una pérdida esperando a ocurrir.

**Consecuencia metodológica:** no hay evidencia de **fase roja**, que el brief
exigía. No se puede reconstruir a posteriori. Se sustituyó por **prueba de
mutación**, que es el procedimiento que `METODOLOGIA_TDD.md` fija para código
que llega sin TDD:

> Mutación aplicada a `assess_affordability`: `stock >= required` → `stock > required`.
> Resultado: **4 fallos de aserción** («coste justo es asequible», «coste justo
> no tiene faltantes», «un solo recurso faltante», «se enumeran varios
> faltantes»). La prueba **discrimina**; no es decorativa.

---

## 2. Los tres hallazgos del Director

| Reportado | Estado |
|---|---|
| «No se muestra el coste hasta realizar la acción» | **Resuelto** |
| «Los edificios aparecen por arte de magia» | **Resuelto** |
| «No están implementados diversos edificios» | **Resuelto** |

Verificado en **captura real a 1920×1080**, no en headless:

- **Catálogo de construcción con DOS edificios** en época 3, no uno. El filtro
  por civilización y `epoch_window` funciona. Que sean dos y no cuatro es lo
  **correcto**: los romanos son de época 5, y el propio panel lo dice —
  «Bloqueados 2 · Requiere época 5». El requisito no económico se ve **antes**
  de intentar la acción (criterio 4).
- **Costes por recurso y con nombre real antes de actuar**: «30 de Piedra ·
  60 de Madera», con el tiempo de obra («obra 600 t»).
- **Faltante en rojo**: «Faltan: 60 Madera · 30 Piedra». Lo mismo en SUBIR
  ÉPOCA.
- **Constructores**: `PLACE_BUILDING` encola `ASSIGN_BUILD` para los ciudadanos
  seleccionados o, si no hay selección, para los ociosos más cercanos
  (`mode=selected` / `mode=idle_nearest`, con traza en consola). Hay contador
  «Aldeanos · construyendo N · ociosos N» y etiqueta **«SIN CONSTRUCTOR: no
  avanza»** sobre la obra parada.

`affordability_view.hpp` sigue el patrón pedido —cabecera de política pura, sin
tipos de Godot, con pruebas en `tests/unit/`— igual que `fog_view` y
`outcome_view`. Es el segundo pago de la deuda del adaptador sin suite.

---

## 3. Salvedad corregida en esta revisión: el texto se cortaba

El **criterio 10** («el HUD de costes no desborda a 1920×1080») **fallaba**. En
la captura de Luna se lee:

```
Faltan 200 de Comida · Faltan 200 de Madera · Falt
```

Dos causas, ambas arregladas:

1. `missing_summary` repetía «Faltan … de …» por cada recurso, generando una
   línea que no cabe en un panel de 330 px.
2. Había truncados duros `.left(50)` y `.left(43)` que **cortan a media
   palabra**.

Ahora es «**Faltan: 200 Comida · 200 Madera · 100 Piedra**», completo y sin
truncar. Verificado en captura nueva.

Un texto cortado a medias es exactamente el hueco invisible que el criterio de
diseño del sprint quería eliminar: *el jugador no debe aprender el juego por
rechazos*, y mucho menos por rechazos ilegibles.

---

## 4. Salvedad NO corregida: las etiquetas del centro se solapan

En el centro del mapa se amontonan hasta cinco textos en el mismo sitio —
nombres de edificio, nombres de depósito y la línea de obra— y el resultado es
ilegible:

```
Establo de carros / Centro de asentamiento / Comida 1500 / Piedra 600 / SIN CONSTRUCT…
```

**No es un defecto de este sprint**: ya estaba registrado como deuda antes de
lanzarlo. Pero el 1.8E **lo empeora**, porque añade una línea más por edificio
en construcción.

**No se arregla aquí a propósito.** Un apaño (acortar cadenas, quitar
etiquetas) esconde el problema sin resolverlo. Lo que hace falta es una pasada
de **descolisión de etiquetas**: al dibujar, descartar la etiqueta cuya
posición caiga a menos de N píxeles de otra ya dibujada en ese frame, con
prioridad estable —selección > obra en curso > edificio > depósito— para que el
resultado sea determinista y no parpadee entre frames.

Registrado como **1.8G** en el plan maestro.

---

## 5. Verificación ejecutada

| Comprobación | Resultado |
|---|---|
| Compilación del kernel y del adaptador | limpia |
| `ctest -L fast` | **31/31** (era 30; +`affordability_view`) |
| `ctest` completo | verde |
| `git diff -- addons/chunsa_sim/core/ data/ tools/` | **vacío** (el brief lo prohibía) |
| Trayectorias | **sin tocar**: este sprint no entra en `Step()` |
| Godot ejecutado de verdad | sí, con consola y **dos capturas miradas** |
| Mutación de `assess_affordability` | detectada, 4 fallos |

---

## 6. Lo que queda abierto

1. **1.8G**: descolisión de etiquetas en el mundo (§4).
2. **Sin `RESULT_LUNA_1.8E.md`**: este documento lo sustituye.
3. **«Población 0/200»** con unidades vivas en pantalla: aparece en ambas
   capturas. No se investigó en este sprint; puede ser correcto (los aldeanos
   quizá no cuenten como población) o un contador mal cableado. **Pendiente de
   confirmar**, no de arreglar a ciegas.
