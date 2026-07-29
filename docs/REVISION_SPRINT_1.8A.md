# Revisión del Arquitecto — 1.8A: ampliación estructural del vector de recursos

Fecha: 2026-07-29 · Rama: `arch/sprint-1.8a-resource-count`
Implementación: GPT-5.6 SOL · Contrato: `SOL_1.8A_RESOURCE_COUNT.md`
**Primer sprint bajo `METODOLOGIA_TDD.md`.**

## Veredicto

**Aprobado e integrado.** Y el protocolo TDD funcionó a la primera.

## 1. Evidencia de la fase roja — lo primero que se revisa

Doce fallos, todos de **aserción**, cada uno con esperado y obtenido:

```text
CHECK_EQ L61: esperado=32 obtenido=3 (RESOURCE_COUNT)
CHECK_EQ L62: esperado=14 obtenido=13 (SAVE_FORMAT_VERSION)
CHECK_EQ L63: esperado=9  obtenido=8  (CHECKSUM_ALGO_VERSION)
CHECK_EQ L79: esperado=4  obtenido=1  (sizeof(Mask))
...
resource_count: 12 fallos
```

Tres cosas que hacen válida esta evidencia:

1. **El target compiló y enlazó antes de la corrida**, así que son fallos de
   aserción y no de compilación (§2.1 de la metodología).
2. Usó el recurso que el brief autorizaba: declarar `RESOURCE_COUNT = 3`
   deliberadamente incorrecto para que el rojo fuese enseñable.
3. Declara explícitamente que **ninguna exigencia nueva pasó en fase roja**.
   Ése es el control que detecta pruebas sin poder, y es exactamente lo que
   faltó en los cuatro entregables que motivaron la metodología.

**No se hizo prueba de mutación**: no hace falta cuando hay fase roja legítima.

## 2. Verificación independiente

```text
BUILD=0     0 avisos
CTEST=0     100% tests passed out of 30      (eran 29; +1 resource_count)
```

Restricciones duras del contrato:

- `git diff --stat main..HEAD -- data/` → **vacío**. No tocó datos.
- `grep -rn "cost_a\|cost_b\|cost_me" addons/chunsa_sim/core/` → **sin
  resultados**.

## 3. Lo que importaba de verdad: las invariantes

| Invariante | Antes | Después | |
|---|---:|---:|---|
| apertura `end_tick` | 9317 | **9317** | idéntico |
| eco `end_tick` | 1107 | **1107** | idéntico |
| G1 `alloc_delta` | 0 | 0 | |
| golden | 1074 / 0 | 1074 / 0 | |

**Ningún `end_tick` se movió**, que era el criterio duro: un sprint estructural
que cambie comportamiento está mal hecho. Los checksums sí cambian, por el bump
V8→V9, y están re-registrados.

El informe dice «no se re-registró ningún `end_tick`; ambos están aserverados
en la suite». Comprobado: es cierto, y es la diferencia entre demostrar y
afirmar.

## 4. Omisión del contrato — mía, no suya

El brief **no pidió** los ítems de SPEC-008 que `PLAN_MAESTRO` asigna al 1.8A:
etiquetas `fast`/`slow` en ctest, esqueleto de `chunsa_perf`, y medición del
coste del checksum antes y después de ampliar el dominio.

Escribí el brief antes de que SPEC-008 existiera y luego añadí esos requisitos
al plan sin volver sobre el contrato. SOL entregó exactamente lo contratado.

**Se arrastran a un cambio aparte inmediato**, no al 1.8B: la suite está en
254 s y el reparto rápido/lento es barato y beneficia a todos los sprints
siguientes.

## 5. Lección

Es el segundo defecto de contrato que produzco: primero exigí bit-identidad
junto a un bump de checksum (imposible), ahora asigno trabajo en el plan que no
está en el brief. **Los requisitos deben entrar por el contrato o no existen**;
el plan no es ejecutable por un agente.
