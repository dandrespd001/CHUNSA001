#pragma once

// chunsa_sim_core — EntityTable: handles generacionales y free-list normativa (SPEC-001 §3.1)
// generado: minimax-m3 · revisado: Arquitecto 2026-07-17

#include <cstdint>
#include "chunsa/fatal.hpp"

namespace chunsa {

// -----------------------------------------------------------------------------
// Constantes y tipos públicos
// -----------------------------------------------------------------------------

// Tope duro de capacidad; las tablas se dimensionan en tiempo de compilación
// contra este límite para evitar asignación dinámica (los arrays viven dentro
// de la struct).
inline constexpr uint32_t ENTITY_HARD_CAP = 65536;

// Handle generacional: index + generation. Generación 0 se reserva para
// NULL_HANDLE, por lo que las generaciones reales comienzan en 1.
struct EntityHandle {
    uint32_t index;
    uint32_t generation;
};

// Handle centinela: representa "sin entidad". Nunca debe compararse como
// vivo (la generación 0 nunca aparece en slots inicializados a 1).
inline constexpr EntityHandle NULL_HANDLE{0xFFFFFFFFu, 0u};

inline bool handle_eq(EntityHandle a, EntityHandle b) noexcept {
    return a.index == b.index && a.generation == b.generation;
}

// -----------------------------------------------------------------------------
// Tabla de entidades
// -----------------------------------------------------------------------------
//
// ORDEN NORMATIVO DE LA FREE-LIST (SPEC-001 §2.6 / §3.1):
//   * et_init empuja los índices desde capacity-1 hacia 0, de modo que el
//     PRIMER pop devuelve el índice 0; los spawns iniciales reciben índices
//     en orden ASCENDENTE (0, 1, 2, ...).
//   * DestroyBatch entrega los índices ordenados ASCENDENTEMENTE; el caller
//     los empuja en ese mismo orden mediante et_release_index. Combinado
//     con la inicialización anterior, el siguiente spawn (LIFO puro)
//     reutiliza el MAYOR índice destruido más reciente.
//   * Determinismo bit-exacto: dos simulaciones con la misma secuencia de
//     comandos producen exactamente la misma asignación de índices.
// -----------------------------------------------------------------------------

struct EntityTable {
    uint32_t capacity;                          // límite runtime validado (<= ENTITY_HARD_CAP)
    uint32_t alive_count;
    uint32_t free_top;                          // nº de entradas válidas en la pila
    uint32_t free_stack[ENTITY_HARD_CAP];      // pila LIFO de índices libres
    uint32_t generation[ENTITY_HARD_CAP];      // generación actual por slot
    uint8_t  alive[ENTITY_HARD_CAP];           // 0/1: slot ocupado por una entidad viva
    uint8_t  retired[ENTITY_HARD_CAP];         // 1 = slot retirado permanentemente
};

// -----------------------------------------------------------------------------
// API
// -----------------------------------------------------------------------------

// Inicializa la tabla con la capacidad dada. El caller YA ha validado que
// capacity <= ENTITY_HARD_CAP. Estados iniciales:
//   * generation[i] = 1  (la generación 0 está reservada para NULL_HANDLE)
//   * alive[i]      = 0
//   * retired[i]    = 0
//   * alive_count   = 0
//   * free_top      = capacity
// La pila se rellena con índices [capacity-1, capacity-2, ..., 1, 0]; al
// estar la cima en el último elemento empujado (0), el primer pop devuelve 0.
inline void et_init(EntityTable& t, uint32_t capacity) noexcept {
    t.capacity    = capacity;
    t.alive_count = 0u;
    t.free_top    = capacity;
    for (uint32_t i = 0; i < capacity; ++i) {
        const uint32_t idx = capacity - 1u - i;
        t.free_stack[idx]   = idx;          // placeholder, sobreescrito abajo
        // Corrección: la pila se rellena por posición secuencial; el orden
        // de los índices es lo que importa para la cima final.
        t.generation[idx]   = 1u;
        t.alive[idx]        = 0u;
        t.retired[idx]      = 0u;
    }
    // Segundo pase: colocar los índices en la pila de modo que la cima (último
    // elemento válido) sea 0. free_stack[0] = capacity-1, free_stack[1] = capacity-2,
    // ..., free_stack[capacity-1] = 0. Pop inicial lee free_stack[free_top-1] = 0.
    for (uint32_t i = 0; i < capacity; ++i) {
        t.free_stack[i] = capacity - 1u - i;
    }
}

// Comprueba si un handle está vivo. Chequeo de bounds PRIMERO para evitar
// lecturas fuera de rango si el caller pasa un handle forjado.
inline bool et_is_alive(const EntityTable& t, EntityHandle h) noexcept {
    if (h.index >= t.capacity)        return false;
    if (t.retired[h.index] != 0u)     return false;
    if (t.alive[h.index] == 0u)       return false;
    if (t.generation[h.index] != h.generation) return false;
    return true;
}

// Crea una nueva entidad extrayendo un índice de la free-list.
// Si la pila está vacía, devuelve NULL_HANDLE. Por especificación NO se
// marca fatal aquí: el caller (despachador de comandos) traduce el NULL_HANDLE
// en un rechazo POOL_EXHAUSTED del comando y, si procede, eleva el fatal.
inline EntityHandle et_spawn(EntityTable& t) noexcept {
    if (t.free_top == 0u) {
        return NULL_HANDLE;
    }
    --t.free_top;
    const uint32_t idx = t.free_stack[t.free_top];
    t.alive[idx] = 1u;
    ++t.alive_count;
    return EntityHandle{idx, t.generation[idx]};
}

// Marca una entidad como muerta DURANTE el tick. Los sistemas que se ejecuten
// después en el mismo tick ya no la verán (et_is_alive devuelve false).
// NO recicla el slot: el reciclaje ocurre al final del tick, en el paso 6 de
// Step, vía et_release_index.
//
// Precondición: index < t.capacity y t.alive[index] == 1 (entidad viva).
inline void et_mark_dead(EntityTable& t, uint32_t index) noexcept {
    t.alive[index] = 0u;
    --t.alive_count;
}

// Recicla un slot al FINAL del tick. El DestroyBatch ya viene ordenado
// ASCENDENTEMENTE por el caller; los índices se empujan en ese mismo orden,
// produciendo LIFO puro: el siguiente spawn reutiliza el MAYOR índice del
// batch (el último empujado, cima de la pila).
//
// Si la generación del slot está al máximo (0xFFFFFFFFu), incrementar
// desbordaría a 0 y colisionaría con NULL_HANDLE. En ese caso el slot se
// RETIRA permanentemente (retired=1) y NO vuelve a la pila; la capacidad
// efectiva de la tabla se reduce en uno para siempre.
//
// Precondición: t.alive[index] == 0 (la entidad ya fue marcada muerta por
// et_mark_dead en este mismo tick) e index < t.capacity.
inline void et_release_index(EntityTable& t, uint32_t index) noexcept {
    if (t.generation[index] == 0xFFFFFFFFu) {
        t.retired[index] = 1u;
        return; // slot retirado; no se empuja a la pila
    }
    ++t.generation[index];
    t.free_stack[t.free_top] = index;
    ++t.free_top;
}

// Número de entidades vivas (que et_is_alive aceptaría como válidas).
inline uint32_t et_alive_count(const EntityTable& t) noexcept {
    return t.alive_count;
}

} // namespace chunsa