# Metodología de desarrollo: TDD

**Directriz del Director (2026-07-28). Vigente desde el Sprint 1.8A para todo
trabajo nuevo de kernel, datos y adaptador.**

---

## §1 Por qué, en este proyecto concreto

No es una preferencia de estilo. Es la respuesta a un fallo que se ha repetido
**cuatro veces seguidas** en entregables delegados:

| Sprint | Prueba entregada | Por qué no valía |
|---|---|---|
| 1.6B K3 | Aserto de F-02 | **Tautológico**: no podía fallar |
| 1.6B K2 | `CHECK(out.winner != 0xFF)` | Pasaba aunque ganase el bando equivocado |
| 1.6B K2 | «save a mitad de recolección» | Guardaba tras un dropoff completo, no a mitad |
| 1.6B (adaptador) | «lo ejecuté en Godot» | La ejecución no había ocurrido |

El patrón es siempre el mismo: **la prueba se escribió después del código**, así
que se escribió para pasar. Una prueba que nace verde no demuestra nada, y no
hay forma de distinguirla de una buena leyendo la suite.

TDD lo impide por construcción: si la prueba se escribe **antes** y se **exige
verla fallar**, una prueba sin poder se detecta en el acto — porque pasaría en
la fase roja, que es imposible si de verdad prueba algo que aún no existe.

---

## §2 El ciclo, y la parte que no es negociable

**ROJO → VERDE → REFACTOR.**

La única fase que este proyecto trata como obligación contractual es la
**roja**, porque es la que produce la evidencia:

1. **ROJO.** Se escribe la prueba. Se compila. Se ejecuta. **Falla.** Se pega
   la salida del fallo en el informe.
2. **VERDE.** Se implementa lo mínimo para que pase. Se pega la salida verde.
3. **REFACTOR.** Se limpia sin cambiar comportamiento; la suite sigue verde.

**Sin salida roja pegada, el entregable se rechaza.** No es rigidez: es que sin
esa salida no existe ninguna forma de saber si la prueba tiene poder.

### §2.1 Fallar por la razón correcta

Una prueba que falla porque **no compila** no está en fase roja: está rota. La
fase roja exige un **fallo de aserción**, con el valor esperado y el obtenido.

Si la función bajo prueba todavía no existe, se declara con un cuerpo mínimo
que devuelva algo inválido, para que el fallo sea de aserción y no de enlazado.

---

## §3 El caso difícil: checksums y baselines

Aquí TDD no se aplica tal cual, y conviene decirlo antes de que alguien finja
que sí.

**No se puede escribir primero un `CHECK(checksum == 0x71774aaa...)`**: ese
valor no se conoce hasta ejecutar. Escribirlo después y llamarlo TDD sería
justo la mentira que esta metodología quiere eliminar.

La regla para determinismo:

- **Lo que sí se escribe primero son las INVARIANTES**, que sí se conocen desde
  la spec y sí pueden fallar: `winner == 1`, `end_tick < 36000`, las cuatro
  fases observadas, `alloc_delta == 0`, «los escenarios sin ciudadanos no
  cambian de trayectoria».
- **El checksum se registra después**, y su papel no es probar corrección sino
  **cerrojo de regresión**: detectar que algo cambió sin querer.
- Todo cambio de baseline se justifica en el commit que lo introduce. Un
  baseline que cambia sin explicación es un fallo, no una actualización.

Dicho de otro modo: la invariante prueba **que está bien**; el checksum prueba
**que sigue igual**. Son cosas distintas y solo la primera se escribe primero.

---

## §4 Cuando TDD no basta: prueba de mutación

Para código que ya existe sin fase roja —todo lo anterior al Sprint 1.8A— la
forma de comprobar que una prueba tiene poder es **romper el código a
propósito** y verificar que la suite se pone roja.

Si se invierte una condición, se cambia un `<=` por `<` o se comenta una línea
y **la suite sigue verde**, esa prueba no cubre lo que dice cubrir.

Es obligatorio en la revisión de cualquier prueba que llegue sin evidencia
roja. Barato y concluyente.

---

## §5 Qué cambia en las specs

Cada spec que defina comportamiento nuevo debe incluir, **antes** de describir
la implementación, una sección de **criterios de aceptación observables**:
afirmaciones concretas, comprobables, que puedan fallar.

Mal: «los aldeanos recolectan de forma sensata».
Bien: «un aldeano con un depósito del mismo recurso fuera de zona aliada y otro
de recurso distinto dentro **elige el de dentro**».

Esa lista **es** la lista de pruebas del sprint. Si un criterio no se puede
expresar como prueba que falle, el criterio está mal redactado.

## §6 Qué cambia en los briefs

Todo brief delegado incorpora esta sección, literal:

> ## Protocolo TDD — obligatorio
>
> 1. Escribe **primero** las pruebas de los criterios de aceptación.
> 2. Compílalas y **ejecútalas antes de implementar nada**. Deben **fallar**.
> 3. **Pega la salida roja en el informe**, con el fallo de aserción concreto.
>    Un fallo de compilación **no** cuenta como fase roja.
> 4. Implementa lo mínimo para ponerlas verdes.
> 5. Pega la salida verde.
> 6. Refactoriza si hace falta; la suite queda verde.
>
> **Un informe sin salida roja se rechaza sin revisar el código.** Si una
> prueba pasó en la fase roja, dilo: significa que esa prueba no prueba nada y
> hay que rehacerla, y decirlo es exactamente lo que se espera de ti.

## §7 Qué cambia en la revisión

Al verificar un entregable, **antes** de leer el código:

1. ¿Está la salida roja? Si no, se rechaza.
2. ¿Los fallos rojos son de aserción, no de compilación?
3. ¿Hay una prueba roja por cada criterio de aceptación de la spec?
4. ¿Alguna prueba pasó en rojo? Ésa se rehace.
5. Después, y solo después, se revisa la implementación.

## §8 Excepciones honestas

TDD no aplica igual a todo. Estos casos quedan **explícitamente exentos**, y
fingir lo contrario sería peor que la exención:

- **Migraciones puramente estructurales** (Sprint 1.8A: ampliar el vector de
  recursos sin cambiar comportamiento). No hay comportamiento nuevo que probar;
  la prueba es que **nada observable cambia** salvo los checksums.
- **Datos** (`data/*.yaml`). Se validan por esquema y por el compilador del
  catálogo, no por TDD.
- **Presentación en Godot.** El adaptador no tiene suite. Su verificación es
  ejecución real con evidencia —captura o salida de consola—, nunca la
  afirmación de haberlo ejecutado. Ya nos costó una sesión de pruebas del
  Director creerlo sin comprobarlo.
