// Política de aldeano ocioso (Sprint 1.15).
//
// Prueba la cabecera pura, SIN godot-cpp. Es el motivo de que la política viva
// fuera del `.cpp` del nodo: allí estaría entre llamadas de dibujo y no se
// podría comprobar.

#include <cstdint>
#include <cstdio>

#include "idle_view.hpp"

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fails; std::printf("CHECK L%d: %s\n", __LINE__, #cond); } } while (0)

using namespace chunsa_view;

namespace {

// Aldeano propio, quieto y sin nada que hacer: el caso base de OCIOSO.
IdleInputs parado() {
    IdleInputs in{};
    in.is_own_citizen = true;
    in.citizen_task = 0;            // CITIZEN_TASK_IDLE
    in.has_build_target = false;
    in.has_deposit = false;
    in.carry = 0;
    in.had_previous_sample = true;
    in.x_raw = 100 * 65536; in.y_raw = 50 * 65536;
    in.prev_x_raw = in.x_raw; in.prev_y_raw = in.y_raw;
    return in;
}

}  // namespace

int main() {
    // 1) El caso base.
    CHECK(is_idle_citizen(parado()));

    // 2) EL ARREGLO DEL SPRINT: andando NO es ocioso, aunque no tenga tarea,
    //    ni obra, ni depósito, ni carga. Es exactamente el caso que el
    //    contador anterior daba por ocioso y que el Director vio en pantalla.
    {
        IdleInputs in = parado();
        in.x_raw += 2 * 65536;      // dos tiles desde el fotograma anterior
        CHECK(is_moving(in));
        CHECK(!is_idle_citizen(in));
    }

    // 3) También cuenta el movimiento en Y, y el movimiento NEGATIVO: si sólo
    //    se mirara el signo positivo, un aldeano que vuelve hacia el centro
    //    saldría ocioso. Es la clase de asimetría que se cuela sola.
    {
        IdleInputs in = parado();
        in.y_raw -= 2 * 65536;
        CHECK(is_moving(in));
        CHECK(!is_idle_citizen(in));
    }

    // 4) Una deriva de SUBUNIDADES no es andar. La posición es Q47.16 y un
    //    redondeo de unos pocos raw no debe apagar el contador: si el umbral
    //    fuera cero, el número parpadearía sin que nadie se moviera.
    {
        IdleInputs in = parado();
        in.x_raw += 10;             // muy por debajo del epsilon (65)
        CHECK(!is_moving(in));
        CHECK(is_idle_citizen(in));
    }

    // 5) Justo EN el umbral tampoco cuenta (la comparación es estricta), y
    //    justo por encima sí. Fijar los dos lados evita que un cambio de `>` a
    //    `>=` pase inadvertido.
    {
        IdleInputs in = parado();
        in.x_raw += IDLE_MOVE_EPSILON_RAW;
        CHECK(!is_moving(in));
        in.x_raw += 1;
        CHECK(is_moving(in));
    }

    // 6) Sin muestra anterior NO se afirma que se mueva. Un aldeano recién
    //    entrenado aparece sin referencia; decir que anda seria inventarselo,
    //    y ademas es justo el que el jugador quiere encontrar como ocioso.
    {
        IdleInputs in = parado();
        in.had_previous_sample = false;
        in.prev_x_raw = 0; in.prev_y_raw = 0;   // basura: no debe mirarse
        CHECK(!is_moving(in));
        CHECK(is_idle_citizen(in));
    }

    // 7) Las cuatro condiciones anteriores siguen mandando: obra, depósito,
    //    carga y tarea. El sprint AÑADE una, no sustituye las que había.
    {
        IdleInputs in = parado(); in.has_build_target = true;
        CHECK(!is_idle_citizen(in));
    }
    {
        IdleInputs in = parado(); in.has_deposit = true;
        CHECK(!is_idle_citizen(in));
    }
    {
        IdleInputs in = parado(); in.carry = 3;
        CHECK(!is_idle_citizen(in));
    }
    {
        IdleInputs in = parado(); in.citizen_task = 2;
        CHECK(!is_idle_citizen(in));
    }
    {
        IdleInputs in = parado(); in.is_own_citizen = false;
        CHECK(!is_idle_citizen(in));
    }

    // 8) El RECORRIDO circular del botón. Con ociosos en 1, 4 y 7 sobre 8
    //    ranuras, pulsar repetidamente debe visitarlos todos y volver.
    {
        const bool ocioso[8] = {false, true, false, false, true, false, false, true};
        auto es = [&](uint32_t i) { return ocioso[i]; };
        CHECK(next_idle_from(0, 8, es) == 1);
        CHECK(next_idle_from(1, 8, es) == 4);
        CHECK(next_idle_from(4, 8, es) == 7);
        CHECK(next_idle_from(7, 8, es) == 1);   // da la vuelta
    }

    // 9) Con UN SOLO ocioso, pulsar desde él vuelve a él. Es lo que hace AoE2
    //    y es lo correcto: el jugador confirma que no hay mas.
    {
        const bool ocioso[4] = {false, false, true, false};
        auto es = [&](uint32_t i) { return ocioso[i]; };
        CHECK(next_idle_from(2, 4, es) == 2);
    }

    // 10) SIN ociosos devuelve `count`, la señal de "no muevas la seleccion".
    //     Es importante que no devuelva 0: el 0 es una ranura valida y el
    //     boton saltaria a una unidad ocupada cada vez que no hubiera ninguna
    //     ociosa, que es peor que no hacer nada.
    {
        auto es = [](uint32_t) { return false; };
        CHECK(next_idle_from(0, 5, es) == 5);
    }

    // 11) Cota: count 0 no debe entrar en el bucle ni dividir por cero.
    {
        auto es = [](uint32_t) { return true; };
        CHECK(next_idle_from(0, 0, es) == 0);
    }

    if (g_fails == 0) {
        std::printf("idle_view OK\n");
        return 0;
    }
    std::printf("idle_view: %d fallo(s)\n", g_fails);
    return 1;
}
