# El pánico permanente: por qué la partida no termina nunca

Arquitecto Jefe · 2026-08-04 · última causa del plan de desatasco

---

## §1 Lo que mide el banco, sin interpretar

Tras el Sprint 1.46 la economía ya no es el muro (madera p0=1600, p1=1240). La
partida sigue sin terminar en 120 000 ticks, y el censo militar dice por qué:

| | p0 | p1 |
|---|---|---|
| unidades | 43 | 36 |
| centroide (tiles) | (20.3, 128.6) | (22.6, 132.0) |
| moral | **100** | **26**, congelada |
| huyendo | 0 | **1** |
| objetivo de ataque | **ninguno** | lo tiene, y no lo usa |

Los dos ejércitos están a **cuatro tiles** uno del otro. Ninguno ataca. Nadie
muere. La partida no puede terminar porque la condición de victoria exige que
alguien pierda algo, y no se pierde nada.

## §2 La causa exacta: HAY UNA ZONA MUERTA EN LA MORAL

`morale_system` tiene dos ramas y **no cubren todos los casos**:

```cpp
if (enemies > allies + 1) {
    g.morale[i] -= MORALE_DROP;      // en desventaja: cunde el pánico
} else if (enemies == 0) {
    g.morale[i] += MORALE_REGEN;     // a salvo: se recupera
}
// y si hay enemigos pero NO estás en desventaja... no pasa NADA
```

Ese hueco es el fallo. Una unidad **en un pulso igualado** —enemigos cerca,
números parejos— tiene la moral **congelada**. Ni baja ni sube.

Y ahí es donde la congelación se vuelve permanente:

- se entra en pánico con moral ≤ 20 (`MORALE_PANIC`);
- se deja de huir con moral ≥ 50 (`MORALE_RALLY`);
- **una unidad en pánico solo puede rehacerse regenerando**, y regenerar exige
  que NO quede ni un enemigo cerca.

Las unidades de p1 se quedaron en **26**: por encima del pánico, muy por debajo
del rally, y sin ninguna forma de moverse de ahí. Huyendo para siempre. Y una
unidad que huye no ataca (`step.hpp`, línea 1330), así que p1 tiene objetivo
asignado y jamás pega. p0, por su parte, está a cuatro tiles: fuera de alcance
cuerpo a cuerpo, sin objetivo, con moral 100 y nada que hacer.

**El 26 congelado es la firma del fallo, y es la prueba de que el diagnóstico no
es una hipótesis:** ningún otro mecanismo del kernel deja un número exactamente
quieto tick tras tick.

## §3 Los dos arreglos, y por qué son dos

### A — Cerrar la zona muerta

Si no estás en desventaja, te rehaces. Aguantar la línea con números parejos es
precisamente lo que sostiene la moral de una tropa; el modelo actual dice lo
contrario, que sólo te recuperas cuando el enemigo ya no está.

```
en desventaja (enemies > allies + 1)  ->  baja
en cualquier otro caso                ->  sube
```

Sin ramas huecas. Y no es una concesión de balance: es que **todo estado debe
tener salida**, o el sistema admite estados absorbentes que congelan la partida.

### B — Acorralado, se pelea

Una unidad que huye pero **no consigue alejarse** —porque la empujan contra el
borde del mundo, contra un muro, o porque no hay a dónde ir— debe plantarse y
pelear. Es lo que hace la tropa acorralada y es lo que hace que las batallas se
resuelvan.

La comprobación es determinista y barata: tras el paso de huida, si la distancia
al enemigo más cercano **no ha aumentado**, la unidad está acorralada. Se le
quita el pánico y se le sube la moral al umbral de rally.

Va con A y no en su lugar porque A sola deja una puerta abierta: en desventaja
real y sin escapatoria, la moral seguiría bajando y el pánico sería permanente
otra vez. B cierra esa puerta. **A quita el estado absorbente del caso igualado;
B lo quita del caso perdido.**

## §4 Lo que NO se toca

- **No se cambian los umbrales** (`MORALE_PANIC` 20, `MORALE_RALLY` 50,
  `MORALE_DROP` 8, `MORALE_REGEN` 2). Mover números para tapar un fallo
  estructural es la peor clase de arreglo: parece que funciona y vuelve más
  tarde.
- **No se toca el daño ni el alcance.** Que p0 no tenga objetivo a cuatro tiles
  es correcto: es cuerpo a cuerpo. Lo que falla no es el alcance, es que el
  rival no puede pelear.
- **No se toca la IA.** Esto es kernel: una regla de la simulación, igual para
  el humano que para la máquina.

## §5 Criterio de aceptación

**En el banco, la partida TERMINA con vencedor antes de los 120 000 ticks.**

Y si no termina, el entregable es igual de valioso siempre que venga con el tick
y el estado exactos: significaría que hay un tercer estado absorbente que
todavía no hemos visto, y saberlo vale más que un arreglo a medias.
