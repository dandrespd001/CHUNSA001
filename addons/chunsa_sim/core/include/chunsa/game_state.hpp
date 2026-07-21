#pragma once
#include <cstdint>
#include <cstring>

#include "chunsa/fatal.hpp"
#include "chunsa/fixed64.hpp"
#include "chunsa/vec2fx.hpp"
#include "chunsa/entity_table.hpp"
#include "chunsa/spatial_hash.hpp"
#include "chunsa/commands.hpp"
#include "chunsa/vision.hpp"
#include "chunsa/flow_field.hpp"

// chunsa_sim_core — GameState (SoA, pools preasignados) y su configuración.
// SPEC-001 §1.2/§3.2. Autor: Arquitecto.
// El struct pesa varios MB: el caller lo asigna UNA vez (heap) en StartMatch;
// dentro de Step() no hay ninguna asignación (contador de allocations en CI).

namespace chunsa {

struct MatchConfig01A {
    uint32_t max_entities;               // 1..ENTITY_HARD_CAP
    uint8_t  player_count;               // 1..8 (emisores humanos en 0.1A)
    uint16_t human_input_delay_ticks;    // ≥ 0 ([DEFAULT] 1)
    uint16_t max_future_command_ticks;   // ventana OUT_OF_WINDOW ([DEFAULT] 20)
    uint16_t checksum_every_ticks;       // divisor de 20: {1,2,4,5,10,20}
    uint32_t map_tiles_x;                // ≤ WORLD_TILES_MAX; grid del spatial hash
    uint32_t map_tiles_y;
    uint64_t seed;
};

inline bool config_validate(const MatchConfig01A& c) noexcept {
    if (c.max_entities == 0 || c.max_entities > ENTITY_HARD_CAP) return false;
    if (c.player_count == 0 || c.player_count > 8) return false;
    const uint16_t n = c.checksum_every_ticks;
    if (!(n == 1 || n == 2 || n == 4 || n == 5 || n == 10 || n == 20)) return false;
    if (c.map_tiles_x == 0 || c.map_tiles_y == 0) return false;
    if (c.map_tiles_x > WORLD_TILES_MAX || c.map_tiles_y > WORLD_TILES_MAX) return false;
    const uint64_t cells = ((c.map_tiles_x + 1) / 2) * (uint64_t)((c.map_tiles_y + 1) / 2);
    if (cells > SH_MAX_CELLS) return false;
    return true;
}

struct GameState {
    uint32_t tick;                 // el SIGUIENTE tick a ejecutar (SPEC-001 §2)
    FatalReason fatal;
    MatchConfig01A cfg;

    EntityTable entities;

    // Componentes SoA (raw Q47.16 salvo indicación) — índice = EntityHandle.index.
    int64_t pos_x[ENTITY_HARD_CAP];
    int64_t pos_y[ENTITY_HARD_CAP];
    int64_t vel_x[ENTITY_HARD_CAP];
    int64_t vel_y[ENTITY_HARD_CAP];
    int64_t tgt_x[ENTITY_HARD_CAP];
    int64_t tgt_y[ENTITY_HARD_CAP];
    int32_t speed_mtpt[ENTITY_HARD_CAP];   // mili-tiles/tick (entero canónico §2.4.3 base)
    uint8_t owner[ENTITY_HARD_CAP];

    PendingCommandState pending;
    uint64_t last_seq[MAX_EMITTERS];       // última sequence aceptada-en-agenda por emisor
    ReceiptMailbox mailbox[MAX_EMITTERS];

    SpatialHash shash;

    // Visión (SPEC-001 §8, fase t%4==1). `explored` es ESTADO (acumulativo,
    // serializado y checksummeado); `visible` es DERIVADA (se reconstruye).
    VisionGrid vision;

    // Flujo de navegación (Sprint 0.2). cost_grid/flow_mode/flow_goal_cell/flow_has_goal
    // son ESTADO (serializados, checksummeados). `flow` es DERIVADA (excluida): se
    // recomputa cuando flow_dirty. Layout 256×256, stride FF_AXIS (== map fijo 256).
    uint8_t  cost_grid[FF_CELLS];        // 1..254 transitable, FF_WALL=255 muro
    uint8_t  flow_mode[ENTITY_HARD_CAP]; // 0 = seek directo (tgt); 1 = seguir flujo
    uint32_t flow_goal_cell;             // celda goal activa (ty*FF_AXIS+tx)
    uint8_t  flow_has_goal;              // 0/1
    uint8_t  flow_dirty;                 // 1 = recalcular flow al inicio del próximo movement
    FlowField flow;                      // DERIVADA — NO serializar, NO checksummear

    // DestroyBatch del tick en curso (paso 6 de Step: se ordena ASC y se recicla).
    uint32_t destroy_batch[PENDING_CAP];
    uint32_t destroy_count;

    // Buffer de trabajo del tick (comandos debidos) — parte del estado por simplicidad POD.
    ScheduledCommand due[PENDING_CAP];
};

inline void zero_components(GameState& g, uint32_t i) noexcept {
    g.pos_x[i] = 0; g.pos_y[i] = 0;
    g.vel_x[i] = 0; g.vel_y[i] = 0;
    g.tgt_x[i] = 0; g.tgt_y[i] = 0;
    g.speed_mtpt[i] = 0;
    g.owner[i] = 0;
}

// Patrón determinista fijo del cost_grid de navegación (Sprint 0.2): todo
// transitable (1), salvo un muro vertical en tile x=128 para y en [32,224)
// con un hueco de 8 tiles en y∈[124,132) por donde las unidades se cuelan.
inline void gs_init_cost_grid(GameState& g) noexcept {
    for (uint32_t i = 0; i < FF_CELLS; ++i) g.cost_grid[i] = 1u;
    constexpr uint32_t WALL_X = 128;
    for (uint32_t ty = 32; ty < 224; ++ty) {
        if (ty >= 124 && ty < 132) continue;  // hueco
        g.cost_grid[ty * FF_AXIS + WALL_X] = FF_WALL;
    }
}

// Inicializa el estado. Precondición: config_validate(cfg) == true.
inline void gs_init(GameState& g, const MatchConfig01A& cfg) noexcept {
    std::memset(&g, 0, sizeof(GameState));  // POD: base canónica en cero
    g.tick = 0;
    g.fatal = FatalReason::NONE;
    g.cfg = cfg;
    et_init(g.entities, cfg.max_entities);
    sh_init(g.shash, cfg.map_tiles_x, cfg.map_tiles_y);
    vis_init(g.vision, cfg.map_tiles_x, cfg.map_tiles_y);
    gs_init_cost_grid(g);
    g.flow_has_goal = 0;
    g.flow_dirty = 0;
}

}  // namespace chunsa
