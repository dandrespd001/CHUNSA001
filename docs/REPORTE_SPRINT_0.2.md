# REPORTE DE CIERRE — Sprint 0.2 (2026-07-21)

**Estado: COMPLETO.** El Sprint 0.2 puso a CHUNSA en pantalla, decidió la última incógnita arquitectónica (ADR-009), y dio el primer gameplay real (navegación). Repo: `github.com/dandrespd001/CHUNSA001`, main en `df21e69`.

## Entregables vs plan

| Ítem (SPEC base §5.2 / roadmap 0.2) | Resultado | Autor |
|---|---|---|
| Godot 4.7.1 + godot-cpp (pins) | ✅ editor `a13da4feb`, godot-cpp `ba0edfed`, API dumpeada y fijada | Arquitecto |
| Adaptador GDExtension + demo del ring | ✅ `ChunsaSimNode`: kernel 20 Hz en hilo + ring → render; corre en Godot | Kimi K3 + Arq. |
| **SPIKE-RENDER-0 → ADR-009** | ✅ **decidido: (c) mundo 3D ortográfico + depth buffer**; 3 candidatos medidos, informe | Kimi K3 |
| FlowField v1 (Dial determinista) | ✅ integrado con test de propiedades (3305/3305 caminos) | MiniMax M3 |
| **FlowField → MovementSystem** | ✅ unidades rodean muro por el hueco (90.5%); CommandType::FLOW_MOVE, cost_grid, save v3 | Sonnet 5 + Arq. |
| Visión / LoS v1 | ✅ bitsets visible/explored por jugador; `explored`=estado, `visible`=derivada; save v2 | MiniMax M3 + Arq. |
| Render de producción (modo c + interpolación 60 FPS) | ✅ DemoSnapshot por-slot, lerp entre ticks; movimiento fluido con sim a 20 Hz | Kimi K3 |
| zstd vendored + pin | ✅ v1.5.7 single-file, sha256 en TOOLCHAIN | claude-minimax |
| xxHash pin corregido | ✅ v0.8.3 | Arquitecto |

## Determinismo (invariante sagrado, intacto)

Todos los gates verdes tras cada merge, con los checksums evolucionando por el estado nuevo (visión, flujo): golden 1074/1074 · G1 bit-exacto (`6566b4b3`, alloc_delta=0) · G3/G4/G5 · ctest 6/6. Ninguna delegación rompió el determinismo: la cascada de revisión (contrato cerrado → golden objetivo → revisión del Arquitecto) lo garantizó. El único bug real (OOB latente del flow field) era de diseño del contrato (Arquitecto), no de implementación, y se endureció en revisión.

## El reparto de 4 agentes, validado en producción

- **Sonnet 5** (nuevo): FlowField→movimiento — kernel con juicio, contrato al 100%, cero desviaciones reales. Confirmó su nicho (punto medio razonamiento/coste).
- **Kimi K3**: SPIKE-RENDER-0, adaptador, render de producción — frontend/render, su especialidad. Cuota se agotó 2× a media tarea (mitigado con rescate del Arquitecto); esta última terminó limpia.
- **MiniMax M3**: FlowField, visión, y (0.1B) serialize/SHA/Dial — módulos de spec cerrada, impecables. Patrón recurrente: inventa APIs si van en prosa (dárselas como código).
- **claude-minimax** (M3 agéntico): vendor de zstd — tarea de infraestructura en rama, revisada.
- **Arquitecto**: contratos, revisión línea a línea, endurecimientos, integración, decisiones (ADR-009, determinismo). Doctrina completa en `docs/DELEGACION_MODELOS.md`.

## Cómo ver CHUNSA en pantalla (bajo control del Director — abre ventana gráfica)

```bash
cd ~/Imágenes/Project/CHUNSA001
./third_party_build/Godot_v4.7.1-stable_linux.x86_64 --path demo
```
600 unidades marchando con interpolación suave, orden de profundidad por depth buffer. (El Arquitecto NO lo lanzó con ventana por el historial de apagones térmicos del equipo; queda a decisión del Director.)

## Desviaciones / deuda (documentadas, no de contrato)

1. **effective-tick replay** diferido: el replay graba RawCommands (determinista, G5 verde); el stream `ScheduledCommand`+`effective_tick` pleno es refinamiento de robustez → 0.3.
2. **Interpolación, caso borde** de reciclaje de slot (muerte+spawn en el mismo slot en 50 ms): mezclaría dos unidades un frame; blindable con id de generación en el snapshot cuando el reciclaje sea frecuente (0.3).
3. **Screenshot automática**: el entorno headless no tiene display; código listo, requiere Xvfb o display real.
4. **PERF-0 físico** pendiente de la máquina UHD 620 de referencia (cifras del spike son dev-local, orientativas).
5. **Schemas restantes** (civ/epoch/tech) y **fichas T3/arte** (verificación factual): contenido de 0.3.

## Plan 0.3 (borrador)

Combate RPS (componentes hp/attack/class + triángulo de `07`) · moral · economía mínima (recursos de `24`) · schemas de datos restantes + verificación factual de fichas (claude-minimax/web) · effector-tick replay · interpolación con id de generación · selección/órdenes de jugador en la demo · PERF-0 físico si hay hardware.
