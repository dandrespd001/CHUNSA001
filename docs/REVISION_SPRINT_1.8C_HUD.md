# Revisión del Arquitecto — 1.8C-UI: HUD de recursos por familias

Fecha: 2026-07-30 · Implementación: GPT-5.6 Luna Max
Contrato: `LUNA_1.8C_HUD_FAMILIAS.md` · Normativo: SPEC-006 Parte III

## Veredicto

**Aprobado tras corregir un artefacto obsoleto.** El HUD es correcto; lo que
fallaba era un `.chdb` desincronizado.

## 1. Lo que la verificación visual encontró y la headless no

Luna ejecutó **en headless**. La consola salía limpia, con `catálogo` bien
acentuado. Pero el HUD se dibuja en el viewport, y headless **no lo verifica**.

Capturé el viewport con `CHUNSA_SHOT` y lo mirié. Dos defectos visibles:

1. **Ninguna familia en pantalla**, pese al texto «Pulsa una familia para ver
   sus recursos individuales».
2. Los depósitos del mapa etiquetados **«Recurso desconocido 500»**.

## 2. Causa raíz: el CHDB del demo, obsoleto

```text
demo/chunsa_base.chdb          36 999 bytes   (anterior a los 30 recursos)
data/compiled/chunsa_base.chdb 57 690 bytes   (con la tabla de recursos)
```

El demo cargaba un catálogo **sin tabla de recursos**, así que
`catalog.resource_count` era 0 y `resource_display_name()` caía en su rama de
fallback. El código del HUD estaba bien desde el principio.

Sincronizado el fichero, el HUD funciona:

```text
CHUNSA · RECURSOS · Época 3 · Población 0/200
[+] Subsistencia (1)  · más escaso: Comida 0
[+] Construcción (3)  · más escaso: Arcilla 0
[+] Metales base (3)  · más escaso: Cobre 0
[+] Química (1)       · más escaso: Sal 0
```

Y los depósitos pasan a «Comida 500» y «Piedra 300».

## 3. El filtrado por edad es correcto — comprobado a mano

En la época 3 existen: comida (e1), madera (e1), piedra (e1), arcilla (e2),
cobre (e3), oro (e3), plomo (e3) y sal (e3).

Reparto esperado: Subsistencia 1 · Construcción 3 · Metales base 3 · Química 1.
**Es exactamente lo que muestra.** Ninguna familia de edades posteriores
aparece, que era el criterio 2 de SPEC-006 §16.

Los demás criterios verificados en pantalla: familia colapsada muestra su
recurso más escaso (§16.3), nombres en español sin mojibake (§16.5 y lección
del 1.7A), y no desborda a 1920×1080 (§16.6).

## 4. Segunda vez que un artefacto compilado obsoleto produce un bug visible

La primera fue el `.so` sin el adaptador 1.6B, que **costó al Director una
sesión de pruebas completa** y un diagnóstico mío parcialmente equivocado.

`demo/` contiene dos artefactos compilados versionados —`.so` y `.chdb`— y
ninguna prueba comprobaba su frescura. Añadido:

```cmake
add_test(NAME demo_chdb_sincronizado
         COMMAND ${CMAKE_COMMAND} -E compare_files
                 demo/chunsa_base.chdb data/compiled/chunsa_base.chdb)
```

Etiquetado `fast`, así que entra en el ciclo de desarrollo. `ctest` pasa a
**32/32**.

## 5. Desviación de contrato — mía, la sexta

Mi brief pedía regenerar el `.so` y **no mencionaba el `.chdb`**. Luna hizo lo
contratado.

El patrón se repite con una variante nueva: no solo escribo restricciones sin
verificarlas, también **enumero artefactos incompletos**. La prueba de §4 es la
respuesta estructural: en vez de recordarlo en cada brief, que lo compruebe la
suite.

## 6. Deuda menor observada

Las etiquetas de edificios y depósitos **se solapan** en el centro del mapa
(«Establo de carros» sobre «Centro de asentamiento»). Es anterior a este sprint
y es legibilidad, no corrección. Candidato al bloque de arte y feel posterior
al 1.13.
