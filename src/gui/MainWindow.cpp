#include "MainWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QShortcut>
#include <QKeySequence>
#include <QClipboard>
#include <QGuiApplication>
#include <QApplication>
#include <Theme.h>
#include "dialogs/EnvironmentDialog.h"
#include "dialogs/CodeSnippetDialog.h"
#include "dialogs/ImportDialog.h"
#include "dialogs/CollectionRunnerDialog.h"
#include "dialogs/SettingsDialog.h"
#include "dialogs/QuickOpenDialog.h"
#include "dialogs/CookieManagerDialog.h"
#include "dialogs/DiffViewerDialog.h"
#include "dialogs/WebSocketDialog.h"
#include "dialogs/GrpcDialog.h"
#include "dialogs/SseDialog.h"
#include "dialogs/MockServerDialog.h"
#include "dialogs/GitSyncDialog.h"
#include "editors/AssertionsEditor.h"
#include <core/assertions/DeclarativeAssertion.h>
#include <core/exporters/OpenApiExporter.h>
#include <core/exporters/PostmanExporter.h>
#include <core/exporters/InsomniaExporter.h>
#include <core/exporters/HarExporter.h>
#include <core/CookieJar.h>
#include <QTabBar>
#include <QDir>
#include <QInputDialog>
#include <QTextBrowser>
#include <QDialog>
#include <QTimer>
#include <QMenu>
#include <QDesktopServices>
#include <QUrl>
#include <QCompleter>
#include <QStringListModel>
#include <QTableWidget>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <core/DocGenerator.h>
#include <core/VariableResolver.h>

