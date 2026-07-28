[I] 15 edades imponen carga cognitiva extrema; AoE2 usa 4 -> [CHUNSA] parálisis por análisis y curva de aprendizaje vertical
[I] Recurso "electricidad" como insumo material en receta (Aluminio) rompe modelo escalar -> [CHUNSA] requiere sistema de flujo/energía no definido, deuda técnica masiva
[I] `RESOURCE_COUNT=24` fijo viola principio YAGNI; solo 18 recursos listados -> [CHUNSA] desperdicio de memoria y complejidad de checksum innecesaria desde día 1
[V] §3.1 afirma "distinción de datos no de código" pero §5 requiere lógica de regeneración temporal -> [CHUNSA] contradicción arquitectónica: granjas necesitan estado dinámico, no solo datos estáticos
[I] Recuperación de minas vía `recovery_pct` derivado de `player_caps` ignora coste de investigación -> [CHUNSA] incentiva spam de techs sin riesgo económico, rompiendo balance de economía temprana
[I] Ratio bronce 4:1 arbitrario sin playtesting -> [CHUNSA] puede hacer el estaño irrelevante o cuello de botella impredecible según distribución de mapa
[I] Granjas como depósitos con `extracted` decreciente por tick requieren sincronización de red precisa -> [CHUNSA] alta probabilidad de desincronización en multiplayer si el tick no es determinista a nivel de frame
[I] Subir `ECO_MAX_DEPOSITS` a 128 multiplica iteraciones de pathfinding y gestión de memoria -> [CHUNSA] degradación de rendimiento en partidas largas con muchas granjas
[I] Sprint 1.8 toca save, checksum, catálogo, costes y HUD simultáneamente -> [CHUNSA] riesgo de regresión catastrófica; debería dividirse en migración de datos y actualización de UI
[I] No se define qué pasa con depósitos agotados al cambiar de dueño (conquista) -> [CHUNSA] exploit potencial: capturar mina "agotada" para resetear su estado si la recuperación es por jugador
[I] "Científicamente correcto" en recetas (ej. pólvora 75:10:15) añade fricción sin diversión clara -> [CHUNSA] los jugadores RTS buscan fluidez, no simulación química; simplificar a 2 ingredientes mejora UX
[I] Edades 6, 9, 10 sin recursos nuevos crean valles de progresión económica -> [CHUNSA] sensación de estancamiento; el jugador espera novedad tangible cada edad
[I] `dropoff_mask` a `uint32_t` cambia layout de structs persistentes -> [CHUNSA] invalida saves antiguos incluso si no se usan los bits nuevos, forzando migración obligatoria
[I] No hay mecanismo para "agotar" completamente un recurso geológico (reserve_total) -> [CHUNSA] partidas infinitas posibles si se investiga hasta 100% de recuperación, eliminando presión por expandirse
[I] Dependencia de silicio/tierras raras en Edad 15 sin definir depósitos específicos -> [CHUNSA] ambigüedad en diseño de mapa; ¿son raros? ¿aparecen en todas las edades?
[I] Producción de coque sustituye carbón vegetal pero no se define transición automática -> [CHUNSA] confusión de jugador: ¿debe destruir hornos viejos? ¿se acumulan residuos?
[I] Sistema de capacidades (`player_caps`) como bitmask limita expresividad de recetas complejas -> [CHUNSA] difícil añadir requisitos condicionales (ej. "solo si hay agua cerca") sin hackear el bitmask
[I] No se menciona balance de unidades militares dependientes de recursos producidos -> [CHUNSA] riesgo de que ejércitos de Edad 12 sean imbatibles si la cadena de acero es muy eficiente
[I] "Reabrir minas" premia la expansión territorial inicial sobre la defensa -> [CHUNSA] meta-game agresivo que castiga estilos de juego turtling o defensivo
[I] Falta definición de UI para mostrar `extraíble_ahora` vs `reserve_total` -> [CHUNSA] opacidad informativa; jugador no sabrá si vale la pena investigar recuperación
[I] Sprint 1.10 (recuperación) depende de 1.9 (recetas) pero no al revés -> [CHUNSA] orden de implementación correcto, pero riesgo de integrar mecánicas incompletas si 1.9 se retrasa
[I] No se considera impacto en IA: bots deben evaluar valor de minas "casi agotadas" -> [CHUNSA] IA existente fallará al no entender el valor residual de depósitos con baja recuperación
[I] Ancla histórica de Roma en Edad 5 fuerza a todas las civilizaciones a seguir esa línea temporal -> [CHUNSA] inflexibilidad para civs no europeas o con desarrollos tecnológicos asincrónicos
[I] Uso de `uint8_t` a `uint32_t` para masks aumenta ancho de banda en red por paquete de estado -> [CHUNSA] latencia perceptible en conexiones lentas si se envían masks completas frecuentemente
[I] Granjas regenerativas sin límite de ciclos de vida crean economía infinita local -> [CHUNSA] elimina necesidad de comercio o expansión tardía, reduciendo profundidad estratégica
[I] No se define interacción entre múltiples jugadores explotando mismo depósito -> [CHUNSA] conflicto de estado: ¿la extracción de uno reduce el total del otro? ¿O es independiente por recovery_pct?
[I] "Determinista" en §4.3 asume aritmética entera idéntica en todas las plataformas -> [CHUNSA] riesgo de desincronización si hay diferencias en overflow de enteros entre arquitecturas
[I] 24 recursos exigen 24 iconos, 24 tooltips, 24 balances -> [CHUNSA] carga artística y de diseño multiplicada por 8 respecto a AoE2 (3 recursos)
[I] Falta plan de rollback para Sprint 1.8 si el checksum falla -> [CHUNSA] bloqueo total de desarrollo si la migración de datos tiene errores no detectados
[I] Tecnología de "voladura" reabre minas pero no se define coste de investigación -> [CHUNSA] puede ser demasiado barato, haciendo irrelevantes las minas nuevas del mapa
[I] No se menciona cómo afecta la recuperación a la puntuación final -> [CHUNSA] métricas de victoria distorsionadas si explotar minas viejas da más puntos que conquistar nuevas
