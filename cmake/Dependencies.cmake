include(FetchContent)

# Show clone/download progress on the first configure (it can take a few minutes).
set(FETCHCONTENT_QUIET OFF CACHE BOOL "" FORCE)

# ---------------------------------------------------------------------------
# libsodium - Argon2id password hashing. Installed from apt (libsodium-dev),
# not fetched: it is a security-sensitive C library we want the distro to keep
# patched. Found through its pkg-config file and exposed to first-party targets
# as the imported target PkgConfig::libsodium.
# ---------------------------------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(libsodium REQUIRED IMPORTED_TARGET libsodium)

# ---------------------------------------------------------------------------
# OpenSSL - already pulled in transitively by jwt-cpp and cpr, but Step 9 calls
# libcrypto directly (building an RSA public key from a JWK's modulus/exponent),
# so depend on it explicitly and link OpenSSL::Crypto into fitplan_lib.
# ---------------------------------------------------------------------------
find_package(OpenSSL REQUIRED)

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
# SQLiteCpp - RAII C++ wrapper around the SQLite C API.
# We link the system SQLite (libsqlite3-dev) instead of the copy bundled in the
# wrapper (SQLITECPP_INTERNAL_SQLITE=OFF): one well-known, security-patched
# version, and a smaller build.
# ---------------------------------------------------------------------------
set(SQLITECPP_INTERNAL_SQLITE OFF CACHE BOOL "" FORCE)
set(SQLITECPP_RUN_CPPLINT OFF CACHE BOOL "" FORCE)
set(SQLITECPP_RUN_CPPCHECK OFF CACHE BOOL "" FORCE)
set(SQLITECPP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SQLITECPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SQLiteCpp
    GIT_REPOSITORY https://github.com/SRombauts/SQLiteCpp.git
    GIT_TAG        3.3.3
    GIT_SHALLOW    TRUE
    SYSTEM)

# ---------------------------------------------------------------------------
# jwt-cpp - header-only JSON Web Token create / verify. Uses the system OpenSSL
# (libssl-dev) for the HMAC-SHA256 signature.
# ---------------------------------------------------------------------------
set(JWT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JWT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(jwt-cpp
    GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
    GIT_TAG        v0.7.0
    GIT_SHALLOW    TRUE
    SYSTEM)

# ---------------------------------------------------------------------------
# cpr - a small, requests-style C++ wrapper over libcurl for outbound HTTP.
# Step 9 (Google Sign-In) uses it to fetch Google's public signing keys (JWKS)
# over HTTPS. Built against the system libcurl (libcurl4-openssl-dev) instead of
# a bundled copy - same policy as SQLite and OpenSSL: one distro-patched TLS
# stack the platform keeps updated.
# ---------------------------------------------------------------------------
set(CPR_USE_SYSTEM_CURL ON CACHE BOOL "" FORCE)
set(CPR_ENABLE_SSL ON CACHE BOOL "" FORCE)
set(CPR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(cpr
    GIT_REPOSITORY https://github.com/libcpr/cpr.git
    GIT_TAG        1.11.2
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
    FetchContent_MakeAvailable(nlohmann_json spdlog Crow SQLiteCpp jwt-cpp cpr googletest)
else()
    FetchContent_MakeAvailable(nlohmann_json spdlog Crow SQLiteCpp jwt-cpp cpr)
endif()
