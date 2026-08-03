# SPEC-TRAYECTORIA — linajes de civilización a través de las épocas

Arquitecto Jefe · 2026-08-03 · **PROPUESTA, pendiente de aprobación del Director**

Contrato final de ADR-016. El plan maestro §3 lo declara bloqueante: *«se
escribe antes de integrar la 3ª civ»*.

---

## §0 Por qué existe y qué NO hace

Cuando se escribió el plan, esta SPEC era una hoja en blanco. Ya no: los
Sprints 1.22–1.26 **construyeron dos linajes completos** —el valle del Nilo y
la península itálica, de la época 1 a la 15— y el 1.24 construyó el mecanismo
para compartir contenido.

Así que **esta SPEC describe lo que existe y fija las reglas que lo gobiernan.
No inventa un sistema nuevo.** Eso es deliberado: un contrato escrito contra
código que funciona es comprobable; uno escrito contra un diseño imaginado sólo
es una opinión larga.

---

## §1 Qué es un LINAJE

Una civilización de CHUNSA **no es un pueblo**: es un **territorio con
continuidad**, jugado a través de los regímenes que lo ocuparon.

`egipto:dynastic_nile` no significa «los egipcios dinásticos». Significa **el
valle del Nilo**, desde los cazadores qadan hasta la presa de Asuán. Igual
`rome:republic_imperial` es **la península itálica**, del Epigravetiense a
Olivetti.

Esta definición no es una licencia poética: es lo que hace posible el encargo
del Director de que una civilización se juegue de la época 1 a la 15. **En la
Paleolítica no existen ni Egipto ni Roma**, y fingir lo contrario habría sido
el primer dato falso.

### Consecuencia: los periodos son la unidad de contenido

Cada linaje declara `playable_periods` con nombre e intervalo de años. El
periodo, no la civilización, es lo que ata un edificio a un momento.

Los dos linajes existentes tienen **13 periodos cada uno**, de los cuales 4 son
culturas arqueológicas verificadas (Qadan, Epigravetiense, Remedello,
Terramare) y el resto son intervalos de época.

---

## §2 Qué se HEREDA y qué se PIERDE al cambiar de periodo

Es la pregunta que quedó abierta en SPEC-007 §22.2 y que este documento cierra.

### Regla: NO SE PIERDE NADA por cambiar de periodo

El conocimiento **se acumula**. Una civilización que llega a la época 12 sigue
pudiendo levantar una granja de la época 2.

**Por qué esta regla y no la contraria**, que era defendible:

1. **Ya hay un mecanismo de obsolescencia y es mejor.** `epoch_window` permite
   decir «esta unidad deja de estar disponible en la época 8» **por dato y caso
   por caso**, en vez de una regla global que quita todo al cruzar una línea.
   Empire Earth usa exactamente eso: reemplazo con conversión, no pérdida.
2. **Perder por cambio de periodo castiga avanzar**, que es lo contrario de lo
   que un RTS de épocas debe premiar. El jugador que sube de época notaría que
   se le rompe la base.
3. **Es lo que ya está implementado.** Cambiarlo ahora obligaría a revisar los
   42 edificios uno a uno sin que nadie haya pedido esa mecánica jugando.

### La excepción, que sí existe

Un edificio o unidad **puede** declarar un `epoch_window` que termine antes de
la 15. Eso NO es pérdida por periodo: es **caducidad declarada por el dato**,
visible en el catálogo y comprobable. El carro de guerra egipcio no llega a la
época 12, y eso lo dice su ventana, no una regla.

---

## §3 Fondo común contra contenido propio

El Sprint 1.24 dio el **mecanismo** (plantillas con `extends`). Faltaba la
**regla de cuándo usarlo**. Aquí está, y sale de medir lo que ya hay.

### El estado medido (2026-08-03)

| | |
|---|---|
| Edificios totales | 42 |
| Que heredan de plantilla | **32 (76 %)** |
| Plantillas | 16 |
| Plantillas usadas por **ambas** civilizaciones | **16 de 16** |
| Líneas de un record que hereda | **30** |
| Líneas de un record autónomo | **64** |

**Las dieciséis plantillas las usan las dos civilizaciones.** Ninguna quedó
como plantilla de una sola, que era el riesgo obvio del mecanismo.

