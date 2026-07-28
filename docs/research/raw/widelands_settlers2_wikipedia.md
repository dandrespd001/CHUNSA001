# Widelands — The Settlers II (base) Wikipedia
**URLs:** https://en.wikipedia.org/wiki/Widelands · https://en.wikipedia.org/wiki/Settlers_II
**Fetched:** 2026-07-28 (WebFetch)
**Status:** parcial — widelands.org wiki bloqueado por Anubis; widelands Wikipedia y setters2 Wikipedia servir.

## Widelands (resumen)
- Clon libre de Settlers II, slow-paced RTS.
- GPL-2.0-or-later, plataforma múltiple (AmigaOS 4..Windows).
- Motor propio SDL.
- Tribus: Empire, Barbarians, Atlanteans, Frisians, Amazons.
- Repo: github.com/widelands/widelands.

## Settlers II — Motor económico (lo que Widelands replica)
- **Serfs transportan** materiales, herramientas y productos; operan los edificios.
- Aparecen **automáticamente** de la warehouse según necesidad. Jugador no controla settlers individuales.
- Cada partida empieza con **1 edificio (warehouse)** + materias primas y herramientas.
- Carreteras: jugador coloca **flags**; algoritmo encuentra la mejor ruta. Bandera = hub de transporte; un settler deja el item, el siguiente lo recoge. Más flags = más settlers por ruta.
- Distribución organizada en **6 categorías**: foodstuff, grain, iron, coal, boards, water.
- **Herramientas**: un edificio necesita un obrero con la herramienta correcta. Prioridad de fabricación por el jugador.
- Militar: cada soldado necesita espada, escudo y 1 unidad de cerveza. Monedas de oro suben el rango.
- Prioridad de transporte ajustable (qué item se mueve primero si hay varios en una flag).
- **Cita textual del diseñador Thomas Häuser**: "if you allow direct control of the military or give more detailed control about what is transported from where… it completely changes the game."
- Computer Gaming World (reseña): "winning or losing is rooted in economics".

## Implicación
- El modelo de cadenas de producción en Widelands/Settlers II es **distinto** del aldeano de AoE2:
  el jugador no controla al portador, sino la **topología** de flags y la **prioridad** de transporte.
  Esta es la separación "agencia jugador ↔ automatización" que recoge el §22.4 de SPEC-004.
