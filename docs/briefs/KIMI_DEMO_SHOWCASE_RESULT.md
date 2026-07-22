# RESULT — Demo showcase Sprint 0.3 (Kimi K3 + Arquitecto)

**Nota de proceso**: el proceso de Kimi fue interrumpido externamente (`killed`, no un fallo propio) antes de llegar a la verificación/commit. El working tree quedó en estado seguro (rama `kimi/demo-showcase` creada, sin commits, cambios sin comitear en los 2 archivos del adaptador). El Arquitecto retomó desde ahí: revisó el código ya escrito (prácticamente completo), lo verificó de forma independiente, y cierra el ciclo.

## Qué había hecho Kimi (contrato cumplido casi al 100%)

- `DemoSnapshot` ampliado con `owner[1024]`/`unit_class[1024]`/`fleeing[1024]`.
- `build_showcase_batch`: 40% caballería (owner 0), 40% artillería (owner 1), 20% ciudadanos (owner 0) de `demo_units`; spawns dispersos con `rng_range`; `MOVE_TO` en t=1 de ambos ejércitos hacia (128,128).
- `sim_loop`: rellena los 3 arrays nuevos del snapshot; imprime diagnóstico cada 100 ticks (`cav/art/citizens/stock_A`).
- `render_interpolated`: color por bando+pánico ya implementado exactamente como el contrato (ciudadano=amarillo, pánico=blanco, owner0=azul, owner1=rojo), `set_use_colors(true)` ya activo.

**Desviación documentada por Kimi en el propio código**: ciudadanos con `speed_mtpt=800` en vez de los 200 sugeridos en el brief (converge más rápido para una demo corta — razonable, sin impacto en el kernel).

## Verificación (hecha por el Arquitecto, Kimi no llegó a este paso)

1. Build (`nice -19 -j2`): **limpio a la primera**, sin tocar nada.
2. Headless 2000 ticks:
   ```
   CHUNSA cav=239 art=227 citizens=120 stock_A=0      (tick 100)
   CHUNSA cav=239 art=227 citizens=120 stock_A=500     (tick 200)
   ```
   **Combate real**: artillería 240→227 (13 bajas) vs caballería 240→239 (1 baja) — confirma en vivo la ventaja RPS de caballería (+30% vs artillería) ya probada en `test_combat`. Se estabiliza tras el primer choque (los supervivientes quedan fuera de rango — esperado con una sola orden de movimiento, no es un bug).
   **Economía real**: `stock_A` llega a 500 (depósito agotado y entregado íntegro), igual que en `test_economy`.
   Sin errores ni crashes.
3. Kernel intacto: `addons/chunsa_sim/core/` sin tocar (confirmado por diff); golden 1074/1074, ctest 9/9.

## Veredicto del Arquitecto

**ACEPTADO sin cambios de código** — el trabajo de Kimi estaba correcto y completo salvo el paso final de verificación/commit, que hizo el Arquitecto por la interrupción externa del proceso.
