#pragma once

// Baselines canónicos de determinismo.
//
// Procedimiento de actualización: ejecuta el gate canónico afectado, confirma
// primero que el resultado funcional y las invariantes del escenario siguen
// siendo correctos, y sustituye aquí únicamente el valor hexadecimal medido.
// Todo cambio de baseline debe explicar en el commit que lo introduce qué
// cambio de dominio o trayectoria lo hace intencional; nunca se actualiza solo
// para volver verde una prueba.

#include <cstdint>

namespace chunsa::determinism_baselines {

// Vectores dorados Fixed64 + normalize_v1 de tests/determinism/golden.
inline constexpr long GOLDEN_VECTOR_CASES = 1074;

// G1: synthetic_movement_v1@1, 600 unidades, 2000 ticks, seed 20260716.
// Cambió solo por el bump V8→V9; alloc_delta=0 y doble corrida idéntica.
//
// Sprint 1.29 (ECO_MAX_DEPOSITS 32 -> 64): SOLO cambian los hashes, porque
// checksum.hpp recorre TODOS los ECO_MAX_DEPOSITS slots de deposits[] y ahora
// son 64 en vez de 32. Determinismo intacto (doble corrida idéntica en los
// seis gates) y end_ticks inalterados —1227, 1108, 10473—.
//
// Sprint 1.45 (bosques como zonas): CAMBIO DE DOMINIO, no de trayectoria.
// El checksum sube a V15 (EcoDeposit gana radius_raw/initial_amount). Doble
// corrida idéntica y alloc_delta=0, como siempre; solo cambia el digest.
inline constexpr uint64_t G1_SYNTHETIC_STATE = 0x7cf54d12f5281c6full;

// G3: savetest canónico sin IA, save@200 y continuación hasta tick 400.
// Cambió solo por el bump V8→V9; save/load conserva la continuación.
// Sprint 1.45: re-registrado por el bump V14→V15 (dominio, no trayectoria).
inline constexpr uint64_t G3_SAVETEST_STATE = 0x630ddb81724ce9d0ull;
inline constexpr uint64_t G3_SAVETEST_CONTINUATION = 0xcb1ecd05ec77cb71ull;

// G4: savetest canónico con IA, save@200 y continuación hasta tick 400.
// Cambió solo por el bump V8→V9; save/load con IA conserva la continuación.
// Sprint 1.45: re-registrado por el bump V14→V15 (dominio, no trayectoria).
inline constexpr uint64_t G4_SAVETEST_AI_STATE = 0x22fc67229df9cb57ull;
inline constexpr uint64_t G4_SAVETEST_AI_CONTINUATION = 0xf081b518bffd1528ull;

// SPEC-005 §8.3: skirmish militar sin ciudadanos.
// Cambió solo por el bump V8→V9; winner=1 y end_tick=1226 intactos.
// Sprint 1.45: re-registrado por el bump V14→V15; end_tick=1227 intacto.
//
// Sprint 2026-08-04 (pánico permanente): re-registro por CAMBIO DE
// COMPORTAMIENTO, no de dominio — la zona muerta de la moral se cierra (todo
// estado debe tener salida) y el acorralado se planta y pelea. No sube
// CHECKSUM_ALGO_VERSION: el dominio hasheado no cambia, cambia la trayectoria.
// winner=1 y fin <36000 siguen intactos; la partida termina 24 ticks antes
// (1227 → 1203) porque la moral ya no congela a las unidades en pánico.
inline constexpr uint64_t AI_SKIRMISH_STATE = 0x38e1e69c96f6e122ull;
inline constexpr uint64_t AI_SKIRMISH_CONTINUATION = 0x6c57fe9ff02ca876ull;

// SPEC-004 §7.1: skirmish con economía y ciudadanos vulnerables.
// Sprint 1.7 §23: trayectoria nueva por zona aliada y depósito base del
// fixture sintético; conserva economía real, winner=1 y fin <36000.
// Sprint 1.8A: hashes cambiados solo por V8→V9; end_tick=1107 intacto.
// Sprint 1.45: re-registrado por el bump V14→V15; end_tick=1123 intacto.
//
// Sprint 2026-08-04 (pánico permanente): re-registro por CAMBIO DE
// COMPORTAMIENTO, mismo motivo que ai_skirmish — sin bump de
// CHECKSUM_ALGO_VERSION (el dominio no cambia). winner=1 y fin <36000
// intactos; end_tick 1123 → 1118, 5 ticks antes por la moral sin zona muerta.
inline constexpr uint64_t AI_SKIRMISH_ECO_STATE = 0xd9aacf45961a5085ull;
inline constexpr uint64_t AI_SKIRMISH_ECO_CONTINUATION = 0x768450812606b778ull;
inline constexpr uint32_t AI_SKIRMISH_ECO_END_TICK = 1118u;

// SPEC-004 §20/§22: apertura económica completa con control de ciudadano.
// Sprint 1.7 §23: la auto-recolección acotada evita marchas a neutrales
// remotos; winner=1 y las cuatro fases se conservan, fin 12292→9317.
// Sprint 1.8A: hashes cambiados solo por V8→V9; end_tick=9317 intacto.
// Sprint 1.9C (RESOURCE_COUNT 32 -> 64 y seis textiles): SOLO cambian los
// hashes, por el bump a checksum V12 y save V17. Los end_tick quedan intactos
// —1227, 1108, 10473— porque los textiles todavia no los consume nada.
//
// Sprint 1.9B (mena de hierro en el mapa): dos depositos mas cambian el reparto
// de aldeanos y la apertura se acorta 11160 -> 10473. winner=1 y las cuatro
// fases, intactas.
//
// Sprint 1.19 fase B (la IA usa las ordenes de combate): desplazamientos
// pequenos y en la direccion esperada —skirmish 1226->1227, eco 1107->1108,
// apertura 11001->11160—. La IA pelea de forma mas ordenada: fuego focalizado
// al atacar y ATTACK_MOVE al replegarse. winner=1 y las cuatro fases, intactas.
//
// Sprint 1.13 fase C (proyectiles SIN persecucion, correccion del Director):
// la flecha vuela al punto PREDICHO y puede FALLAR si el objetivo se aparta,
// como en AoE2. La apertura se alarga 9542 -> 11001 justamente por eso: el
// combate a distancia deja de ser certero. winner=1 y las cuatro fases,
// intactas. Es el efecto BUSCADO, no una regresion.
//
// Sprint 1.13 fase B (proyectiles con viaje): los hashes vuelven a moverse
// —n_projectiles y el array entran al dominio— pero TODOS los end_tick siguen
// intactos, incluida la apertura en 9542. Eso NO estaba garantizado: con la
// primera version, que volaba en linea recta, los proyectiles no impactaban
// nunca contra objetivos en movimiento y la apertura se iba a 11001. Con el
// proyectil PERSIGUIENDO al objetivo, el ritmo de combate vuelve al de antes.
//
// Sprint 1.13 fase A (ordenes de combate): SOLO cambian los hashes, por el bump del
// checksum a V11 (attack_target/order_mode entran al dominio). TODOS los
// end_tick quedan intactos —1226, 1107, 9542— porque el sistema de ordenes no
// hace nada cuando nadie da ordenes, que es el caso de los escenarios de gate.
//
// Sprint 1.18 (armadura por tipo de dano): el combate deja de usar el
// multiplicador opaco rps_mult y pasa a `attack - armadura + bono`. Solo se
// movio la APERTURA (9411 -> 9542), que es el unico escenario con catalogo
// real; G1/G3/G4, skirmish y eco quedaron BIT-IDENTICOS porque sus fixtures
// sinteticos no llevan armadura ni bonos. winner=1 y las cuatro fases, intactos.
//
// Sprint 1.9 (recetas y CRAFT): el checksum sube a V10 (craft_recipe/
// craft_progress entran al dominio) y el mapa gana un par espejado de ESTAÑO
// sin el cual el bronce es infabricable, asi que la trayectoria se mueve:
// 9438 -> 9411. winner=1 y las CUATRO fases se conservan.
//
// Sprint 1.8D (contenido, depósitos y costes reales): trayectorias se mueven
// INTENCIONALMENTE — los nuevos depósitos cubren las épocas 1-4 (cobre/oro/
// arcilla/sal en el centro, food/wood/stone en zona propia con cantidades
// 3x) y los costes de unidades/edificios se redimensionan (≤3 recursos cada
// uno, sin nuevos recursos en coste para no romper regresiones en el catálogo
// golden). winner=1, las cuatro fases observadas, fin 9438<36000. Los
// hashes y el end_tick son los medidos por el gate canónico contra el CHDB
// recompilado — re-registrados con justificación en docs/RESULT_MINIMAX_1.8D.md.
// Sprint 1.22 (epocas 1-5 jugables): los dos hashes de la APERTURA se
// re-registran, y el motivo NO es que cambiara ninguna regla del kernel. El
// catalogo pasa de 8 a 16 edificios y de 5 a 7 unidades, y `BuildingId`/
// `UnitId` son EL INDICE en orden bytewise de record_id: meter
// `egipto:flint_workshop` o `rome:epigravettian_camp` REORDENA los
// identificadores de los edificios que ya existian. A eso se suman 4
// depositos nuevos en el mapa (lino x2, lana x2), que cambian la economia
// desde el tick 0. Cualquiera de las dos cosas basta para mover el hash.
//
// end_tick se queda en 10473 y winner=1: la partida se juega igual, lo que
// cambia es la numeracion interna. Esa es justo la comprobacion que separa
// "cambio de datos" de "cambio de comportamiento", y por eso el end_tick NO
// se toca aqui.
// Sprint 1.25 (marco de 15 epocas): mismo caso que el 1.22 y por el mismo
// motivo. El catalogo pasa de 16 a 36 edificios y BuildingId es EL INDICE en
// orden bytewise de record_id, asi que los identificadores de los edificios
// que ya existian se reordenan. end_tick sigue en 10473 y winner=1: la partida
// se juega igual, cambia la numeracion.
//
// Re-registrados otra vez en el 1.26 por la MISMA causa y no por una nueva:
// cinco edificios de andamiaje pasaron a tener nombre propio (nilometer,
// murano_glassworks, suez_canal, pirelli_works, aswan_dam) y renombrar
// reordena el indice igual que anadir. end_tick 10473 y winner=1, otra vez
// intactos: es la comprobacion que distingue renombrar de cambiar el juego.
//
// Y una vez mas en el 1.26 al ascender seis celdas mas (horreum_annonae,
// wikala, opificio_pietre_dure, pavia_physics_cabinet, misr_spinning,
// olivetti_works). Tres re-registros seguidos con end_tick 10473 y winner=1
// invariables: eso ya no es casualidad, es la firma de un cambio que solo
// toca NOMBRES. Si alguna vez el end_tick se mueve con un renombrado, ahi hay
// un fallo de verdad que buscar.
//
// Sprint 1.33 — cambio de DOMINIO otra vez, no de comportamiento. Los precios
// de mercado entran al checksum y al guardado (SAVE_FORMAT 18->19,
// CHECKSUM_ALGO 13->14) porque son ESTADO: cambian con cada operacion y
// condicionan las siguientes. Sin guardarlos, una partida reanudada volveria al
// precio base y el jugador recuperaria gratis un mercado que habia hundido.
//
// Los end_tick no se mueven: nadie comercia todavia en los escenarios.
//
// Sprint 1.32 — ESTE re-registro SI es un cambio de comportamiento, y conviene
// distinguirlo de todos los anteriores. La saturacion por deposito hace que
// amontonar aldeanos rinda menos por cabeza, asi que el escenario de economia
// tarda mas: end_tick 1108 -> 1123. Quince ticks. Winner sigue siendo 1.
//
// Los renombrados movian el hash y dejaban el end_tick quieto. Este mueve los
// dos, y eso es exactamente lo que debe pasar cuando entra una REGLA NUEVA. Si
// no se hubiera movido, habria que preguntarse si la regla hace algo.
//
// Sprint 1.28 — re-registro por CAMBIO DE DOMINIO, no por renombrado. Suben
// SAVE_FORMAT_VERSION (17->18) y CHECKSUM_ALGO_VERSION (12->13) porque
// ECO_MAX_DEPOSITS pasa de 32 a 64 y EcoDeposit gana cinco campos
// (regeneracion, techo, dueno, reserva y capacidad que la abre). El guardado
// escribe ECO_MAX_DEPOSITS entradas: un save viejo tiene 32 y el lector nuevo
// espera 64, asi que sin el bump se malinterpretaria en silencio. Mismo
// precedente exacto que el 1.9C con RESOURCE_COUNT.
//
// Los end_tick NO se mueven: 1227, 1108 y 10473 siguen igual. Cambia lo que se
// hashea, no como se juega.
//
// Sprint 1.14 — ESTE re-registro es de otra naturaleza y conviene no
// confundirlo con los anteriores. Los del 1.22, 1.25 y 1.26 eran renombrados:
// cambiaba la numeracion y nada mas. Este cambia DOS REGLAS de verdad: el tope
// de poblacion pasa a construirse (SPEC-004 §11.3) y la IA aprende a levantar
// viviendas (AI_ALGO_VERSION 7->8). Que los hashes se muevan era inevitable.
//
// Lo que SI merece atencion: end_tick sigue en 10473 y winner=1. Una regla que
// limita el crecimiento y un procedimiento de IA nuevo, y la apertura dura
// exactamente lo mismo. No es casualidad: la IA construye casas a tiempo y no
// llega a quedarse bloqueada, asi que su ritmo no cambia. Si el end_tick se
// hubiera disparado, habria significado que la IA se atasca esperando
// poblacion — y eso si habria sido un fallo que corregir, no un baseline que
// re-registrar.
//
// Sprint 1.45 — re-registro por CAMBIO DE DOMINIO, no por comportamiento.
// Suben SAVE_FORMAT_VERSION (19->20) y CHECKSUM_ALGO_VERSION (14->15) porque
// EcoDeposit gana radius_raw/initial_amount (bosques como zonas) y ambos
// entran al guardado y al checksum. end_tick sigue en 10473 y winner=1: la
// partida se juega igual, cambia lo que se hashea. La comprobacion de que el
// orden de seleccion de depositos NO se movio es justo que ningun end_tick
// cambie: si la distancia al borde hubiera reordenado candidatos, la economia
// de la apertura habria cambiado desde el tick 0.
//
// Sprint 1.46 — re-registro por CAMBIO DE DOMINIO, no por comportamiento, y
// a diferencia del 1.45 ESTA vez el dominio SI cambia de verdad. El mapa gana
// 6 bosques con radius_millitiles (22 -> 28 depositos), asi que la partida se
// juega distinta desde el tick 0 y el CHDB sube a formato 1.2 / schema_set 3.
// Los hashes se mueven (67b9b900 -> c1697c1f, bb8abcf3 -> 2c6bcaa2) POR ESO.
// El end_tick sigue en 10473 y winner=1: la apertura (época 5 fijada) sigue
// terminando con vencedor y sin dispararse. La madera extra de los bosques no
// era el cuello de botella de este escenario (lo es del banco, que arranca en
// la época 1 y no se fija la época — ahí es donde se mide).
// Sprint 1.49 — re-registro por CAMBIO DE COMPORTAMIENTO, y del bueno: el
// suelo de contacto (MELEE_CONTACT_RAW = 1 tile) hace que las armas cuerpo a
// cuerpo GOLPEEN. Seis de las nueve unidades del catalogo tenian
// range_millitiles: 0, y con eso el filtro de combate exigia d2==0 —el enemigo
// en la MISMA coordenada raw—, asi que solo peleaban por coincidencia de
// apilamiento.
//
// EL END_TICK BAJA DE 10473 A 9963, y esa direccion es la que confirma que el
// arreglo es correcto: la partida se resuelve ANTES porque el combate por fin
// conecta. Si hubiera subido, o si el vencedor hubiera cambiado, seria un
// fallo. winner=1 intacto.
// Sprint 1.52 — re-registro por CAMBIO DE DOMINIO. El catalogo gana 6
// edificios defensivos (64 -> 70), y BuildingId es el INDICE en orden bytewise
// de record_id: anadir edificios reordena los ids y mueve el digest de todo
// estado que los contenga. end_tick sigue en 9963 y winner=1, o sea que la
// partida se juega igual. Precedente identico documentado arriba para el 1.45.
inline constexpr uint64_t AI_SKIRMISH_APERTURA_STATE = 0x854eb72c765c9cc1ull;
inline constexpr uint64_t AI_SKIRMISH_APERTURA_CONTINUATION = 0x9306bf2568e5eda2ull;
inline constexpr uint32_t AI_SKIRMISH_APERTURA_END_TICK = 9963u;

}  // namespace chunsa::determinism_baselines
