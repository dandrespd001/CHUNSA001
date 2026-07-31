// Sprint 1.8C: catálogo tipado de metadatos de recurso.
// TDD obligatorio: este test se escribió y ejecutó contra stubs antes de
// implementar la reconstrucción de ResourceDefinitionV1.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

#include "chunsa/data_catalog.hpp"

#ifndef CHUNSA_GOLDEN_CHDB_PATH
#error "CHUNSA_GOLDEN_CHDB_PATH debe definirse via CMake"
#endif

static int g_fails = 0;

#define CHECK_EQ_U64(expected, obtained, label) do { \
    const uint64_t expected_value = static_cast<uint64_t>(expected); \
    const uint64_t obtained_value = static_cast<uint64_t>(obtained); \
    if (expected_value != obtained_value) { \
        ++g_fails; \
        std::printf("CHECK_EQ %s: esperado=%llu obtenido=%llu\n", label, \
                    static_cast<unsigned long long>(expected_value), \
                    static_cast<unsigned long long>(obtained_value)); \
    } \
} while (0)

using namespace chunsa;

static std::vector<uint8_t> read_all(const char* path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<uint8_t>(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>());
}

static ResourceId find_resource(const DataCatalogV1& catalog, const char* id) {
    return catalog_find_resource(catalog, id, std::strlen(id));
}

static const ResourceDefinitionV1* definition_or_null(
        const DataCatalogV1& catalog, const char* id) {
    const ResourceId resource_id = find_resource(catalog, id);
    if (resource_id == INVALID_RESOURCE_ID || resource_id >= catalog.resource_count) {
        return nullptr;
    }
    return &catalog.resources[resource_id];
}

static bool replace_ascii_once(
        std::vector<uint8_t>& bytes, const char* from, const char* to) {
    const size_t size = std::strlen(from);
    if (size != std::strlen(to)) return false;
    for (size_t pos = 0; pos + size <= bytes.size(); ++pos) {
        if (std::memcmp(bytes.data() + pos, from, size) == 0) {
            std::memcpy(bytes.data() + pos, to, size);
            return true;
        }
    }
    return false;
}

static bool replace_first_epoch(std::vector<uint8_t>& bytes, uint64_t epoch) {
    static constexpr char key[] = "appearance_epoch";
    constexpr size_t key_size = sizeof(key) - 1u;
    for (size_t pos = 0; pos + key_size + 9u <= bytes.size(); ++pos) {
        if (std::memcmp(bytes.data() + pos, key, key_size) != 0) continue;
        const size_t value_pos = pos + key_size;
        if (bytes[value_pos] != 0x10u) continue;  // CVE1 int64
        for (uint32_t byte = 0; byte < 8; ++byte) {
            bytes[value_pos + 1u + byte] =
                static_cast<uint8_t>((epoch >> (8u * byte)) & 0xFFu);
        }
        return true;
    }
    return false;
}

static void expect_invalid_resource(
        const std::vector<uint8_t>& bytes, const char* label) {
    DataCatalogStorageV1 storage;
    const CatalogLoadCode code = catalog_load_bytes_v1(
        bytes.data(), bytes.size(), CatalogLoadProfile::Verified, storage);
    CHECK_EQ_U64(
        static_cast<uint8_t>(CatalogLoadCode::InvalidResource),
        static_cast<uint8_t>(code),
        label);
}

