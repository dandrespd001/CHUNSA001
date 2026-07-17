#pragma once

// chunsa_sim_core — SpatialHash: grid denso, rebuild por tick, orden normativo (SPEC-001 §12)
// generado: minimax-m3 · revisado: Arquitecto 2026-07-17

#include <cstdint>

namespace chunsa {

// --- Constantes normativas (SPEC-001 §12) ---
inline constexpr uint32_t SH_ENTITY_CAP = 65536;
inline constexpr int64_t  SH_CELL_RAW   = 131072;        // celda = 2.0 tiles (2*65536 raw) [DEFAULT SPEC-001 §12]
inline constexpr uint32_t SH_MAX_CELLS  = 512 * 512;     // hasta 1024×1024 tiles de mapa en 0.1A
inline constexpr uint32_t SH_EMPTY      = 0xFFFFFFFFu;

// --- Grid espacial denso (POD, sin asignación dinámica) ---
// Los arrays viven DENTRO de la struct (no punteros): head[] cubre todas las
// celdas posibles (SH_MAX_CELLS), next[] cubre todas las entidades posibles
// (SH_ENTITY_CAP). Determinismo bit-exacto: ninguna indirección externa.
struct SpatialHash {
    uint32_t cells_x;
    uint32_t cells_y;
    uint32_t head[SH_MAX_CELLS];    // índice de entidad o SH_EMPTY
    uint32_t next[SH_ENTITY_CAP];   // lista enlazada intrusiva por celda
};

// -----------------------------------------------------------------------------
// 1) sh_init — Inicializa el grid para un mapa de map_tiles_x × map_tiles_y tiles.
//    cells_x = (map_tiles_x + 1) / 2
//    cells_y = (map_tiles_y + 1) / 2
//    (celdas de 2 tiles, redondeo arriba).
//
//    PRECONDICIÓN (documentada, NO comprobada en runtime):
//        cells_x * cells_y <= SH_MAX_CELLS
//    Es responsabilidad del invocador garantizarla; el desbordamiento
//    corrompería head[] y rompería el determinismo.
//
//    Limpia head[0 .. cells_x*cells_y) a SH_EMPTY. El resto de head[] no se
//    toca (zona no usada del array; se asume inicializada de otra forma o
//    irrelevante porque nunca se indexa fuera del rango activo).
// -----------------------------------------------------------------------------
inline void sh_init(SpatialHash& sh, uint32_t map_tiles_x, uint32_t map_tiles_y) noexcept {
    sh.cells_x = (map_tiles_x + 1u) / 2u;
    sh.cells_y = (map_tiles_y + 1u) / 2u;
    const uint32_t total = sh.cells_x * sh.cells_y;
    for (uint32_t i = 0; i < total; ++i) {
        sh.head[i] = SH_EMPTY;
    }
}

// -----------------------------------------------------------------------------
// 2) sh_cell_index — Calcula el índice lineal de celda a partir de coordenadas
//    raw (raw Q47.16, SPEC-001 §12).
//        cx = clamp(x_raw / SH_CELL_RAW, 0, cells_x - 1)
//        cy = clamp(y_raw / SH_CELL_RAW, 0, cells_y - 1)
//        retorno = cy * cells_x + cx
//
//    Las posiciones raw se asumen >= 0 por cota de mundo; el clamp inferior
//    es defensivo (por si un caller viola la cota). El clamp superior protege
//    contra posiciones que excedan el borde del mapa en este tick
//    (movimiento truncado, teleport defensivo, etc.).
// -----------------------------------------------------------------------------
inline uint32_t sh_cell_index(const SpatialHash& sh, int64_t x_raw, int64_t y_raw) noexcept {
    uint32_t cx;
    if (x_raw <= 0) {
        cx = 0;
    } else {
        const uint64_t cx64 = static_cast<uint64_t>(x_raw / SH_CELL_RAW);
        cx = (cx64 >= sh.cells_x) ? (sh.cells_x - 1u) : static_cast<uint32_t>(cx64);
    }
    uint32_t cy;
    if (y_raw <= 0) {
        cy = 0;
    } else {
        const uint64_t cy64 = static_cast<uint64_t>(y_raw / SH_CELL_RAW);
        cy = (cy64 >= sh.cells_y) ? (sh.cells_y - 1u) : static_cast<uint32_t>(cy64);
    }
    return cy * sh.cells_x + cx;
}

// -----------------------------------------------------------------------------
// 3) sh_rebuild — Rebuild COMPLETO del grid en un tick (se invalida todo el
//    estado anterior). Coste: O(cells_x*cells_y + capacity).
//
//    Paso 1: limpia head[0 .. cells_x*cells_y) a SH_EMPTY.
//    Paso 2: itera i = 0 .. capacity-1 en orden ASCENDENTE.
//            si alive[i]:
//                c   = sh_cell_index(sh, pos_x_raw[i], pos_y_raw[i]);
//                PUSH-FRONT en celda c:
//                    next[i] = head[c];
//                    head[c] = i;
//
//    ORDEN NORMATIVO RESULTANTE (parte del contrato, determinista bit-exacto):
//        Como el push-front se aplica en orden ascendente de i, cada lista de
//        celda queda en orden DESCENDENTE de índice de entidad. Los
//        consumidores que iteren listas (vía sh_first / sh_next) deben
//        respetar ese orden tal cual; cualquier reorden rompería el
//        determinismo del simulador (mismo estado inicial, mismo tick → mismo
//        resultado hash → misma traza).
// -----------------------------------------------------------------------------
inline void sh_rebuild(SpatialHash& sh,
                       const int64_t* pos_x_raw,
                       const int64_t* pos_y_raw,
                       const uint8_t* alive,
                       uint32_t capacity) noexcept {
    const uint32_t total = sh.cells_x * sh.cells_y;
    for (uint32_t i = 0; i < total; ++i) {
        sh.head[i] = SH_EMPTY;
    }
    for (uint32_t i = 0; i < capacity; ++i) {
        if (alive[i]) {
            const uint32_t c = sh_cell_index(sh, pos_x_raw[i], pos_y_raw[i]);
            sh.next[i] = sh.head[c];
            sh.head[c] = i;
        }
    }
}

// -----------------------------------------------------------------------------
// 4) Iteración de celda (helpers para consumidores futuros: queries
//    espaciales, vecindad, broadcasting de eventos, etc.).
//
//    Uso canónico:
//        for (uint32_t i = sh_first(sh, c); i != SH_EMPTY; i = sh_next(sh, i)) {
//            // entidad i está en la celda c; orden garantizado DESCENDENTE
//        }
//
//    sh_first: primer índice de entidad de la lista de la celda,
//              o SH_EMPTY si la celda está vacía.
//    sh_next:  siguiente índice de entidad en la lista intrusiva,
//              o SH_EMPTY si era el último.
// -----------------------------------------------------------------------------
inline uint32_t sh_first(const SpatialHash& sh, uint32_t cell) noexcept {
    return sh.head[cell];
}

inline uint32_t sh_next(const SpatialHash& sh, uint32_t i) noexcept {
    return sh.next[i];
}

} // namespace chunsa