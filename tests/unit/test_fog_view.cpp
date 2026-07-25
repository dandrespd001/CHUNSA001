#include <cstdint>
#include <iostream>

#include "fog_view.hpp"

namespace {

using chunsa::VIS_AXIS;
using chunsa::VIS_WORDS;
using chunsa::presentation::FogLevel;

void set_bit(uint64_t* bits, uint32_t tx, uint32_t ty) noexcept {
    const uint32_t bit = ty * VIS_AXIS + tx;
    bits[bit >> 6] |= uint64_t{1} << (bit & 63u);
}

void expect(bool condition, const char* name, int& failures) {
    if (!condition) {
        std::cerr << "FALLO: " << name << '\n';
        ++failures;
    }
}

}  // namespace

int main() {
    uint64_t visible[VIS_WORDS] = {};
    uint64_t explored[VIS_WORDS] = {};
    int failures = 0;

    set_bit(explored, 10u, 20u);
    set_bit(visible, 10u, 20u);
    expect(chunsa::presentation::fog_level_at(
               visible, explored, 256u, 256u, 10u, 20u) == FogLevel::VISIBLE,
           "visible tiene precedencia sobre explored", failures);

    set_bit(explored, 11u, 20u);
    expect(chunsa::presentation::fog_level_at(
               visible, explored, 256u, 256u, 11u, 20u) == FogLevel::EXPLORED,
           "tile explorado no visible", failures);
    expect(chunsa::presentation::fog_level_at(
               visible, explored, 256u, 256u, 12u, 20u) == FogLevel::UNEXPLORED,
           "tile nunca explorado", failures);

    expect(chunsa::presentation::fog_level_at(
               nullptr, explored, 256u, 256u, 10u, 20u) == FogLevel::UNEXPLORED,
           "visible nulo", failures);
    expect(chunsa::presentation::fog_level_at(
               visible, nullptr, 256u, 256u, 10u, 20u) == FogLevel::UNEXPLORED,
           "explored nulo", failures);
    expect(chunsa::presentation::fog_level_at(
               visible, explored, 10u, 10u, 10u, 0u) == FogLevel::UNEXPLORED,
           "tile fuera de ancho", failures);
    expect(chunsa::presentation::fog_level_at(
               visible, explored, 10u, 10u, 0u, 10u) == FogLevel::UNEXPLORED,
           "tile fuera de alto", failures);

    expect(chunsa::presentation::entity_visible_to_player(
               0u, 0u, nullptr, 0u, 0u, -1, -1),
           "entidad propia siempre visible", failures);
    expect(chunsa::presentation::entity_visible_to_player(
               1u, 0u, visible, 256u, 256u,
               static_cast<int64_t>(10u) << 16,
               static_cast<int64_t>(20u) << 16),
           "enemigo en tile visible", failures);
    expect(!chunsa::presentation::entity_visible_to_player(
               1u, 0u, visible, 256u, 256u,
               static_cast<int64_t>(11u) << 16,
               static_cast<int64_t>(20u) << 16),
           "enemigo en tile no visible", failures);
    expect(chunsa::presentation::entity_visible_to_player(
               1u, 0u, visible, 256u, 256u,
               (static_cast<int64_t>(10u) << 16) + 65535,
               (static_cast<int64_t>(20u) << 16) + 12345),
           "conversion Q47.16", failures);
    expect(!chunsa::presentation::entity_visible_to_player(
               1u, 0u, visible, 256u, 256u, -1, 0),
           "coordenada x negativa", failures);
    expect(!chunsa::presentation::entity_visible_to_player(
               1u, 0u, visible, 256u, 256u, 0, -65536),
           "coordenada y negativa", failures);
    expect(!chunsa::presentation::entity_visible_to_player(
               1u, 0u, nullptr, 256u, 256u, 0, 0),
           "enemigo con visible nulo", failures);
    expect(!chunsa::presentation::entity_visible_to_player(
               1u, 0u, visible, 16u, 16u,
               static_cast<int64_t>(16u) << 16, 0),
           "enemigo fuera del mapa", failures);

    uint64_t block_visible[VIS_WORDS] = {};
    uint64_t block_explored[VIS_WORDS] = {};
    set_bit(block_explored, 4u, 4u);
    set_bit(block_visible, 5u, 5u);
    expect(chunsa::presentation::fog_block_level(
               block_visible, block_explored, 256u, 256u, 4u, 4u, 4u) ==
               FogLevel::VISIBLE,
           "bloque mixto visible", failures);

    uint64_t explored_only_visible[VIS_WORDS] = {};
    uint64_t explored_only[VIS_WORDS] = {};
    set_bit(explored_only, 32u, 33u);
    expect(chunsa::presentation::fog_block_level(
               explored_only_visible, explored_only,
               256u, 256u, 32u, 32u, 4u) == FogLevel::EXPLORED,
           "bloque solo explorado", failures);
    expect(chunsa::presentation::fog_block_level(
               explored_only_visible, explored_only,
               256u, 256u, 40u, 40u, 4u) == FogLevel::UNEXPLORED,
           "bloque desconocido", failures);

    uint64_t edge_visible[VIS_WORDS] = {};
    uint64_t edge_explored[VIS_WORDS] = {};
    set_bit(edge_explored, 255u, 255u);
    expect(chunsa::presentation::fog_block_level(
               edge_visible, edge_explored,
               256u, 256u, 255u, 255u, 64u) == FogLevel::EXPLORED,
           "recorte en borde 255,255", failures);
    expect(chunsa::presentation::fog_block_level(
               edge_visible, edge_explored,
               256u, 256u, 255u, 255u, 0u) == FogLevel::UNEXPLORED,
           "block_size cero", failures);
    expect(chunsa::presentation::fog_block_level(
               nullptr, edge_explored,
               256u, 256u, 255u, 255u, 1u) == FogLevel::UNEXPLORED,
           "bloque con visible nulo", failures);
    expect(chunsa::presentation::fog_block_level(
               edge_visible, nullptr,
               256u, 256u, 255u, 255u, 1u) == FogLevel::UNEXPLORED,
           "bloque con explored nulo", failures);
    expect(chunsa::presentation::fog_block_level(
               edge_visible, edge_explored,
               255u, 255u, 255u, 255u, 1u) == FogLevel::UNEXPLORED,
           "inicio de bloque fuera del mapa", failures);

    std::cout << "fog_view: " << failures << " fallos\n";
    return failures == 0 ? 0 : 1;
}
