# ¿Es reutilizable el core? — análisis y recomendación

Arquitecto Jefe · 2026-07-31 · a petición del Director
Pendiente de aprobación antes de construir nada.

## La pregunta

> Que se pueda reutilizar el core, y que añadir una civilización sea
> configurarlo, no rehacerlo.

## Veredicto corto

**La idea es correcta y el core YA la cumple.** No hay que rehacer la
arquitectura: hay que tapar tres fugas concretas, y sólo una es de verdad cara.

Lo importante es que esto no es una opinión — el Sprint 1.22, terminado hoy,
fue el experimento que lo mide.

## Prueba 1 — el kernel no sabe qué civilizaciones existen

Búsqueda de `egipto` y `rome:` en los ficheros del núcleo:

| Fichero | Menciones | Qué son |
|---|---|---|
| `step.hpp` | **0** | — |
| `checksum.hpp` | **0** | — |
| `game_state.hpp` | 2 | **comentarios** |
| `data_catalog.hpp` | 1 | **comentario** |
| `ai_stub.hpp` | 1 | **comentario** |

**Cero acoplamiento funcional.** Todas las apariciones son prosa explicativa.
El kernel opera sobre `CivId`, `BuildingId`, `UnitId` — índices de un catálogo
que le llega cargado. Le da igual si dentro hay dos civilizaciones o veinte.

Los únicos sitios donde un nombre propio se escribe en código son
`skirmish_apertura.hpp` (un **escenario de prueba**, que legítimamente nombra
las dos civs que conduce) y el adaptador de Godot.

## Prueba 2 — el 1.22 no tocó ni una línea del kernel

Lo entregado hoy: **8 edificios, 2 unidades, 2 tecnologías, 4 periodos
históricos nuevos, 2 civilizaciones ampliadas de 2 épocas a 5**.

Cambios en `step.hpp`, `game_state.hpp`, `data_catalog.hpp`, `checksum.hpp`:
**ninguno**. Todo fue YAML más recompilar el catálogo.

Es exactamente lo que el Director quiere que pase, y ya pasa.

## Las tres fugas

### Fuga 1 — los gemelos copiados a mano (la cara)

Hoy escribí `egipto:flint_workshop` y `rome:flint_workshop` con **los mismos
números**. Medido:

| Pareja | Líneas mecánicas distintas |
|---|---|
| `qadan_camp` vs `epigravettian_camp` | 10 de 47 |
| `flint_workshop` (eg) vs `flint_workshop` (ro) | 10 de 45 |
| `merimde_storage` vs `italic_storehouse` | 6 de 46 |

**Entre el 79 % y el 87 % de cada gemelo es idéntico**, y lo que cambia es el
nombre, la cita y algún recurso — no la mecánica.

Con 2 civilizaciones son 3 duplicados. **Con 6 civilizaciones y 15 épocas, el
mismo criterio produce cientos**, y cada corrección de balance habría que
aplicarla N veces sin que nada garantice que no se olvida una. Esta es la
rehechura que el Director teme, y es real.

Pero conviene ver dónde vive: **en la capa de datos, no en el core**. El core
no se entera.

### Fuga 2 — las pruebas fijan la FORMA del catálogo

Al crecer el catálogo hubo que editar 6 ficheros de prueba con constantes como
`building_count == 8` → `== 16`, listas de identificadores y dos hashes.

No son un defecto —esos guardianes cazaron el error de época que costó tres
bisecciones— pero **cobran peaje a cada adición de contenido**. Un peaje que se
multiplica por civilización.

### Fuga 3 — la IA es procedimental, y hoy se ha visto

La fuga más grave, y la descubrió el propio sprint: al arrancar en la época 1,
**la apertura dejó de terminar** — 36000 ticks, sin vencedor, cero
construcciones militares.

Causa: `ai_execute` **nunca emite `ADVANCE_EPOCH`**. No sabe subir de época.
Mira un catálogo militar que empieza en la 5 y no hace nada.

Es decir: **las épocas 1–4 son hoy jugables por un humano y no por la máquina.**
Y esto no se arregla con datos, porque la IA es procedimiento, no configuración.

## Recomendación

Tres movimientos, en este orden. Ninguno toca la arquitectura.

### 1. Plantillas de contenido común (ataca la fuga 1)

Un paquete `base:` con los records genéricos —campamento, taller lítico,
almacén— y que una civilización los **referencie o los especialice**, en vez de
copiarlos.

El diseño **ya previó esto**: el manifiesto declara `package_kind: base | mod`
y un `declared_variant_groups` hoy vacío, y `tech.schema.json` tiene un
`regional_variant_group` sin usar. Los ganchos existen; falta el mecanismo de
herencia en el compilador.

Esto es trabajo de **compilador y esquema**, no de kernel, y es donde está
casi todo el ahorro futuro.

### 2. Cambiar las constantes por invariantes (ataca la fuga 2)

`building_count == 16` caduca cada sprint. `test_epoch_playability`, escrito
hoy, **no caduca**: afirma que ninguna civilización queda atrapada en ninguna
época, y eso sigue siendo verdad con 2 civs o con 20.

La regla: **preferir invariantes estructurales a recuentos**. Donde el recuento
sea el guardián de verdad (la época inicial), se conserva.

### 3. Política de época para la IA (ataca la fuga 3)

Que `ai_execute` sepa subir de época, y que el **cuándo** venga del perfil de
IA en datos, no del código. Obliga a subir `AI_ALGO_VERSION` y a respetar la
regla de oro de SPEC-005 §0 (función pura de `(g, source_tick,
runtime_before)`).

Sin esto, cada época nueva que se añada será jugable a medias.

## Lo que NO recomiendo

**Un motor de reglas genérico o scripting en datos.** Sería la respuesta
ambiciosa a esta pregunta y rompería lo que sostiene el proyecto: el
determinismo bit a bit. Un intérprete dentro de `Step()` es entropía con otro
nombre. El límite entre «configuración» y «programa» debe seguir donde está.

## Decisión que te toca

1. ¿Apruebas las plantillas de contenido común como Sprint siguiente? Es el que
   más ahorra a largo plazo y el que menos se nota jugando.
2. ¿O prefieres primero la política de época de la IA, que es lo que hace que
   las épocas 1–4 recién abiertas se puedan jugar de verdad contra la máquina?

Mi recomendación: **la IA primero**. Lo que se acaba de abrir hoy está a medio
usar hasta que la máquina sepa jugarlo, y las plantillas rinden sobre todo
cuando entre la tercera civilización — que todavía no existe.
