cmake_minimum_required(VERSION 3.24)

project(FitPlan
    VERSION 0.1.0
    DESCRIPTION "Workout tracking platform - REST API for coaches and trainees"
    HOMEPAGE_URL "https://github.com/inbarayaso/FitPlan"
    LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Global build settings
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(FITPLAN_BUILD_TESTS "Build unit and integration tests" ON)
option(FITPLAN_WERROR "Treat compiler warnings as errors" OFF)

# ---------------------------------------------------------------------------
# Warning flags, exposed as an interface target that first-party code links to.
# Third-party dependencies are pulled in as SYSTEM includes (see cmake/Dependencies.cmake)
# so their headers do not trip these warnings.
# ---------------------------------------------------------------------------
add_library(fitplan_warnings INTERFACE)
target_compile_options(fitplan_warnings INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Wnull-dereference
    -Wdouble-promotion -Wimplicit-fallthrough -Wformat=2
    $<$<BOOL:${FITPLAN_WERROR}>:-Werror>)

# ---------------------------------------------------------------------------
# Dependencies (FetchContent) + generated version header
# ---------------------------------------------------------------------------
include(Dependencies)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/fitplan/Version.hpp"
    @ONLY)

# ---------------------------------------------------------------------------
# Core library: all application code except the process entry point, so that
# both the executable and the test binaries can link against it.
# ---------------------------------------------------------------------------
add_library(fitplan_lib STATIC
    src/config/Config.cpp
    src/db/Database.cpp
    src/db/MigrationSet.cpp
    src/repositories/ExerciseRepository.cpp
    src/repositories/UserRepository.cpp)

target_include_directories(fitplan_lib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")

target_link_libraries(fitplan_lib
    PUBLIC
        Crow::Crow
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        SQLiteCpp
    PRIVATE
        fitplan_warnings)

# ---------------------------------------------------------------------------
# Server executable
# ---------------------------------------------------------------------------
add_executable(fitplan src/main.cpp)
target_link_libraries(fitplan PRIVATE fitplan_lib fitplan_warnings)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(FITPLAN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
