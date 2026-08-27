include(GoogleTest)

add_executable(fitplan_unit_tests
    unit/config_test.cpp
    unit/database_test.cpp
    unit/exercise_repository_test.cpp
    unit/user_repository_test.cpp)

target_link_libraries(fitplan_unit_tests PRIVATE
    fitplan_lib
    fitplan_warnings
    GTest::gtest_main)

# Absolute path to the migrations folder so DB tests do not depend on the
# working directory ctest happens to run them from.
target_compile_definitions(fitplan_unit_tests PRIVATE
    FITPLAN_TEST_MIGRATIONS_DIR="${PROJECT_SOURCE_DIR}/src/db/migrations")

gtest_discover_tests(fitplan_unit_tests
    PROPERTIES LABELS "unit")
