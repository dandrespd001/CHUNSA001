# Encargo de investigación — AoE2 y RTS libres, aplicado a CHUNSA

**Modelo:** MiniMax-M3 (`claude-minimax`, ventana de 1M)
**Rol:** investigador y sintetizador. **No escribes código ni tocas el kernel.**
**Entregable único:** `docs/research/SINTESIS_RTS.md`

---

## 0. Tus herramientas (verificado en este equipo)

- **`WebFetch` FUNCIONA.** Es tu instrumento principal. Úsalo sobre cada URL de
  §2 y sobre los enlaces que descubras dentro de ellas.
- **`WebSearch` NO devuelve resultados útiles** en esta configuración. No
  pierdas tiempo con ella. Si necesitas algo que no está en §2, navega por
  enlaces desde las páginas que sí puedes leer.

Si una URL falla, dilo en el informe y sigue con las demás. **No rellenes
huecos con conocimiento propio presentándolo como hallazgo.**

---

## 1. Para qué sirve esto

CHUNSA es un RTS histórico determinista (kernel C++20 sin float, frontend
Godot). Estamos rediseñando el modelo de recursos y edades. Necesito saber cómo
resuelven estos problemas los juegos que ya lo han hecho bien, con **números
concretos**, no con generalidades.

Lee primero `docs/specs/SPEC-007_RECURSOS_Y_EDADES.md` (la propuesta actual) y
`docs/specs/SPEC-004_SISTEMAS_PARTIDA.md` §16–§23 (la economía tal como está
hoy). Todo lo que investigues debe evaluarse **contra eso**.

---

## 2. Fuentes de partida

### Age of Empires II — economía y edades
- https://ageofempires.fandom.com/wiki/Resources_(Age_of_Empires_II)
- https://ageofempires.fandom.com/wiki/Villager_(Age_of_Empires_II)
- https://ageofempires.fandom.com/wiki/Farm_(Age_of_Empires_II)
- https://ageofempires.fandom.com/wiki/Age_(Age_of_Empires_II)
- https://ageofempires.fandom.com/wiki/Gold_Mine
- https://ageofempires.fandom.com/wiki/Stone_Mine

### 0 A.D. — arquitectura y datos
- https://play0ad.com/
- https://trac.wildfiregames.com/wiki/Manual_Settings
- https://gitea.wildfiregames.com/0ad/0ad
- https://trac.wildfiregames.com/wiki/SimulationArchitecture

### Widelands — cadenas de producción
- https://www.widelands.org/wiki/
- https://www.widelands.org/wiki/Economy/
- https://www.widelands.org/documentation/

### openage — ingeniería inversa de AoE2
- https://github.com/SFTtech/openage
- https://openage.dev/
- https://github.com/SFTtech/openage/blob/master/doc/README.md

### OpenRA — determinismo y lockstep
- https://github.com/OpenRA/OpenRA
- https://github.com/OpenRA/OpenRA/wiki

### Panorama
- https://en.wikipedia.org/wiki/List_of_open-source_video_games
- https://beyondallreason.info/
- https://wz2100.net/

---

## 3. Qué quiero saber, en orden de valor

1. **Números de la economía AoE2**: cantidad de cada depósito (árbol, mina de
   oro, mina de piedra, ciervo, jabalí, bayas), tasa de recolección por
   segundo y por recurso, capacidad de carga del aldeano, distancia y efecto
   del dropoff.
2. **Granjas AoE2**: coste, comida total antes de agotarse, re-siembra, por qué
   son finitas, cómo cambió entre versiones y qué efecto tiene en el ritmo.
3. **Progresión de edades AoE2**: coste y requisitos exactos de Feudal, Castle
   e Imperial; por qué existe el doble gate de coste + edificios.
4. **Comportamiento automático de aldeanos**: qué hacen al agotar un recurso,
   cómo eligen el siguiente depósito, si hay radio de búsqueda, y **qué se
   queja históricamente la gente de ese automatismo**. Esto nos interesa
   muchísimo: acabamos de arreglar un bug donde nuestros aldeanos cruzaban el
   mapa entero.
5. **Widelands**: cómo declara en datos que un edificio consume unos bienes y
   produce otros. Quiero **un ejemplo real de fichero**, pegado.
6. **0 A.D.**: separación simulación/presentación, si usa lockstep
   determinista, cómo define unidades en plantillas, cómo modela las phases.
   Rutas reales del repositorio.
7. **openage**: qué formatos y fórmulas de AoE2 han documentado y dónde. Qué es
   su modelo `nyan` y para qué sirve.
8. **OpenRA**: cómo consigue determinismo, qué problemas concretos tuvieron y
   cómo detectan desincronización.
9. **Metalurgia y energía por edad** en juegos históricos (Empire Earth, Rise
   of Nations, Civilization, Anno, Songs of Syx): cómo distinguen recurso
   recolectado de producido, y **cómo evitan abrumar al jugador** cuando hay
   muchos recursos. Este punto alimenta directamente una decisión abierta
   nuestra.

---

## 4. Formato del entregable — LEE ESTO DOS VECES

El informe lo leerá un modelo con presupuesto de contexto limitado. **La
compresión es parte del encargo, no un extra.** Requisitos duros:

- **Máximo 400 líneas.** Si no cabe, recorta lo genérico, nunca los números.
- **Un hallazgo por línea**, en la forma:
  `[FUENTE] afirmación densa con cifras → [CHUNSA] implicación concreta`
- **Prohibido**: introducciones, "en resumen", "es importante destacar",
  recapitulaciones, cortesías, y repetir la pregunta antes de responderla.
- **Números siempre que existan.** «Los aldeanos cargan poco» no vale;
  «capacidad 10, oro 0,38/s» sí.
- **Marca la confianza** al inicio de cada línea:
  - `[V]` verificado, leído directamente en una fuente que citas
  - `[I]` inferencia tuya a partir de lo leído
  - `[?]` no pudiste confirmarlo
  Una línea `[V]` sin fuente citada es un fallo del encargo.
- **Tablas** para todo lo que sea números comparables. Son más densas que la
  prosa.
- Al final, una sección **`## Aplicable a CHUNSA ya`** con las 10 cosas de
  mayor valor accionable, ordenadas, cada una en una línea, referida a la
  sección de SPEC-007 o SPEC-004 que afecta.
- Y otra sección **`## Contradice nuestra propuesta`**: todo lo que hayas
  encontrado que sugiera que SPEC-007 se equivoca. **Esta sección es la más
  valiosa del informe.** Si está vacía, sospecharé que no buscaste.

Guarda también las páginas crudas que descargues en `docs/research/raw/`, un
fichero por fuente, por si luego hay que verificar algo. Esas no tienen límite
de tamaño; el límite es solo para la síntesis.

---

## 5. Honestidad

No inventes cifras. No presentes conocimiento previo como si lo hubieras
leído. Si una fuente se contradice con otra, **dilo y cita ambas** — eso es
información, no un problema. Si no llegaste a investigar algo de §3, ponlo en
una lista de pendientes al final en vez de improvisar.
