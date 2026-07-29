# Investigación — fuerza motriz por edad y revisión de los materiales producidos

## El problema que hay que resolver

CHUNSA abarca **15 edades**, del Paleolítico a la Era de la Información. En el
diseño actual, las recetas que «necesitan energía» consumen **electricidad**,
que aparece en la edad 12.

**Eso es un anacronismo.** Antes de la electricidad también hacía falta energía
para mover cosas, y venía de otras fuentes. Un molino del siglo XIII no se
enchufa.

Queremos modelar la **sucesión histórica de la fuerza motriz** de forma
científicamente correcta y que además funcione como mecánica de juego.

## Nuestro modelo actual (critícalo)

**Energía**: una sola magnitud, no almacenable, derivada por tick como
`producción − consumo`. Si falta, las recetas dependientes **se paran en seco**.
Aparece en la edad 12 como «electricidad».

**Producidos (8)**: bronce (cobre+estaño, e4) · carbón vegetal (madera, e5) ·
hierro forjado (mena+carbón vegetal+caliza, e5) · pólvora (salitre+azufre+carbón
vegetal, e8) · coque (carbón, e9) · acero (hierro forjado+coque, e12) ·
aluminio (bauxita+electricidad, e12) · derivados del petróleo (petróleo, e13).

## Lo que necesito saber

### 1. La sucesión de la fuerza motriz

Para cada etapa histórica, qué movía las máquinas y **desde cuándo**:

- Músculo humano y animal (¿qué potencia real? ¿qué podía y no podía mover?)
- Rueda hidráulica y molino de viento: fechas de difusión, qué industrias
  permitieron (¿molienda? ¿fuelles de fundición? ¿martillos pilón?)
- Máquina de vapor: Newcomen 1712, Watt 1776 — qué cambió realmente y qué
  combustible usaba en cada momento
- Combustión interna y electricidad: cuándo desplazan al vapor y en qué usos

Para cada etapa: **qué combustible o fuente consume**, y si la energía se
transporta o hay que estar físicamente al lado de la fuente. Eso último es
clave: determina si en el juego la energía es global o local.

### 2. Qué NO podía hacerse sin cada salto

¿Qué procesos industriales eran **imposibles** antes de cada fuente? Ejemplos
de lo que busco: ¿se podía producir acero a escala sin vapor? ¿Se podía
electrolizar aluminio con cualquier otra cosa que no fuese electricidad?

Interesa especialmente **el caso del alto horno**: los fuelles ¿eran manuales,
hidráulicos, de vapor? ¿Cuándo cambió y qué permitió?

### 3. Revisión de los 8 producidos

¿Falta alguno indispensable? ¿Sobra alguno? Candidatos que sospechamos:

- **Cal viva** (de la caliza) — ¿es un producido propio o basta la caliza cruda?
- **Vidrio** — ¿merece existir como producido?
- **Ladrillo** (de arcilla) — ¿o se abstrae en «construcción»?
- **Cemento/hormigón** — el romano existía; el Portland es de 1824
- **Ácido sulfúrico** — insumo químico central del s. XIX
- **Papel** — ¿relevante para mecánicas o decorativo?

Para cada uno: **¿aporta una decisión al jugador o solo un paso más?**

### 4. Cómo integrarlo sin saturar

Dos opciones que estamos valorando, y quiero argumentos:

- **(A) Una sola magnitud «energía»** cuya *fuente* cambia por edad. Simple,
  pero ¿pierde el sabor histórico?
- **(B) Fuerza motriz por tipo** (muscular, hidráulica, térmica, eléctrica),
  donde cada máquina exige un tipo concreto. Más fiel, más carga.

¿Qué hacen los juegos que lo han intentado —Anno 1800, Victoria 3, Factorio,
Foundation, Manor Lords, Songs of Syx, Rise of Nations— y qué les funcionó?

### 5. El detalle que más nos importa

**¿La energía preindustrial es LOCAL?** Una rueda hidráulica mueve el molino
que tiene encima, no el del valle siguiente. Si eso es históricamente cierto,
la mecánica correcta no es un contador global sino **adyacencia**: el edificio
debe estar junto al río, o junto a la máquina de vapor.

Confírmalo o desmiéntelo con fuentes, y dinos desde cuándo la energía empieza a
transportarse de verdad (¿ejes y correas? ¿electricidad?).

## Formato

Máximo 150 líneas. Una línea por hallazgo:
`[MARCA] afirmación con dato y fecha -> [CHUNSA] qué hacemos`

`[V]` verificado citando la fuente leída · `[I]` inferencia · `[?]` sin
confirmar. **Una línea `[V]` sin fuente citada es un fallo del encargo.**

Termina con:

- **`## Sucesión propuesta`** — tabla edad → fuente de energía → combustible.
- **`## Producidos: añadir / quitar`** — con el criterio que lo justifica.
- **`## Global o local`** — veredicto sobre §5, que es el que más cambia el diseño.

No inventes fechas. Si una fuente no carga, dilo y sigue.
