# CHUNSA001 — kernel de simulación determinista (Sprint 0.1A)
# SPEC-001 v1.1 (APPROVED_FOR_EXECUTION). fast-math PROHIBIDO (SPEC-001 §1.1.2 base §1.2).
cmake_minimum_required(VERSION 3.24)
project(chunsa_sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CHUNSA_WIDE128_FORCE_PORTABLE "Forzar el backend portable de Wide128 (lane de CI)" OFF)

# --- Núcleo: header-only, C++ puro, CERO dependencias de Godot -----------------
add_library(chunsa_sim_core INTERFACE)
target_include_directories(chunsa_sim_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/addons/chunsa_sim/core/include)
if(CHUNSA_WIDE128_FORCE_PORTABLE)
    target_compile_definitions(chunsa_sim_core INTERFACE CHUNSA_WIDE128_FORCE_PORTABLE=1)
endif()

# --- Warnings-as-errors (goal 0.1A: build limpio, 0 warnings) ------------------
if(MSVC)
    add_compile_options(/W4 /WX /permissive-)
    # fopen/fseek estándar: silenciar C4996 (no usamos las variantes _s por portabilidad)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wshadow -Werror)
endif()

# --- CLI ------------------------------------------------------------------------
add_executable(chunsa_sim_cli addons/chunsa_sim/cli/main.cpp)
target_link_libraries(chunsa_sim_cli PRIVATE chunsa_sim_core)
target_include_directories(chunsa_sim_cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/determinism)

# --- Tests ----------------------------------------------------------------------
enable_testing()
add_executable(chunsa_test_props tests/unit/test_props.cpp)
target_link_libraries(chunsa_test_props PRIVATE chunsa_sim_core)
add_test(NAME props COMMAND chunsa_test_props)
add_test(NAME golden COMMAND chunsa_sim_cli golden --vectors ${CMAKE_CURRENT_SOURCE_DIR}/tests/determinism/golden)

# --- Gates de determinismo aserverados (Sprint 1.7, SPEC-004 §22 / brief C.2)
add_test(NAME gate_g1 COMMAND chunsa_sim_cli run --selftest-g1)
add_test(NAME gate_g3 COMMAND chunsa_sim_cli savetest
    --save-file ${CMAKE_CURRENT_BINARY_DIR}/ctest-g3.sav)
add_test(NAME gate_g4 COMMAND chunsa_sim_cli savetest --ai
    --save-file ${CMAKE_CURRENT_BINARY_DIR}/ctest-g4.sav)
set_tests_properties(gate_g1 gate_g3 gate_g4 PROPERTIES
    LABELS "determinism_gate"
    TIMEOUT 120)

# --- Tests 0.1B ------------------------------------------------------------------
add_executable(chunsa_test_state tests/unit/test_state.cpp)
target_link_libraries(chunsa_test_state PRIVATE chunsa_sim_core)
add_test(NAME state COMMAND chunsa_test_state)
find_package(Threads REQUIRED)
add_executable(chunsa_test_ring tests/unit/test_ring.cpp)
target_link_libraries(chunsa_test_ring PRIVATE chunsa_sim_core Threads::Threads)
add_test(NAME ring COMMAND chunsa_test_ring)
add_executable(chunsa_test_ff tests/unit/test_flow_field.cpp)
target_link_libraries(chunsa_test_ff PRIVATE chunsa_sim_core)
add_test(NAME flow_field COMMAND chunsa_test_ff)

# --- Test Sprint 0.2: FlowField -> MovementSystem --------------------------------
add_executable(chunsa_test_flow_move tests/unit/test_flow_move.cpp)
target_link_libraries(chunsa_test_flow_move PRIVATE chunsa_sim_core)
add_test(NAME flow_move COMMAND chunsa_test_flow_move)

# --- Test Sprint 0.3: Combate RPS -------------------------------------------------
add_executable(chunsa_test_combat tests/unit/test_combat.cpp)
target_link_libraries(chunsa_test_combat PRIVATE chunsa_sim_core)
add_test(NAME combat COMMAND chunsa_test_combat)

# --- Test Sprint 0.3: Moral y pánico -----------------------------------------------
add_executable(chunsa_test_morale tests/unit/test_morale.cpp)
target_link_libraries(chunsa_test_morale PRIVATE chunsa_sim_core)
add_test(NAME morale COMMAND chunsa_test_morale)

