include(GoogleTest)

add_executable(fitplan_unit_tests
    unit/config_test.cpp)

target_link_libraries(fitplan_unit_tests PRIVATE
    fitplan_lib
    fitplan_warnings
    GTest::gtest_main)

gtest_discover_tests(fitplan_unit_tests
    PROPERTIES LABELS "unit")
