# SPEC-008 — Rendimiento, fiabilidad y escalabilidad

**Estado: PROPUESTA.** Autor: Arquitecto Jefe · 2026-07-28
Origen: directriz del Director — «buenas prácticas, TDD, optimización,
fiabilidad, escalabilidad».

---

## §0 Por qué existe

Ninguna spec cubre hoy estas tres propiedades. Se han tratado como consecuencia
de hacer las cosas bien, no como requisitos con número. El resultado es que
**no sabemos si estamos empeorando**: no hay presupuesto que se pueda violar,
así que no hay nada que pueda fallar.

Ya hay señales medidas de que importa:

- `ai_skirmish_apertura` consume **232 s de los ~316 s** de la suite: el 73%.
  Cada sprint pagará ese peaje varias veces.
- `[V]` 0 A.D. tuvo que **optimizar el cómputo de su hash de simulación** en la
  versión A28 porque causaba tirones. Nosotros vamos a ampliar el dominio del
  checksum en 1.8A **sin haber medido nunca cuánto cuesta**.
- `ECO_MAX_DEPOSITS` pasará de 32 a 128 con las granjas, y la búsqueda de
  depósito es lineal por ciudadano y por tick.

Esta spec convierte las tres propiedades en **contratos comprobables**, en la
misma línea que TDD: si no se puede escribir una prueba que falle, no es un
requisito, es un deseo.

---

## §1 Principio rector: medir antes de optimizar

**Prohibido optimizar sin medición previa registrada.** Una optimización sin
número antes y después no es una mejora: es un cambio de riesgo desconocido en
un kernel determinista donde cualquier reordenación puede alterar trayectorias.

Corolario: **el determinismo manda sobre el rendimiento**. Ninguna optimización
puede introducir float, reordenar iteraciones, ni hacer el resultado dependiente
del número de hilos. Ante la duda, se descarta.

---

## §2 Presupuestos de rendimiento (contratos)

### §2.1 Presupuesto por tick

| Concepto | Presupuesto | Escenario de referencia |
|---|---:|---|
| `Step()` completo | **≤ 2,0 ms** | 4 jugadores × 200 entidades, 128 depósitos |
| `state_checksum_v1` | **≤ 0,2 ms** | mismo |
| Sistema económico | ≤ 0,3 ms | 200 ciudadanos activos |
| `sh_rebuild` (hash espacial) | ≤ 0,4 ms | 800 entidades |

Justificación del número principal: la simulación corre a **20 Hz**, luego el
presupuesto duro por tick es 50 ms. Reservar 2,0 ms deja un factor 25 de margen
para presentación, sistema operativo y hardware peor que el de desarrollo.

`[V]` El caso de 0 A.D. justifica aislar el checksum: fue exactamente ahí donde
les dolió.

### §2.2 Presupuesto de la suite

| Concepto | Presupuesto |
|---|---:|
| Suite de PR (`ctest -L fast`) | **≤ 60 s** |
| Suite completa (merge/nightly) | ≤ 600 s |
| Ninguna prueba individual en la suite rápida | ≤ 10 s |

**Acción exigida en 1.8A**: etiquetar `ai_skirmish_apertura` y
`ai_skirmish_eco` como `integration/slow` y excluirlas de la suite rápida. Una
suite de 5 minutos en cada PR es una suite que la gente deja de ejecutar, y una
suite que no se ejecuta no protege nada.

### §2.3 Cómo se miden

Un binario `chunsa_perf` que ejecute los escenarios de referencia y emita
tiempos en formato estable. Se ejecuta **manualmente y en nightly**, nunca como
gate de PR: el tiempo de pared es ruidoso y un gate ruidoso se acaba ignorando.

**Regla anti-ruido**: se compara la **mediana de 5 corridas** contra el
presupuesto. Una regresión se declara solo si supera el presupuesto **dos
noches seguidas**.

---

## §3 Fiabilidad

### §3.1 Qué significa aquí

Que el juego **falle de forma ruidosa y reproducible**, nunca en silencio. Un
kernel determinista tiene una ventaja enorme y hay que explotarla: cualquier
fallo es reproducible por replay.

### §3.2 Contratos

| # | Contrato | Cómo se prueba |
|---|---|---|
| R1 | Un save corrupto **nunca** produce un estado silenciosamente incorrecto: o carga íntegro o falla con código | Fuzzing del envelope; ya existe, se amplía a los campos nuevos de cada sprint |
| R2 | Un replay que diverge **se detecta**, no continúa | Comparación de checksum por tick; `schedule_mismatches` |
| R3 | Ningún dato de catálogo malformado llega al kernel | Validación en el loader; ya cazó un depósito fuera de cota (P1 de auditoría) |
| R4 | Ningún índice de recurso, depósito o entidad se escribe sin cota | Prueba explícita por cada límite: `RESOURCE_COUNT`, `ECO_MAX_DEPOSITS`, `MAX_PER_TICK` |
| R5 | `FatalReason` se propaga siempre; nunca se descarta un fatal real | Auditoría de los `local_fatal` descartados a propósito |

