set(POPPY_EXTRA_SOURCES)
if(WIN32)
    list(APPEND POPPY_EXTRA_SOURCES poppy.rc)
endif()

add_executable(poppy
    main.cpp
    resources.qrc
    ${POPPY_EXTRA_SOURCES}
    MainWindow.h MainWindow.cpp
    Theme.h Theme.cpp
    components/KeyValueTable.h components/KeyValueTable.cpp
    components/JsonSyntaxHighlighter.h components/JsonSyntaxHighlighter.cpp
    editors/ParamsEditor.h editors/ParamsEditor.cpp
    editors/HeadersEditor.h editors/HeadersEditor.cpp
    editors/BodyEditor.h editors/BodyEditor.cpp
    editors/AuthEditor.h editors/AuthEditor.cpp
    editors/ScriptEditor.h editors/ScriptEditor.cpp
    editors/AssertionsEditor.h editors/AssertionsEditor.cpp
    inspectors/ResponseInspector.h inspectors/ResponseInspector.cpp
    sidebar/CollectionSidebar.h sidebar/CollectionSidebar.cpp
    dialogs/EnvironmentDialog.h dialogs/EnvironmentDialog.cpp
    dialogs/CodeSnippetDialog.h dialogs/CodeSnippetDialog.cpp
    dialogs/ImportDialog.h dialogs/ImportDialog.cpp
    dialogs/CollectionRunnerDialog.h dialogs/CollectionRunnerDialog.cpp
    dialogs/SettingsDialog.h dialogs/SettingsDialog.cpp
    dialogs/OAuth2TokenDialog.h dialogs/OAuth2TokenDialog.cpp
    dialogs/QuickOpenDialog.h dialogs/QuickOpenDialog.cpp
    dialogs/CookieManagerDialog.h dialogs/CookieManagerDialog.cpp
    dialogs/DiffViewerDialog.h dialogs/DiffViewerDialog.cpp
    dialogs/WebSocketDialog.h dialogs/WebSocketDialog.cpp
    dialogs/GrpcDialog.h dialogs/GrpcDialog.cpp
    dialogs/SseDialog.h dialogs/SseDialog.cpp
    dialogs/MockServerDialog.h dialogs/MockServerDialog.cpp
    dialogs/GitSyncDialog.h dialogs/GitSyncDialog.cpp
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