namespace poppy::gui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Poppy - Native API Client");
    setWindowIcon(QIcon(":/icons/app_icon.png"));
    resize(1200, 750);
    setMinimumSize(800, 500);

    m_historyManager.loadFromFile(core::HistoryManager::defaultHistoryFilePath());

    setupUi();
    setupMenus();

    // Set default initial request tab
    m_currentRequest.name = "Quick Request";
    m_currentRequest.method = core::HttpMethod::GET;
    m_currentRequest.url = "https://httpbin.org/get";
    OpenTabInfo initTab{nullptr, m_currentRequest, false};
    m_openTabs.append(initTab);
    m_openRequestsTabBar->addTab("Quick Request");
    m_currentTabIndex = 0;
    loadRequestIntoUi(m_currentRequest);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi() {
    auto* centralWidget = new QWidget(this);
    auto* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 1. Central Horizontal Splitter: Sidebar | Main Content
    auto* mainSplitter = new QSplitter(Qt::Horizontal, this);

    // Sidebar
    m_sidebar = new CollectionSidebar(&m_collectionModel, &m_historyManager, this);
    connect(m_sidebar, &CollectionSidebar::requestSelected, this, &MainWindow::onRequestSelected);
    connect(m_sidebar, &CollectionSidebar::historyItemSelected, this, &MainWindow::onHistoryItemSelected);
    connect(m_sidebar, &CollectionSidebar::environmentChanged, this, &MainWindow::onEnvironmentChanged);
    connect(m_sidebar, &CollectionSidebar::manageEnvironmentsRequested, this, &MainWindow::onManageEnvironments);
    connect(m_sidebar, &CollectionSidebar::openCollectionRequested, this, &MainWindow::onOpenCollection);
    mainSplitter->addWidget(m_sidebar);

    // Right Content Area (Vertical Splitter: Request Editor | Response Inspector)
    auto* contentSplitter = new QSplitter(Qt::Vertical, this);

    // Request Editor Container
    auto* requestEditorWidget = new QWidget(this);
    auto* reqLayout = new QVBoxLayout(requestEditorWidget);
    reqLayout->setContentsMargins(12, 12, 12, 6);
    reqLayout->setSpacing(8);

    // 0. Top Open Requests TabBar
    m_openRequestsTabBar = new QTabBar(this);
    m_openRequestsTabBar->setTabsClosable(true);
    m_openRequestsTabBar->setMovable(true);
    m_openRequestsTabBar->setExpanding(false);
    m_openRequestsTabBar->setDrawBase(false);
    m_openRequestsTabBar->setStyleSheet(
        "QTabBar::tab { background: #18181b; color: #a1a1aa; padding: 5px 12px; margin-right: 4px; border-top-left-radius: 4px; border-top-right-radius: 4px; border: 1px solid #27272a; font-size: 12px; }"
        "QTabBar::tab:selected { background: #27272a; color: #f4f4f5; font-weight: bold; border-color: #3f3f46; }"
        "QTabBar::tab:hover { background: #222226; color: #f4f4f5; }"
    );
    m_openRequestsTabBar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_openRequestsTabBar, &QTabBar::currentChanged, this, &MainWindow::onTabChanged);
    connect(m_openRequestsTabBar, &QTabBar::tabCloseRequested, this, &MainWindow::onTabCloseRequested);
    connect(m_openRequestsTabBar, &QTabBar::tabBarDoubleClicked, this, &MainWindow::onRenameTab);
    connect(m_openRequestsTabBar, &QTabBar::customContextMenuRequested, this, &MainWindow::onTabContextMenu);
    reqLayout->addWidget(m_openRequestsTabBar);

    // Auto-save timer setup
    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setSingleShot(true);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &MainWindow::onAutoSaveTimerTimeout);

    // Top Request Info Bar
    auto* reqInfoBar = new QHBoxLayout();
    m_requestNameLabel = new QLabel("Quick Request", this);
    m_requestNameLabel->setStyleSheet("font-size: 15px; font-weight: bold; color: #f4f4f5;");
    reqInfoBar->addWidget(m_requestNameLabel);
    reqInfoBar->addStretch();

    m_curlBtn = new QPushButton("Copy as cURL", this);
    connect(m_curlBtn, &QPushButton::clicked, this, &MainWindow::onCopyAsCurl);
    reqInfoBar->addWidget(m_curlBtn);

    m_snippetBtn = new QPushButton("Generate Code", this);
    connect(m_snippetBtn, &QPushButton::clicked, this, &MainWindow::onShowCodeSnippets);
    reqInfoBar->addWidget(m_snippetBtn);

    m_saveBtn = new QPushButton("Save", this);
    connect(m_saveBtn, &QPushButton::clicked, this, &MainWindow::onSaveRequest);
    reqInfoBar->addWidget(m_saveBtn);

    m_proxyBtn = new QPushButton("Proxy", this);
    m_proxyBtn->setToolTip("Configure per-request proxy override");
    connect(m_proxyBtn, &QPushButton::clicked, this, &MainWindow::onConfigureRequestProxy);
    reqInfoBar->addWidget(m_proxyBtn);

    reqLayout->addLayout(reqInfoBar);

    // URL & Method & Send Bar
    auto* urlBarLayout = new QHBoxLayout();
    urlBarLayout->setSpacing(8);

    m_methodCombo = new QComboBox(this);
    m_methodCombo->addItem("GET", static_cast<int>(core::HttpMethod::GET));
    m_methodCombo->addItem("POST", static_cast<int>(core::HttpMethod::POST));
    m_methodCombo->addItem("PUT", static_cast<int>(core::HttpMethod::PUT));
    m_methodCombo->addItem("DELETE", static_cast<int>(core::HttpMethod::DELETE));
    m_methodCombo->addItem("PATCH", static_cast<int>(core::HttpMethod::PATCH));
    m_methodCombo->addItem("HEAD", static_cast<int>(core::HttpMethod::HEAD));
    m_methodCombo->addItem("OPTIONS", static_cast<int>(core::HttpMethod::OPTIONS));
    m_methodCombo->setFixedWidth(100);
    connect(m_methodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onMethodChanged);
    connect(m_methodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { markCurrentTabDirty(); });
    urlBarLayout->addWidget(m_methodCombo);

    m_urlEdit = new QLineEdit(this);
    m_urlEdit->setPlaceholderText("Enter request URL or {{baseUrl}}/path... (Press Enter to send)");
    connect(m_urlEdit, &QLineEdit::textChanged, this, &MainWindow::markCurrentTabDirty);
    connect(m_urlEdit, &QLineEdit::textChanged, this, &MainWindow::updateUrlVariableInspection);
    connect(m_urlEdit, &QLineEdit::returnPressed, this, &MainWindow::onSendClicked);

    m_urlCompleter = new QCompleter(this);
    m_urlCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    m_urlCompleter->setCompletionMode(QCompleter::PopupCompletion);
    m_urlEdit->setCompleter(m_urlCompleter);

    urlBarLayout->addWidget(m_urlEdit, 1);

    m_sendBtn = new QPushButton("Send", this);
    m_sendBtn->setObjectName("primaryBtn");
    m_sendBtn->setFixedWidth(90);
    connect(m_sendBtn, &QPushButton::clicked, this, &MainWindow::onSendClicked);
    urlBarLayout->addWidget(m_sendBtn);

    reqLayout->addLayout(urlBarLayout);

    // Request Tabs
    m_requestTabs = new QTabWidget(this);
    m_paramsEditor = new ParamsEditor(this);
    m_headersEditor = new HeadersEditor(this);
    m_bodyEditor = new BodyEditor(this);
    m_authEditor = new AuthEditor(this);
    m_authEditor->setNetworkEngine(&m_networkEngine);
    m_assertionsEditor = new AssertionsEditor(this);
    m_scriptEditor = new ScriptEditor(this);

    m_requestTabs->addTab(m_paramsEditor, "Params");
    m_requestTabs->addTab(m_headersEditor, "Headers");
    m_requestTabs->addTab(m_bodyEditor, "Body");
    m_requestTabs->addTab(m_authEditor, "Auth");
    m_requestTabs->addTab(m_assertionsEditor, "Assertions");
    m_requestTabs->addTab(m_scriptEditor, "Scripts & Tests");

    reqLayout->addWidget(m_requestTabs);
    contentSplitter->addWidget(requestEditorWidget);

    // Response Inspector
    m_responseInspector = new ResponseInspector(this);
    contentSplitter->addWidget(m_responseInspector);

    contentSplitter->setSizes({380, 370});
    mainSplitter->addWidget(contentSplitter);

    mainSplitter->setSizes({260, 940});
    mainLayout->addWidget(mainSplitter);

    setCentralWidget(centralWidget);

    // Global Shortcuts
    auto* newShortcut = new QShortcut(QKeySequence::New, this);
    connect(newShortcut, &QShortcut::activated, this, &MainWindow::onNewRequest);

    auto* sendShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), this);
    connect(sendShortcut, &QShortcut::activated, this, &MainWindow::onSendClicked);

    auto* saveShortcut = new QShortcut(QKeySequence::Save, this);
    connect(saveShortcut, &QShortcut::activated, this, &MainWindow::onSaveRequest);

    auto* openShortcut = new QShortcut(QKeySequence::Open, this);
    connect(openShortcut, &QShortcut::activated, this, &MainWindow::onOpenCollection);

    auto* themeShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_T), this);
    connect(themeShortcut, &QShortcut::activated, this, &MainWindow::onToggleTheme);

    // Status Bar & Variable Quick-Look Widget
    auto* sb = statusBar();
    m_varQuickBtn = new QPushButton("Active Variables: 0", this);
    m_varQuickBtn->setCursor(Qt::PointingHandCursor);
    m_varQuickBtn->setToolTip("View all active variables across Environment, Folder, and Collection scopes (Click to inspect)");
    m_varQuickBtn->setStyleSheet("QPushButton { border: 1px solid #3f3f46; border-radius: 3px; padding: 2px 8px; font-size: 11px; background: #27272a; color: #a1a1aa; } QPushButton:hover { background: #3f3f46; color: #ffffff; }");
    connect(m_varQuickBtn, &QPushButton::clicked, this, &MainWindow::onShowQuickVariables);
    sb->addPermanentWidget(m_varQuickBtn);

    onMethodChanged(0);
}

