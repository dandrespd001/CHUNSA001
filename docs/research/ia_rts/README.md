# Investigación: la IA que gobierna un RTS

Carpeta **estable y versionada en git**. No es un directorio temporal: nada de
aquí se borra solo, y todo entra en el historial del repositorio.

## Por qué existe

CHUNSA tiene una IA que juega ambos bandos y que, medida con
`chunsa_bench_partida_larga`, produce siempre la misma partida: dos ejércitos
se aniquilan en la época 3, nadie investiga nunca, y el ganador termina con
20 000 de comida sin gastar.

Cinco sprints de arreglos encadenados han demostrado que el problema **no es un
fallo suelto**, y un panel de tres familias de modelos coincidió en que falta
contrajuego a la guerra temprana. Pero la última medición añadió un matiz que
ninguno anticipó: **el contrajuego existe, se construye y es demasiado débil
para importar**. Eso ya no es una pregunta de mecanismos, es una pregunta sobre
cómo decide una IA de RTS.

De ahí esta investigación: antes de seguir añadiendo piezas, entender cómo
resuelven este problema los RTS que sí funcionan.

## Qué NO es

No es una búsqueda de "la mejor IA". Es una búsqueda de **decisiones de diseño
documentadas** en juegos reales, con sus compromisos. Preferimos un dato
marcado como dudoso a uno inventado con seguridad.

## Índice

- **[SINTESIS.md](SINTESIS.md)** — léela primero. Lo que cambia y lo que no.
- [P1_ARQUITECTURA.md](P1_ARQUITECTURA.md) — repertorio de arquitecturas, capas,
  el techo documentado de los sistemas de utilidad, la IA de Age of Empires II.
- [P2_ESTRATEGIA.md](P2_ESTRATEGIA.md) — reparto eco/militar/tech, ventana de
  ataque, sumideros económicos, dificultades sin trampas.
- [P3_TACTICA.md](P3_TACTICA.md) — escaramuza, retirada, moral, y por qué la
  aniquilación mutua es estructural.
- [P4_DETERMINISMO.md](P4_DETERMINISMO.md) — lockstep, desincronización, y qué
  mide de verdad un banco de auto-juego simétrico.

Todas de MiniMax M3, 2026-08-05. Ver §6 de la síntesis sobre el límite de tener
una sola familia de fuentes.
