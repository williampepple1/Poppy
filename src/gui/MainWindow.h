#pragma once

#include <QMainWindow>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QSplitter>
#include <QLabel>
#include <QCloseEvent>
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

    bool openPath(const QString& path, bool remember = true);
    void onCloseCollection();

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(class QDragEnterEvent* event) override;
    void dropEvent(class QDropEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void updateRecentCollectionsMenu();
    void clearRecentCollections();
    void onNewRequest();
    void onQuickOpen();
    void onSendClicked();
    void onOpenCollection();
    void onExportOpenApi();
    void onExportPostman();
    void onExportInsomnia();
    void onExportHar();
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
    void onTabMoved(int from, int to);
    void onTabCloseRequested(int index);
    void onCloseCurrentTab();
    void onItemAboutToBeDeleted(core::CollectionItem* item);
    void onCollectionAboutToReload();
    void onCollectionLoaded();
    void markCurrentTabDirty();
    void onHistoryItemSelected(const core::HistoryItem& item);
    void onManageCookies();
    void onClearCookieJar();
    void onRenameTab(int index);
    void onShowShortcuts();
    void onTabContextMenu(const QPoint& pos);
    void onTogglePinCurrentTab();
    void onAutoSaveTimerTimeout();
    void onConfigureRequestProxy();
    void onOpenDiffViewer();
    void onOpenWebSocket();
    void onOpenGrpc();
    void onOpenMockServer();
    void onOpenSse();
    void onOpenGitSync(const QString& targetPath = QString(), const QString& initialCommitMsg = QString());
    void onGenerateDocumentation();
    void onToggleTheme();
    void onSelectTheme(const QString& themeId);
    void applyTheme(const QString& themeId = QString(), bool notifyUser = true);
    void onShowQuickVariables();
    void updateUrlVariableInspection();
    void onShowSessionTelemetry();
    void onExportMarkdown();
    void onFindAndReplace();
    void onOpenCommandPalette();
    void onVariableSaved(const QString& name, const QString& value, const QString& scope, bool isSecret);

private:
    void setupUi();
    void setupMenus();
    void applyRequestChrome();
    void loadRequestIntoUi(const core::RequestModel& req);
    void saveUiIntoRequest(core::RequestModel& req);
    void closeTab(int index);
    void updateTabTitle(int index);
    void updateRequestTabBadges();
    core::VariableResolver currentVariableResolver() const;
    core::CollectionItem* scopeItem() const;
    core::RequestModel requestForExecution() const;
    void updateTopEnvCombo();
    void rebindOpenTabs();
    void updateSessionTelemetryWidget();
    void refreshEnvironmentUi();
    core::EnvironmentModel* mutableActiveEnvironment();
    void persistActiveEnvironment();
    void flushDirtyTab(int index);
    void saveAppState();
    void restoreAppState();

    // Core & Network engines
    core::CollectionModel m_collectionModel;
    core::HistoryManager m_historyManager;
    network::CurlNetworkEngine m_networkEngine;
    core::ScriptRunner m_scriptRunner;

    // Active state & Tab management
    struct OpenTabInfo {
        quint64 tabId{0};
        core::CollectionItem* item{nullptr};
        QString itemPath;
        QString resolvePath;
        core::RequestModel request;
        bool isDirty{false};
        bool isPinned{false};
        core::ResponseModel lastResponse;
        core::TestReport lastReport;
        bool hasResponse{false};
    };
    QList<OpenTabInfo> m_openTabs;
    int m_currentTabIndex{-1};
    quint64 m_nextTabId{1};
    class QTabBar* m_openRequestsTabBar{nullptr};
    QTimer* m_autoSaveTimer{nullptr};
    bool m_loadingUi{false};
    quint64 m_sendGeneration{0};

    core::CollectionItem* m_activeItem{nullptr};
    core::RequestModel m_currentRequest;
    QString m_activeEnvName;

    // Session Telemetry Tracker
    QPushButton* m_sessionTelemetryBtn{nullptr};
    int m_sessionReqCount{0};
    qint64 m_sessionBytesReceived{0};
    qint64 m_sessionTotalLatencyMs{0};
    int m_sessionErrorCount{0};

    // UI elements
    CollectionSidebar* m_sidebar;
    QLabel* m_requestNameLabel{nullptr};
    QComboBox* m_methodCombo;
    QLineEdit* m_urlEdit;
    QPushButton* m_sendBtn;
    QPushButton* m_saveBtn;
    QPushButton* m_curlBtn;
    QPushButton* m_snippetBtn;
    QPushButton* m_proxyBtn;

    QTabWidget* m_requestTabs;
    ParamsEditor* m_paramsEditor;
    HeadersEditor* m_headersEditor;
    BodyEditor* m_bodyEditor;
    AuthEditor* m_authEditor;
    ScriptEditor* m_scriptEditor;
    class AssertionsEditor* m_assertionsEditor;

    ResponseInspector* m_responseInspector;
    class QCompleter* m_urlCompleter{nullptr};
    QPushButton* m_varQuickBtn{nullptr};
    QComboBox* m_topEnvCombo{nullptr};
    QSplitter* m_mainSplitter{nullptr};
    QSplitter* m_contentSplitter{nullptr};  // Request | Response splitter (toggled H/V)
    QMenu* m_recentCollectionsMenu{nullptr};
    QAction* m_closeCollectionAction{nullptr};
    class VariableHoverPopup* m_variableHoverPopup{nullptr};
    QMap<QString, QString> m_sessionGlobals;
};

} // namespace poppy::gui
