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

# --- Tests ----------------------------------------------------------------------
enable_testing()
add_executable(chunsa_test_props tests/unit/test_props.cpp)
target_link_libraries(chunsa_test_props PRIVATE chunsa_sim_core)
add_test(NAME props COMMAND chunsa_test_props)
add_test(NAME golden COMMAND chunsa_sim_cli golden --vectors ${CMAKE_CURRENT_SOURCE_DIR}/tests/determinism/golden)

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