int main() {
    DataCatalogStorageV1 storage;
    const CatalogLoadCode load_code = catalog_load_file_v1(
        CHUNSA_GOLDEN_CHDB_PATH, CatalogLoadProfile::Verified, storage);
    CHECK_EQ_U64(
        static_cast<uint8_t>(CatalogLoadCode::Ok),
        static_cast<uint8_t>(load_code),
        "golden_load_code");
    CHECK_EQ_U64(1u, storage.valid() ? 1u : 0u, "golden_storage_valid");
    if (!storage.valid()) {
        std::printf("resource_catalog: %d fallos\n", g_fails);
        return 1;
    }

    const DataCatalogV1& catalog = storage.catalog();
    CHECK_EQ_U64(36u, catalog.resource_count, "resource_count");

    const ResourceDefinitionV1* copper =
        definition_or_null(catalog, "chunsa:copper");
    CHECK_EQ_U64(
        static_cast<uint8_t>(ResourceFamilyV1::BaseMetals),
        copper ? static_cast<uint8_t>(copper->family) : 0u,
        "copper_family");

    struct EpochCase {
        const char* id;
        uint8_t expected;
    };
    static constexpr EpochCase EPOCH_CASES[] = {
        {"chunsa:food", 1u},
        {"chunsa:steel", 12u},
        {"chunsa:uranium", 14u},
    };
    for (const EpochCase& test : EPOCH_CASES) {
        const ResourceDefinitionV1* definition =
            definition_or_null(catalog, test.id);
        CHECK_EQ_U64(
            test.expected,
            definition ? definition->appearance_epoch : 0u,
            test.id);
    }

    const ResourceDefinitionV1* bronze =
        definition_or_null(catalog, "chunsa:bronze");
    CHECK_EQ_U64(
        static_cast<uint8_t>(ResourceNatureV1::Produced),
        bronze ? static_cast<uint8_t>(bronze->nature) : 0u,
        "bronze_nature");
    CHECK_EQ_U64(
        static_cast<uint8_t>(ResourceNatureV1::Collected),
        copper ? static_cast<uint8_t>(copper->nature) : 0u,
        "copper_nature");

    uint32_t populated = 0;
    for (uint32_t resource = 0; resource < catalog.resource_count; ++resource) {
        const ResourceDefinitionV1& definition = catalog.resources[resource];
        if (definition.display_name_key_utf8 != nullptr
                && definition.display_name_key_bytes != 0
                && definition.family != ResourceFamilyV1::Invalid
                && definition.appearance_epoch != 0
                && definition.nature != ResourceNatureV1::Invalid) {
            ++populated;
        }
    }
    CHECK_EQ_U64(36u, populated, "resources_with_four_metadata_fields");

    struct IndexCase {
        const char* id;
        uint8_t expected;
    };
    static constexpr IndexCase INDEX_CASES[] = {
        // Sprint 1.9C: los seis textiles se intercalan ALFABETICAMENTE y
        // renumeran todo lo que va detras. Es el comportamiento correcto del
        // compilador —el indice es determinista y reproducible— pero implica
        // que anadir un recurso SIEMPRE obliga a subir save y checksum.
        {"chunsa:food", 0}, {"chunsa:wood", 1}, {"chunsa:stone", 2},
        {"chunsa:aluminum", 3}, {"chunsa:bauxite", 4}, {"chunsa:bronze", 5},
        {"chunsa:cement", 6}, {"chunsa:charcoal", 7}, {"chunsa:clay", 8},
        {"chunsa:cloth", 9}, {"chunsa:coal", 10}, {"chunsa:coke", 11},
        {"chunsa:copper", 12}, {"chunsa:cotton", 13}, {"chunsa:flax", 14},
        {"chunsa:gold", 15}, {"chunsa:gunpowder", 16}, {"chunsa:iron_ore", 17},
        {"chunsa:lead", 18}, {"chunsa:limestone", 19}, {"chunsa:nitre", 20},
        {"chunsa:nitrogen_fixed", 21}, {"chunsa:oil", 22}, {"chunsa:oil_products", 23},
        {"chunsa:quicklime", 24}, {"chunsa:rare_earths", 25}, {"chunsa:salt", 26},
        {"chunsa:silicon", 27}, {"chunsa:silk", 28}, {"chunsa:steel", 29},
        {"chunsa:sulfur", 30}, {"chunsa:synthetic_fiber", 31}, {"chunsa:tin", 32},
        {"chunsa:uranium", 33}, {"chunsa:wool", 34}, {"chunsa:wrought_iron", 35},
    };
    for (const IndexCase& test : INDEX_CASES) {
        const ResourceDefinitionV1* definition =
            definition_or_null(catalog, test.id);
        CHECK_EQ_U64(
            test.expected,
            definition ? definition->index : UINT64_MAX,
            test.id);
    }
    CHECK_EQ_U64(
        INVALID_RESOURCE_ID,
        find_resource(catalog, "chunsa:missing"),
        "unknown_resource");

    const std::vector<uint8_t> golden = read_all(CHUNSA_GOLDEN_CHDB_PATH);
    {
        std::vector<uint8_t> invalid_family = golden;
        CHECK_EQ_U64(
            1u,
            replace_ascii_once(invalid_family, "base_metals", "invalidxxxx") ? 1u : 0u,
            "mutate_family_fixture");
        expect_invalid_resource(invalid_family, "invalid_family_load_code");
    }
    for (const uint64_t invalid_epoch : {0u, 16u}) {
        std::vector<uint8_t> invalid = golden;
        CHECK_EQ_U64(
            1u,
            replace_first_epoch(invalid, invalid_epoch) ? 1u : 0u,
            "mutate_epoch_fixture");
        expect_invalid_resource(
            invalid,
            invalid_epoch == 0u
                ? "appearance_epoch_zero_load_code"
                : "appearance_epoch_above_15_load_code");
    }

    std::printf(
        "resource_catalog: %s (%d fallos)\n",
        g_fails == 0 ? "OK" : "FAIL",
        g_fails);
    return g_fails == 0 ? 0 : 1;
}
