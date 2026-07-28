# Reporte de cierre — Sprint 1.7

Fecha: 2026-07-28 · Rama: `main` @ `44ba852`

## Qué entró

| Pieza | Contrato | Implementó | Verificación |
|---|---|---|---|
| Modelo de tareas del ciudadano | SPEC-004 §22 | SOL (K1) | ctest 29/29, ASan/UBSan |
| Checksum V8 universal | corrección K1-B | SOL | rechazado el condicional por contenido |
| Blindaje de gates | brief K1 Parte C | SOL | G1/G3/G4 dentro de ctest, baselines aserverados |
| Auto-recolección en zona aliada | SPEC-004 §23 | SOL (K2) | mutación: 5 asertos caen |
| Adaptador apertura económica | brief 1.6B | Luna | ejecución real |
| HUD: codificación y feedback | brief 1.7A | Luna | ejecución real del Director |

## Verificación final

`ctest` **29/29** sobre `main`. Build sin avisos. `.so` regenerado contra el
kernel actual (save v13, checksum v8).

Sesión del Director (2026-07-28), consola real:

```text
CHUNSA catálogo OK: ...                    ← acentos correctos
stock_A=0 → 100 → 200 → 250 → 300 → 500    ← economía viva
estado ilegal — Requiere época 4 (estás en 3); Edificio en construcción
estado ilegal — Necesitas 2 edificios completos de la época actual;
                Aún no se alcanza el tiempo mínimo; Faltan 200 B
```

Los rechazos son **reglas del juego funcionando**, ya no fallos: las techs
piden época 4 y el jugador está en la 3; el `EPOCH_UP` en el tick 789 choca
con el mínimo de 6000. Antes todos decían lo mismo sin distinguir la causa.

## El defecto que este sprint corrigió

El Director no podía construir, entrenar ni investigar **nada**. Causa raíz:
la regla de preferencia de recurso de §18 quedó sin cota de distancia, así que
los aldeanos cruzaban 100+ tiles al siguiente depósito del mismo recurso
ignorando otros a 8–20 tiles. Sin stock de `A`, todo comando con coste moría en
`ILLEGAL_STATE`.

Efecto medido de la corrección: la apertura pasa de **12292 a 9317 ticks**.

## Lecciones que quedan como norma

1. **TDD desde 1.8A** (`METODOLOGIA_TDD.md`). Nace de cuatro entregables
   seguidos con pruebas escritas después del código, que por tanto no podían
   fallar.
2. **Prueba de mutación** para todo lo anterior a TDD. Aplicada al K2: cinco
   asertos caen al reproducir el bug original.
3. **Panel multimodelo antes de cada merge de kernel** (`PROTOCOLO_PANEL.md`).
4. **Nunca cambiar de rama en un worktree compartido.** Le cambié el `.so` al
   Director bajo los pies y le costé una sesión de pruebas entera con un
   diagnóstico que además di por bueno a medias.
5. **No preguntar a un modelo si tiene una capacidad: probarla.**

## Deuda abierta

- Con 4 aldeanos y auto-asignación al más cercano, **todos van al mismo
  recurso**; la diversificación depende del jugador desde el primer minuto.
  Dato de balance para la calibración, no defecto.
- `pop_used` sigue siendo aproximado.
- Efectos de tech sobre stats (Parte III de SPEC-004).
- Niebla para la IA · PERF-0 sin hardware.
- Combate v2 con formaciones (directriz del Director, candidato a 2.4).
