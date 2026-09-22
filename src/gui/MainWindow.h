#pragma once

#include <QMainWindow>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QSplitter>
#include <QLabel>
#include <core/RequestModel.h>
#include <core/ResponseModel.h>
#include <core/CollectionModel.h>
#include <core/HistoryManager.h>
#include <core/VariableResolver.h>
#include <core/ScriptRunner.h>
#include <network/CurlNetworkEngine.h>

#include "sidebar/CollectionSidebar.h"
#include "editors/ParamsEditor.h"
#include "editors/HeadersEditor.h"
#include "editors/BodyEditor.h"
#include "editors/AuthEditor.h"
#include "editors/ScriptEditor.h"
#include "inspectors/ResponseInspector.h"

namespace poppy::gui {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onNewRequest();
    void onQuickOpen();
    void onSendClicked();
    void onOpenCollection();
    void onExportOpenApi();
    void onSaveRequest();
    void onCopyAsCurl();
    void onShowCodeSnippets();
    void onImport();
    void onRunCollection();
    void onManageEnvironments();
    void onRequestSelected(core::CollectionItem* item);
    void onEnvironmentChanged(const QString& envName);
    void onMethodChanged(int index);
    void onOpenSettings();
    void onTabChanged(int index);
    void onTabCloseRequested(int index);
    void onCloseCurrentTab();
    void markCurrentTabDirty();
    void onHistoryItemSelected(const core::HistoryItem& item);
    void onManageCookies();
    void onClearCookieJar();
    void onRenameTab(int index);
    void onShowShortcuts();

private:
    void setupUi();
    void setupMenus();
    void loadRequestIntoUi(const core::RequestModel& req);
    void saveUiIntoRequest(core::RequestModel& req);
    void closeTab(int index);

    // Core & Network engines
    core::CollectionModel m_collectionModel;
    core::HistoryManager m_historyManager;
    network::CurlNetworkEngine m_networkEngine;
    core::ScriptRunner m_scriptRunner;

    // Active state & Tab management
    struct OpenTabInfo {
        core::CollectionItem* item{nullptr};
        core::RequestModel request;
        bool isDirty{false};
    };
    QList<OpenTabInfo> m_openTabs;
    int m_currentTabIndex{-1};
    class QTabBar* m_openRequestsTabBar{nullptr};

    core::CollectionItem* m_activeItem{nullptr};
    core::RequestModel m_currentRequest;
    QString m_activeEnvName;

    // UI elements
    CollectionSidebar* m_sidebar;
    QLabel* m_requestNameLabel;
    QComboBox* m_methodCombo;
    QLineEdit* m_urlEdit;
    QPushButton* m_sendBtn;
    QPushButton* m_saveBtn;
    QPushButton* m_curlBtn;
    QPushButton* m_snippetBtn;

    QTabWidget* m_requestTabs;
    ParamsEditor* m_paramsEditor;
    HeadersEditor* m_headersEditor;
    BodyEditor* m_bodyEditor;
    AuthEditor* m_authEditor;
    ScriptEditor* m_scriptEditor;
    class AssertionsEditor* m_assertionsEditor;

    ResponseInspector* m_responseInspector;
};

} // namespace poppy::gui
