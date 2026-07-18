# Doctrina de delegación de modelos — CHUNSA (2026-07-18)

**Autor**: Arquitecto. Basada en benchmarks públicos (julio 2026) + evidencia empírica de los Sprints 0.1A/0.1B.

## Los tres roles

| Agente | Acceso | Para qué es MEJOR | Evitar |
|---|---|---|---|
| **Claude (Arquitecto)** | esta sesión | Arquitectura, specs/contratos, revisión línea a línea, integración, seguridad, decisiones, orquestación | Generación masiva (caro en tokens) |
| **Kimi K3** (Moonshot, 2.8T) | `~/.kimi-code/bin/kimi -p "..."` (+`-y` para editar; agéntico) | **① Frontend/UI** (#1 en Frontend Code Arena, 1679 Elo): GDScript de UI/HUD, herramientas visuales, escenas · **② Tareas agénticas de largo horizonte**: implementaciones multi-archivo, refactors de repo, "monta X de punta a punta" (Coding Index 76.2, sostiene tareas largas con terminal) · ③ Cuando haga falta más músculo de razonamiento que M3 | Micro-tareas de un archivo (desperdicio); tocar `main` directamente |
| **MiniMax M3** | ① bridge MCP (fire-and-forget paralelo, **cap real 600 s**) · ② `fish -c 'claude-minimax -p "..."'` (Claude Code sobre M3 **contexto 1M**, sin cap, con herramientas web) | **① Módulos únicos bajo spec cerrada** (probado: serialize/sha/Dial impecables) · **② Contexto masivo barato** (MSA: 1/20 de cómputo a 1M — analizar corpus/logs/datos grandes) · **③ Investigación con navegación** (BrowseComp 83.5 > Opus 4.7) vía claude-minimax · ④ Lotes de datos masivos (T1) | Razonamiento abstracto novedoso ("ejecutor competente, no gran razonador"); tareas grandes por el bridge (se sobre-razona y muere a los 600 s — visto 2×) |

## Reglas operativas (aprendidas a golpes)

1. **Firmas de API como CÓDIGO literal en el prompt** — M3 inventa `read_X()→bool` sistemáticamente si se las describes en prosa.
2. **Bridge MCP solo para tareas <600 s** (bug de plomería del timeout pendiente); lo largo va por `claude-minimax` o Kimi.
3. **Agénticos jamás sobre `main`**: Kimi/claude-minimax con `-y` trabajan en una **rama o worktree dedicado**; el Arquitecto revisa el diff e integra (la revisión no se delega).
4. **Todo pasa la cascada**: compilación `-Werror` → tests/golden → revisión del Arquitecto → integración con procedencia (`generado: <modelo> · revisado: Arquitecto`).
5. **Regla de fallback**: 2 fallos → lo escribe el Arquitecto.
6. **Recursos locales**: los tres son inferencia remota (CPU local ~0) ✓; los builds que disparen los agénticos deben heredar el protocolo `nice -19 -j2`.

## Asignaciones tipo para los próximos sprints

- Adaptador GDExtension multi-archivo + demo → **Kimi** (agéntico, multi-archivo).
- HUD/UI del juego (0.3+), herramientas de visualización → **Kimi** (frontend).
- Sistemas del kernel de a un módulo (visión, combate, moral) → **M3 bridge** (spec cerrada).
- Verificación factual de fichas T3/T-B7 con fuentes web → **claude-minimax** (navegación) + veredicto final del Arquitecto.
- Análisis del corpus completo de /Investigación (17K líneas) → **claude-minimax** (1M ctx barato).
