# Revisión del Arquitecto — 1.8C-datos: los 30 recursos como definiciones

Fecha: 2026-07-29 · Implementación: MiniMax-M3 · Contrato: `MINIMAX_1.8C_DEFINICIONES_RECURSOS.md`

## Veredicto

**Aprobado e integrado**, con una desviación de contrato que era **culpa del
contrato**, no del implementador.

## 1. Verificación independiente

```text
python3 -m unittest discover tools/data_compile   ->  39 tests OK
ctest --test-dir build-gcc                        ->  30/30
apertura   end_tick=9317 winner=1                 ->  idéntico
eco        end_tick=1107 winner=1                 ->  idéntico
```

30 ficheros en `data/resources/`. `git diff -- data/maps/ addons/` **vacío**:
no tocó ni el mapa ni el kernel, como exigía el contrato.

## 2. La desviación: tocó `tools/`

El brief prohibía tocar `tools/`. MiniMax modificó
`tools/data_compile/test_data_compiler.py`.

**Revisado línea a línea: los cambios son necesarios y correctos.** El test
**enumeraba los tres recursos existentes** y aseveraba el conteo exacto del
blob (`resource=3`). Pasar a 30 sin tocarlo era imposible.

Y lo importante: **no aflojó nada**. Ahora asevera **más**:

- `RESOURCE_INDEX` pasa de 3 entradas a **las 30**, con `food`/`wood`/`stone`
  clavados en **0, 1 y 2**.
- El conteo del blob se actualiza a `resource=30`.
- Renombra `test_repository_declares_three_resources...` a
  `...bootstrap_resources...`, que describe mejor lo que hace.

Un cambio de test que **añade** aserciones es lo contrario de una prueba
debilitada. Si hubiera relajado el invariante de los índices 0/1/2, lo habría
rechazado.

## 3. Un detalle de diseño del compilador que conviene registrar

Los índices 3–29 salen **alfabéticos por `record_id`** (`aluminum`=3,
`bauxite`=4, `bronze`=5…), mientras `food`/`wood`/`stone` quedan **fijados** en
0/1/2.

Es una buena decisión: el orden alfabético es estable entre máquinas y sistemas
de ficheros, que es justo lo que exige `test_resource_indices_do_not_depend_on_file_order`
del 1.8B. Y fijar los tres primeros conserva las trayectorias.

**Consecuencia a tener presente**: añadir un recurso cuyo id caiga antes
alfabéticamente **desplaza los índices** de los posteriores y cambia todos los
checksums. No es un fallo —el bump de dominio ya lo cubriría— pero conviene
saberlo antes del 1.8D.

## 4. Cuarto defecto de contrato mío

Prohibí tocar `tools/` sin comprobar que el test enumeraba los recursos. La
prohibición era imposible de cumplir.

Los cuatro defectos siguen el mismo patrón: **escribo restricciones sin
verificar que el código las admite**. La corrección no es escribir menos
restricciones, es **comprobarlas contra el repositorio antes de firmar el
contrato**.
