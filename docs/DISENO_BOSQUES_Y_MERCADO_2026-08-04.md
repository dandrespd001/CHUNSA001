# Bosques como zonas + la IA compra madera

Arquitecto Jefe · 2026-08-04 · orden del Director tras la medición del 1.41

---

## §0 De dónde viene esto

El banco de partida larga dejó la comida resuelta (p1 pasó de 1005 a 10 984) y
movió el muro a la **madera**: el bosque plantado es de época [7,15] y no se
llega a la época 7 sin madera. El Director decide **las dos vías a la vez**:

1. **La IA compra madera** con el excedente de comida, usando el mercado que ya
   existe. Es la vía inmediata: no añade contenido, hace útil lo construido.
2. **Aparecen bosques como ZONAS** al estilo Age of Empires II, que se talan
   para sacar madera. Es la vía estructural: el mapa vuelve a tener madera.

Ninguna de las dos toca la época del bosque plantado. Mover ese dato habría
tapado el síntoma; esto ataca la causa por los dos lados.

## §1 El bosque es un ÁREA, no un punto

Hoy `EcoDeposit` es un punto con una cantidad. Un bosque no: un bosque **ocupa
sitio**, se tala por el **borde**, y **retrocede** conforme lo cortas. Ésa es la
sensación de AoE2 y es lo que hay que reproducir.

El límite duro que manda en el diseño: `ECO_MAX_DEPOSITS = 64`. No podemos
llevar miles de árboles individuales — ni falta hace. **Un bosque = UN depósito
con centro, radio y madera**, y el radio encoge al talar.

### La relación entre radio y madera restante

No es lineal, y la razón es geométrica. La madera de un bosque es proporcional a
su **área**, y el área va con el **cuadrado** del radio:

```
madera ∝ r²   ⟹   r = R₀ · √(restante / inicial)
```

En enteros y sin coma flotante, que está prohibida en el kernel:

```
r² = R₀² · restante / inicial      (cabe en 64 bits)
r  = isqrt_u64(r²)                 (ya existe, en wide128.hpp)
```

Consecuencia jugable, y es la correcta: **el bosque aguanta grande mucho
tiempo y se desploma al final**. Talado a la mitad conserva el 70 % del radio.
Un bosque que encogiera lineal parecería derretirse desde el primer hachazo.

### Se tala por el BORDE

El aldeano hoy camina al **centro** del depósito y recolecta a un tile. Con un
bosque eso sería andar *dentro* de los árboles. La llegada pasa a ser:

```
llega cuando   dist ≤ radio_actual + ECO_ARRIVE_RADIUS_RAW
```

Es un cambio de una línea y hace todo el trabajo: se tala en el borde, y como el
borde retrocede, **el aldeano se adentra conforme el bosque se consume**. Sale
gratis: no hay nada que animar, la geometría lo produce sola.

### Elegir bosque: por el BORDE, no por el centro

Si el candidato se elige por distancia al centro, un bosque enorme al lado
pierde contra una mancha diminuta más allá — absurdo, porque el bosque grande
está *más cerca* en todo lo que importa. La comparación pasa a ser por
**distancia al borde**: `d_borde = max(0, dist − radio)`.

**Y esto NO puede mover los mapas actuales.** Con radio 0 la distancia al borde
es la distancia de siempre, pero `isqrt` trunca y dos depósitos distintos pueden
empatar tras truncar. Por eso el orden es de **tres claves**:

1. distancia al borde (entera, vía `isqrt`)
2. **`d_sq` exacta** — desempata sin truncar
3. índice más bajo — la regla de oro de siempre

Con todos los radios a 0 esto produce **exactamente** el orden anterior. El
cambio es invisible donde no hay bosques, que es la prueba de que está bien
puesto.

## §2 Campos nuevos de `EcoDeposit`

| Campo | Tipo | Significado |
|---|---|---|
| `radius_raw` | `int64_t` | radio del bosque a plena carga. **0 = depósito puntual**, comportamiento de siempre |
| `initial_amount` | `int32_t` | madera con la que nació, para calcular el encogimiento |

Los dos por defecto a 0, así que **los 22 yacimientos del mapa y las granjas no
cambian en nada**. Un bosque es un depósito con `radius_raw > 0`.

`SAVE_FORMAT_VERSION` 19→20 y `CHECKSUM_ALGO_VERSION` 14→15: los campos entran
en el guardado y en el checksum.

## §3 Qué NO se hace, y por qué

- **Los árboles no bloquean el paso.** En AoE2 el bosque es muro, y es medio
  juego táctico. Aquí no hay geometría de colisión por casilla en el kernel, y
  metérsela por esta puerta sería un sprint de pathfinding disfrazado de sprint
  de recursos. Queda **anotado como deuda explícita**, no olvidado.
- **No se toca la época del bosque plantado.** Sigue en [7,15]. La
  reforestación es tecnología, no un parche de balance.
- **No hay árboles individuales.** Un bosque es una zona. Si algún día el
  frontend quiere dibujar árboles sueltos, que los derive del centro, el radio
  y la semilla: es presentación, no simulación.

## §4 La IA compra madera

`ai_find_trade` hoy hace dos cosas y las dos se quedan cortas:

- **compra** sólo para desatascar una *receta*;
- **vende** sólo "excedente claro", definido como un recurso que **nada** en la
  civilización consume. La comida la consume medio catálogo, así que **jamás**
  cuenta como excedente — y por eso p1 se sienta encima de 10 984 de comida
  mientras se muere sin madera.

Dos casos nuevos, y van **emparejados a propósito**:

1. **Vender excedente ABRUMADOR**: un recurso con stock ≥ `AI_TRADE_GLUT`
   (2000, veinte lotes) se vende **sólo si otro recurso está bloqueando** algo
   que la IA quiere de verdad. Sin esa condición la IA vendería comida por
   costumbre y movería el precio en su contra.
2. **Comprar para CONSTRUIR**, no sólo para fabricar: si falta un único recurso
   no-oro para el edificio que la IA quiere levantar, y un lote lo cubre, se
   compra.

El emparejamiento es la clave: **vender no es una política, es el pago de una
compra concreta**. Se vende porque hay algo que comprar.

## §5 Orden de trabajo y reparto

| Sprint | Qué | Ruta |
|---|---|---|
| 1.45 | Zonas de bosque en el kernel (`economy.hpp`) | DeepSeek v4-flash, brief con el código exacto |
| 1.46 | Campo `radius` en esquema de mapa + loader | loader C++: DeepSeek · YAML del mapa: MiniMax |
| 1.47 | La IA compra madera (`ai_stub.hpp`) | **yo** — el fichero pasa de 1400 líneas y ahí ya fallaron dos delegaciones |
| 1.48 | Medición con el banco y auditoría | banco: yo · auditoría: DeepSeek en modo `auditar` |

La nota de referencia sobre bosques en RTS (AoE2 y comparables) la está
haciendo MiniMax M3 en paralelo; sirve para **calibrar cifras**, no para decidir
el diseño, que ya está decidido arriba.
