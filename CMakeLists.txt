cmake_minimum_required(VERSION 3.16)
project(cuesearch LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)          # GCC 8.1.0 (MinGW) — C++17, NOT C++20
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Determinism: no FMA contraction, no fast-math. Reproducible FP across builds.
add_compile_options(-ffp-contract=off -fno-fast-math -Wall -Wextra)

include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.5.2
)
FetchContent_MakeAvailable(Catch2)

add_library(cuesearch_core INTERFACE)
target_include_directories(cuesearch_core INTERFACE ${CMAKE_SOURCE_DIR})

add_library(cuesearch_math STATIC math/poly_solvers.cpp)
target_include_directories(cuesearch_math PUBLIC ${CMAKE_SOURCE_DIR})

add_library(cuesearch_engine STATIC
  engine/motion.cpp engine/cuestrike.cpp engine/world.cpp
  engine/resolve_ballball.cpp engine/resolve_cushion.cpp engine/game.cpp
  engine/layout.cpp)
find_package(Threads REQUIRED)
target_include_directories(cuesearch_engine PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(cuesearch_engine PUBLIC cuesearch_math)

enable_testing()

# Self-contained exes: avoid MinGW runtime-DLL lookup; aid reproducibility.
# Register CTest entries WITHOUT running the exe at build time
# (catch_discover_tests executes it during build -> MinGW DLL issues).
function(add_unit_test name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE cuesearch_core Catch2::Catch2WithMain)
  target_link_options(${name} PRIVATE -static -static-libgcc -static-libstdc++)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

add_unit_test(unit_tests tests/test_poly_solvers.cpp)
target_link_libraries(unit_tests PRIVATE cuesearch_math)

add_unit_test(motion_tests tests/test_motion.cpp)
target_link_libraries(motion_tests PRIVATE cuesearch_engine)

add_unit_test(world_tests tests/test_world.cpp)
target_link_libraries(world_tests PRIVATE cuesearch_engine)

add_unit_test(resolve_tests tests/test_resolve.cpp)
target_link_libraries(resolve_tests PRIVATE cuesearch_engine)

add_unit_test(effects_tests tests/test_effects.cpp)
target_link_libraries(effects_tests PRIVATE cuesearch_engine)

add_unit_test(cushion_tests tests/test_cushion.cpp)
target_link_libraries(cushion_tests PRIVATE cuesearch_engine)

add_unit_test(invariant_tests tests/test_invariants.cpp)
target_link_libraries(invariant_tests PRIVATE cuesearch_engine)

add_unit_test(game_tests tests/test_game.cpp)
target_link_libraries(game_tests PRIVATE cuesearch_engine)

add_library(cuesearch_solver STATIC
  solver/solver.cpp solver/plan.cpp solver/winsolve.cpp solver/shape.cpp
  solver/difficulty.cpp)
target_include_directories(cuesearch_solver PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(cuesearch_solver PUBLIC cuesearch_engine Threads::Threads)

add_unit_test(solver_tests tests/test_solver.cpp)
target_link_libraries(solver_tests PRIVATE cuesearch_solver)

add_unit_test(plan_tests tests/test_plan.cpp)
target_link_libraries(plan_tests PRIVATE cuesearch_solver)

add_unit_test(io_tests tests/test_io.cpp)
target_link_libraries(io_tests PRIVATE cuesearch_solver)

add_unit_test(winsolve_tests tests/test_winsolve.cpp)
target_link_libraries(winsolve_tests PRIVATE cuesearch_solver)

add_unit_test(leave_tests tests/test_leave.cpp)
target_link_libraries(leave_tests PRIVATE cuesearch_engine)

add_unit_test(shape_tests tests/test_shape.cpp)
target_link_libraries(shape_tests PRIVATE cuesearch_solver)

add_unit_test(runout_tests tests/test_runout.cpp)
target_link_libraries(runout_tests PRIVATE cuesearch_solver)

add_unit_test(potcurve_tests tests/test_potcurve.cpp)
target_link_libraries(potcurve_tests PRIVATE cuesearch_solver)

add_unit_test(runout_default_tests tests/test_runout_default.cpp)
target_link_libraries(runout_default_tests PRIVATE cuesearch_solver)

add_unit_test(difficulty_tests tests/test_difficulty.cpp)
target_link_libraries(difficulty_tests PRIVATE cuesearch_solver)

add_unit_test(combo_tests tests/test_combo.cpp)
target_link_libraries(combo_tests PRIVATE cuesearch_solver)

add_executable(bench_solver bench/bench_solver.cpp)
target_link_libraries(bench_solver PRIVATE cuesearch_solver)
target_link_options(bench_solver PRIVATE -static -static-libgcc -static-libstdc++)

add_executable(trace_shot tools/trace_shot.cpp)
target_link_libraries(trace_shot PRIVATE cuesearch_solver)
target_link_options(trace_shot PRIVATE -static -static-libgcc -static-libstdc++)

add_executable(solve tools/solve.cpp)
target_link_libraries(solve PRIVATE cuesearch_solver)
target_link_options(solve PRIVATE -static -static-libgcc -static-libstdc++)

add_executable(randtable tools/randtable.cpp)
target_link_libraries(randtable PRIVATE cuesearch_core)
target_link_options(randtable PRIVATE -static -static-libgcc -static-libstdc++)

add_executable(match tools/match.cpp)
target_link_libraries(match PRIVATE cuesearch_solver)
target_link_options(match PRIVATE -static -static-libgcc -static-libstdc++)