### La regla

> **Compartido por defecto, propio por excepción — y la excepción hay que
> justificarla con una fuente.**

Un edificio va a plantilla compartida salvo que se cumpla **al menos una**:

1. **Su función es distinta**, no sólo su nombre. El khan mameluco es acopio y
   el ṭirāz es producción: son dos edificios.
2. **Tiene una cita que sostiene una particularidad mecánica.** El taller
   Terramare cuesta piedra *y* arcilla porque la fuente dice que fundía bronce
   «in moulds of stone and clay». Sin esa frase, sería el taller genérico.
3. **Marca la identidad de juego de la civilización.** El nilómetro es el verbo
   `coordinate_flood` hecho edificio.

Si ninguna se cumple, va a plantilla. **Dar nombres distintos a números
idénticos no es asimetría: es duplicación.**

### Lo que NUNCA se comparte

**La procedencia.** La plantilla lleva la mecánica; el record lleva identidad,
periodos y **fuentes**. Que Qadan y Epigravetiense compartan números es un hecho
arqueológico defendible; que compartieran cita sería falsificarla.

---

## §4 Espacios de nombres

Regla ya vigente y aquí formalizada, para que la 3ª y 4ª civilización no
obliguen a renumerar nada:

- Todo record es `espacio:nombre`. El compilador lo exige.
- **El espacio de una civilización es suyo**: `egipto:`, `rome:`. Un record de
  una civ debe llevar su espacio — el compilador ya lo comprueba
  (`E_NAMESPACE`).
- **`base:` es el espacio del fondo común** y no pertenece a nadie. Sólo lo usan
  las plantillas.
- Los identificadores numéricos (`BuildingId`, `UnitId`, `TechId`) son **el
  índice en orden bytewise del `record_id`**. Consecuencia práctica que ya nos
  ha mordido tres veces: **añadir o renombrar un record reordena los índices** y
  mueve los hashes de determinismo, sin que la partida cambie. Es esperado; lo
  que NO debe cambiar es el `end_tick`.

---

## §5 Coste de añadir la tercera civilización, estimado con datos

No es una corazonada: sale de las cifras de §3.

Un linaje completo de 15 épocas necesita hoy, como mínimo, **2 edificios y 1
unidad trabajadora por época** (lo exige `test_epoch_playability`, que a su vez
lo saca de la puerta de `ADVANCE_EPOCH`).

Con las 16 plantillas existentes cubriendo el esqueleto, una civilización nueva
necesita sobre todo **records delgados de identidad y cita**: ~30 líneas cada
uno en vez de ~64.

**La prueba de fuego es exactamente ésa**: si la 3ª civilización entra
reutilizando las plantillas, el 1.24 valió la pena. Si hay que escribir 42
edificios desde cero, no.

---

## §6 Decisiones que necesito del Director

1. **¿Se aprueba la regla de §2** (no se pierde nada por cambiar de periodo; la
   obsolescencia se declara con `epoch_window`)?
2. **¿Cuál es la 3ª civilización?** ADR-012 nombra Imperio de Mali y
   Tawantinsuyu. Mi recomendación es **Tawantinsuyu**, por una razón práctica:
   el corpus ya tiene metalurgia andina verificada —Curamba, Viña del Cerro— que
   quedó sin usar, así que arranca con anclajes reales en la mano.
3. **¿Cuántos periodos por civilización para la 1.0?** Los dos linajes actuales
   tienen 13. Mantener esa cifra da coherencia; bajarla ahorraría trabajo pero
   dejaría huecos de época que el guardián rechazaría.

---

## §7 Lo que esta SPEC deja fuera a propósito

- **Campañas.** Son SPEC-009 y otra cosa: un linaje es la línea temporal de una
  partida de escaramuza, no un guion.
- **Reemplazo con conversión** al estilo Empire Earth (que una unidad se
  transforme al subir de época). Es interesante y no hace falta todavía:
  `epoch_window` cubre el caso sin mecánica nueva. Si se pide, entra como
  enmienda.
- **Balance entre linajes.** Ahora mismo los dos comparten casi todos los
  números. Diferenciarlos es trabajo de balance con partidas medidas, no de
  contrato.
