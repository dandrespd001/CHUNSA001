# Doctrina de delegación de modelos — CHUNSA (2026-07-18)

**Autor**: Arquitecto. Basada en benchmarks públicos (julio 2026) + evidencia empírica de los Sprints 0.1A/0.1B.

## Los cuatro roles

| Agente | Acceso | Para qué es MEJOR | Evitar |
|---|---|---|---|
| **Claude Arquitecto** (esta sesión, Opus 4.8) | — | Arquitectura, specs/contratos, revisión línea a línea, integración, seguridad, decisiones de determinismo, orquestación | Generación masiva (caro) |
| **Sonnet 5** (Claude, tu suscripción) | `claude --model sonnet -p "..." --dangerously-skip-permissions` (agéntico; rama propia) | **Tareas agénticas con JUICIO durante la implementación**: conectar sistemas del kernel, refactors delicados, código donde la corrección importa más que el volumen. El punto medio: mejor razonamiento que M3, más barato que Opus/Fable. Cuando M3 no razona lo suficiente y no justifica gastar al Arquitecto | Boilerplate trivial (usa M3); frontend puro (usa Kimi) |
| **Kimi K3** (Moonshot, 2.8T) | `~/.kimi-code/bin/kimi -p "..."` (agéntico; `-p` ya auto-aprueba) | **① Frontend/UI** (#1 Frontend Code Arena, 1679 Elo): render, GDScript de HUD, escenas · **② Tareas agénticas de largo horizonte multi-archivo** (Coding Index 76.2) | Micro-tareas de un archivo; **cuota limitada — se agota (visto 2×), no dársela a tareas que deban terminar sí o sí** |
| **MiniMax M3** | ① bridge MCP **v2.1** (paralelo, cap 600 s) · ② `fish -c 'claude-minimax --dangerously-skip-permissions -p "..."'` (Claude Code sobre M3 **ctx 1M**, sin cap, con web) | **① CÓDIGO EXTENSO de spec cerrada** (módulos grandes, multi-archivo) — es el más BARATO (MSA: 1/20 de coste a 1M) → darle el VOLUMEN · ② Módulos únicos (serialize/sha/Dial/visión impecables) · ③ Contexto masivo barato · ④ Investigación con navegación (BrowseComp 83.5) vía claude-minimax · ⑤ Datos masivos. Bridge = cero carga local (el más seguro para paralelo) | Razonamiento abstracto novedoso ("ejecutor competente, no gran razonador"); firmas de API en prosa (las inventa — dáselas como código); código EXTENSO por el bridge si tarda >600 s (usa claude-minimax sin cap) |

## Estrategia de reparto por consumo (equilibrar las 3 suscripciones)

**Principio: cada token en el modelo más barato que lo hace bien.** De más barato a más caro por token: **MiniMax M3 ≪ Kimi K3 < Sonnet 5 < Opus (Arquitecto)**.

- **El VOLUMEN de código → MiniMax M3** (bridge v2.1). Es 1/20 del coste; ahora optimizado para código extenso: `max_tokens=65536`, `thinking=disabled` (no gasta tokens/tiempo razonando lo que la spec ya trae → menos truncamiento, menos timeouts), `temp=1.0/top_p=0.95`. Dale módulos grandes, boilerplate, datos, cualquier cosa donde la spec cerrada + golden basten. Código extenso que pase de 600 s → por `claude-minimax` (sin cap).
- **El JUICIO de kernel → Sonnet 5.** Reservado para donde la corrección/determinismo importa más que el volumen (combate, moral, integraciones delicadas). No malgastarlo en boilerplate (eso es de M3).
- **El FRONTEND/render → Kimi K3.** Su especialidad; cuota limitada → no para lo que deba terminar sí o sí.
- **Lo indelegable → Arquitecto (Opus).** Contratos, revisión línea a línea, integración, decisiones. El más caro: solo lo que nadie más puede hacer.

**Bridge v2.1 (2026-07-21):** `MINIMAX_MCP_THINKING` (enabled/disabled), `MINIMAX_MCP_MAX_TOKENS` hasta 131072, `MINIMAX_MCP_TOP_P`. Config del proyecto: thinking disabled + 65K tokens (perfil código extenso). Requiere `/mcp` reconectar para activar.

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
