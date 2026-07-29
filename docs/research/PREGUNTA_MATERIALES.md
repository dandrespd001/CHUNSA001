# Investigación — ¿qué materiales exige el realismo científico sin saturar el juego?

## Contexto

CHUNSA es un RTS histórico determinista que abarca **15 edades**, del
Paleolítico a la Era de la Información. Queremos que la progresión tecnológica
sea **científicamente correcta**: que un material esté porque hacía falta de
verdad, no de adorno.

La tensión es real y ya nos la señaló un panel previo: Age of Empires 2 maneja
**4 recursos**; nuestra propuesta llega a **26 activos** en la última edad. El
Director acepta más carga a cambio de realismo, pero **no a cualquier precio**.

## Nuestra lista actual, para que la critiques punto por punto

**Recolectados (17)**, con la edad en que aparecen:

| Edad | Recursos |
|---|---|
| 1 | comida, madera, piedra |
| 2 | arcilla |
| 3 | cobre, oro |
| 4 | estaño |
| 5 | mena de hierro |
| 8 | plomo, salitre, azufre |
| 11 | carbón |
| 13 | petróleo |
| 14 | bauxita, uranio |
| 15 | silicio, tierras raras |

**Producidos (8)** por receta en un edificio:

| Producido | Receta | Edad |
|---|---|---|
| carbón vegetal | madera | 5 |
| bronce | cobre + estaño | 4 |
| hierro forjado | mena de hierro + carbón vegetal | 5 |
| pólvora | salitre + azufre + carbón vegetal | 8 |
| coque | carbón | 11 |
| acero | hierro forjado + coque | 12 |
| aluminio | bauxita + **electricidad** | 14 |
| derivados del petróleo | petróleo | 13 |

**Energía**: no es un stock. Se deriva por tick; si falta, las recetas
dependientes **se paran en seco**.

## Lo que necesito saber, en orden de valor

### 1. Qué falta que sea INDISPENSABLE

¿Qué material hace **imposible** una transición si no está? Me interesan
especialmente los insumos «invisibles» que la gente olvida:

- **Fundentes** (caliza) para reducir mena de hierro: ¿es imprescindible o se
  puede abstraer?
- **Arcilla refractaria** para hornos capaces de alcanzar la temperatura del
  hierro.
- **Sal**: conservación de alimentos, y por tanto ejércitos en campaña.
- **Agua** como insumo industrial (vapor, refrigeración, electrólisis).
- **Nitrógeno fijado** (Haber-Bosch) para explosivos y fertilizantes del s. XX.

Para cada uno: ¿lo notaría un jugador informado si falta, o es ruido?

### 2. Qué está mal en nuestra lista

Errores de hecho, de fecha o de cadena. Por ejemplo: ¿es correcto que el carbón
vegetal aparezca en la edad 5 y el coque lo sustituya en la 11? ¿El plomo en la
edad 8 es la fecha correcta?

### 3. Qué se puede FUSIONAR sin perder credibilidad

Éste es el punto más valioso. **¿Qué materiales puede agrupar el juego sin que
un jugador con formación técnica lo note?**

Ejemplos de lo que busco: ¿«tierras raras» puede ser un solo recurso o el
jugador informado esperaría distinguirlas? ¿«derivados del petróleo» puede
cubrir plásticos, combustible y lubricantes a la vez?

Dame el **criterio**, no solo la lista: qué hace que una abstracción sea
aceptable y otra chirríe.

### 4. Cómo lo resuelven otros juegos históricos

Empire Earth, Rise of Nations, Anno 1800, Victoria 3, Songs of Syx, Factorio,
Foundation, Manor Lords. Para cada uno: cuántos materiales maneja, cuántos
**simultáneos**, y qué técnica usa para que no abrumen (agrupación, desbloqueo
por edad, cadenas ocultas, automatización).

### 5. El umbral de saturación

¿Existe evidencia —reseñas, análisis de diseño, postmortems— de **cuántos
recursos simultáneos** empieza a rechazar el jugador de estrategia en tiempo
real? Nos interesa el número y de dónde sale.

## Formato

Máximo 150 líneas. Un hallazgo por línea:
`[MARCA] afirmación con dato concreto -> [CHUNSA] qué hacemos con ella`

Marca `[V]` verificado citando la fuente que leíste, `[I]` inferencia, `[?]` no
confirmado. **Una línea `[V]` sin fuente citada es un fallo del encargo.**

Termina con dos secciones obligatorias:

- **`## Añadir`** — materiales que faltan, ordenados por cuánto duele su
  ausencia.
- **`## Fusionar o quitar`** — qué sobra, con el criterio que lo justifica.

No inventes cifras. Si una fuente no carga, dilo y sigue.
