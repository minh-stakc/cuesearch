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
  engine/motion.cpp engine/cuestrike.cpp engine/world.cpp)
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