**R4 tiene historia**: el desbordamiento de heap del Sprint 1.6B era
exactamente esto — el loader admitía 4096 comandos y el consumidor
dimensionaba para 72. **Por cada constante de capacidad debe existir una prueba
que la ponga a prueba en el límite y uno más allá.**

### §3.3 Degradación

Cuando algo excede un límite, el orden de preferencia es:

1. **Rechazar limpio** con código de error (comandos, datos).
2. **Acotar** de forma determinista y documentada (clamps defensivos, como el
   de `gs_init_economy_from_catalog`).
3. **Fatal explícito** si continuar corrompería el estado.

**Nunca**: truncar en silencio, saltar el elemento sin avisar, o continuar con
un valor por defecto que altere la trayectoria.

---

## §4 Escalabilidad

### §4.1 Límites declarados

Un límite sin número no es un límite. Estos son los objetivos de v1.0:

| Dimensión | Hoy | Objetivo v1.0 | Nota |
|---|---:|---:|---|
| Jugadores | 2 | **8** | `MAX_EMITTERS` debe cubrirlo |
| Entidades vivas | ~64 | **1600** (8 × 200) | `ENTITY_HARD_CAP` |
| Depósitos | 12 (cap 32) | **128** | granjas (SPEC-007 §5) |
| Recursos | 3 | **32** | `RESOURCE_COUNT` (SPEC-007 §9.3) |
| Comandos por tick | 4096 | 4096 | `MAX_PER_TICK`, ya suficiente |

### §4.2 Complejidad algorítmica: dónde duele

| Sistema | Complejidad hoy | A escala v1.0 | Veredicto |
|---|---|---|---|
| Búsqueda de depósito | O(ciudadanos × depósitos) | 200 × 128 = 25 600/tick | **Aceptable**, vigilar |
| Zona aliada | O(edificios × depósitos), **1 vez/tick** | ~50 × 128 = 6 400 | Bien: SOL lo precalculó |
| Checksum | O(entidades + depósitos + recursos) | crece con `RESOURCE_COUNT` | **Medir en 1.8A** |
| Combate/aggro | O(entidades) con hash espacial | 1600 | Bien |
| Dropoff por edificio | O(edificios) por entrega | aceptable | |

**El patrón que salvó la zona aliada debe generalizarse**: cuando algo se
consulta por entidad y por tick pero solo cambia por tick, **se precalcula una
vez**. SOL lo hizo bien sin que el contrato se lo pidiera.

### §4.3 Lo que NO se hace

- **No** se paraleliza `Step()`. El determinismo con hilos exige orden
  garantizado, y el coste de equivocarse es una desincronización silenciosa.
  Si algún día hace falta, será con contrato propio y gate específico.
- **No** se cambia SoA por AoS ni al revés sin medición previa (§1).
- **No** se introducen cachés con invalidación implícita: una caché mal
  invalidada en un kernel determinista es un desastre reproducible.

---

## §5 Criterios de aceptación (formato TDD)

Comprobables, y por tanto capaces de fallar. Esta lista **es** la lista de
pruebas.

**Rendimiento**
1. `chunsa_perf` existe y emite tiempos en formato estable para los escenarios
   de §2.1.
2. La suite rápida (`ctest -L fast`) termina en **≤ 60 s**.
3. `ai_skirmish_apertura` **no** está en la suite rápida.

**Fiabilidad**
4. Escribir en `RESOURCE_COUNT` y en `RESOURCE_COUNT - 1` se comporta como
   dicta §3.3: el primero se rechaza o acota, el segundo funciona.
5. Ídem para `ECO_MAX_DEPOSITS` y `MAX_PER_TICK`.
6. Un save con un byte alterado en cada región del envelope **falla al cargar**,
   nunca carga un estado distinto en silencio.

**Escalabilidad**
7. Un escenario de 8 jugadores × 200 entidades **completa un tick sin fatal**.
8. Con 128 depósitos, la búsqueda económica sigue siendo determinista y el
   escenario reproduce checksum idéntico en dos corridas.

---

## §6 Aplicación por sprint

| Sprint | Qué de esta spec |
|---|---|
| **1.8A** | Etiquetas `fast`/`slow` en ctest (§2.2) · esqueleto de `chunsa_perf` · **medir el coste del checksum antes y después** de ampliar el dominio · criterios 4 y 5 sobre `RESOURCE_COUNT` |
| 1.9–1.12 | Cada sprint añade la prueba de límite (§3.2 R4) de la constante que introduce |
| 1.13 | Escenario de escala (criterio 7) junto al cierre mecánico |
| Fase 4 | PERF-2 en hardware real; ADR-011 deja de ser objetivo y pasa a contrato |

**No se pide optimizar nada todavía.** Se pide **poder saber** si empeoramos.
Optimizar sin ese instrumento es exactamente lo que §1 prohíbe.
