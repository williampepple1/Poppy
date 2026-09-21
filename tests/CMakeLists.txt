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

add_executable(test_assertions test_assertions.cpp)
target_link_libraries(test_assertions PRIVATE poppy_core Qt6::Core Qt6::Qml)
add_test(NAME test_assertions COMMAND test_assertions)

add_executable(test_codegen test_codegen.cpp)
target_link_libraries(test_codegen PRIVATE poppy_core Qt6::Core)
add_test(NAME test_codegen COMMAND test_codegen)

add_executable(test_importers test_importers.cpp)
target_link_libraries(test_importers PRIVATE poppy_core Qt6::Core)
add_test(NAME test_importers COMMAND test_importers)

add_executable(test_graphql test_graphql.cpp)
target_link_libraries(test_graphql PRIVATE poppy_core Qt6::Core)
add_test(NAME test_graphql COMMAND test_graphql)

add_executable(test_aws_sigv4 test_aws_sigv4.cpp)
target_link_libraries(test_aws_sigv4 PRIVATE poppy_core Qt6::Core)
add_test(NAME test_aws_sigv4 COMMAND test_aws_sigv4)

add_executable(test_insomnia_importer test_insomnia_importer.cpp)
target_link_libraries(test_insomnia_importer PRIVATE poppy_core Qt6::Core)
add_test(NAME test_insomnia_importer COMMAND test_insomnia_importer)

add_executable(test_auth_digest_ntlm test_auth_digest_ntlm.cpp)
target_link_libraries(test_auth_digest_ntlm PRIVATE poppy_core Qt6::Core)
add_test(NAME test_auth_digest_ntlm COMMAND test_auth_digest_ntlm)

add_executable(test_openapi_exporter test_openapi_exporter.cpp)
target_link_libraries(test_openapi_exporter PRIVATE poppy_core Qt6::Core)
add_test(NAME test_openapi_exporter COMMAND test_openapi_exporter)

add_executable(test_jsonpath_evaluator test_jsonpath_evaluator.cpp)
target_link_libraries(test_jsonpath_evaluator PRIVATE poppy_core Qt6::Core)
add_test(NAME test_jsonpath_evaluator COMMAND test_jsonpath_evaluator)

add_executable(test_cookie_jar test_cookie_jar.cpp)
target_link_libraries(test_cookie_jar PRIVATE poppy_core Qt6::Core)
add_test(NAME test_cookie_jar COMMAND test_cookie_jar)

add_executable(test_history_manager test_history_manager.cpp)
target_link_libraries(test_history_manager PRIVATE poppy_core Qt6::Core)
add_test(NAME test_history_manager COMMAND test_history_manager)
