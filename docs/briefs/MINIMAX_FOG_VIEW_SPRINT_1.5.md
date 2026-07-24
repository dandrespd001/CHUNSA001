# BRIEF MiniMax — helpers puros de fog de guerra · Sprint 1.5A

## Objetivo acotado

Crear dos archivos nuevos, sin tocar ningún archivo existente:

- `addons/chunsa_sim/gdextension/fog_view.hpp`
- `tests/unit/test_fog_view.cpp`

El helper pertenece a presentación. No puede mutar `GameState`, depender de
Godot, asignar heap, usar float, RNG, excepciones ni estado global mutable.

## API literal obligatoria

```cpp
#pragma once

#include <cstdint>

#include <chunsa/vision.hpp>

namespace chunsa::presentation {

enum class FogLevel : uint8_t {
    UNEXPLORED = 0,
    EXPLORED = 1,
    VISIBLE = 2,
};

inline FogLevel fog_level_at(const uint64_t* visible,
                             const uint64_t* explored,
                             uint32_t map_w,
                             uint32_t map_h,
                             uint32_t tx,
                             uint32_t ty) noexcept;

inline bool entity_visible_to_player(uint8_t entity_owner,
                                     uint8_t viewer,
                                     const uint64_t* visible,
                                     uint32_t map_w,
                                     uint32_t map_h,
                                     int64_t x_raw,
                                     int64_t y_raw) noexcept;

inline FogLevel fog_block_level(const uint64_t* visible,
                                const uint64_t* explored,
                                uint32_t map_w,
                                uint32_t map_h,
                                uint32_t block_tx,
                                uint32_t block_ty,
                                uint32_t block_size) noexcept;

}  // namespace chunsa::presentation
```

## Contrato

1. Los bitsets usan el layout de `chunsa::VisionGrid`: stride fijo
   `VIS_AXIS == 256`, bit `ty * VIS_AXIS + tx`.
2. `fog_level_at`:
   - puntero nulo o coordenada fuera de `map_w/map_h` => `UNEXPLORED`;
   - bit visible => `VISIBLE`;
   - si no visible pero sí explored => `EXPLORED`;
   - resto => `UNEXPLORED`.
3. `entity_visible_to_player`:
   - entidad propia (`entity_owner == viewer`) => `true`, incluso si el bitset
     es nulo o inconsistente;
   - enemigo => convertir Q47.16 a tile con `>> 16`; coordenada negativa,
     fuera del mapa o puntero visible nulo => `false`;
   - enemigo dentro del mapa => `true` sólo si su tile está `VISIBLE`.
4. `fog_block_level`:
   - `block_size == 0`, puntero visible/explored nulo o inicio fuera del mapa
     => `UNEXPLORED`;
   - recorta el bloque al borde útil del mapa;
   - si cualquier tile es visible => `VISIBLE`;
   - en otro caso, si cualquier tile fue explorado => `EXPLORED`;
   - resto => `UNEXPLORED`.
5. Todo `inline`, `noexcept`, C++17, sin warnings con `-Wall -Wextra -Werror
   -Wconversion -Wsign-conversion`.

## Tests obligatorios

`tests/unit/test_fog_view.cpp` debe ser autocontenido, con `main()` y devolver
0 sólo si todos los casos pasan. Cubrir:

- precedencia visible > explored;
- tile explorado no visible;
- tile nunca explorado;
- bounds y punteros nulos;
- propio siempre visible;
- enemigo visible/no visible;
- Q47.16 y coordenadas negativas;
- bloque mixto visible;
- bloque sólo explorado;
- bloque desconocido;
- recorte de bloque en borde `(255,255)`;
- `block_size == 0`.

Salida de éxito:

```text
fog_view: 0 fallos
```

## Exclusiones

- No modificar `vision.hpp`, el kernel, CMake, Godot ni documentación.
- No implementar memoria de última posición, IA justa por fog, textura,
  render, audio ni pacing.
- No renombrar ni ampliar la API.
