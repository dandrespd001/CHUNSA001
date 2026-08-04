# Plan de desatasco — la partida larga no termina

Arquitecto Jefe · 2026-08-04 · tras la medición del Sprint 1.40

---

## §1 Lo que mide el banco, sin interpretar

| | |
|---|---|
| Último cambio de estado | tick **18 012** |
| Del tick 24 000 al 120 000 | **filas idénticas** |
| Época máxima | p0 = **3**, p1 = **4** |
| Granjas pedidas | **0** |
| `EPOCH_UP` rechazados | 421 (p0) · 739 (p1) |
| Partida terminada | **no**, en 120 000 ticks |

## §2 Las cuatro causas están ENCADENADAS

Ése es el hallazgo que decide el orden. No son cuatro fallos sueltos:

```
El mapa se agota
      ↓
Los aldeanos quedan OCIOSOS y nadie los redirige      ← causa (2)
      ↓
La caja deja de crecer... pero tampoco se GASTA
      ↓
El outlook de comida nunca baja del umbral            ← causa (3)
      ↓
No se piden granjas → la comida no se renueva
      ↓
No hay con qué pagar el segundo edificio de época
      ↓
ADVANCE_EPOCH rechaza para siempre                    ← causa (1)
      ↓
Nadie muere porque nadie ataca en pánico              ← causa (4)
      ↓
La partida no termina nunca
```

**Atacar la (1) primero sería tratar el síntoma.** La puerta de época funciona
exactamente como está diseñada; lo que falla es que la economía muere antes de
poder pagarle.

## §3 Orden de arreglo, y por qué

### Sprint A — Los aldeanos ociosos vuelven al trabajo · **el más grave**

Hoy la capa económica sólo emite `GATHER` cuando un recurso baja de 75. Cuando
los depósitos cercanos se agotan, los aldeanos pasan a ocioso y **ya nadie los
vuelve a mirar**, con recursos en el suelo a medias.

Es el fallo más elemental de los cuatro y el que más desbloquea: **un aldeano
parado con recursos disponibles es un fallo visible para cualquiera que mire la
pantalla**, no una sutileza de balance.

**DoD**: en el banco, los aldeanos no quedan ociosos mientras haya depósito
alcanzable con existencias.

### Sprint B — El gatillo de la granja mira la ESCASEZ, no la caja

El outlook del Sprint 1.34 es `caja + depósitos alcanzables`. La idea era
buena y el borde no se previó: **si la caja no se gasta, el outlook no baja
jamás**, y el sistema de comida renovable queda inalcanzable justo donde hace
falta.

El arreglo no es bajar el umbral: es que el disparo mire **la comida en el
suelo**, que es lo que de verdad se agota, y no lo embolsado.

**DoD**: en el banco se piden granjas antes del tick 20 000.

### Sprint C — Toda época debe tener DOS edificios pagables

`test_epoch_playability` ya exige dos edificios por época. Lo que **no**
comprueba es que se puedan **pagar** con lo que hay. La época 3 los tiene sobre
el papel y en la práctica sólo cuenta el centro.

Es trabajo de datos y de guardián, no de kernel.

**DoD**: el guardián comprueba también asequibilidad; el banco llega al menos a
la época 8.

### Sprint D — El pánico permanente

Un ejército que llega a la base enemiga y se queda huyendo para siempre, sin
que nadie muera, es lo que impide que la partida **termine** aunque la economía
se arregle.

Va el último a propósito: es el único de los cuatro que no bloquea el
progreso, sólo el final.

**DoD**: en el banco, la partida termina con vencedor.

## §4 Lo que NO se hace hasta que esto esté cerrado

- **Nada de contenido nuevo.** Ni cuarta civilización, ni más celdas ancladas,
  ni arte. Añadir sobre un juego que se atasca en la época 3 es más de lo
  mismo, roto.
- **No se toca la rampa de épocas.** El banco demuestra que NO es el cuello:
  ambos jugadores suben en los ticks 6012 y 12012, justo en el suelo teórico.
  La rampa funciona; lo que falla es la economía.

## §5 Lo que esta medición enseña sobre el método

Las 51 pruebas estaban en verde. La apertura terminaba con vencedor en 10 473
ticks. **Todo lo que medíamos era corto**, y el juego se rompe en el tramo
largo — justo el que abrimos en el Sprint 1.25.

El banco de partida larga se queda como parte del método, no como herramienta
de una vez: cualquier sprint que toque economía o épocas se mide con él antes
de darse por cerrado.
