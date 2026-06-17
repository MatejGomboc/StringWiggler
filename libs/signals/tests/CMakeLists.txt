add_executable(signal_tests
    signal_tests.cpp
)

target_link_libraries(signal_tests PRIVATE signals testing)

add_test(NAME signal_tests COMMAND signal_tests)
set_tests_properties(signal_tests PROPERTIES TIMEOUT 10)
