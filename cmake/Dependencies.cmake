include(FetchContent)

# Show clone/download progress on the first configure (it can take a few minutes).
set(FETCHCONTENT_QUIET OFF CACHE BOOL "" FORCE)

# ---------------------------------------------------------------------------
# nlohmann/json - JSON parsing / serialization
# ---------------------------------------------------------------------------
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
    SYSTEM)

# ---------------------------------------------------------------------------
# spdlog - structured logging
# ---------------------------------------------------------------------------
set(SPDLOG_BUILD_PIC ON CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.15.3
    GIT_SHALLOW    TRUE
    SYSTEM)

# ---------------------------------------------------------------------------
# Crow - HTTP micro-framework (uses the system standalone Asio: libasio-dev)
# ---------------------------------------------------------------------------
set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CROW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CROW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(Crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG        v1.2.1.2
    GIT_SHALLOW    TRUE
    SYSTEM)

# ---------------------------------------------------------------------------
# GoogleTest - unit / integration test framework
# ---------------------------------------------------------------------------
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
    GIT_SHALLOW    TRUE
    SYSTEM)

if(FITPLAN_BUILD_TESTS)
    FetchContent_MakeAvailable(nlohmann_json spdlog Crow googletest)
else()
    FetchContent_MakeAvailable(nlohmann_json spdlog Crow)
endif()
