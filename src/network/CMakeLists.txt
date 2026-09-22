add_library(poppy_network STATIC
    INetworkEngine.h
    CurlNetworkEngine.h CurlNetworkEngine.cpp
    WebSocketClient.h WebSocketClient.cpp
    GrpcClient.h GrpcClient.cpp
    MockServer.h MockServer.cpp
)

target_include_directories(poppy_network PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(poppy_network PUBLIC
    poppy_core
    Qt6::Core
    Qt6::Network
    libcurl
)