add_executable(chunsa_test_economy tests/unit/test_economy.cpp)
target_link_libraries(chunsa_test_economy PRIVATE chunsa_sim_core)
add_test(NAME economy COMMAND chunsa_test_economy)

add_executable(chunsa_test_aggro tests/unit/test_aggro.cpp)
target_link_libraries(chunsa_test_aggro PRIVATE chunsa_sim_core)
add_test(NAME aggro COMMAND chunsa_test_aggro)

add_executable(chunsa_test_replay_v2 tests/unit/test_replay_v2.cpp)
target_link_libraries(chunsa_test_replay_v2 PRIVATE chunsa_sim_core)
add_test(NAME replay_v2 COMMAND chunsa_test_replay_v2)

# --- Test Sprint 0.4: kernel data-driven (loader CHDB v1 + SPAWN_UNIT/CITIZEN) ---
add_executable(chunsa_test_data_blob tests/unit/test_data_blob.cpp)
target_link_libraries(chunsa_test_data_blob PRIVATE chunsa_sim_core)
# Ruta absoluta al golden versionado: el test NO depende del cwd implícito de
# ctest (brief SONNET_KERNEL_DATOS_SPEC002 §9).
target_compile_definitions(chunsa_test_data_blob PRIVATE
    CHUNSA_GOLDEN_CHDB_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/compiled/chunsa_base.chdb")
add_test(NAME data_blob COMMAND chunsa_test_data_blob)

# --- Test Sprint 1.1: edificios y construcción (SPEC-004 Parte I) ---------------
add_executable(chunsa_test_buildings tests/unit/test_buildings.cpp)
target_link_libraries(chunsa_test_buildings PRIVATE chunsa_sim_core)
add_test(NAME buildings COMMAND chunsa_test_buildings)

# --- Test Sprint 1.2 (K1): replay v3 + save v9 + ventana de setup (SPEC-004 §10) -
add_executable(chunsa_test_replay_v3 tests/unit/test_replay_v3.cpp)
target_link_libraries(chunsa_test_replay_v3 PRIVATE chunsa_sim_core)
add_test(NAME replay_v3 COMMAND chunsa_test_replay_v3)

# --- Test Sprint 1.2 (K2): producción, tecnología y épocas (SPEC-004 §11+§12) ---
add_executable(chunsa_test_production_tech tests/unit/test_production_tech.cpp)
target_link_libraries(chunsa_test_production_tech PRIVATE chunsa_sim_core)
target_compile_definitions(chunsa_test_production_tech PRIVATE
    CHUNSA_GOLDEN_CHDB_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/compiled/chunsa_base.chdb")
add_test(NAME production_tech COMMAND chunsa_test_production_tech)

# --- Test Sprint 1.4 (K1): victoria/derrota + perfil de IA tipado (SPEC-005) ---
add_executable(chunsa_test_victory_ai_profile tests/unit/test_victory_ai_profile.cpp)
target_link_libraries(chunsa_test_victory_ai_profile PRIVATE chunsa_sim_core)
target_compile_definitions(chunsa_test_victory_ai_profile PRIVATE
    CHUNSA_GOLDEN_CHDB_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/compiled/chunsa_base.chdb")
add_test(NAME victory_ai_profile COMMAND chunsa_test_victory_ai_profile)

# --- Test Sprint 1.4 (K2): IA de 3 capas — fixtures aislados (SPEC-005 §4/§8.4) ---
add_executable(chunsa_test_ai_layers tests/unit/test_ai_layers.cpp)
target_link_libraries(chunsa_test_ai_layers PRIVATE chunsa_sim_core)
add_test(NAME ai_layers COMMAND chunsa_test_ai_layers)

# --- Test Sprint 1.4 (K2): gate de fase — skirmish (SPEC-005 §8.3) -------------
add_executable(chunsa_test_ai_skirmish tests/unit/test_ai_skirmish.cpp)
target_link_libraries(chunsa_test_ai_skirmish PRIVATE chunsa_sim_core)
target_include_directories(chunsa_test_ai_skirmish PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/determinism)
add_test(NAME ai_skirmish COMMAND chunsa_test_ai_skirmish)

# --- Test Sprint 1.4 (K3): skirmish CON ECONOMÍA — aldeanos vulnerables --------
# (SPEC-004 §7.1: partida completa con economía que termina por conquista)
add_executable(chunsa_test_ai_skirmish_eco tests/unit/test_ai_skirmish_eco.cpp)
target_link_libraries(chunsa_test_ai_skirmish_eco PRIVATE chunsa_sim_core)
target_include_directories(chunsa_test_ai_skirmish_eco PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/determinism)
add_test(NAME ai_skirmish_eco COMMAND chunsa_test_ai_skirmish_eco)

# --- Test Sprint 1.6B (K1): civ_id tipado + depósitos desde el mapa (SPEC-004
# Parte III §15.3/§16/§17/§20) ---------------------------------------------------
add_executable(chunsa_test_civ_deposits tests/unit/test_civ_deposits.cpp)
target_link_libraries(chunsa_test_civ_deposits PRIVATE chunsa_sim_core)
target_compile_definitions(chunsa_test_civ_deposits PRIVATE
    CHUNSA_GOLDEN_CHDB_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/compiled/chunsa_base.chdb")
add_test(NAME civ_deposits COMMAND chunsa_test_civ_deposits)

# --- Test Sprint 1.6B (K2): comando GATHER + agotamiento/reasignación (SPEC-004
# §18) ----------------------------------------------------------------------
add_executable(chunsa_test_gather tests/unit/test_gather.cpp)
target_link_libraries(chunsa_test_gather PRIVATE chunsa_sim_core)
add_test(NAME gather COMMAND chunsa_test_gather)

# --- Test Sprint 1.7: modelo de tareas del ciudadano (SPEC-004 §22) -----------
add_executable(chunsa_test_citizen_task tests/unit/test_citizen_task.cpp)
target_link_libraries(chunsa_test_citizen_task PRIVATE chunsa_sim_core)
add_test(NAME citizen_task COMMAND chunsa_test_citizen_task)

# --- Test Sprint 1.8A: capacidad estructural de 32 recursos (SPEC-007 §9.3) --
add_executable(chunsa_test_resource_count tests/unit/test_resource_count.cpp)
target_link_libraries(chunsa_test_resource_count PRIVATE chunsa_sim_core)
add_test(NAME resource_count COMMAND chunsa_test_resource_count)

# --- Test Sprint 1.8C: metadatos tipados del catálogo de recursos -----------
add_executable(chunsa_test_resource_catalog tests/unit/test_resource_catalog.cpp)
target_link_libraries(chunsa_test_resource_catalog PRIVATE chunsa_sim_core)
target_compile_definitions(chunsa_test_resource_catalog PRIVATE
    CHUNSA_GOLDEN_CHDB_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/compiled/chunsa_base.chdb")
add_test(NAME resource_catalog COMMAND chunsa_test_resource_catalog)

# --- Test Sprint 1.6B (K2): la IA juega la apertura (SPEC-004 §19) -------------
add_executable(chunsa_test_ai_apertura tests/unit/test_ai_apertura.cpp)
target_link_libraries(chunsa_test_ai_apertura PRIVATE chunsa_sim_core)
add_test(NAME ai_apertura COMMAND chunsa_test_ai_apertura)

# --- Test Sprint 1.6B (K2): el escenario del DoD — apertura económica real
# (SPEC-004 §20, el corazón del sprint) --------------------------------------
add_executable(chunsa_test_ai_skirmish_apertura tests/unit/test_ai_skirmish_apertura.cpp)
target_link_libraries(chunsa_test_ai_skirmish_apertura PRIVATE chunsa_sim_core)
target_include_directories(chunsa_test_ai_skirmish_apertura PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/determinism)
target_compile_definitions(chunsa_test_ai_skirmish_apertura PRIVATE
    CHUNSA_GOLDEN_CHDB_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/compiled/chunsa_base.chdb")
add_test(NAME ai_skirmish_apertura COMMAND chunsa_test_ai_skirmish_apertura)

# --- Test Sprint 1.5A: política pura de fog para presentación -------------------
add_executable(chunsa_test_fog_view tests/unit/test_fog_view.cpp)
target_link_libraries(chunsa_test_fog_view PRIVATE chunsa_sim_core)
target_include_directories(chunsa_test_fog_view PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/addons/chunsa_sim/gdextension)
add_test(NAME fog_view COMMAND chunsa_test_fog_view)

# --- Test Sprint 1.8F: política pura de presentación del resultado -------------
# Mismo patrón que fog_view: la lógica del adaptador que se puede probar sin
# godot-cpp vive en una cabecera propia y SÍ tiene pruebas. Es la respuesta a la
# deuda registrada tras tres desincronizaciones kernel/adaptador.
add_executable(chunsa_test_outcome_view tests/unit/test_outcome_view.cpp)
target_link_libraries(chunsa_test_outcome_view PRIVATE chunsa_sim_core)
target_include_directories(chunsa_test_outcome_view PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/addons/chunsa_sim/gdextension)
add_test(NAME outcome_view COMMAND chunsa_test_outcome_view)

# --- Test Sprint 1.8H: política pura del panel de comandos ---------------------
add_executable(chunsa_test_command_panel_view tests/unit/test_command_panel_view.cpp)
target_link_libraries(chunsa_test_command_panel_view PRIVATE chunsa_sim_core)
target_include_directories(chunsa_test_command_panel_view PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/addons/chunsa_sim/gdextension)
add_test(NAME command_panel_view COMMAND chunsa_test_command_panel_view)

# --- Test Sprint 1.9: recetas y CRAFT (SPEC-007 §12.6) -------------------------
add_executable(chunsa_test_craft tests/unit/test_craft.cpp)
target_link_libraries(chunsa_test_craft PRIVATE chunsa_sim_core)
add_test(NAME craft COMMAND chunsa_test_craft)

# --- Guardián Sprint 1.9: época inicial por civilización ----------------------
# Un dato nuevo con epoch_min bajo mueve en silencio la época de arranque de su
# civilización y puede apagar la partida entera. Con 4-6 civs planificadas esto
# solo puede empeorar, asi que el acoplamiento se hace ruidoso.
add_executable(chunsa_test_civ_start_epoch tests/unit/test_civ_start_epoch.cpp)
target_link_libraries(chunsa_test_civ_start_epoch PRIVATE chunsa_sim_core)
target_compile_definitions(chunsa_test_civ_start_epoch PRIVATE
    CHUNSA_GOLDEN_CHDB_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/compiled/chunsa_base.chdb")
add_test(NAME civ_start_epoch COMMAND chunsa_test_civ_start_epoch)

# --- Test Sprint 1.18: fórmula de daño por tipo (SPEC-004 Parte VI) -----------
add_executable(chunsa_test_combat_damage tests/unit/test_combat_damage.cpp)
target_link_libraries(chunsa_test_combat_damage PRIVATE chunsa_sim_core)
add_test(NAME combat_damage COMMAND chunsa_test_combat_damage)

# --- Test Sprint 1.8E: política pura de asequibilidad del HUD ------------------
add_executable(chunsa_test_affordability_view tests/unit/test_affordability_view.cpp)
target_link_libraries(chunsa_test_affordability_view PRIVATE chunsa_sim_core)
target_include_directories(chunsa_test_affordability_view PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/addons/chunsa_sim/gdextension)
add_test(NAME affordability_view COMMAND chunsa_test_affordability_view)

# --- Gate Sprint 0.4: schemas, compilador y golden CHDB ------------------------
# tools/data_compile requiere PyYAML/jsonschema/referencing (chunsa_data_compiler.py,
# no parte del juego: solo tooling de datos). El intérprete se elige por el
# PRIMERO que REALMENTE pueda importar esas deps (verificado con un import real,
# para no elegir un Python "externally managed"/uv que las tenga ausentes), en
# orden: (1) venv local .venv (gitignored), (2) descubrimiento estándar de CMake,
# (3) /usr/bin/python3 (system Python de Linux, que suele traerlas). Reconciliación
# del Arquitecto: unifica los dos enfoques de las ramas de datos (venv vs system).
set(_chunsa_pycandidates "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.venv/bin/python3")
    list(APPEND _chunsa_pycandidates "${CMAKE_CURRENT_SOURCE_DIR}/.venv/bin/python3")
endif()
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    list(APPEND _chunsa_pycandidates "${Python3_EXECUTABLE}")
endif()
if(EXISTS "/usr/bin/python3")
    list(APPEND _chunsa_pycandidates "/usr/bin/python3")
endif()
set(CHUNSA_DATA_PY "")
foreach(_py IN LISTS _chunsa_pycandidates)
    execute_process(COMMAND "${_py}" -c "import jsonschema, yaml, referencing"
        RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_QUIET)
    if(_rc EQUAL 0)
        set(CHUNSA_DATA_PY "${_py}")
        break()
    endif()
endforeach()
if(CHUNSA_DATA_PY STREQUAL "")
    message(WARNING "data_compile: ningún Python con jsonschema+yaml+referencing; "
        "el gate fallará. Instala esas deps (pip) o crea un .venv en la raíz.")
    set(CHUNSA_DATA_PY "${Python3_EXECUTABLE}")
endif()
add_test(NAME data_compile
    COMMAND ${CHUNSA_DATA_PY} -m unittest discover
            -s ${CMAKE_CURRENT_SOURCE_DIR}/tools/data_compile
            -p test_*.py -v)
set_tests_properties(data_compile PROPERTIES
    ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

# --- Adaptador GDExtension + demo (Sprint 0.2; fuentes: kimi-k3, revisado) -------
option(CHUNSA_BUILD_GODOT "Compilar el adaptador GDExtension y la demo" OFF)
if(CHUNSA_BUILD_GODOT)
    add_library(chunsa_godot SHARED
        addons/chunsa_sim/gdextension/register_types.cpp
        addons/chunsa_sim/gdextension/chunsa_sim_node.cpp)
    target_include_directories(chunsa_godot PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/addons/chunsa_sim/core/include)
    target_include_directories(chunsa_godot SYSTEM PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party_build/godot-cpp/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party_build/godot-cpp/build/gen/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party_build/godot-cpp/gdextension)
    target_compile_definitions(chunsa_godot PRIVATE
        DEBUG_ENABLED GDEXTENSION LINUX_ENABLED THREADS_ENABLED UNIX_ENABLED)
    find_package(Threads REQUIRED)
    target_link_libraries(chunsa_godot PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party_build/godot-cpp/build/bin/libgodot-cpp.linux.template_debug.x86_64.a
        Threads::Threads)
    set_target_properties(chunsa_godot PROPERTIES
        OUTPUT_NAME "chunsa_godot"
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/demo/bin)
endif()

# --- SPEC-008 §2.2: reparto rápido/lento de la suite ---
# La suite completa ronda los 254 s y ai_skirmish_apertura sola son 187 s.
# Una suite de varios minutos en cada PR es una suite que se deja de ejecutar,
# y una suite que no se ejecuta no protege nada.
#   ctest -L fast   -> ciclo de desarrollo, objetivo <= 60 s (SPEC-008 §2.2)
#   ctest           -> todo, en merge y nightly
#
# TODA prueba nueva debe llevar etiqueta. Sin etiqueta no entra en `-L fast`
# y deja de protegernos en el ciclo de desarrollo sin que nadie se entere.
set_tests_properties(ai_skirmish_apertura ai_skirmish_eco ai_skirmish
                     PROPERTIES LABELS "slow")
set_tests_properties(props golden gate_g1 gate_g3 gate_g4
                     state ring flow_field flow_move combat
                     morale economy aggro replay_v2 data_blob
                     buildings replay_v3 production_tech victory_ai_profile ai_layers
                     civ_deposits gather citizen_task resource_count ai_apertura
                     resource_catalog fog_view outcome_view affordability_view
                     command_panel_view craft civ_start_epoch combat_damage data_compile
                     PROPERTIES LABELS "fast")

# --- Guardián de artefactos compilados del demo ---
# El .chdb y el .so de demo/ son artefactos compilados versionados. Dos veces
# han estado obsoletos respecto a la fuente de verdad y han producido bugs
# VISIBLES que ninguna prueba detectaba: primero un .so sin el adaptador 1.6B
# (el Director perdió una sesión de pruebas), luego un .chdb sin la tabla de
# recursos ("Recurso desconocido" en pantalla, HUD sin familias).
# Esta prueba falla ruidosamente en vez de dejarlo pasar.
add_test(NAME demo_chdb_sincronizado
         COMMAND ${CMAKE_COMMAND} -E compare_files
                 ${CMAKE_CURRENT_SOURCE_DIR}/demo/chunsa_base.chdb
                 ${CMAKE_CURRENT_SOURCE_DIR}/data/compiled/chunsa_base.chdb)
set_tests_properties(demo_chdb_sincronizado PROPERTIES LABELS "fast")
