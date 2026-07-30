# Verificación factual — aleación de bronce (Sprint 1.9)

Verificador: Arquitecto (Claude Opus 5) · Fecha: 2026-07-30
Motivo: la receta `cobre + estaño → bronce` de las fundiciones necesita
procedencia comprobable, no una cita de adorno.

## Resumen

- Afirmaciones revisadas: **2**
- **CONFIRMADAS: 1 · DISEÑO: 1**
- Veredicto: **APTA**

## Detalle

| # | Afirmación | Veredicto | Fuente |
|---|---|---|---|
| 1 | El bronce es una aleación principalmente de cobre, con en torno a un 12–12,5 % de estaño | **CONFIRMADA** | Wikipedia, *Bronze* — consultada 2026-07-30 |
| 2 | Proporción de la receta 3 cobre : 1 estaño, salida 2, duración 240 ticks | **DISEÑO** | Decisión del proyecto |

## Nota sobre la afirmación 2

La proporción del juego (**25 % de estaño**) **no** reproduce el 12,5 % histórico:
con números enteros pequeños, 7:1 sería más fiel pero obliga a manejar lotes de
ocho unidades para una sola operación, y eso convierte una decisión de partida
en aritmética. Se elige **3:1** como el entero más pequeño que conserva lo que
importa jugando —**el cobre es el grueso y el estaño el cuello de botella**— y se
deja dicho aquí que es una simplificación, no un dato histórico.

Es exactamente la clase de afirmación que en este proyecto ya se ha cazado
disfrazada de historia, así que queda separada como DISEÑO.

## Fuentes localizadas

- https://en.wikipedia.org/wiki/Bronze — consultada 2026-07-30.
