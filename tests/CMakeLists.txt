# Unit test executables

add_executable(test_bru_parser test_bru_parser.cpp)
target_link_libraries(test_bru_parser PRIVATE poppy_core Qt6::Core)
add_test(NAME test_bru_parser COMMAND test_bru_parser)

add_executable(test_variable_resolver test_variable_resolver.cpp)
target_link_libraries(test_variable_resolver PRIVATE poppy_core Qt6::Core)
add_test(NAME test_variable_resolver COMMAND test_variable_resolver)

add_executable(test_script_runner test_script_runner.cpp)
target_link_libraries(test_script_runner PRIVATE poppy_core Qt6::Core Qt6::Qml)
add_test(NAME test_script_runner COMMAND test_script_runner)

add_executable(test_network test_network.cpp)
target_link_libraries(test_network PRIVATE poppy_core poppy_network Qt6::Core Qt6::Network)
add_test(NAME test_network COMMAND test_network)
