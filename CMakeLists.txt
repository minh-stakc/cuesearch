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

enable_testing()
add_executable(unit_tests tests/test_poly_solvers.cpp)
target_link_libraries(unit_tests PRIVATE cuesearch_math cuesearch_core Catch2::Catch2WithMain)
# Self-contained exe: avoids MinGW runtime-DLL lookup; aids reproducibility.
target_link_options(unit_tests PRIVATE -static -static-libgcc -static-libstdc++)

# Register as a single CTest entry WITHOUT running the exe at build time
# (catch_discover_tests executes it during build -> MinGW DLL issues).
add_test(NAME unit_tests COMMAND unit_tests)
