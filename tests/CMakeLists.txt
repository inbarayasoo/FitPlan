include(GoogleTest)

# Absolute path to the migrations folder so tests do not depend on the working
# directory ctest happens to run them from. Shared by both test binaries.
set(FITPLAN_TEST_MIGRATIONS_DEFINE
    FITPLAN_TEST_MIGRATIONS_DIR="${PROJECT_SOURCE_DIR}/src/db/migrations")

# ---------------------------------------------------------------------------
# Unit tests - pure logic, no sockets.
# ---------------------------------------------------------------------------
add_executable(fitplan_unit_tests
    unit/auth_service_test.cpp
    unit/coach_trainee_repository_test.cpp
    unit/config_test.cpp
    unit/database_test.cpp
    unit/exercise_repository_test.cpp
    unit/hashing_test.cpp
    unit/plan_item_repository_test.cpp
    unit/plan_repository_test.cpp
    unit/jwt_auth_middleware_test.cpp
    unit/jwt_test.cpp
    unit/user_repository_test.cpp
    unit/validation_test.cpp)

target_link_libraries(fitplan_unit_tests PRIVATE
    fitplan_lib
    fitplan_warnings
    GTest::gtest_main)

target_compile_definitions(fitplan_unit_tests PRIVATE
    ${FITPLAN_TEST_MIGRATIONS_DEFINE})

gtest_discover_tests(fitplan_unit_tests
    PROPERTIES LABELS "unit")

# ---------------------------------------------------------------------------
# Integration tests - start a real server on a loopback port and drive it over
# a TCP socket.
# ---------------------------------------------------------------------------
find_package(Threads REQUIRED)

add_executable(fitplan_integration_tests
    integration/auth_flow_test.cpp
    integration/exercise_flow_test.cpp
    integration/plan_flow_test.cpp)

target_link_libraries(fitplan_integration_tests PRIVATE
    fitplan_lib
    fitplan_warnings
    GTest::gtest_main
    Threads::Threads)

target_compile_definitions(fitplan_integration_tests PRIVATE
    ${FITPLAN_TEST_MIGRATIONS_DEFINE})

gtest_discover_tests(fitplan_integration_tests
    PROPERTIES LABELS "integration")
