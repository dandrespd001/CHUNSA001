# Revisión del Arquitecto — 1.8B: reconciliación del esquema de recursos

Fecha: 2026-07-29 · Implementación: GPT-5.6 SOL
Contrato: `SOL_1.8B_ESQUEMA_RECURSOS.md` · Normativo: SPEC-007 §18

## Veredicto

**Aprobado e integrado.** Segunda entrega consecutiva con TDD correcto.

## 1. Fase roja

Nueve pruebas, **todas fallando por `AssertionError`**, con el estado previo
intacto («el schema, los datos, el compilador y el loader seguían en su estado
anterior»).

Entre ellas la que más me importaba y que añadí yo al contrato:
`test_resource_indices_do_not_depend_on_file_order`.

## 2. Verificación independiente

```text
BUILD=0     0 avisos
CTEST=0     100% tests passed out of 30
test_resource_indices_do_not_depend_on_file_order ... OK
```

Comprobaciones dirigidas del contrato:

- Enum de 8 letras en `common.schema.json` → **eliminado**.
- `material_costs` / `material_cost` → **no existen** en ningún esquema.
- `El` como recurso almacenable → **no aparece**.
- `resource.schema.json` → **existe**.

## 3. Las invariantes

| Invariante | Esperado | Obtenido |
|---|---:|---:|
| apertura `end_tick` | 9317 | **9317** |
| eco `end_tick` | 1107 | **1107** |
| G1 `alloc_delta` | 0 | 0 |
| golden | 1074 / 0 | 1074 / 0 |

**Ninguna se movió**, que era el criterio duro: renombrar conservando índices
no puede cambiar comportamiento.

## 4. La decisión de versionado, y está bien razonada

SOL **no subió** `SAVE_FORMAT_VERSION` (14) ni `CHECKSUM_ALGO_VERSION` (9), y
lo justificó: renombrar recursos no altera el dominio del checksum, porque los
índices son los mismos y el stream hashea índices, no nombres.

Es correcto. Y es lo contrario de lo que hizo falta en 1.8A, donde el dominio
sí crecía. Que distinga los dos casos en vez de bumpear por costumbre es buena
señal.

## 5. Lo que este sprint evita

`test_resource_indices_do_not_depend_on_file_order` protege contra un fallo que
no habría aparecido en esta máquina: el orden de `readdir` no es estable entre
sistemas de ficheros, así que un índice derivado de él haría el blob distinto
**entre equipos**. En un proyecto con checksums bit-exactos, eso son horas de
depuración persiguiendo un fantasma.

## 6. Desviación de proceso — mía

SOL **no commiteó**: dejó todo en el árbol de trabajo. Mi brief no lo pedía
explícitamente y mi lanzamiento solo decía «no hagas merge a main».

Tercera vez que un requisito vive en mi cabeza y no en el contrato. Se añade al
DoD estándar de todos los briefs siguientes: **«commitea en la rama; no
fusiones»**.