void MainWindow::setupMenus() {
    auto* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("&New Request", QKeySequence::New, this, &MainWindow::onNewRequest);
    fileMenu->addAction("&Quick Open...", QKeySequence(Qt::CTRL | Qt::Key_P), this, &MainWindow::onQuickOpen);
    fileMenu->addAction("&Open Collection...", QKeySequence::Open, this, &MainWindow::onOpenCollection);
    fileMenu->addAction("&Import...", this, &MainWindow::onImport);
    auto* exportMenu = fileMenu->addMenu("&Export Collection");
    exportMenu->addAction("as &OpenAPI 3.0...", this, &MainWindow::onExportOpenApi);
    exportMenu->addAction("as &Postman Collection (v2.1)...", this, &MainWindow::onExportPostman);
    exportMenu->addAction("as &Insomnia Collection (v4)...", this, &MainWindow::onExportInsomnia);
    exportMenu->addAction("as &HTTP Archive (.har)...", this, &MainWindow::onExportHar);
    fileMenu->addAction("&Run Collection...", this, &MainWindow::onRunCollection);
    fileMenu->addAction("&Save Request", QKeySequence::Save, this, &MainWindow::onSaveRequest);
    fileMenu->addAction("&Close Tab", QKeySequence::Close, this, &MainWindow::onCloseCurrentTab);
    fileMenu->addSeparator();
    fileMenu->addAction("&Settings...", QKeySequence::Preferences, this, &MainWindow::onOpenSettings);
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", this, &QWidget::close);

    auto* envMenu = menuBar()->addMenu("&Environments");
    envMenu->addAction("&Manage Environments...", QKeySequence(Qt::CTRL | Qt::Key_E), this, &MainWindow::onManageEnvironments);

    auto* toolsMenu = menuBar()->addMenu("&Tools");
    toolsMenu->addAction("&Cookie Manager...", QKeySequence(Qt::CTRL | Qt::Key_K), this, &MainWindow::onManageCookies);
    toolsMenu->addAction("Clear &Cookie Jar", this, &MainWindow::onClearCookieJar);
    toolsMenu->addSeparator();
    toolsMenu->addAction("Clear &History", this, [this]() {
        if (m_historyManager.count() > 0) {
            auto res = QMessageBox::question(this, "Clear History", "Clear all request execution history?", QMessageBox::Yes | QMessageBox::No);
            if (res == QMessageBox::Yes) {
                m_historyManager.clear();
            }
        }
    });
    toolsMenu->addSeparator();
    toolsMenu->addAction("Response &Diff Viewer...", this, &MainWindow::onOpenDiffViewer);
    toolsMenu->addAction("&WebSocket Client...", this, &MainWindow::onOpenWebSocket);
    toolsMenu->addAction("&gRPC Client...", this, &MainWindow::onOpenGrpc);
    toolsMenu->addAction("&Server-Sent Events (SSE)...", this, &MainWindow::onOpenSse);
    toolsMenu->addAction("&Mock Server...", this, &MainWindow::onOpenMockServer);
    toolsMenu->addAction("&Git Sync...", this, &MainWindow::onOpenGitSync);
    toolsMenu->addSeparator();
    toolsMenu->addAction("&Generate API Documentation...", this, &MainWindow::onGenerateDocumentation);

    auto* viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction("Toggle &Dark/Light Theme", QKeySequence(Qt::CTRL | Qt::Key_T), this, &MainWindow::onToggleTheme);

    auto* helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("&Keyboard Shortcuts...", QKeySequence(Qt::CTRL | Qt::Key_Slash), this, &MainWindow::onShowShortcuts);
    helpMenu->addSeparator();
    helpMenu->addAction("&About Poppy", this, [this]() {
        QMessageBox::about(this, "About Poppy",
            "<h3>Poppy API Client</h3>"
            "<p>A native, ultra-fast, local-first API client & test runner written in C++20 and Qt 6.</p>"
            "<p>Inspired by Bruno. Complete local ownership of your collections and environments.</p>");
    });
}

void MainWindow::onMethodChanged(int index) {
    auto method = static_cast<core::HttpMethod>(m_methodCombo->itemData(index).toInt());
    QColor c = Theme::methodColor(method);
    m_methodCombo->setStyleSheet(QString("QComboBox { color: %1; font-weight: bold; }").arg(c.name()));
}

void MainWindow::loadRequestIntoUi(const core::RequestModel& req) {
    m_currentRequest = req;
    m_requestNameLabel->setText(req.name.isEmpty() ? "Untitled Request" : req.name);

    int idx = m_methodCombo->findData(static_cast<int>(req.method));
    if (idx >= 0) m_methodCombo->setCurrentIndex(idx);

    m_urlEdit->setText(req.url);
    m_paramsEditor->loadFromRequest(req);
    m_headersEditor->loadFromRequest(req);
    m_bodyEditor->loadFromRequest(req);
    m_authEditor->loadFromRequest(req);
    m_assertionsEditor->loadFromRequest(req);
    m_scriptEditor->loadFromRequest(req);

    if (m_proxyBtn) {
        m_proxyBtn->setText(req.proxy.isEmpty() ? "Proxy" : ("Proxy: " + req.proxy));
        m_proxyBtn->setStyleSheet(req.proxy.isEmpty() ? "" : "background-color: #3b82f6; color: #ffffff; font-weight: bold;");
    }

    m_responseInspector->clear();
    updateUrlVariableInspection();
}

void MainWindow::saveUiIntoRequest(core::RequestModel& req) {
    req.name = m_requestNameLabel->text();
    req.method = static_cast<core::HttpMethod>(m_methodCombo->currentData().toInt());
    req.url = m_urlEdit->text().trimmed();
    req.proxy = m_currentRequest.proxy;

    m_paramsEditor->saveToRequest(req);
    m_headersEditor->saveToRequest(req);
    m_bodyEditor->saveToRequest(req);
    m_authEditor->saveToRequest(req);
    m_assertionsEditor->saveToRequest(req);
    m_scriptEditor->saveToRequest(req);
}

