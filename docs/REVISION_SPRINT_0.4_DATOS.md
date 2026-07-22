# Revisión de la capa de DATOS — Sprint 0.4 (implementó: Codex/GPT · revisó: Arquitecto Claude + Opus)

Fecha: 2026-07-22 · Rango revisado: `git diff 0e460d6..HEAD` (11 commits) · main `a3ddb29`

## Alcance de lo entregado por Codex
La **mitad de datos** del Sprint 0.4, dejando el kernel data-driven como contrato cerrado (no ejecutado — decisión limpia, no metió el kernel a medias):
- `SPEC-002_DATOS_Y_SCHEMAS.md` (16 secciones): formato binario CHDB v1, contrato C++ de spawn (§8), save v7/replay v3 (§9), corrección factual (§12), gates/DoD (§13).
- 8 schemas JSON (`data/schemas/`), compilador CHDB reescrito (`chunsa_data_compiler.py`) + tests, datos YAML de Egipto/Roma, blob `chunsa_base.chdb` + content hash, gate `data_compile` en CMake.
- Corrección y promoción de las 4 fichas del slice (`REVISION_ARQUITECTO_SPRINT_0.4.md`, `CORRECCION_SPRINT_0.4.md`).
- Fix al kernel: rechazo de delays fuera de `u16` en el driver.

## Verificación objetiva (Arquitecto)
**Determinismo intacto** tras todo el trabajo: golden 1074/1074 · G1 `e35e928300ff995d` (alloc_delta=0) · G3/G4 `a93fd36d`/`b847bc32` · G5 OK (`dffa137e`, schedule_mismatches=0, replay_v=2) · **ctest 12/12** (nuevo `data_compile` incluido, 4.65s). Todos los checksums idénticos a los previos → ningún cambio alteró el comportamiento del kernel.

## Revisión del fix al kernel (Arquitecto — indelegable)
**APROBADO.** El commit `beeacb9` destapó un bug latente que yo dejé en el replay v2: los campos `uint32_t` de `DriveOpts` (`human_input_delay_ticks`/`max_future_command_ticks`) se truncaban en silencio al copiarse al `uint16_t` de `MatchConfig01A`. Un replay v2 con delay >65535 habría divergido calladamente — justo lo que el replay v2 busca prevenir. El fix rechaza el valor (`return 2`) y castea explícito, con test que cubre el borde 65535/65536. Correcto y bien alineado.

## Revisión profunda de datos (delegada a Opus — informe completo)
**Determinismo del blob: SIN P0.** El blob recompila **bit-exacto**, confirmado por 4 vías independientes: doble recompilado + `cmp`; `PYTHONHASHSEED=random` (no depende de `hash()` de Python); recompilado desde otra ruta absoluta (sin fuga de rutas/cwd/timestamps); `content_hash` reproducible `f19640cc…b759bab`. El compilador ordena claves por bytes UTF-8, usa `hashlib.sha256` (no `hash()`), recorre archivos con `sorted(...)`, rechaza floats en lexer y schema. Schemas: toda cantidad autoritativa es `integer` (cero `number`). Tests reales (reordenar→mismos bytes; mutar hp→bytes distintos), no superficiales.

## Revisión de canonicidad de fichas (Arquitecto — ADR-014)
La corrección de Codex es **legítima y rigurosa**: retiró las citas fabricadas (las 3 que la verificación había cazado), transliteraciones espurias, y corrigió cronologías, terminología militar, Songhai/Tombuctú, q'eswa/mit'a. Veredictos de promoción con límites explícitos (abstracciones `DISEÑO` no se convierten en hechos). **Ratifico la promoción de Egipto y Roma** (el slice del blob D1). *Nota menor*: Codex ajustó el localizador de Plinio a NH 33.70–76 (mi verificación decía 33.96–97) tras un segundo pase web; discrepancia de detalle no bloqueante, ambas rondan la *ruina montium*.

## Hallazgos a corregir

| # | Sev | Qué | Corrección |
|---|---|---|---|
| P1-1 | **Peso** | `verification_reports` en los YAML apuntan a `game_data/research/verificacion/*.md`, que vive **fuera del repo** (directorio padre, no trackeado). Un clon limpio no puede resolver la evidencia → cadena de procedencia rota. El compilador solo valida forma de ruta, no existencia. | Vendorizar los informes de verificación **dentro del repo** (p.ej. `docs/provenance/`) y actualizar las rutas en los YAML; endurecer el compilador para exigir existencia (`E_PROVENANCE`) relativa al `source_root`. Recompilar blob + hash + re-correr `data_compile`. |
| P2-2 | Menor | `REVISION_ARQUITECTO_SPRINT_0.4.md` listado como `verification_report` cuando es el *veredicto de promoción*, no un informe de verificación. | Separar en un campo propio (p.ej. `promotion_report`) o documentar la semántica. |
| P2-4 | Menor | `glob("*.yaml")` ignora `*.yml` en silencio. | Incluir `*.yml` o fallar ante extensiones no reconocidas. |
| P2-5 | Menor | Periodo `rome:republican_expansion` (-264..-133) declarado pero sin unidades/edificios/tech que lo referencien → injugable. | Añadir contenido mínimo o marcar el periodo como no-jugable en D1. |

## Veredicto del revisor
**Capa de datos: ACEPTABLE CON CORRECCIONES.** El determinismo (lo sagrado) es impecable y los datos/schemas/tests son sólidos. El único hallazgo de peso es P1-1 (procedencia auto-contenida en el repo), corregible sin tocar el diseño. Los P2 son menores.

## Estado del Sprint 0.4 y siguiente paso
- ✅ Capa de datos (SPEC-002, schemas, compilador, blob, fichas) — aceptada con P1-1 pendiente.
- ⏳ **Kernel data-driven** (SPAWN_UNIT por `unit_id`, save v7, replay v3) — brief cerrado listo en `docs/briefs/SONNET_KERNEL_DATOS_SPEC002.md`, NO ejecutado. Es la otra mitad del sprint.
- Recomendación: (1) corregir P1-1 (+ P2s) — delegable a Sonnet o al implementador; (2) ejecutar el kernel data-driven — delegar a Sonnet con el brief existente, ciclo completo de revisión + gates. Ambos son construcción → requieren OK del Director.
