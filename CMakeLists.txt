cmake_minimum_required(VERSION 3.20)
project(Poppy LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable automatic Qt MOC, UIC, and RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Configure output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find Qt 6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Qml)
if(TARGET Qt6::Test)
    find_package(Qt6 COMPONENTS Test QUIET)
endif()

# Fetch and configure libcurl (static, using Windows Schannel)
set(BUILD_CURL_EXE OFF CACHE INTERNAL "")
set(BUILD_TESTING OFF CACHE INTERNAL "")
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
set(CURL_USE_SCHANNEL ON CACHE INTERNAL "")
set(CURL_ENABLE_SSL ON CACHE INTERNAL "")
set(ENABLE_CURL_MANUAL OFF CACHE INTERNAL "")
set(CURL_DISABLE_LDAP ON CACHE INTERNAL "")
set(CURL_DISABLE_LDAPS ON CACHE INTERNAL "")

include(FetchContent)
FetchContent_Declare(
    curl
    GIT_REPOSITORY https://github.com/curl/curl.git
    GIT_TAG curl-8_10_1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(curl)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Subdirectories
add_subdirectory(src/core)
add_subdirectory(src/network)
add_subdirectory(src/gui)
add_subdirectory(src/cli)

# Testing
enable_testing()
add_subdirectory(tests)