void MainWindow::updateTabTitle(int index) {
    if (index < 0 || index >= m_openTabs.size()) return;
    const auto& tab = m_openTabs[index];
    QString title = tab.request.name.isEmpty() ? "Untitled" : tab.request.name;
    QString prefix;
    if (tab.isPinned) prefix += "📌 ";
    if (tab.isDirty) prefix += "* ";
    m_openRequestsTabBar->setTabText(index, prefix + title);
}

void MainWindow::markCurrentTabDirty() {
    if (m_currentTabIndex >= 0 && m_currentTabIndex < m_openTabs.size()) {
        if (!m_openTabs[m_currentTabIndex].isDirty) {
            m_openTabs[m_currentTabIndex].isDirty = true;
            updateTabTitle(m_currentTabIndex);
        }
        if (m_autoSaveTimer) {
            m_autoSaveTimer->start(1500); // 1.5s debounced auto-save
        }
    }
}

void MainWindow::onRequestSelected(core::CollectionItem* item) {
    if (!item || !item->request()) return;

    // Check if already open
    for (int i = 0; i < m_openTabs.size(); ++i) {
        if (m_openTabs[i].item == item) {
            m_openRequestsTabBar->setCurrentIndex(i);
            return;
        }
    }

    // If only single tab open and it's the pristine Quick Request, reuse it
    if (m_openTabs.size() == 1 && m_openTabs[0].item == nullptr && !m_openTabs[0].isDirty && !m_openTabs[0].isPinned) {
        m_openTabs[0].item = item;
        m_openTabs[0].request = *item->request();
        m_openTabs[0].isDirty = false;
        m_openRequestsTabBar->setTabText(0, item->name());
        m_activeItem = item;
        loadRequestIntoUi(*item->request());
        return;
    }

    // Save current tab before creating new one
    if (m_currentTabIndex >= 0 && m_currentTabIndex < m_openTabs.size()) {
        saveUiIntoRequest(m_openTabs[m_currentTabIndex].request);
    }

    OpenTabInfo newTab;
    newTab.item = item;
    newTab.request = *item->request();
    newTab.isDirty = false;
    newTab.isPinned = false;
    m_openTabs.append(newTab);
    int newIdx = m_openRequestsTabBar->addTab(item->name());
    m_openRequestsTabBar->setCurrentIndex(newIdx);
}

void MainWindow::onTabChanged(int index) {
    if (index < 0 || index >= m_openTabs.size()) return;
    if (index == m_currentTabIndex) return;

    // Save active tab
    if (m_currentTabIndex >= 0 && m_currentTabIndex < m_openTabs.size()) {
        saveUiIntoRequest(m_openTabs[m_currentTabIndex].request);
    }

    m_currentTabIndex = index;
    m_activeItem = m_openTabs[index].item;
    loadRequestIntoUi(m_openTabs[index].request);
}

void MainWindow::onTabCloseRequested(int index) {
    closeTab(index);
}

void MainWindow::onCloseCurrentTab() {
    if (m_currentTabIndex >= 0) {
        closeTab(m_currentTabIndex);
    }
}

