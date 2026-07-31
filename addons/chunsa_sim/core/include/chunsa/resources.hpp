#pragma once

#include <cstdint>

namespace chunsa {

// Sprint 1.8A/1.8B (SPEC-007 §9.3/§18): capacidad estructural completa del
// stock y nombres estables de los tres recursos existentes. Los autores de
// YAML usan record_id; estos slots los asigna el compilador de datos.
// Sprint 1.9C (decision del Director, 2026-07-31): 32 -> 64.
//
// El tope NO era un numero redondo elegido al azar: era el ancho de
// `dropoff_mask`, un uint32 con UN BIT POR RECURSO. Con 30 definidos quedaban
// DOS huecos, y los textiles bien hechos piden cinco o seis —lino egipcio,
// seda china, lana andina, algodon indio, tejido, sinteticas— que es
// justamente lo que diferencia a unas civilizaciones de otras.
//
// Subirlo es barato: duplicar los vectores de coste son unos pocos KB. Lo que
// cuesta es el bump de save y checksum, y por eso se hace UNA vez y con
// margen, no cada vez que aparezca un material.
inline constexpr uint32_t RESOURCE_COUNT = 64u;
inline constexpr uint32_t RESOURCE_INDEX_FOOD = 0u;
inline constexpr uint32_t RESOURCE_INDEX_WOOD = 1u;
inline constexpr uint32_t RESOURCE_INDEX_STONE = 2u;

}  // namespace chunsa
