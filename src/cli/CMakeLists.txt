add_executable(poppy-cli
    main.cpp
)

target_include_directories(poppy-cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(poppy-cli PRIVATE
    poppy_core
    poppy_network
    Qt6::Core
    Qt6::Network
    Qt6::Qml
)
