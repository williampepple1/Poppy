add_executable(poppy
    main.cpp
    MainWindow.h MainWindow.cpp
    Theme.h Theme.cpp
    components/KeyValueTable.h components/KeyValueTable.cpp
    components/JsonSyntaxHighlighter.h components/JsonSyntaxHighlighter.cpp
    editors/ParamsEditor.h editors/ParamsEditor.cpp
    editors/HeadersEditor.h editors/HeadersEditor.cpp
    editors/BodyEditor.h editors/BodyEditor.cpp
    editors/AuthEditor.h editors/AuthEditor.cpp
    editors/ScriptEditor.h editors/ScriptEditor.cpp
    inspectors/ResponseInspector.h inspectors/ResponseInspector.cpp
    sidebar/CollectionSidebar.h sidebar/CollectionSidebar.cpp
    dialogs/EnvironmentDialog.h dialogs/EnvironmentDialog.cpp
)

target_include_directories(poppy PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(poppy PRIVATE
    poppy_core
    poppy_network
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Qml
)

if(WIN32)
    set_target_properties(poppy PROPERTIES
        WIN32_EXECUTABLE FALSE
    )
endif()
