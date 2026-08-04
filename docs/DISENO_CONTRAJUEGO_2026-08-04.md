# Contrajuego a la guerra temprana: torres, murallas y un coste que se note

Arquitecto Jefe · 2026-08-04 · decisión del Director tras el panel de tres familias

---

## §0 Qué se decide y por qué las dos cosas a la vez

Las dos vías del panel son **la misma ecuación por sus dos lados**:

- dar defensa sin encarecer el ataque solo **alarga** la guerra;
- encarecer el ataque sin dar defensa solo la **retrasa**.

Juntas, la agresión temprana pasa a ser *una* línea entre varias en vez de la
única. Van en el mismo sprint por eso, no por prisa.

## §0-bis CORRECCION: dos cosas que escribi aqui eran FALSAS

Lo descubri implementando, no disenando, y lo dejo escrito porque el documento
circulo y porque el error es instructivo.

**Escribi que `combat_system` "excluye a los ciudadanos, NO a los edificios".**
Falso. Los edificios llevan `unit_class = 255`, asi que el guard `> 2` los
excluia exactamente igual que a los ciudadanos. La torre NO era gratis: hacia
falta relajar ese guard.

**Y anuncie una "trampa": que las torres echarian a andar al entrar en el
aggro.** Tampoco existia, por el mismo motivo — `aggro_system` usa ese mismo
guard, asi que los edificios ya estaban fuera. Avise de un peligro imaginario y
me perdi el obstaculo real.

Los dos errores tienen la misma raiz: **lei el comentario del guard en vez de
comprobar el valor**. El comentario decia "ciudadanos" y yo lo crei; el codigo
decia `> 2` y los edificios son 255.

Lo que SI se sostiene del diseno: la torre sigue siendo barata (una condicion
en un guard, dos campos y una copia), la muralla sigue siendo gratis, y la
guarnicion sigue siendo la cara. La decision no cambia; el trabajo era un poco
mayor de lo que dije.

## §1 La torre: un campo, no un sistema

`combat_system` excluye a los **ciudadanos** (`unit_class > 2`), **no a los
edificios**. Barre buscando al enemigo más cercano dentro de `range_mt` y le
pega. Un edificio con ataque y alcance ya encajaría en ese barrido sin tocar una
línea del sistema de combate.

Lo que falta es sólo el dato: `BuildingDefinitionV1` no tiene `attack` ni
`range_millitiles`, así que todo edificio nace con `attack = 0`.

## §2 LA TRAMPA, y es lo que hay que cerrar antes de nada

`aggro_system` hoy no toca a los edificios **por accidente**: los descarta en
`if (g.attack[i] <= 0) continue`. Su `unit_class` es 0, que pasa el filtro de
`> 2` sin problema.

**En cuanto un edificio tenga ataque, entrará en el aggro. Y el aggro MUEVE**:
cuando el enemigo está fuera del alcance del arma, empuja a la entidad hacia un
punto de standoff.

**Las torres echarían a andar.** Es exactamente la clase de fallo que este
proyecto ya conoce: un guard que protegía por casualidad y deja de proteger
cuando cambias un dato. Hay que excluir `entity_kind == 1` del aggro
explícitamente, y con prueba.

Y no hace falta que las torres usen el aggro para nada: `combat_system` busca
blanco por su cuenta cada tick. Una torre no necesita perseguir; para eso es una
torre.

## §3 La muralla ya está construida y no lo sabíamos

Al colocar un edificio, `step.hpp` escribe `FF_WALL` en cada celda de su huella:

```cpp
            for (uint64_t cy = ty; cy < ty + fh; ++cy) {
                for (uint64_t cx = tx; cx < tx + fw; ++cx) {
                    g.cost_grid[cy * FF_AXIS + cx] = FF_WALL;
                }
            }
```

O sea: **todo edificio ya bloquea el paso**. Una muralla es un edificio barato
de huella 1×1 y mucha vida. Es **dato puro**, cero kernel.

Eso convierte lo que parecía el mecanismo más caro en el más barato, y es la
razón de que este sprint sea abordable.

## §4 El coste de época: existe y no se nota

Subir de época cuesta 200 + 200 + 100 y exige dos edificios completos. DeepSeek
lo llamó "un regalo del reloj" y se equivocaba en la letra — pero acertaba en el
fondo, y los números lo agravan: **el ganador termina con 20.000 de comida**.
Doscientos es el **uno por ciento** de lo que le sobra.

Que un coste esté puesto no significa que se note. Un coste que no se nota no es
una decisión, es papeleo.

**El coste pasa a escalar con la época.** No es un número mayor porque sí: la
época 15 debe costar lo que cuesta una civilización industrial, y la 2 lo que
cuesta salir del paleolítico. Un coste plano dice que las dos cosas valen igual.

**Y se calibra MIDIENDO, no discutiendo.** El criterio de aceptación no es "el
coste sube", es que la partida **pase de la época 3**. Si la primera constante no
lo consigue, se ajusta y se vuelve a medir; el banco es el juez.

## §5 Lo que NO se hace

- **Guarnición, no.** Es el mecanismo más reconocible de AoE2 y el único de los
  tres que exige un sistema nuevo en el kernel: estado de unidad-dentro-de,
  salida, transferencia de daño. Queda anotado, no descartado.
- **No se llena el árbol tecnológico.** Las tres familias coincidieron: es
  contenido que no se ejerce hasta que la partida dura.
- **No se toca la intención de construcción de la IA todavía.** Ese era mi plan
  y el panel lo tumbó. Volverá cuando haya algo que valga la pena construir.

## §6 Reparto

| Trabajo | Ruta | Por qué |
|---|---|---|
| Kernel: ataque/alcance en edificios + guard del aggro | DeepSeek, contrato **corto** | Los tres contratos largos anteriores se le cortaron a mitad |
| Datos: torres y murallas de las tres civilizaciones | MiniMax M3 | Hizo el corpus histórico; sabe anclar los nombres |
| Coste de época y **toda la medición** | yo | El banco es el juez y la calibración es decisión de diseño |

## §7 Criterio de aceptación

**La partida llega más allá de la época 3.** No "hay torres". No "el coste
sube". Que el banco muestre una partida que pasa de donde hoy se muere.

Y si no lo consigue, el resultado sigue valiendo siempre que venga con números:
significaría que el contrajuego tampoco era la causa, y eso lo querríamos saber
antes de gastar otro sprint en la dirección equivocada.
