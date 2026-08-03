#pragma once
#include <cstdint>

// Política de ALDEANO OCIOSO (Sprint 1.15).
//
// POR QUÉ ES UNA CABECERA APARTE. Es el mismo patrón que `fog_view.hpp`,
// `outcome_view.hpp` y `command_panel_view.hpp`: una decisión de presentación
// que se puede razonar y PROBAR sin godot-cpp. Dentro del `.cpp` del nodo
// estaría enterrada entre llamadas de dibujo y nadie podría escribirle una
// prueba.
//
// EL PROBLEMA QUE ARREGLA. El Director lo señaló mirando la pantalla: «los
// aldeanos que están en movimiento no se deben considerar ociosos, solo los
// que estén quietos sin hacer nada». El contador anterior comprobaba cuatro
// cosas —tarea, obra, depósito asignado y carga— y NINGUNA de ellas detecta a
// un aldeano al que le has dado una orden de MOVERSE y nada más: su
// `citizen_task` sigue en IDLE, no tiene obra, ni depósito, ni carga. Andaba
// por el mapa contado como ocioso.
//
// CÓMO SE DETECTA EL MOVIMIENTO, y por qué así. No comparando contra un
// destino —el snapshot de presentación no lleva `tgt_x`/`tgt_y`— sino
// **comparando la posición con la del snapshot anterior**, que el adaptador ya
// guarda para interpolar. Es mejor que mirar el destino por dos razones:
//   · No hace falta ampliar el snapshot ni tocar el kernel.
//   · Mide lo que el jugador VE. Una unidad bloqueada contra un muro tiene
//     destino y no avanza; para quien mira la pantalla está parada, y contarla
//     como ocupada sería mentir en la dirección contraria.
//
// El umbral no es cero: la posición es Q47.16 y una unidad puede tener una
// deriva de subunidades sin desplazarse de verdad. Se exige un movimiento
// mínimo apreciable.

namespace chunsa_view {

// Un milésimo de tile en crudo Q47.16 (FX_ONE_RAW = 65536). Por debajo de esto
// no es andar, es ruido de redondeo del punto fijo.
inline constexpr int64_t IDLE_MOVE_EPSILON_RAW = 65;

struct IdleInputs {
    bool    is_own_citizen;        // vivo, del jugador, unidad, clase Citizen
    uint8_t citizen_task;          // CITIZEN_TASK_IDLE == 0
    bool    has_build_target;      // asignado a una obra
    bool    has_deposit;           // asignado a recolectar
    int32_t carry;                 // lleva recursos encima
    bool    had_previous_sample;   // ¿existía en el snapshot anterior?
    int64_t x_raw, y_raw;          // posición actual
    int64_t prev_x_raw, prev_y_raw;
};

// ¿Se ha movido de forma apreciable desde el snapshot anterior?
inline bool is_moving(const IdleInputs& in) noexcept {
    if (!in.had_previous_sample) return false;   // sin referencia, no se afirma
    const int64_t dx = in.x_raw - in.prev_x_raw;
    const int64_t dy = in.y_raw - in.prev_y_raw;
    const int64_t adx = dx < 0 ? -dx : dx;
    const int64_t ady = dy < 0 ? -dy : dy;
    // Distancia de Chebyshev: basta y evita multiplicar (nada de desbordar).
    return (adx > IDLE_MOVE_EPSILON_RAW) || (ady > IDLE_MOVE_EPSILON_RAW);
}

// Ocioso = MÍO, sin tarea, sin obra, sin depósito, sin carga y QUIETO.
//
// El orden de las condiciones no es casual: primero las baratas y las que más
// descartan, y `is_moving` la última porque es la única que toca dos
// snapshots.
inline bool is_idle_citizen(const IdleInputs& in) noexcept {
    if (!in.is_own_citizen) return false;
    if (in.citizen_task != 0u) return false;     // CITIZEN_TASK_IDLE
    if (in.has_build_target) return false;
    if (in.has_deposit) return false;
    if (in.carry != 0) return false;
    if (is_moving(in)) return false;             // el arreglo del 1.15
    return true;
}

// Siguiente ocioso tras `desde`, en orden ASCENDENTE y circular.
//
// Es el comportamiento del punto (.) de AoE2: pulsar repetidamente recorre
// TODOS los ociosos y vuelve al principio, sin quedarse clavado en el primero.
// Circular y por índice ascendente para que la vuelta sea siempre la misma —un
// recorrido que dependiera del orden de pantalla saltaría de forma distinta
// según dónde estuviera la cámara, y eso se siente como un fallo.
//
// Devuelve `count` si no hay ninguno; el llamante no debe mover la selección.
template <typename EsOcioso>
inline uint32_t next_idle_from(uint32_t desde, uint32_t count, EsOcioso ocioso) noexcept {
    if (count == 0u) return 0u;
    for (uint32_t k = 1u; k <= count; ++k) {
        const uint32_t i = (desde + k) % count;
        if (ocioso(i)) return i;
    }
    // Ni siquiera el propio `desde` lo es: no hay ociosos.
    return count;
}

}  // namespace chunsa_view
