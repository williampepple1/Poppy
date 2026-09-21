add_library(poppy_core STATIC
    RequestModel.h RequestModel.cpp
    ResponseModel.h ResponseModel.cpp
    EnvironmentModel.h EnvironmentModel.cpp
    VariableResolver.h VariableResolver.cpp
    BruParser.h BruParser.cpp
    BruWriter.h BruWriter.cpp
    CollectionModel.h CollectionModel.cpp
    ScriptRunner.h ScriptRunner.cpp
    assertions/DeclarativeAssertion.h assertions/DeclarativeAssertion.cpp
    importers/CurlImporter.h importers/CurlImporter.cpp
    importers/PostmanImporter.h importers/PostmanImporter.cpp
    importers/OpenApiImporter.h importers/OpenApiImporter.cpp
    importers/InsomniaImporter.h importers/InsomniaImporter.cpp
    auth/AwsSigV4Signer.h auth/AwsSigV4Signer.cpp
    codegen/CodeGenerator.h codegen/CodeGenerator.cpp
    exporters/OpenApiExporter.h exporters/OpenApiExporter.cpp
)

target_include_directories(poppy_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(poppy_core PUBLIC
    Qt6::Core
    Qt6::Qml
)