void MainWindow::closeTab(int index) {
    if (index < 0 || index >= m_openTabs.size()) return;

    if (m_openTabs[index].isPinned) {
        QMessageBox::information(this, "Tab Pinned", "This tab is pinned. Unpin it first before closing.");
        return;
    }

    if (m_openTabs[index].isDirty && m_openTabs[index].item) {
        auto res = QMessageBox::question(this, "Unsaved Changes",
            QString("Save changes to '%1' before closing?").arg(m_openTabs[index].request.name),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (res == QMessageBox::Cancel) return;
        if (res == QMessageBox::Save) {
            saveUiIntoRequest(m_openTabs[index].request);
            if (m_openTabs[index].item && m_openTabs[index].item->request()) {
                *m_openTabs[index].item->request() = m_openTabs[index].request;
                m_collectionModel.saveRequest(m_openTabs[index].item);
            }
        }
    }

    m_openRequestsTabBar->blockSignals(true);
    m_openRequestsTabBar->removeTab(index);
    m_openTabs.removeAt(index);
    m_openRequestsTabBar->blockSignals(false);

    if (m_openTabs.isEmpty()) {
        core::RequestModel quickReq;
        quickReq.name = "Quick Request";
        quickReq.method = core::HttpMethod::GET;
        quickReq.url = "https://httpbin.org/get";
        OpenTabInfo quickTab{nullptr, quickReq, false};
        m_openTabs.append(quickTab);
        m_openRequestsTabBar->addTab("Quick Request");
        m_currentTabIndex = 0;
        m_activeItem = nullptr;
        loadRequestIntoUi(quickReq);
    } else {
        int nextIdx = qBound(0, index == 0 ? 0 : index - 1, m_openTabs.size() - 1);
        m_currentTabIndex = -1;
        m_openRequestsTabBar->setCurrentIndex(nextIdx);
        onTabChanged(nextIdx);
    }
}

void MainWindow::onNewRequest() {
    if (m_currentTabIndex >= 0 && m_currentTabIndex < m_openTabs.size()) {
        saveUiIntoRequest(m_openTabs[m_currentTabIndex].request);
    }

    core::RequestModel newReq;
    newReq.name = "Untitled Request";
    newReq.method = core::HttpMethod::GET;
    newReq.url = "";

    OpenTabInfo tabInfo;
    tabInfo.item = nullptr;
    tabInfo.request = newReq;
    tabInfo.isDirty = false;
    m_openTabs.append(tabInfo);

    int newIdx = m_openRequestsTabBar->addTab("Untitled Request");
    m_openRequestsTabBar->setCurrentIndex(newIdx);
}

void MainWindow::onQuickOpen() {
    QuickOpenDialog dialog(&m_collectionModel, this);
    if (dialog.exec() == QDialog::Accepted && dialog.selectedItem()) {
        onRequestSelected(dialog.selectedItem());
    }
}

void MainWindow::onExportOpenApi() {
    auto requests = m_collectionModel.allRequests();
    if (requests.isEmpty()) {
        saveUiIntoRequest(m_currentRequest);
        requests.append(m_currentRequest);
    }

    QString defaultName = m_collectionModel.name().isEmpty() ? "openapi.json" : (m_collectionModel.name() + "_openapi.json");
    QString savePath = QFileDialog::getSaveFileName(this, "Export Collection as OpenAPI 3.0", defaultName, "OpenAPI JSON (*.json);;All Files (*.*)");
    if (savePath.isEmpty()) return;

    QString error;
    QString colName = m_collectionModel.name().isEmpty() ? "Poppy Collection" : m_collectionModel.name();
    if (core::OpenApiExporter::exportToFile(savePath, requests, colName, "1.0.0", &error)) {
        QMessageBox::information(this, "Export Succeeded", QString("Collection exported as OpenAPI 3.0 spec successfully to:\n%1").arg(savePath));
    } else {
        QMessageBox::warning(this, "Export Failed", QString("Could not export collection:\n%1").arg(error));
    }
}

void MainWindow::onExportPostman() {
    auto requests = m_collectionModel.allRequests();
    if (requests.isEmpty()) {
        saveUiIntoRequest(m_currentRequest);
        requests.append(m_currentRequest);
    }

    QString defaultName = m_collectionModel.name().isEmpty() ? "postman_collection.json" : (m_collectionModel.name() + "_postman.json");
    QString savePath = QFileDialog::getSaveFileName(this, "Export Collection as Postman v2.1", defaultName, "Postman Collection (*.json);;All Files (*.*)");
    if (savePath.isEmpty()) return;

    QString error;
    QString colName = m_collectionModel.name().isEmpty() ? "Poppy Collection" : m_collectionModel.name();
    if (core::PostmanExporter::exportToFile(savePath, requests, colName, &error)) {
        QMessageBox::information(this, "Export Succeeded", QString("Collection exported as Postman Collection v2.1 successfully to:\n%1").arg(savePath));
    } else {
        QMessageBox::warning(this, "Export Failed", QString("Could not export collection:\n%1").arg(error));
    }
}

void MainWindow::onExportInsomnia() {
    auto requests = m_collectionModel.allRequests();
    if (requests.isEmpty()) {
        saveUiIntoRequest(m_currentRequest);
        requests.append(m_currentRequest);
    }

    QString defaultName = m_collectionModel.name().isEmpty() ? "insomnia_collection.json" : (m_collectionModel.name() + "_insomnia.json");
    QString savePath = QFileDialog::getSaveFileName(this, "Export Collection as Insomnia v4", defaultName, "Insomnia Export (*.json);;All Files (*.*)");
    if (savePath.isEmpty()) return;

    QString error;
    QString colName = m_collectionModel.name().isEmpty() ? "Poppy Collection" : m_collectionModel.name();
    if (core::InsomniaExporter::exportToFile(savePath, requests, colName, &error)) {
        QMessageBox::information(this, "Export Succeeded", QString("Collection exported as Insomnia v4 collection successfully to:\n%1").arg(savePath));
    } else {
        QMessageBox::warning(this, "Export Failed", QString("Could not export collection:\n%1").arg(error));
    }
}

void MainWindow::onExportHar() {
    auto requests = m_collectionModel.allRequests();
    if (requests.isEmpty()) {
        saveUiIntoRequest(m_currentRequest);
        requests.append(m_currentRequest);
    }

    QString defaultName = m_collectionModel.name().isEmpty() ? "collection.har" : (m_collectionModel.name() + ".har");
    QString savePath = QFileDialog::getSaveFileName(this, "Export Collection as HTTP Archive (.har)", defaultName, "HAR Archive (*.har *.json);;All Files (*.*)");
    if (savePath.isEmpty()) return;

    QString error;
    if (core::HarExporter::exportToFile(savePath, requests, &error)) {
        QMessageBox::information(this, "Export Succeeded", QString("Collection exported as HTTP Archive (.har) successfully to:\n%1").arg(savePath));
    } else {
        QMessageBox::warning(this, "Export Failed", QString("Could not export collection:\n%1").arg(error));
    }
}

void MainWindow::onOpenCollection() {
    QString dir = QFileDialog::getExistingDirectory(this, "Open Collection Directory", QString());
    if (!dir.isEmpty()) {
        if (m_collectionModel.openDirectory(dir)) {
            m_sidebar->refreshTree();
            updateUrlVariableInspection();
        } else {
            QMessageBox::warning(this, "Error", "Failed to open collection directory.");
        }
    }
}

void MainWindow::onSaveRequest() {
    saveUiIntoRequest(m_currentRequest);
    if (m_currentTabIndex >= 0 && m_currentTabIndex < m_openTabs.size()) {
        m_openTabs[m_currentTabIndex].request = m_currentRequest;
        m_openTabs[m_currentTabIndex].isDirty = false;
        QString title = m_currentRequest.name.isEmpty() ? "Untitled" : m_currentRequest.name;
        m_openRequestsTabBar->setTabText(m_currentTabIndex, title);
    }
    if (m_activeItem) {
        if (m_activeItem->request()) {
            *m_activeItem->request() = m_currentRequest;
        }
        if (m_collectionModel.saveRequest(m_activeItem)) {
            statusBar()->showMessage("Request saved successfully.", 3000);
        } else {
            statusBar()->showMessage("Failed to save request to disk.", 3000);
        }
    } else {
        statusBar()->showMessage("Quick request updated.", 3000);
    }
}

void MainWindow::onCopyAsCurl() {
    saveUiIntoRequest(m_currentRequest);
    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setText(m_currentRequest.toCurlCommand());
    statusBar()->showMessage("cURL command copied to clipboard!", 3000);
}

void MainWindow::onEnvironmentChanged(const QString& envName) {
    m_activeEnvName = envName;
    updateUrlVariableInspection();
    statusBar()->showMessage(envName.isEmpty() ? "No environment active." : ("Active environment: " + envName), 3000);
}

void MainWindow::onShowCodeSnippets() {
    saveUiIntoRequest(m_currentRequest);
    core::VariableResolver resolver = currentVariableResolver();
    core::RequestModel resolvedReq = resolver.resolveRequest(m_currentRequest);

    CodeSnippetDialog dlg(resolvedReq, this);
    dlg.exec();
}

void MainWindow::onImport() {
    ImportDialog dlg(m_collectionModel.rootPath(), this);
    connect(&dlg, &ImportDialog::curlImported, this, [this](const core::RequestModel& req) {
        loadRequestIntoUi(req);
        statusBar()->showMessage("cURL command imported successfully!", 3000);
    });
    connect(&dlg, &ImportDialog::collectionImported, this, [this](const QString& dirPath) {
        m_collectionModel.openDirectory(dirPath);
        m_sidebar->refreshTree();
        statusBar()->showMessage("Collection imported successfully!", 3000);
    });
    dlg.exec();
}

void MainWindow::onRunCollection() {
    CollectionRunnerDialog dlg(&m_collectionModel, &m_networkEngine, &m_scriptRunner, m_activeEnvName, this);
    dlg.exec();
}

void MainWindow::onManageEnvironments() {
    EnvironmentDialog dlg(m_collectionModel.environments(), m_activeEnvName, m_collectionModel.rootPath(), this);
    if (dlg.exec() == QDialog::Accepted) {
        m_sidebar->updateEnvironmentsCombo();
    }
}

void MainWindow::onSendClicked() {
    saveUiIntoRequest(m_currentRequest);

    // 1. Get active environment model
    core::EnvironmentModel activeEnv(m_activeEnvName);
    for (const auto& env : m_collectionModel.environments()) {
        if (env.name() == m_activeEnvName) {
            activeEnv = env;
            break;
        }
    }

    // 2. Variable resolution
    core::VariableResolver resolver = currentVariableResolver();
    core::RequestModel resolvedReq = resolver.resolveRequest(m_currentRequest);

    // 3. Pre-request script
    QString scriptErr;
    if (!m_scriptRunner.runPreRequestScript(resolvedReq.scripts.preRequestScript, resolvedReq, activeEnv, &scriptErr)) {
        QMessageBox::warning(this, "Pre-request Script Error", scriptErr);
        return;
    }

    // 4. Update UI to Sending state
    m_sendBtn->setEnabled(false);
    m_sendBtn->setText("Sending...");

    // 5. Send asynchronously via libcurl
    m_networkEngine.sendRequestAsync(resolvedReq, [this, resolvedReq, activeEnv](const core::ResponseModel& res) mutable {
        m_sendBtn->setEnabled(true);
        m_sendBtn->setText("Send");

        // 6. Post-response script
        QString postErr;
        m_scriptRunner.runPostResponseScript(resolvedReq.scripts.postResponseScript, resolvedReq, res, activeEnv, &postErr);

        // 7. Tests & Declarative Assertions
        core::TestReport report = m_scriptRunner.runTests(resolvedReq.scripts.tests, resolvedReq, res, activeEnv);
        auto declResults = core::DeclarativeAssertionEvaluator::evaluateAll(resolvedReq.assertions, res);
        for (const auto& dr : declResults) {
            report.results.append(dr);
        }

        // 8. Update Response Inspector
        m_responseInspector->setResponse(res, &report);

        // 9. Add to History
        m_historyManager.addEntry(resolvedReq, res);
    });
}

void MainWindow::onHistoryItemSelected(const core::HistoryItem& item) {
    loadRequestIntoUi(item.request);
    core::ResponseModel res = item.toResponseModel();
    m_responseInspector->setResponse(res, nullptr);
    statusBar()->showMessage(QString("Loaded historical request: %1 (%2)").arg(item.request.url).arg(item.statusCode), 3000);
}

void MainWindow::onManageCookies() {
    QString cpath = m_networkEngine.cookieJarPath();
    if (cpath.isEmpty()) {
        cpath = QDir::tempPath() + "/poppy_cookies.txt";
    }
    CookieManagerDialog dlg(cpath, this);
    dlg.exec();
}

void MainWindow::onClearCookieJar() {
    QString cpath = m_networkEngine.cookieJarPath();
    if (cpath.isEmpty()) {
        cpath = QDir::tempPath() + "/poppy_cookies.txt";
    }
    core::CookieJar jar;
    jar.clear();
    jar.saveToFile(cpath);
    statusBar()->showMessage("Cookie jar cleared.", 3000);
}

void MainWindow::onOpenSettings() {
    SettingsDialog dlg(&m_networkEngine, this);
    dlg.exec();
}

void MainWindow::onRenameTab(int index) {
    if (index < 0 || index >= m_openTabs.size()) return;
    QString current = m_openTabs[index].request.name;
    bool ok;
    QString newName = QInputDialog::getText(this, "Rename Tab", "Request name:", QLineEdit::Normal, current, &ok);
    if (ok && !newName.trimmed().isEmpty()) {
        m_openTabs[index].request.name = newName.trimmed();
        m_openTabs[index].isDirty = true;
        m_openRequestsTabBar->setTabText(index, "* " + newName.trimmed());
        if (m_currentTabIndex == index) {
            m_requestNameLabel->setText(newName.trimmed());
        }
    }
}

void MainWindow::onShowShortcuts() {
    QDialog dlg(this);
    dlg.setWindowTitle("Keyboard Shortcuts");
    dlg.resize(460, 520);
    auto* layout = new QVBoxLayout(&dlg);

    auto* text = new QTextBrowser(&dlg);
    text->setOpenExternalLinks(false);
    text->setHtml(R"(
<style>
  body { font-family: sans-serif; font-size: 13px; color: #f4f4f5; background: #18181b; margin: 8px; }
  h3   { color: #a78bfa; margin-top: 14px; margin-bottom: 4px; }
  table { width: 100%; border-collapse: collapse; }
  td   { padding: 4px 8px; }
  td:first-child { font-weight: bold; color: #fbbf24; white-space: nowrap; }
  tr:hover td { background: #27272a; }
</style>
<body>
<h3>Request</h3>
<table>
  <tr><td>Ctrl+Return</td><td>Send request</td></tr>
  <tr><td>Ctrl+S</td><td>Save request</td></tr>
  <tr><td>Ctrl+N</td><td>New request tab</td></tr>
  <tr><td>Ctrl+W</td><td>Close current tab</td></tr>
  <tr><td>Double-click tab</td><td>Rename tab</td></tr>
</table>
<h3>Navigation</h3>
<table>
  <tr><td>Ctrl+P</td><td>Quick Open (fuzzy search)</td></tr>
  <tr><td>Ctrl+O</td><td>Open collection</td></tr>
  <tr><td>Ctrl+F</td><td>Find in response body</td></tr>
</table>
<h3>Tools</h3>
<table>
  <tr><td>Ctrl+K</td><td>Cookie Manager</td></tr>
  <tr><td>Ctrl+E</td><td>Manage Environments</td></tr>
  <tr><td>Ctrl+/</td><td>Keyboard Shortcuts</td></tr>
</table>
<h3>General</h3>
<table>
  <tr><td>Ctrl+,</td><td>Settings</td></tr>
</table>
</body>)");

    layout->addWidget(text);
    auto* closeBtn = new QPushButton("Close", &dlg);
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    layout->addWidget(closeBtn);
    dlg.exec();
}

void MainWindow::onTabContextMenu(const QPoint& pos) {
    int idx = m_openRequestsTabBar->tabAt(pos);
    if (idx < 0 || idx >= m_openTabs.size()) return;

    QMenu menu(this);
    bool isPinned = m_openTabs[idx].isPinned;
    menu.addAction(isPinned ? "Unpin Tab" : "📌 Pin Tab", [this, idx]() {
        m_openTabs[idx].isPinned = !m_openTabs[idx].isPinned;
        updateTabTitle(idx);
    });
    menu.addAction("Rename Tab...", [this, idx]() {
        onRenameTab(idx);
    });
    menu.addSeparator();
    if (!isPinned) {
        menu.addAction("Close Tab", [this, idx]() {
            closeTab(idx);
        });
    }
    menu.addAction("Close Other Tabs", [this, idx]() {
        for (int i = m_openTabs.size() - 1; i >= 0; --i) {
            if (i != idx && !m_openTabs[i].isPinned) {
                closeTab(i);
            }
        }
    });

    menu.exec(m_openRequestsTabBar->mapToGlobal(pos));
}

void MainWindow::onTogglePinCurrentTab() {
    if (m_currentTabIndex >= 0 && m_currentTabIndex < m_openTabs.size()) {
        m_openTabs[m_currentTabIndex].isPinned = !m_openTabs[m_currentTabIndex].isPinned;
        updateTabTitle(m_currentTabIndex);
    }
}

void MainWindow::onAutoSaveTimerTimeout() {
    if (m_currentTabIndex >= 0 && m_currentTabIndex < m_openTabs.size()) {
        if (m_openTabs[m_currentTabIndex].isDirty && m_openTabs[m_currentTabIndex].item) {
            saveUiIntoRequest(m_openTabs[m_currentTabIndex].request);
            if (m_openTabs[m_currentTabIndex].item->request()) {
                *m_openTabs[m_currentTabIndex].item->request() = m_openTabs[m_currentTabIndex].request;
            }
            m_collectionModel.saveRequest(m_openTabs[m_currentTabIndex].item);
            m_openTabs[m_currentTabIndex].isDirty = false;
            updateTabTitle(m_currentTabIndex);
            statusBar()->showMessage("Auto-saved changes.", 2000);
        }
    }
}

void MainWindow::onConfigureRequestProxy() {
    bool ok;
    QString current = m_currentRequest.proxy;
    QString newProxy = QInputDialog::getText(this, "Request Proxy",
        "Set custom proxy for this request (e.g. http://127.0.0.1:8080 or socks5://127.0.0.1:1080):\nLeave empty to use global setting:",
        QLineEdit::Normal, current, &ok);
    if (ok) {
        m_currentRequest.proxy = newProxy.trimmed();
        if (m_proxyBtn) {
            m_proxyBtn->setText(m_currentRequest.proxy.isEmpty() ? "Proxy" : ("Proxy: " + m_currentRequest.proxy));
            m_proxyBtn->setStyleSheet(m_currentRequest.proxy.isEmpty() ? "" : "background-color: #3b82f6; color: #ffffff; font-weight: bold;");
        }
        markCurrentTabDirty();
    }
}

void MainWindow::onOpenDiffViewer() {
    QString currentBody = m_responseInspector ? m_responseInspector->currentBody() : QString();
    auto dlg = new DiffViewerDialog(currentBody, QString(), &m_historyManager, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

void MainWindow::onOpenWebSocket() {
    auto dlg = new WebSocketDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

void MainWindow::onOpenGrpc() {
    auto dlg = new GrpcDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

void MainWindow::onOpenMockServer() {
    auto dlg = new MockServerDialog(&m_collectionModel, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

void MainWindow::onOpenSse() {
    QString currentUrl = m_urlEdit ? m_urlEdit->text().trimmed() : QString();
    auto dlg = new SseDialog(currentUrl, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

void MainWindow::onOpenGitSync() {
    QString repoPath = m_collectionModel.rootPath();
    if (repoPath.isEmpty()) repoPath = QDir::currentPath();
    auto dlg = new GitSyncDialog(repoPath, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

core::VariableResolver MainWindow::currentVariableResolver() const {
    core::VariableResolver resolver;
    core::EnvironmentModel activeEnv(m_activeEnvName);
    for (const auto& env : m_collectionModel.environments()) {
        if (env.name() == m_activeEnvName) {
            activeEnv = env;
            break;
        }
    }
    resolver.setEnvironment(activeEnv);
    if (m_activeItem) {
        resolver.setFolderVariables(m_activeItem->effectiveVariables());
    }
    return resolver;
}

void MainWindow::onGenerateDocumentation() {
    auto requests = m_collectionModel.allRequests();
    if (requests.isEmpty()) {
        saveUiIntoRequest(m_currentRequest);
        requests.append(m_currentRequest);
    }

    QString colName = m_collectionModel.name().isEmpty() ? "Poppy Collection" : m_collectionModel.name();
    QString defaultPath = QDir::current().filePath(colName.toLower().replace(' ', '_') + "_docs.html");
    QString savePath = QFileDialog::getSaveFileName(this, "Generate API Documentation", defaultPath, "HTML Document (*.html);;All Files (*.*)");
    if (savePath.isEmpty()) return;

    QString error;
    core::VariableResolver resolver = currentVariableResolver();
    if (core::DocGenerator::generateHtmlFile(savePath, requests, colName, "Generated with Poppy API Client", resolver, &error)) {
        auto res = QMessageBox::information(this, "Documentation Generated",
            QString("Interactive API Documentation generated successfully:\n%1\n\nOpen in default web browser?").arg(savePath),
            QMessageBox::Yes | QMessageBox::No);
        if (res == QMessageBox::Yes) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(savePath));
        }
    } else {
        QMessageBox::warning(this, "Documentation Generation Failed", QString("Could not generate documentation:\n%1").arg(error));
    }
}

void MainWindow::onToggleTheme() {
    bool dark = Theme::toggleTheme();
    if (auto* app = qobject_cast<QApplication*>(QApplication::instance())) {
        app->setStyleSheet(dark ? Theme::darkStyleSheet() : Theme::lightStyleSheet());
    }
    m_responseInspector->setTheme(dark);
    statusBar()->showMessage(QString("Switched to %1 theme (Ctrl+T)").arg(dark ? "Dark" : "Light"), 3000);
}

void MainWindow::onShowQuickVariables() {
    core::VariableResolver resolver = currentVariableResolver();
    auto allVars = resolver.allAvailableVariables();

    QDialog dlg(this);
    dlg.setWindowTitle("Active Variables Quick-Look");
    dlg.resize(550, 380);
    auto* layout = new QVBoxLayout(&dlg);

    auto* headerLabel = new QLabel(QString("Active Environment: <b>%1</b> &bull; Total Available Variables: <b>%2</b>")
        .arg(m_activeEnvName.isEmpty() ? "None" : m_activeEnvName)
        .arg(allVars.size()), &dlg);
    headerLabel->setStyleSheet("padding: 4px; font-size: 12px;");
    layout->addWidget(headerLabel);

    auto* filterEdit = new QLineEdit(&dlg);
    filterEdit->setPlaceholderText("Filter active variables...");
    layout->addWidget(filterEdit);

    auto* table = new QTableWidget(&dlg);
    table->setColumnCount(3);
    table->setHorizontalHeaderLabels({"Variable Name", "Scope", "Resolved Value"});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table->setColumnWidth(0, 160);
    table->setColumnWidth(1, 130);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->verticalHeader()->setVisible(false);

    int row = 0;
    for (auto it = allVars.begin(); it != allVars.end(); ++it) {
        QString scope;
        QString val = resolver.lookupVariableWithScope(it.key(), &scope);
        table->insertRow(row);
        table->setItem(row, 0, new QTableWidgetItem(it.key()));
        table->setItem(row, 1, new QTableWidgetItem(scope));
        table->setItem(row, 2, new QTableWidgetItem(val));
        row++;
    }
    layout->addWidget(table);

    connect(filterEdit, &QLineEdit::textChanged, [&dlg, table](const QString& q) {
        for (int r = 0; r < table->rowCount(); ++r) {
            bool matches = q.isEmpty();
            for (int c = 0; !matches && c < table->columnCount(); ++c) {
                auto* item = table->item(r, c);
                if (item && item->text().contains(q, Qt::CaseInsensitive)) {
                    matches = true;
                }
            }
            table->setRowHidden(r, !matches);
        }
    });

    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::accept);
    layout->addWidget(btnBox);

    dlg.exec();
}

void MainWindow::updateUrlVariableInspection() {
    core::VariableResolver resolver = currentVariableResolver();
    auto allVars = resolver.allAvailableVariables();

    // Update autocompleter list
    if (m_urlCompleter) {
        QStringList varTokens;
        for (auto it = allVars.begin(); it != allVars.end(); ++it) {
            varTokens.append(QString("{{%1}}").arg(it.key()));
        }
        auto* model = new QStringListModel(varTokens, m_urlCompleter);
        m_urlCompleter->setModel(model);
    }

    // Update status bar variable count button
    if (m_varQuickBtn) {
        m_varQuickBtn->setText(QString("Active Variables: %1").arg(allVars.size()));
    }

    // Inspect variables in current URL text
    if (!m_urlEdit) return;
    QString text = m_urlEdit->text();
    static const QRegularExpression varRegex(R"(\{\{([^}]+)\}\})");
    auto matchIter = varRegex.globalMatch(text);
    if (!matchIter.hasNext()) {
        m_urlEdit->setToolTip("Enter request URL or use {{variable_name}} for dynamic resolution");
        return;
    }

    QString tooltip = "<div style='font-family: Consolas, monospace; font-size: 11px;'><b>Variable Hover Inspection:</b><br/>";
    while (matchIter.hasNext()) {
        auto match = matchIter.next();
        QString varName = match.captured(1).trimmed();
        QString scope;
        QString val = resolver.lookupVariableWithScope(varName, &scope);
        if (!scope.isEmpty()) {
            tooltip += QString("&bull; <code style='color:#60a5fa;'>{{%1}}</code> &rarr; <b>%2</b> <span style='color:#a1a1aa;'>(Scope: %3)</span><br/>")
                .arg(varName.toHtmlEscaped(), val.toHtmlEscaped(), scope.toHtmlEscaped());
        } else {
            tooltip += QString("&bull; <code style='color:#ef4444;'>{{%1}}</code> &rarr; <span style='color:#ef4444;'><i>[Unresolved Variable]</i></span><br/>")
                .arg(varName.toHtmlEscaped());
        }
    }
    tooltip += "</div>";
    m_urlEdit->setToolTip(tooltip);
}

} // namespace poppy::gui
