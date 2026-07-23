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
#include "chunsa/economy.hpp"
#include "chunsa/data_catalog.hpp"

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
    // Sprint 0.4 (SPEC-002 §8.4): AÑADIDO AL FINAL a propósito — varios call
    // sites (tests, gdextension) construyen MatchConfig01A por agregado
    // POSICIONAL; insertar un campo en medio desalinearía esos inicializadores
    // en silencio. 0 = producción (SPAWN_UNIT/SPAWN_CITIZEN exigen unit_id de
    // catálogo); 1 = habilita el camino debug legado (solo tests explícitos).
    // Sin default member initializer a propósito (rompe -Wclass-memaccess en
    // el memset de GameState, que contiene `cfg`; ver nota igual en
    // commands.hpp::CmdPayload::unit_id) — `MatchConfig01A cfg{};` ya lo deja
    // en 0 por value-init de agregado sin necesidad de un DMI.
    uint8_t  allow_debug_stat_payload;
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
    if (c.allow_debug_stat_payload > 1u) return false;
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

    // Combate (Sprint 0.3). ESTADO: serializado + checksummeado.
    int32_t  hp[ENTITY_HARD_CAP];         // <=0 ⇒ muere al final del tick
    int32_t  max_hp[ENTITY_HARD_CAP];
    int32_t  attack[ENTITY_HARD_CAP];     // daño base (entero)
    int32_t  range_mt[ENTITY_HARD_CAP];   // alcance en mili-tiles (1000 = 1 tile)
    uint8_t  unit_class[ENTITY_HARD_CAP]; // 0=infantry 1=cavalry 2=artillery
    uint16_t atk_cd[ENTITY_HARD_CAP];     // ticks restantes hasta poder atacar de nuevo

    // Moral (Sprint 0.3, doc 07 §7.6). ESTADO: serializado + checksummeado.
    int32_t morale[ENTITY_HARD_CAP];   // 0..MORALE_MAX; SPAWN_UNIT lo pone a MORALE_MAX
    uint8_t fleeing[ENTITY_HARD_CAP];  // 1 = en pánico (huye, no ataca)

    // Catálogo de datos (Sprint 0.4, SPEC-002 §8.1/§8.4). ESTADO: unit_id se
    // serializa y checksummea (identidad del dato con el que se spawneó cada
    // entidad); INVALID_UNIT_ID para el camino debug legado. `catalog` es
    // BINDING RUNTIME puro (como `data_catalog` en MatchConfigPersistedV1 del
    // brief): jamás se serializa/checksummea ni se toca dentro de Step() salvo
    // para lectura de tablas ya validadas fuera de él. El owner (fuera de
    // GameState) no debe moverse/destruirse/mutarse mientras esté enlazado.
    UnitId unit_id[ENTITY_HARD_CAP];
    const DataCatalogV1* catalog;

    // Economía mínima (Sprint 0.3, base §3.4). ESTADO: serializado + checksummeado.
    // Módulo economy.hpp es autocontenido (sin GameState); el wiring vive aquí y
    // en step.hpp. Depósitos: posiciones fijas deterministas (gs_init_deposits);
    // remaining SÍ cambia en juego, por eso es estado, no derivada.
    EcoDeposit deposits[ECO_MAX_DEPOSITS];
    uint32_t   n_deposits;
    int64_t    dropoff_x[MAX_EMITTERS];
    int64_t    dropoff_y[MAX_EMITTERS];
    int64_t    player_stock[MAX_EMITTERS][3];   // índice: 0=A, 1=B, 2=Me
    EcoState   eco_state[ENTITY_HARD_CAP];
    uint32_t   eco_assigned_deposit[ENTITY_HARD_CAP];
    int32_t    eco_carry[ENTITY_HARD_CAP];
    uint8_t    eco_carry_resource[ENTITY_HARD_CAP];

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
    g.hp[i] = 0; g.max_hp[i] = 0;
    g.attack[i] = 0; g.range_mt[i] = 0;
    g.unit_class[i] = 0; g.atk_cd[i] = 0;
    g.morale[i] = 0; g.fleeing[i] = 0;
    g.eco_state[i] = EcoState::SEEK;
    g.eco_assigned_deposit[i] = ECO_NO_DEPOSIT;
    g.eco_carry[i] = 0;
    g.eco_carry_resource[i] = 0;
    g.unit_id[i] = INVALID_UNIT_ID;
}

// Patrón determinista fijo de depósitos (Sprint 0.3, economía mínima): 2 de
// cada recurso (A/B/Me), posiciones fijas repartidas por el mapa 256×256.
// El dropoff de cada jugador es un punto fijo por índice de emisor.
inline void gs_init_economy(GameState& g) noexcept {
    const int64_t T = FX_ONE_RAW;  // 1 tile en raw
    struct { int64_t tx, ty; uint8_t res; } fixed[6] = {
        {40, 40, 0}, {216, 216, 0},   // A
        {40, 216, 1}, {216, 40, 1},   // B
        {128, 40, 2}, {128, 216, 2},  // Me
    };
    g.n_deposits = 6;
    for (uint32_t i = 0; i < 6; ++i) {
        g.deposits[i].x_raw = fixed[i].tx * T + T / 2;
        g.deposits[i].y_raw = fixed[i].ty * T + T / 2;
        g.deposits[i].resource_idx = fixed[i].res;
        g.deposits[i].remaining = 500;
    }
    for (uint32_t e = 0; e < MAX_EMITTERS; ++e) {
        const int64_t dtx = 20 + static_cast<int64_t>(e) * 28;
        g.dropoff_x[e] = dtx * T + T / 2;
        g.dropoff_y[e] = 128 * T + T / 2;
        g.player_stock[e][0] = 0;
        g.player_stock[e][1] = 0;
        g.player_stock[e][2] = 0;
    }
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
    gs_init_economy(g);
    gs_init_cost_grid(g);
    g.flow_has_goal = 0;
    g.flow_dirty = 0;
    // unit_id sigue la convención de los componentes de combate/moral (Sprint
    // 0.3): TODOS los slots hasta capacity, no solo los vivos (checksum.hpp y
    // serialize.hpp lo recorren sin gate de alive[]). memset ya dejó 0, que
    // bajo la nueva semántica de UnitId significaría "unidad 0 del catálogo",
    // NO "sin catálogo" — hay que forzar INVALID_UNIT_ID explícitamente.
    for (uint32_t i = 0; i < g.entities.capacity; ++i) g.unit_id[i] = INVALID_UNIT_ID;
    g.catalog = nullptr;
}

// Enlaza el catálogo de datos al GameState (Sprint 0.4). Binding runtime puro:
// NO se serializa/checksummea/valida contra un hash persistido en esta
// versión (deviación documentada frente al `gs_init(..., catalog, policy)`
// literal de SPEC-002 §8.2 — ver RESULT del sprint). Precondición del
// contrato de lifecycle: `catalog` debe seguir vivo y sin mutar mientras
// `g` exista; el caller es responsable (igual que el resto del kernel, sin
// refcounting dentro del núcleo determinista).
inline void gs_bind_catalog(GameState& g, const DataCatalogV1& catalog) noexcept {
    g.catalog = &catalog;
}

}  // namespace chunsa
