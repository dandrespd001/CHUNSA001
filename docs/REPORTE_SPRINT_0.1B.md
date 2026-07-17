# REPORTE DE CIERRE — Sprint 0.1B (2026-07-17)

**Estado: COMPLETO** (con 4 desviaciones documentadas abajo, ninguna de contrato).

## Gates y entregables

| Ítem (SPEC-001 §15 / base §5.2) | Resultado |
|---|---|
| Serialización canónica + envelope + orden de carga seguro | ✅ `serialize.hpp` + `save_io.hpp` (digest SHA-256 sobre header+payload ANTES de parsear; caps duros; versiones exactas) |
| **G3** (save→load→continue ≡ ininterrumpido, sin IA) | ✅ state y continuation bit-exactos |
| **G4** (con IA viva y job en vuelo) | ✅ **matriz 8/8**: offsets 8–11 × {natural, hold-DISPATCHED}; las 8 celdas convergen al mismo estado final; **idéntico entre gcc/clang/portable** |
| **G5** (replay sin ejecutar la IA) | ✅ `ai_executions=0`, checksum reproducido |
| Fuzzing del deserializador | ✅ 90/90 (80 bit-flips + 10 truncamientos) rechazados limpiamente, 0 crashes |
| Ring de snapshots (ADR-017) | ✅ palabra de control única, stress 2 hilos × 1M publicaciones: 0 lecturas rotas |
| SHA-256 | ✅ vectores NIST FIPS 180-4 (3/3) + rechazo por manipulación de 1 bit |
| `chunsa_data_compiler` + `unit.schema.json` | ✅ perfil YAML restringido (aliases/duplicados/floats rechazados con ruta), 2 pasos, blob canónico con sha256 reproducible; fixtures 2✓/4✗ exactos |
| Ficha de coste de arte | ✅ borrador en staging (`37_FICHA_COSTE_ARTE_DRAFT.md`), pendiente verificación del Arquitecto |
| PERF-0 físico (UHD 620) | ⏸ **pendiente de hardware de referencia** — bench dev etiquetado synthetic/dev-local (p95 98µs @1000u) |

## Delegación (scorecard honesto)

**MiniMax-M3, 6 tareas de código**: `serialize.hpp` (c5a03a24 — **excelente**, simetría perfecta) · `sha256.hpp` (83b0e986 — NIST 3/3 a la primera) · `replay.hpp` (73269ade) · `ai_stub.hpp` (37992866) · `snapshot_ring.hpp` (6c97dab1 — protocolo y stress test correctos) · ficha de arte (a6f2a6a9). **Divergencia sistemática detectada**: el generador inventa una API `read_X()→bool`/`write_X()` para ByteReader/Writer aunque el prompt da la real — corregido con parches mecánicos en `ai_stub` y `replay` (lección: incluir la firma EXACTA como código en futuros prompts).

**Fallback ejercido**: `chunsa_data_compiler.py` — 2 intentos delegados muertos por timeout (M3 generó >195K chars de razonamiento) → **escrito por el Arquitecto** según la regla del goal.

**Autoría Arquitecto**: `save_io.hpp` (asiento de seguridad), `driver.hpp` (orquestación G3/G4/G5), `test_state`/`test_ring`, subcomandos CLI, `unit.schema.json` + fixtures, compilador (fallback).

## Bug de infraestructura encontrado

`timeout_seconds` de `delegate_generation_task` **no llega al servidor** (siempre aplica 600s; verificado en logs con 1200/1800 pasados). Workaround vigente: tareas dimensionadas a <600s. Investigar plomería FastMCP en la próxima sesión de mantenimiento del bridge.

## Desviaciones (ninguna de contrato; todas con destino)

1. **Zstd diferido a 0.2** — el envelope ya versiona `compression` (=0); los saves de test no lo necesitan.
2. **Replay 0.1B graba RawCommands** (la normalización del kernel es determinista y versionada); el stream `ScheduledCommand`+`effective_tick` pleno llega en 0.2 con el recorder del adaptador.
3. **AI stub lee `g` en vivo** al ejecutar (legal bajo el fixture: sin spawns/destroys en la ventana de decisión); el `AiWorldViewV1` congelado de SPEC-001 §7.1 llega en 0.2 — G4 valida la state machine y la serialización reales.
4. **Demo visual del ring → 0.2** (requiere montar Godot+adaptador; el protocolo quedó validado por el stress test de hilos).

## Plan 0.2 (SPEC-001/base)

Setup Godot 4.7.1 + godot-cpp (pins) → adaptador GDExtension + demo visual del ring → **SPIKE-RENDER-0** (decide ADR-009) → flow field jerárquico v1 → visión/LoS → schemas restantes → zstd + effective-tick replay → G5 endurecido en CI → PERF-0 físico si hay hardware.
