# Doctrina de delegación de modelos — CHUNSA (actualizada 2026-07-24)

**Autor**: Arquitecto. Basada en benchmarks públicos (julio 2026) + evidencia
empírica de los sprints ejecutados. Los nombres de perfiles disponibles dependen
del runtime de cada sesión; declarar un perfil aquí no implica que haya sido
invocado.

## Pool principal Codex

| Agente | Para qué es mejor | Evitar |
|---|---|---|
| **GPT-5.6 Sol Xhigh** | Arquitectura, contratos, revisión final, integración, seguridad y determinismo | Generación masiva que pueda cerrarse con un contrato |
| **GPT-5.6 Tierra High** | Auditoría independiente, dependencias, riesgos y segunda lectura de kernel | Trabajo visual o boilerplate extenso |
| **GPT-5.6 Luna Max** | Godot, UI/HUD, adaptadores y paquetes multiarchivo de alcance cerrado | Decisiones finales de estado/save/replay |
| **GPT-5.6 Sol Low/Medium** | Inventarios, tests puros, documentación y verificación focal | Migraciones arquitectónicas no especificadas |
| **GPT-5.3 Codex-Spark** (condicional) | Microtareas rápidas: búsquedas dirigidas, reproducción localizada, diffs pequeños y tests focalizados | Arquitectura, cambios amplios o cualquier tarea que requiera asumir disponibilidad |

**Disponibilidad registrada**: GPT-5.3 Codex-Spark **no estuvo disponible para
invocación en la sesión que produjo el replan de Sprints 1.6A–1.7**. Por tanto,
no se le atribuye análisis, código, pruebas ni revisión ya ejecutados. Su uso
futuro exige comprobar primero que el runtime lo expone y registrar la
invocación real en el reporte del sprint.

## Ejecutores externos o heredados

| Agente | Uso permitido | Condición |
|---|---|---|
| **MiniMax M3** | Volumen de código con spec cerrada, helpers puros, datos y boilerplate | API oficial supervisada, allowlist mínima, revisión del manifiesto y aprobación de egress de plataforma |
| **Sonnet 5** | Integraciones de kernel delicadas cuando esté disponible por un canal autorizado | Rama/worktree acotado y revisión completa del Arquitecto |
| **Kimi K3** | Frontend/UI y cambios Godot de horizonte largo | Sólo cuando su runtime/cuota estén disponibles; no es dependencia crítica |

## Estrategia de reparto por consumo

**Principio: cada token en el perfil menos costoso que satisfaga el contrato,
sin delegar la aceptación.** Codex-Spark no entra en la planificación efectiva
hasta que el runtime exponga disponibilidad y coste.

- **Arquitectura y aceptación → Sol Xhigh.**
- **Auditoría independiente → Tierra High.**
- **Frontend/volumen acotado → Luna Max.**
- **QA y documentación → Sol Low/Medium.**
- **Volumen externo → MiniMax M3**, sólo mediante el runner seguro y si la
  plataforma aprueba el paquete exacto.
- **La MICROITERACIÓN → Codex-Spark, condicional.** Si está disponible, usarlo
  para una pregunta o diff pequeño con archivos y tests enumerados; si no está
  expuesto, reasignar sin bloquear el sprint y sin inventar una ejecución.
- **Lo indelegable → Arquitecto/Sol Xhigh.** Contratos, revisión línea a
  línea, integración y decisión final.

## Reglas operativas (aprendidas a golpes)

1. **Firmas de API como CÓDIGO literal en el prompt** — M3 inventa `read_X()→bool` sistemáticamente si se las describes en prosa.
2. **Bridge MCP solo para tareas <600 s** (bug de plomería del timeout pendiente); lo largo va por `claude-minimax` o Kimi.
3. **Agénticos jamás sobre `main`**: Kimi/claude-minimax con `-y` trabajan en una **rama o worktree dedicado**; el Arquitecto revisa el diff e integra (la revisión no se delega).
4. **Todo pasa la cascada**: compilación `-Werror` → tests/golden → revisión del Arquitecto → integración con procedencia (`generado: <modelo> · revisado: Arquitecto`).
5. **Regla de fallback**: 2 fallos → lo escribe el Arquitecto.
6. **Recursos locales**: los tres son inferencia remota (CPU local ~0) ✓; los builds que disparen los agénticos deben heredar el protocolo `nice -19 -j2`.
7. **Disponibilidad antes de atribución**: un modelo condicional solo cuenta
   como delegado si existe una invocación verificable en la sesión; una
   intención, perfil o brief preparado no es ejecución.

## Asignaciones tipo desde Sprint 1.6

- Adaptador GDExtension, demo y HUD → **Luna Max**.
- Sistemas del kernel por módulo → **Tierra High** para auditoría y
  **Luna Max/MiniMax M3** para implementación cerrada, con aceptación Sol Xhigh.
- Inventarios y matrices de cobertura → **Sol Low/Medium**.
- Paquetes externos de volumen → **MiniMax M3** sólo con allowlist y egress
  aprobados; si la plataforma deniega, reasignar localmente.
- **Sprint 1.6A**: contrato ranged/movilidad/cadencia → Arquitecto; kernel
  acotado → Tierra/Luna; búsqueda de call sites y tests unitarios pequeños →
  Sol Low o Codex-Spark si está disponible.
- **Sprint 1.6B**: recursos de mapa/compilador y helper de recolección →
  Luna/MiniMax; civ/época y command stream → Tierra + Arquitecto; Godot/UI →
  Luna.
- **Sprint 1.6C**: población, cancelaciones y persistencia → Tierra/Luna;
  datos y fixtures → MiniMax o Sol Medium; HUD → Luna; microdiffs/tests →
  Codex-Spark condicional.
- **Sprint 1.7**: contrato ATTACK/ATTACK_MOVE y orden de sistemas →
  Arquitecto; proyectiles deterministas → Tierra/Luna/MiniMax por módulo;
  input y feedback provisional → Luna.
- **Arte/audio**: no asignar antes de aceptar el cierre mecánico de Sprint 1.7.
