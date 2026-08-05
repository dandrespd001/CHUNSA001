# Síntesis de las cuatro partes

Arquitecto Jefe · 2026-08-05 · MiniMax M3, investigación repartida en cuatro

---

## §0 El hallazgo que cambia más cosas

**Mi instrumento de medida era el correcto para el trabajo anterior y es el
equivocado para la pregunta de ahora.**

La parte 4 lo separa con precisión. El auto-juego simétrico —dos IA idénticas,
que es mi banco— mide **bien**:

- estabilidad: que la partida termine sin desincronización ni bucles;
- indicadores económicos absolutos: una economía que no arranca está rota
  contra cualquier rival;
- **detección de empantanamiento**: si nadie gana en 120 000 ticks, hay un
  problema;
- reproducibilidad bit a bit.

Y mide **mal**:

- el equilibrio del juego;
- la variedad estratégica: las dos IA usan la misma función de utilidad, así
  que sus decisiones se correlacionan y **convergen a la misma jugada**;
- la dificultad real.

Su frase: *"Para diagnóstico, una IA idéntica es útil; para calidad de IA, no."*

**Consecuencia sobre lo ya hecho, y hay que decirla en las dos direcciones:**

- Los cinco sprints del desatasco —moral congelada, alcance cero,
  ataque-movimiento, bosques, granjas— estaban medidos con el instrumento
  **correcto**. Todos eran empantanamientos y fallos absolutos, justo lo que el
  auto-juego simétrico detecta bien. Ese trabajo se sostiene.
- La conclusión del último sprint —"las torres se construyen y no cambian
  nada"— está medida con el instrumento **equivocado**, y hay un artefacto
  documentado que la explica: en espejo, ambas IA construyen las mismas torres
  y el efecto se cancela. **No sé si las torres sirven; sé que en espejo no se
  nota.**

## §1 La aniquilación mutua es ESTRUCTURAL, no numérica

La parte 3 responde a la pregunta que le puse por delante, y la respuesta
descarta la vía en la que yo estaba:

> Si daño, moral y composición son iguales en ambos bandos, **no hay
> realimentación que rompa a uno antes**. La solución no es más HP, más DPS ni
> más moral: es una asimetría mecánica que rompa la simetría.

Es decir: subir el ataque de las torres o poner cuatro en vez de dos **no
puede** arreglarlo. Estaba a punto de calibrar la dosis de algo que no falla
por dosis.

Y señala la pieza concreta que nos falta: **el contagio**.

> Sin contagio, dos ejércitos pierden unidades al mismo ritmo y nunca se
> desencadena la realimentación que colapsa a uno. Con contagio, el primero en
> perder una unidad tiene probabilidad de perder dos, luego tres.

De su lista de palancas, **dos ya las tenemos** —las unidades en pánico hacen
daño cero, y el pánico es casi instantáneo— y ésa es justamente la que llama
"la palanca decisiva". Lo que falta es lo que convierte una rotura local en un
derrumbe: que el pánico se propague a los vecinos.

## §2 La economía no tiene sumideros

La parte 2 pone nombre a los 20 000 de comida sin gastar:

> Sin presión de gasto, no hay decisión.

Y da las tres formas conocidas de crear esa presión: mantenimiento del ejército
(*upkeep*), bonificaciones que consumen a lo largo del tiempo, o desgaste de los
edificios. Ninguna la tenemos.

Esto explica también por qué **encarecer la época empeoró la partida**: yo
intenté crear presión subiendo un precio, y lo que subí fue el precio de la
única cosa que quería que hiciera. La presión no se crea encareciendo el
objetivo; se crea drenando el excedente.

## §3 El ataque no tiene ventana ni consecuencia

De la parte 2, y es el diagnóstico más directo de la degeneración:

> Un ataque que fracasa cuesta N ticks de bloqueo en los que la IA no puede
> repetirlo; durante ese bloqueo debe cambiar algo del plan.

Hoy nuestra IA reemite ataque cada ciclo, sin ventana de oportunidad y sin
coste por fracasar. No es que ataque demasiado: es que **atacar no le cuesta
nada y fracasar tampoco**.

Añade algo que no había considerado: *"las torres que no cambian nada son
síntoma de que el reconocimiento no alimenta la decisión"*. La IA rival no
modela la torre como amenaza, así que ataca igual. Una defensa que el atacante
no ve no disuade a nadie.

## §4 El techo del sistema de utilidad

La parte 1 confirma que la arquitectura actual tiene un límite documentado:

> Los sistemas de utilidad son viables y han producido buenos resultados —Halo,
> Civilization, Quake— pero son **miopes por construcción**: eligen una acción
> por ciclo mirando el presente.

Con quince épocas y cadenas de construcción e investigación, eso pega de lleno.
No dice "cámbialo ya"; dice que hay un techo y que conviene saber dónde está
antes de seguir puliendo debajo de él.

## §5 Lo que cambia en cómo pienso el problema

1. **Dejar de calibrar dosis.** La aniquilación mutua no se arregla con números
   mayores.
2. **Un rival distinto antes que un mecanismo nuevo.** Mientras mida en espejo,
   no puedo saber si lo que añado sirve. Es lo más barato de todo lo que hay
   pendiente y condiciona todo lo demás.
3. **Drenar el excedente, no encarecer la meta.**
4. **Que fracasar cueste.**

## §6 Honestidad sobre estas fuentes

Las cuatro partes marcan con *(sin confirmar)* lo que no pueden anclar, que era
la condición del encargo. Aun así son de **un solo modelo**: coinciden entre sí
en parte porque comparten autor. Antes de tomar una decisión grande sobre la
arquitectura de la IA convendría un contraste de otra familia, igual que se hizo
con el panel de tres del 2026-08-04.

Lo que sí es verificable sin panel es lo del §0, porque no es una opinión de
diseño sino una propiedad de mi banco: **puedo comprobarlo poniendo dos IA
distintas a jugar y viendo si la partida cambia.**
