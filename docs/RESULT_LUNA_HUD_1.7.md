# Resultado — Luna HUD Nombres (Sprint 1.7A)

Fecha: 2026-07-28
Rama: `gpt/hud-nombres-1.7`

## Cambios

- Añadido `U(const char*)`, basado en `godot::String::utf8`, y aplicado a
  todos los literales no ASCII del adaptador, incluidos los mensajes de
  `UtilityFunctions::print`. El diagnóstico ya sale como `CHUNSA catálogo OK`
  y `estado ilegal`, sin reinterpretación Latin-1.
- El snapshot conserva `unit_id`, estado de tech/civilización y el adaptador
  mantiene una predicción de presentación asociada a cada secuencia. La orden
  se sigue enviando siempre al kernel; la predicción solo enriquece el receipt.
- Los nombres de selección, acciones, colas y etiquetas del mundo pasan por
  una tabla provisional. Si falta un registro, el fallback quita la civ,
  cambia `_` por espacios, capitaliza el inicio y nunca devuelve el ID crudo ni
  una cadena vacía.
- Los receipts `ILLEGAL_STATE` detallan todas las causas conocidas: época,
  faltantes exactos de `A/B/Me`, cola llena, población, edificio incompleto,
  investigación/prerrequisitos y civilización cuando el snapshot lo permite.
  Si el kernel acepta una orden que el adaptador predijo bloqueada, se emite un
  warning explícito.

## Tabla provisional de presentación

Se elimina cuando exista localización real, porque `display_name_key` todavía
no llega al blob y mantenerla entonces duplicaría la fuente de verdad.

| Tipo | `record_id` | Nombre |
|---|---|---|
| Unidad | `egipto:chariot_warrior` | Guerrero de carros |
| Unidad | `egipto:work_crew` | Cuadrilla de trabajo |
| Unidad | `rome:ballista_crew` | Equipo de balista |
| Unidad | `rome:camp_work_crew` | Cuadrilla de campamento |
| Unidad | `rome:legionary` | Legionario |
| Edificio | `egipto:chariotry_stable` | Establo de carros |
| Edificio | `egipto:settlement_center` | Centro de asentamiento |
| Edificio | `egipto:shena_granary` | Granero Shena |
| Edificio | `rome:castra_barracks` | Cuartel Castra |
| Edificio | `rome:forum_center` | Foro romano |
| Edificio | `rome:horreum` | Horreum |
| Tecnología | `egipto:composite_bow_program` | Programa de arco compuesto |
| Tecnología | `egipto:corvee_logistics` | Logistica de corvea |
| Tecnología | `rome:marching_drill` | Instruccion de marcha |
| Tecnología | `rome:road_engineering` | Ingenieria de caminos |
| Civilización | `egipto:dynastic_nile` | Egipto dinastico |
| Civilización | `rome:republic_imperial` | Roma republicana e imperial |

## Ejemplos de feedback

Los mensajes se muestran junto a `Último comando #N` y conservan las
cantidades faltantes, no el coste total:

- `Requiere época 4 (estás en la 3)`
- `Faltan 60 B`
- `Cola de producción llena`
- Cuando concurren causas: `Requiere época 5 (estás en la 3); Cola de producción llena; Faltan 45 A; Faltan 25 B; Faltan 30 Me`

## Verificación

- `cmake --build build-godot -j2`: OK; regeneró
  `demo/bin/libchunsa_godot.so` (6,405,784 bytes).
- `cmake --build build-gcc -j2`: OK.
- `ctest --test-dir build-gcc --output-on-failure`: **25/25 OK**.
- `./build-gcc/chunsa_sim_cli run --selftest-g1`: `fefa48125dd35736`.
- `./build-gcc/chunsa_sim_cli savetest --ai`:
  `774316057e5667fb` / `d52ac0019700684f`.

## Evidencia visual y desviación

El comando solicitado exactamente (`CHUNSA_SHOT=/tmp/hud17 godot --path demo
--quit-after 900`) se ejecutó con el Godot 4.7.1 del sistema durante la
verificación del sprint.
Tras importar la GDExtension, el demo arrancó y mostró `CHUNSA catálogo OK`,
pero la ejecución de 900 iteraciones quedó bloqueada en la espera GPU al tomar
la imagen (`drm_syncobj_array_wait_timeout`), por lo que no se generó
`/tmp/hud17.f600.png`. No se declara esa captura como realizada.

Como evidencia visual reproducible, se ejecutó con el mismo renderer y el `.so`
final:

```text
CHUNSA_UNITS=2 CHUNSA_SHOT=/tmp/hud17-final godot --path demo --quit-after 650 --disable-vsync
```

Terminó con exit 0 y generó [hud17-final.f600.png](/tmp/hud17-final.f600.png).
La imagen fue inspeccionada: `Época`, `Población`, `Aldeanos`, `Establo de
carros` y `Centro de asentamiento` se ven correctamente, sin mojibake.

No se tocaron `addons/chunsa_sim/core/` ni `tests/`, y no se hizo merge.
