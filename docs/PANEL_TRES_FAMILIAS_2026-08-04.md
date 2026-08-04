# Panel de tres familias: ¿por qué el juego promete quince épocas y entrega tres?

Arquitecto Jefe · 2026-08-04 · GPT Luna Max · DeepSeek v4-flash · MiniMax M3

Primer panel del proyecto con tres familias de modelos distintas. Hasta ahora
era imposible: solo respondían dos rutas.

---

## §1 En qué coinciden las tres, sin excepción

1. **Es un fallo respecto a la promesa, no un rush legítimo.** Un rush puede
   decidir una partida; un juego donde *todas* las partidas son un rush tiene un
   defecto estructural. MiniMax lo dijo más seco: *"La promesa está rota, no el
   equilibrio."*

2. **Que la IA no investigue es un SÍNTOMA, no la causa.** Las tres rechazaron
   la vía que yo iba a tomar. DeepSeek: *"forzar a la IA a construir el edificio
   de investigación solo producirá una IA más ornamental."*

3. **NO llenar todavía el árbol tecnológico.** Luna: *"echar agua en un
   recipiente roto."* MiniMax: *"pintar una casa a la que le falta el tejado."*

4. **Primero, que la guerra temprana tenga contrajuego.**

Que tres modelos de tres familias, con encargos distintos, converjan en el mismo
orden es la señal más fuerte que ha dado este panel.

## §2 En qué se separan, y es lo que hay que decidir

| | Primer mecanismo | Argumento |
|---|---|---|
| **Luna** | Guarnición en edificios + fortificaciones, con coste y límite | Que cuatro edificios no equivalgan a la partida perdida |
| **MiniMax** | Herramientas del defensor como **mecánica base, no tech-gated**: muros, torres con DPS real, edificios reparables | Ninguna exige investigación previa, y por eso funcionan desde el minuto uno |
| **DeepSeek** | **Coste de oportunidad**: que subir de época sea una decisión cara, no un regalo del reloj | Atacar mucho en la 3 debe significar llegar tarde y débil a la 4 |

Luna y MiniMax dicen lo mismo por dos caminos: **darle herramientas al que
defiende**. DeepSeek ataca el otro lado de la misma ecuación: **encarecer al que
agrede**.

## §3 El argumento que más pesa, y es de MiniMax

Es el único que se apoya en NUESTROS datos en vez de en principios generales:

> Aunque la IA construyera el campamento paleolítico e investigara
> perfectísimamente, **las únicas tecnologías de las épocas 1-3 son económicas**
> (millet, wild grain, backed bladelet). La primera con función de contrajuego
> claro es `corvee_logistics`, de época 4 — con la partida ya acabada.

Eso es falsable y lo comprobé: es cierto. Arreglar la intención de construcción
de la IA **no habría cambiado el resultado de la partida**, y yo estaba a punto
de dedicarle un sprint.

## §4 Dos correcciones mías al panel

**A DeepSeek, sobre su premisa.** Dijo que el salto de época es "un regalo del
reloj". No lo es: cuesta 200 + 200 + 100 de recursos y exige dos edificios
completos. Su premisa es imprecisa — pero **su conclusión sobrevive**, y por un
motivo que refuerza su tesis: el ganador termina con **20.000 de comida**, así
que 200 es el **uno por ciento** de lo que le sobra. El coste existe y es
demasiado pequeño para ser una decisión. Que un coste esté puesto no significa
que se note.

**A Luna, sobre un dato que le di mal yo.** Le pasé "el 73 % de los comandos son
de ataque" como medida de agresividad. No lo es: son reemisiones de la capa
táctica, más de una por ciclo y por unidad. Se lo corregí a las otras dos.

## §5 Lo que descubrí revisando el kernel, y cambia el coste de las opciones

Ninguno de los tres podía saberlo, y afecta a cuál es barata:

- **Los muros ya existen de hecho.** Todo edificio escribe `FF_WALL` en la
  rejilla de coste al colocarse: ya bloquean el paso. Una muralla es un edificio
  barato de huella pequeña — casi todo dato.
- **La torre está más cerca de lo que parece.** `combat_system` excluye a los
  ciudadanos (`unit_class > 2`), no a los edificios. Lo que falta es que
  `BuildingDefinitionV1` lleve ataque y alcance y se copien al colocar. Es un
  campo, no un sistema.
- **La guarnición es la cara de las tres.** Exige estado de unidad-dentro-de,
  salida, y transferencia de daño. Es un sistema nuevo, no un campo.

Así que la propuesta de MiniMax —muros y torres como mecánica base— resulta ser
además **la más barata en nuestro kernel**, y la de Luna —guarnición— la más
cara. Eso no se veía desde fuera.

## §6 Mi recomendación al Director

**Torres y murallas primero, y subir el coste de época en el mismo sprint.**

Las dos vías del panel son la misma ecuación por sus dos lados, y ninguna
funciona sola: dar defensa sin encarecer el ataque solo alarga la guerra;
encarecer el ataque sin dar defensa solo la retrasa. Juntas, la agresión
temprana pasa a ser *una* línea entre varias en vez de la única.

Y se miden con el banco, que es lo que este proyecto ha aprendido a hacer: el
criterio de aceptación no es "hay torres", es **que la partida llegue más allá
de la época 3**.
