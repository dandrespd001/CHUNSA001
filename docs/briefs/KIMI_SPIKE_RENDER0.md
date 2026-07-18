# BRIEF — SPIKE-RENDER-0: decide ADR-009 con datos (Kimi K3)

Eres ingeniero Godot/C++ senior en el repo CHUNSA001. **Trabaja en la rama `kimi/spike-render0`** (créala desde main). El spike que decide la arquitectura de render 2.5D (SPEC base §1.5.3): comparar técnicas de instanciado masivo Y resolver el **orden de profundidad isométrico** — el problema real que MultiMeshInstance2D no resuelve solo.

## ⚠️ RECURSOS (el equipo se apaga por térmica): builds `nice -n 19 ... -j2`; los runs de Godot con `nice -n 19`; UNO a la vez.

## Contexto ya montado
- Adaptador funcionando: `addons/chunsa_sim/gdextension/chunsa_sim_node.{h,cpp}` (kernel 20 Hz en hilo + `SnapshotRing<DemoSnapshot>` → `_draw` con círculos). Demo en `demo/` (gl_compatibility). Editor: `./third_party_build/Godot_v4.7.1-stable_linux.x86_64` (headless para medir: `--headless`; para FPS reales usa `--quit-after N` y el tiempo total).
- El snapshot trae hasta 1024 posiciones float en tiles (mapa 256×256; pantalla = tiles × 4 px).

## Tarea: implementar y comparar 3 candidatos (el 4º queda documentado)
En la demo, un selector por variable de entorno `CHUNSA_RENDER=a|b|c` (leído en `_ready` con `OS::get_environment`):
- **(a) MultiMesh2D por bandas**: K MultiMeshInstance2D hijos (K=8 bandas horizontales por y); cada tick asignas cada unidad a la banda de su y y rellenas los buffers (`multimesh->set_instance_transform_2d`); las bandas se apilan por `z_index` para el orden vertical grueso.
- **(b) Buffers reordenados**: UN MultiMeshInstance2D; cada frame ordenas los índices por y (std::sort estable sobre copia) y escribes las transforms en ese orden (el orden de instancia ES el orden de pintado en MultiMesh 2D).
- **(c) Quads 3D con cámara ortográfica + depth**: un mundo 3D minimal — Camera3D ortográfica top-down, un MultiMeshInstance3D de quads unlit (StandardMaterial3D, shading off, alpha CUT: disabled), z de cada quad = su y (depth buffer resuelve el orden por píxel).
- (d) RenderingServer directo: NO implementar; sección del informe explicando cuándo valdría la pena.

## Escenario de corrección (además de FPS)
Añade al escenario "edificios" falsos: 20 rectángulos estáticos altos (32×96 px, posiciones deterministas de seed fija) que deben ocluir/ser ocluidos correctamente según y (una unidad "detrás" de un edificio se pinta antes). En (a)/(b) intégralos en el mismo esquema de ordenación (banda o sort conjunto); en (c) son cajas 3D. **Criterio: cruces unidad-edificio y unidad-unidad sin popping visible.**

## Medición (informe con números, no impresiones)
Para cada candidato, con 600 y 1024 unidades (sube el snapshot cap si hace falta — puedes tocar `chunsa_sim_node.*` en esta rama): corre `nice -n 19 ./third_party_build/... --path demo --quit-after 1800` midiendo el TIEMPO TOTAL de pared (frames/segundos = FPS medio; usa `time` o Performance.get_monitor(TIME_FPS) impreso cada 300 frames). También: `Performance` monitors de draw calls y video mem si accesibles. Tabla comparativa en el informe.

## Entregables (commit en la rama; NO merges a main)
1. Código de los 3 candidatos + selector.
2. `docs/SPIKE_RENDER0_INFORME.md`: tabla FPS/drawcalls/mem por candidato×unidades · evaluación de corrección de oclusión (qué se ve mal y cuándo) · complejidad de código · **recomendación razonada para ADR-009** (default para CHUNSA + qué se descarta y por qué) · límites de la medición (headless vs GPU real, hardware dev).
3. Verifica que main sigue intacto (`git status` limpio fuera de tu rama; golden del kernel verde).

Commit: "SPIKE-RENDER-0: candidatos a/b/c + informe (generado: kimi-k3, pendiente revisión Arquitecto)".
