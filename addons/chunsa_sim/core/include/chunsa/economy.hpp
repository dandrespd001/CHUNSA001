#pragma once

// chunsa_sim_core — Economía mínima v1: recolección A/B/Me (base §3.4, doc 24)
// generado: minimax-m3 · revisado: Arquitecto 2026-07-22

#include <cstdint>
#include "chunsa/fatal.hpp"
#include "chunsa/fixed64.hpp"
#include "chunsa/vec2fx.hpp"

namespace chunsa {

inline constexpr uint32_t ECO_MAX_DEPOSITS = 64;
inline constexpr uint32_t ECO_NO_DEPOSIT = 0xFFFFFFFFu;
inline constexpr int32_t  ECO_HARVEST_PER_TICK = 5;
inline constexpr int32_t  ECO_CARRY_CAP = 50;
inline constexpr int64_t  ECO_ARRIVE_RADIUS_RAW = 65536; // 1 tile
// Sprint 1.7 (SPEC-004 §23.4): la auto-asignación solo considera depósitos
// a <=32 tiles de un edificio aliado completo. Los recursos de base están a
// 8..20 tiles y los neutrales disputados a 100+; 32 separa ambos conjuntos y
// deja margen para edificios de expansión. Es balance temporal hasta que el
// catálogo exponga parámetros de partida.
inline constexpr int64_t  ECO_AUTO_GATHER_RADIUS_RAW = 32 * FX_ONE_RAW;
// Sprint 1.6B (SPEC-004 §18): sentinela "sin preferencia de recurso" para
// eco_find_nearest_deposit — mismo patrón que ECO_NO_DEPOSIT/INVALID_UNIT_ID.
// 0/1/2 son los índices reales (A/B/Me); 0xFF nunca choca con ellos.
inline constexpr uint8_t  ECO_ANY_RESOURCE = 0xFFu;
// ECO_MAX_DEPOSITS==64 permite transportar el conjunto elegible sin heap ni
// STL: bit i habilita deposits[i]. Útil para tests unitarios del módulo puro;
// el wiring real siempre pasa la máscara de zona aliada calculada en step.hpp.
inline constexpr uint64_t ECO_ALL_DEPOSITS_MASK = UINT64_MAX;
static_assert(ECO_MAX_DEPOSITS <= 64u);

enum class EcoState : uint8_t { SEEK = 0, HARVEST = 1, RETURN = 2 };

// Sprint 1.28: sentinela "este deposito NO lo creo un edificio", es decir, es
// un yacimiento del mapa de toda la vida.
inline constexpr uint32_t ECO_NO_OWNER = 0xFFFFFFFFu;

struct EcoDeposit {
    int64_t  x_raw;
    int64_t  y_raw;
    uint8_t  resource_idx; // 0=A, 1=B, 2=Me
    int32_t  remaining;    // <=0 = agotado
    // --- Sprint 1.28 (SPEC-007 §15): granjas -------------------------------
    // regen_per_tick == 0 significa FINITO: es el comportamiento de siempre y
    // el valor por defecto, asi que los 22 yacimientos del mapa no cambian.
    // MILESIMAS de unidad por tick, no unidades. 1000 = 1/tick.
    //
    // Por que milesimas y no unidades enteras: el juego corre a 20 ticks por
    // segundo, asi que 1 unidad por tick serian 1200 por minuto — absurdo para
    // una granja. Y con enteros no se puede bajar de 1. Las milesimas dan
    // resolucion suficiente (50 = una unidad cada 20 ticks) sin tocar coma
    // flotante, que esta prohibida en el kernel.
    int32_t  regen_milli_per_tick;
    int32_t  regen_accum;     // resto acumulado, en milesimas
    int32_t  cap;             // techo al que regenera
    // Indice de la entidad-edificio que creo este deposito, o ECO_NO_OWNER.
    //
    // POR QUE EXISTE, y es la correccion del Director del 2026-08-03: LA
    // GRANJA ES UN EDIFICIO, NO UNA MENA. Sin este campo, todo lo que hoy
    // cuenta o dibuja "depositos" trataria una granja como un yacimiento: la
    // pintaria como mineral en el minimapa y la sumaria al recuento del mapa.
    // Con el, la distincion es explicita y quien deba ignorarlas puede.
    uint32_t owner_building;
    // --- Sprint 1.28 (SPEC-007 §4): RESERVA MINERAL ------------------------
    // Correccion del Director (2026-08-03): "los yacimientos minerales no
    // deben desaparecer, sino que no deben poder ser explotados hasta que se
    // desarrolle una nueva tecnologia de explotacion".
    //
    // No es una idea suelta: el corpus lo documenta. La ley del mineral cae
    // con la profundidad y la FLOTACION —bibliografia del U.S. Bureau of Mines
    // de 1916, en docs/research/corpus/extractos/mineria.md— hizo rentable
    // justo lo que antes se abandonaba en la mina. Un yacimiento "agotado" del
    // siglo XIX es una mina rentable del XX.
    //
    // Ademas arregla algo que molestaba jugando: un mapa agotado dejaba de
    // tener nada que hacer. Ahora la tecnologia REABRE el mapa.
    int32_t  reserve;              // 0 = no hay nada mas ahi abajo
    uint32_t reserve_capability;   // capacidad que la desbloquea
};

// Sentinela "esta reserva no la abre ninguna capacidad".
inline constexpr uint32_t ECO_NO_CAPABILITY = 0xFFFFFFFFu;

// Cuanto puede sacar DE VERDAD un jugador de este deposito, segun lo que sepa
// hacer. Es funcion PURA de (deposito, capacidades del jugador).
//
// Por que depende del JUGADOR y no del deposito: un yacimiento es neutral y lo
// disputan los dos bandos. Que uno haya investigado la flotacion y el otro no
// es exactamente la ventaja que la tecnologia debe dar, y meter el estado en
// el deposito la haria comun a ambos.
//
// El orden importa: mientras QUEDE mineral facil, la reserva no se toca.
// Primero se agota lo barato, que es como funciono siempre la mineria.
inline int32_t eco_available_for(const EcoDeposit& d, uint64_t player_caps) noexcept {
    if (d.remaining > 0) return d.remaining;
    if (d.reserve <= 0) return 0;
    if (d.reserve_capability == ECO_NO_CAPABILITY) return 0;
    if (d.reserve_capability >= 64u) return 0;   // fuera de la palabra: no abre
    const bool tiene = ((player_caps >> d.reserve_capability) & 1ull) != 0ull;
    return tiene ? d.reserve : 0;
}

// Un deposito de granja se distingue de un yacimiento del mapa por tener
// edificio dueno. La diferencia no es cosmetica: un yacimiento agotado
// desaparece, una granja agotada sigue ahi y vuelve a crecer.
inline bool eco_deposit_is_farm(const EcoDeposit& d) noexcept {
    return d.owner_building != ECO_NO_OWNER;
}

// Regeneracion de UN deposito, un tick. Aritmetica ENTERA pura: nada de coma
// flotante, que esta prohibida en el kernel.
//
// Tres decisiones que merecen quedar dichas:
//   · regen 0 no toca nada — compatibilidad exacta con lo anterior.
//   · Nunca se pasa del techo, o una granja vieja acumularia comida infinita y
//     premiaria a quien NO juega.
//   · Si ya esta POR ENCIMA del techo (dato mal puesto, o un techo que baja
//     con una tecnologia) NO se recorta. Quitarle recursos al jugador por un
//     cambio de catalogo seria peor que el exceso.
inline void eco_regen_deposit(EcoDeposit& d) noexcept {
    if (d.regen_milli_per_tick <= 0) return;
    if (d.remaining >= d.cap) return;
    // Acumulador ENTERO: se suman milesimas y solo se convierte a unidades
    // cuando hay al menos una completa. El resto se guarda, asi que el ritmo
    // es exacto a largo plazo y no se pierde nada por redondeo.
    d.regen_accum += d.regen_milli_per_tick;
    if (d.regen_accum < 1000) return;
    const int32_t unidades = d.regen_accum / 1000;
    d.regen_accum -= unidades * 1000;
    const int64_t sumado = static_cast<int64_t>(d.remaining) + unidades;
    d.remaining = sumado > static_cast<int64_t>(d.cap)
                ? d.cap
                : static_cast<int32_t>(sumado);
}

struct EcoCitizenIn {
    int64_t  pos_x, pos_y;
    EcoState state;
    uint32_t assigned_deposit;
    int32_t  carry;
    uint8_t  carry_resource_idx;
    int32_t  speed_mtpt;
};

struct EcoCitizenOut {
    int64_t  pos_x, pos_y;
    int64_t  vel_x, vel_y;
    EcoState state;
    uint32_t assigned_deposit;
    int32_t  carry;
    uint8_t  carry_resource_idx;
    bool     did_harvest;
    int32_t  harvested_amount;
    bool     did_dropoff;
    int32_t  dropoff_amount;
    uint8_t  dropoff_resource_idx;
};

// Depósito ELEGIBLE con remaining>0 más cercano a (x_raw,y_raw). El caller
// expresa la zona aliada en `eligible_mask` (bit i = deposits[i] permitido);
// economy.hpp permanece puro y sin conocer GameState/edificios. Desempate:
// menor índice (recorrido ascendente, `d_sq < best_d_sq` estricto conserva el
// primer mínimo). Sprint 1.6B (SPEC-004 §18): dos pasadas deterministas, mismo
// criterio dist_sq/desempate en ambas:
//   1) si preferred_resource_idx != ECO_ANY_RESOURCE, solo depósitos vivos de
//      ESE recurso; si hay alguno, gana (agotamiento -> reasignar al mismo
//      recurso, SPEC-004 §18).
//   2) si no se pidió preferencia, o la pasada 1 no encontró ninguno, cae al
//      criterio de cualquier recurso vivo, siempre dentro de la misma máscara.
inline uint32_t eco_find_nearest_deposit(const EcoDeposit* deposits, uint32_t n_deposits,
                                         int64_t x_raw, int64_t y_raw,
                                         uint8_t preferred_resource_idx,
                                         uint64_t eligible_mask,
                                         FatalReason& f) noexcept {
    Vec2Fx here{Fx{x_raw}, Fx{y_raw}};
    if (preferred_resource_idx != ECO_ANY_RESOURCE) {
        uint32_t best = ECO_NO_DEPOSIT;
        uint64_t best_d_sq = UINT64_MAX;
        for (uint32_t i = 0; i < n_deposits; ++i) {
            if (i >= ECO_MAX_DEPOSITS
                || (eligible_mask & (uint64_t{1} << i)) == 0u) continue;
            if (deposits[i].remaining <= 0) continue;
            if (deposits[i].resource_idx != preferred_resource_idx) continue;
            Vec2Fx there{Fx{deposits[i].x_raw}, Fx{deposits[i].y_raw}};
            uint64_t d_sq = dist_sq_raw(here, there, f);
            if (d_sq < best_d_sq) {
                best_d_sq = d_sq;
                best = i;
            }
        }
        if (best != ECO_NO_DEPOSIT) return best;
        // Ninguno vivo de ese recurso: cae a la pasada 2 (cualquiera).
    }
    uint32_t best = ECO_NO_DEPOSIT;
    uint64_t best_d_sq = UINT64_MAX;
    for (uint32_t i = 0; i < n_deposits; ++i) {
        if (i >= ECO_MAX_DEPOSITS
            || (eligible_mask & (uint64_t{1} << i)) == 0u) continue;
        if (deposits[i].remaining <= 0) continue;
        Vec2Fx there{Fx{deposits[i].x_raw}, Fx{deposits[i].y_raw}};
        uint64_t d_sq = dist_sq_raw(here, there, f);
        if (d_sq < best_d_sq) {
            best_d_sq = d_sq;
            best = i;
        }
    }
    return best;
}

// Tick de la SM SEEK/HARVEST/RETURN. No muta deposits[] ni stock: emite deltas.
inline EcoCitizenOut eco_step_citizen(const EcoCitizenIn& in,
                                      const EcoDeposit* deposits, uint32_t n_deposits,
                                      uint64_t auto_gather_eligible_mask,
                                      int64_t dropoff_x, int64_t dropoff_y,
                                      FatalReason& f) noexcept {
    EcoCitizenOut out{};
    out.pos_x = in.pos_x;
    out.pos_y = in.pos_y;
    out.vel_x = 0;
    out.vel_y = 0;
    out.state = in.state;
    out.assigned_deposit = in.assigned_deposit;
    out.carry = in.carry;
    out.carry_resource_idx = in.carry_resource_idx;
    out.did_harvest = false;
    out.harvested_amount = 0;
    out.did_dropoff = false;
    out.dropoff_amount = 0;
    out.dropoff_resource_idx = 0;

    const int64_t arrive_r_sq = ECO_ARRIVE_RADIUS_RAW * ECO_ARRIVE_RADIUS_RAW; // 2^32

    // Mover (in.pos_x,in.pos_y) -> (tx,ty): snap si step cubre la dist, si no normalize+step.
    auto try_move = [&](int64_t tx, int64_t ty) noexcept {
        Vec2Fx here{Fx{in.pos_x}, Fx{in.pos_y}};
        Vec2Fx there{Fx{tx}, Fx{ty}};
        uint64_t d_sq = dist_sq_raw(here, there, f);
        // step_fx = trunc_to_zero(speed_mtpt * FX_ONE_RAW / 1000)
        int64_t step_i64 = (static_cast<int64_t>(in.speed_mtpt) * FX_ONE_RAW) / 1000;
        if (step_i64 <= 0) {
            out.vel_x = 0;
            out.vel_y = 0;
            return;
        }
        uint64_t step_sq;
        if (static_cast<uint64_t>(step_i64) > UINT32_MAX) {
            // step² desborda uint64_t: el paso cubre cualquier dist razonable -> snap.
            step_sq = UINT64_MAX;
        } else {
            uint64_t s = static_cast<uint64_t>(step_i64);
            step_sq = s * s;
        }
        if (d_sq <= step_sq) {
            out.pos_x = tx;
            out.pos_y = ty;
            out.vel_x = 0;
            out.vel_y = 0;
        } else {
            Fx dx{tx - in.pos_x};
            Fx dy{ty - in.pos_y};
            Vec2Fx d{dx, dy};
            Vec2Fx dir = normalize_v1(d, f);
            Fx step_fx{step_i64};
            Fx vx = fx_mul(dir.x, step_fx, f);
            Fx vy = fx_mul(dir.y, step_fx, f);
            Fx nx = fx_add(Fx{in.pos_x}, vx, f);
            Fx ny = fx_add(Fx{in.pos_y}, vy, f);
            out.pos_x = nx.raw;
            out.pos_y = ny.raw;
            out.vel_x = vx.raw;
            out.vel_y = vy.raw;
        }
    };

    switch (in.state) {
    case EcoState::SEEK: {
        // ¿Reasignar? Short-circuit: el 3er término solo se evalúa si el índice es válido.
        bool need_reassign = (in.assigned_deposit == ECO_NO_DEPOSIT)
                             || (in.assigned_deposit >= n_deposits)
                             || (deposits[in.assigned_deposit].remaining <= 0);
        if (need_reassign) {
            // SPEC-004 §18: preferencia de recurso SOLO cuando el índice
            // previo es válido (memoria legible) — es el caso "el depósito
            // asignado se agotó" (remaining<=0), que es exactamente el
            // agotamiento que el contrato pide reasignar al MISMO recurso
            // primero. Los otros dos casos de need_reassign (ECO_NO_DEPOSIT /
            // índice fuera de rango) no tienen un recurso previo del que
            // partir — se mantiene ECO_ANY_RESOURCE, IDÉNTICO al
            // comportamiento legacy (preserva bit a bit los escenarios que
            // nunca llaman a GATHER).
            uint8_t pref = ECO_ANY_RESOURCE;
            if (in.assigned_deposit != ECO_NO_DEPOSIT && in.assigned_deposit < n_deposits) {
                pref = deposits[in.assigned_deposit].resource_idx;
            }
            uint32_t idx = eco_find_nearest_deposit(
                deposits, n_deposits, in.pos_x, in.pos_y, pref,
                auto_gather_eligible_mask, f);
            if (idx == ECO_NO_DEPOSIT) {
                out.assigned_deposit = ECO_NO_DEPOSIT;
                out.vel_x = 0;
                out.vel_y = 0;
                return out;
            }
            out.assigned_deposit = idx;
        }
        const EcoDeposit& dep = deposits[out.assigned_deposit];
        Vec2Fx here{Fx{in.pos_x}, Fx{in.pos_y}};
        Vec2Fx there{Fx{dep.x_raw}, Fx{dep.y_raw}};
        uint64_t d_sq = dist_sq_raw(here, there, f);
        if (d_sq <= static_cast<uint64_t>(arrive_r_sq)) {
            // Ya en radio: transición directa, sin movimiento este tick (1 tick de latencia aceptable).
            out.state = EcoState::HARVEST;
            out.vel_x = 0;
            out.vel_y = 0;
        } else {
            try_move(dep.x_raw, dep.y_raw);
        }
        break;
    }
    case EcoState::HARVEST: {
        if (in.assigned_deposit == ECO_NO_DEPOSIT
            || in.assigned_deposit >= n_deposits
            || deposits[in.assigned_deposit].remaining <= 0) {
            // Depósito se agotó mientras cosechaba.
            out.state = (out.carry > 0) ? EcoState::RETURN : EcoState::SEEK;
            out.vel_x = 0;
            out.vel_y = 0;
            break;
        }
        const EcoDeposit& dep = deposits[in.assigned_deposit];
        int32_t room = ECO_CARRY_CAP - out.carry;
        if (room < 0) room = 0;
        int32_t amount = ECO_HARVEST_PER_TICK;
        if (dep.remaining < amount) amount = dep.remaining;
        if (room < amount) amount = room;
        if (amount > 0) {
            out.did_harvest = true;
            out.harvested_amount = amount;
            out.carry += amount;
            out.carry_resource_idx = dep.resource_idx;
        }
        if (out.carry >= ECO_CARRY_CAP) {
            out.state = EcoState::RETURN;
        }
        out.vel_x = 0;
        out.vel_y = 0;
        break;
    }
    case EcoState::RETURN: {
        Vec2Fx here{Fx{in.pos_x}, Fx{in.pos_y}};
        Vec2Fx there{Fx{dropoff_x}, Fx{dropoff_y}};
        uint64_t d_sq = dist_sq_raw(here, there, f);
        if (d_sq <= static_cast<uint64_t>(arrive_r_sq)) {
            if (out.carry > 0) {
                out.did_dropoff = true;
                out.dropoff_amount = out.carry;
                out.dropoff_resource_idx = out.carry_resource_idx;
                out.carry = 0;
            }
            out.state = EcoState::SEEK;
            out.vel_x = 0;
            out.vel_y = 0;
        } else {
            try_move(dropoff_x, dropoff_y);
        }
        break;
    }
    }
    return out;
}

} // namespace chunsa
