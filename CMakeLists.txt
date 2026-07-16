# AEON001 — kernel de simulación determinista (Sprint 0.1A)
# SPEC-001 v1.1 (APPROVED_FOR_EXECUTION). fast-math PROHIBIDO (SPEC-001 §1.1.2 base §1.2).
cmake_minimum_required(VERSION 3.24)
project(aeon_sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AEON_WIDE128_FORCE_PORTABLE "Forzar el backend portable de Wide128 (lane de CI)" OFF)

# --- Núcleo: header-only, C++ puro, CERO dependencias de Godot -----------------
add_library(aeon_sim_core INTERFACE)
target_include_directories(aeon_sim_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/addons/aeon_sim/core/include)
if(AEON_WIDE128_FORCE_PORTABLE)
    target_compile_definitions(aeon_sim_core INTERFACE AEON_WIDE128_FORCE_PORTABLE=1)
endif()

# --- Warnings-as-errors (goal 0.1A: build limpio, 0 warnings) ------------------
if(MSVC)
    add_compile_options(/W4 /WX /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wshadow -Werror)
endif()

# --- CLI ------------------------------------------------------------------------
add_executable(aeon_sim_cli addons/aeon_sim/cli/main.cpp)
target_link_libraries(aeon_sim_cli PRIVATE aeon_sim_core)

# --- Tests ----------------------------------------------------------------------
enable_testing()
add_executable(aeon_test_props tests/unit/test_props.cpp)
target_link_libraries(aeon_test_props PRIVATE aeon_sim_core)
add_test(NAME props COMMAND aeon_test_props)
add_test(NAME golden COMMAND aeon_sim_cli golden --vectors ${CMAKE_CURRENT_SOURCE_DIR}/tests/determinism/golden)
