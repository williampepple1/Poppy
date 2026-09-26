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
#include <QSettings>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QMimeData>
#include <QStandardPaths>
#include <Theme.h>
#include "components/VariableHoverPopup.h"
#include "dialogs/EnvironmentDialog.h"
#include "dialogs/CodeSnippetDialog.h"
#include "dialogs/ImportDialog.h"
#include "dialogs/CollectionRunnerDialog.h"
#include "dialogs/SettingsDialog.h"
#include "dialogs/QuickOpenDialog.h"
#include "dialogs/CommandPaletteDialog.h"
#include "dialogs/CookieManagerDialog.h"
#include "dialogs/DiffViewerDialog.h"
#include "dialogs/WebSocketDialog.h"
#include "dialogs/GrpcDialog.h"
#include "dialogs/SseDialog.h"
#include "dialogs/MockServerDialog.h"
#include "dialogs/GitSyncDialog.h"
#include "dialogs/FindReplaceDialog.h"
#include "editors/AssertionsEditor.h"
#include <core/assertions/DeclarativeAssertion.h>
#include <core/exporters/OpenApiExporter.h>
#include <core/exporters/PostmanExporter.h>
#include <core/exporters/InsomniaExporter.h>
#include <core/exporters/HarExporter.h>
#include <core/exporters/MarkdownExporter.h>
#include <core/CookieJar.h>
#include <core/importers/CurlImporter.h>
#include <QTabBar>
#include <QDir>
#include <QUuid>
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
#include <QCloseEvent>
#include <QPointer>
#include <QStyledItemDelegate>
#include <QPainter>
#include <core/DocGenerator.h>
#include <core/VariableResolver.h>

namespace poppy::gui {

namespace {

class MethodItemDelegate : public QStyledItemDelegate {
public:
    explicit MethodItemDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::TextAntialiasing, true);

        const bool isDark = Theme::isDarkMode();
        const bool isSelected = option.state & QStyle::State_Selected;

        if (isSelected) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(isDark ? "#252834" : "#e2e8f0"));
            painter->drawRoundedRect(option.rect.adjusted(2, 1, -2, -1), 4, 4);
        }

        auto method = static_cast<core::HttpMethod>(index.data(Qt::UserRole).toInt());
        QColor color = Theme::methodColor(method);
        QString text = core::methodToString(method);

        // Pill badge
        QRect badgeRect(option.rect.left() + 8, option.rect.top() + (option.rect.height() - 18) / 2, 60, 18);
        QColor bg = color;
        bg.setAlpha(isDark ? 40 : 30);
        QColor border = color;
        border.setAlpha(isDark ? 110 : 80);

        painter->setPen(QPen(border, 1));
        painter->setBrush(bg);
        painter->drawRoundedRect(badgeRect, 3, 3);

        QFont f = painter->font();
        f.setPixelSize(10);
        f.setBold(true);
        painter->setFont(f);
        painter->setPen(color);
        painter->drawText(badgeRect, Qt::AlignCenter, text);

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& /*index*/) const override {
        return QSize(option.rect.width(), 28);
    }
};

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Poppy - Native API Client");
    setWindowIcon(QIcon(":/icons/app_icon.png"));
    resize(1200, 750);
    setMinimumSize(800, 500);
    setAcceptDrops(true);

    m_historyManager.loadFromFile(core::HistoryManager::defaultHistoryFilePath());

    setupUi();
    setupMenus();

    connect(&m_collectionModel, &core::CollectionModel::itemAboutToBeDeleted,
            this, &MainWindow::onItemAboutToBeDeleted);
    connect(&m_collectionModel, &core::CollectionModel::collectionAboutToReload,
            this, &MainWindow::onCollectionAboutToReload);
    connect(&m_collectionModel, &core::CollectionModel::collectionLoaded,
            this, &MainWindow::onCollectionLoaded);

    // Set default initial request tab
    m_currentRequest.name = "Quick Request";
    m_currentRequest.method = core::HttpMethod::GET;
    m_currentRequest.url = "https://httpbin.org/get";
    OpenTabInfo initTab;
    initTab.tabId = ++m_nextTabId;
    initTab.request = m_currentRequest;
    m_openTabs.append(initTab);
    m_openRequestsTabBar->addTab("Quick Request");
    m_currentTabIndex = 0;
    loadRequestIntoUi(m_currentRequest);

    restoreAppState();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi() {
    auto* centralWidget = new QWidget(this);
    auto* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 1. Central Horizontal Splitter: Sidebar | Main Content
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    auto* mainSplitter = m_mainSplitter;

    // Sidebar
    m_sidebar = new CollectionSidebar(&m_collectionModel, &m_historyManager, this);
    connect(m_sidebar, &CollectionSidebar::requestSelected, this, &MainWindow::onRequestSelected);
    connect(m_sidebar, &CollectionSidebar::historyItemSelected, this, &MainWindow::onHistoryItemSelected);
    connect(m_sidebar, &CollectionSidebar::environmentChanged, this, &MainWindow::onEnvironmentChanged);
    connect(m_sidebar, &CollectionSidebar::manageEnvironmentsRequested, this, &MainWindow::onManageEnvironments);
    connect(m_sidebar, &CollectionSidebar::openCollectionRequested, this, &MainWindow::onOpenCollection);
    connect(m_sidebar, &CollectionSidebar::gitSyncRequested, this, &MainWindow::onOpenGitSync);
    mainSplitter->addWidget(m_sidebar);

    // Right Content Area (Vertical Splitter: Request Editor | Response Inspector)
    m_contentSplitter = new QSplitter(Qt::Vertical, this);
    auto* contentSplitter = m_contentSplitter;

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
    m_openRequestsTabBar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_openRequestsTabBar, &QTabBar::currentChanged, this, &MainWindow::onTabChanged);
    connect(m_openRequestsTabBar, &QTabBar::tabMoved, this, &MainWindow::onTabMoved);
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
    applyRequestChrome();
    reqInfoBar->addWidget(m_requestNameLabel);
    reqInfoBar->addStretch();

    m_curlBtn = new QPushButton("📋 cURL", this);
    m_curlBtn->setToolTip("Copy resolved request as cURL command");
    connect(m_curlBtn, &QPushButton::clicked, this, &MainWindow::onCopyAsCurl);
    reqInfoBar->addWidget(m_curlBtn);

    m_snippetBtn = new QPushButton("⚡ Code", this);
    m_snippetBtn->setToolTip("Generate code snippets in Python, JS, Go, etc.");
    connect(m_snippetBtn, &QPushButton::clicked, this, &MainWindow::onShowCodeSnippets);
    reqInfoBar->addWidget(m_snippetBtn);

    m_saveBtn = new QPushButton("💾 Save", this);
    m_saveBtn->setToolTip("Save request changes (Ctrl+S)");
    connect(m_saveBtn, &QPushButton::clicked, this, &MainWindow::onSaveRequest);
    reqInfoBar->addWidget(m_saveBtn);

    m_proxyBtn = new QPushButton("🛡️ Proxy", this);
    m_proxyBtn->setToolTip("Configure per-request proxy override");
    connect(m_proxyBtn, &QPushButton::clicked, this, &MainWindow::onConfigureRequestProxy);
    reqInfoBar->addWidget(m_proxyBtn);

    m_topEnvCombo = new QComboBox(this);
    m_topEnvCombo->setFixedWidth(140);
    m_topEnvCombo->setToolTip("Active Environment");
    connect(m_topEnvCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx < 0 || !m_topEnvCombo) return;
        QString env = m_topEnvCombo->itemData(idx).toString();
        if (env != m_activeEnvName) {
            onEnvironmentChanged(env);
        }
    });
    reqInfoBar->addWidget(m_topEnvCombo);

    reqLayout->addLayout(reqInfoBar);

    // Integrated Bruno Method & URL Bar Container
    auto* urlBarFrame = new QFrame(this);
    urlBarFrame->setObjectName("urlBarContainer");
    auto* urlBarLayout = new QHBoxLayout(urlBarFrame);
    urlBarLayout->setContentsMargins(4, 3, 4, 3);
    urlBarLayout->setSpacing(6);

    m_methodCombo = new QComboBox(urlBarFrame);
    m_methodCombo->setObjectName("methodCombo");
    m_methodCombo->addItem("GET", static_cast<int>(core::HttpMethod::GET));
    m_methodCombo->addItem("POST", static_cast<int>(core::HttpMethod::POST));
    m_methodCombo->addItem("PUT", static_cast<int>(core::HttpMethod::PUT));
    m_methodCombo->addItem("DELETE", static_cast<int>(core::HttpMethod::DELETE));
    m_methodCombo->addItem("PATCH", static_cast<int>(core::HttpMethod::PATCH));
    m_methodCombo->addItem("HEAD", static_cast<int>(core::HttpMethod::HEAD));
    m_methodCombo->addItem("OPTIONS", static_cast<int>(core::HttpMethod::OPTIONS));
    m_methodCombo->setItemDelegate(new MethodItemDelegate(m_methodCombo));
    m_methodCombo->setFixedWidth(102);
    connect(m_methodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onMethodChanged);
    connect(m_methodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { markCurrentTabDirty(); });
    urlBarLayout->addWidget(m_methodCombo);

    auto* urlDivider = new QFrame(urlBarFrame);
    urlDivider->setObjectName("urlBarDivider");
    urlDivider->setFrameShape(QFrame::VLine);
    urlBarLayout->addWidget(urlDivider);

    m_urlEdit = new QLineEdit(urlBarFrame);
    m_urlEdit->setObjectName("urlEdit");
    m_urlEdit->setPlaceholderText("Enter request URL or {{baseUrl}}/path... (Ctrl+Enter to send)");
    connect(m_urlEdit, &QLineEdit::textChanged, this, &MainWindow::markCurrentTabDirty);
    connect(m_urlEdit, &QLineEdit::textChanged, this, &MainWindow::updateUrlVariableInspection);
    connect(m_urlEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        QString trimmed = text.trimmed();
        if (trimmed.startsWith("curl ", Qt::CaseInsensitive) || trimmed.startsWith("curl.exe ", Qt::CaseInsensitive)) {
            auto imported = core::CurlImporter::importCurl(trimmed);
            if (!imported.url.isEmpty()) {
                loadRequestIntoUi(imported);
                if (m_currentTabIndex >= 0 && m_currentTabIndex < m_openTabs.size()) {
                    m_openTabs[m_currentTabIndex].request = m_currentRequest;
                    m_openTabs[m_currentTabIndex].hasResponse = false;
                }
                markCurrentTabDirty();
                statusBar()->showMessage("Detected and imported cURL command into request editor!", 3500);
            }
        }
    });
    connect(m_urlEdit, &QLineEdit::returnPressed, this, &MainWindow::onSendClicked);

    m_urlCompleter = new QCompleter(this);
    m_urlCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    m_urlCompleter->setCompletionMode(QCompleter::PopupCompletion);
    m_urlEdit->setCompleter(m_urlCompleter);

    m_variableHoverPopup = new VariableHoverPopup(this);
    connect(m_variableHoverPopup, &VariableHoverPopup::variableSaved, this, &MainWindow::onVariableSaved);
    connect(m_variableHoverPopup, &VariableHoverPopup::manageEnvironmentsRequested, this, &MainWindow::onManageEnvironments);
    m_urlEdit->installEventFilter(this);

    urlBarLayout->addWidget(m_urlEdit, 1);

    m_sendBtn = new QPushButton("Send  ↵", urlBarFrame);
    m_sendBtn->setObjectName("primaryBtn");
    m_sendBtn->setFixedWidth(100);
    m_sendBtn->setToolTip("Send HTTP Request (Ctrl+Enter)");
    connect(m_sendBtn, &QPushButton::clicked, this, &MainWindow::onSendClicked);
    urlBarLayout->addWidget(m_sendBtn);

    reqLayout->addWidget(urlBarFrame);

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

    auto onEditorModified = [this]() {
        if (!m_loadingUi) {
            saveUiIntoRequest(m_currentRequest);
            updateRequestTabBadges();
            markCurrentTabDirty();
        }
    };
    connect(m_paramsEditor, &ParamsEditor::paramsChanged, this, onEditorModified);
    connect(m_headersEditor, &HeadersEditor::headersChanged, this, onEditorModified);
    connect(m_bodyEditor, &BodyEditor::bodyChanged, this, onEditorModified);
    connect(m_authEditor, &AuthEditor::authChanged, this, onEditorModified);
    connect(m_assertionsEditor, &AssertionsEditor::assertionsChanged, this, onEditorModified);
    connect(m_scriptEditor, &ScriptEditor::scriptChanged, this, onEditorModified);

    reqLayout->addWidget(m_requestTabs);
    contentSplitter->addWidget(requestEditorWidget);

    // Response Inspector
    m_responseInspector = new ResponseInspector(this);
    connect(m_responseInspector, &ResponseInspector::storeVariableRequested, this, [this](const QString& val) {
        if (m_activeEnvName.isEmpty()) {
            QMessageBox::information(this, "No Active Environment", "Please select or activate an environment first before storing variables.");
            return;
        }
        bool ok = false;
        QString varName = QInputDialog::getText(this, "Store Environment Variable",
            QString("Store selection into active environment '%1':").arg(m_activeEnvName),
            QLineEdit::Normal, QString(), &ok);
        if (ok && !varName.trimmed().isEmpty()) {
            for (auto& env : m_collectionModel.environments()) {
                if (env.name() == m_activeEnvName) {
                    env.addOrUpdateVariable(varName.trimmed(), val, true, true);
                    if (!m_collectionModel.rootPath().isEmpty()) {
                        QDir envDir(m_collectionModel.rootPath());
                        envDir.mkpath(QStringLiteral("environments"));
                        env.saveToEnvFile(envDir.filePath("environments/" + env.name() + ".env"));
                        env.saveSecretsToEnvFile(envDir.filePath("environments/" + env.name() + ".secret.env"));
                    }
                    updateUrlVariableInspection();
                    statusBar()->showMessage(QString("Stored secret {{%1}} in environment '%2'.").arg(varName.trimmed(), m_activeEnvName), 3500);
                    break;
                }
            }
        }
    });
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

    // Layout Split Toggle: Ctrl+Shift+L
    auto* layoutToggleShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L), this);
    connect(layoutToggleShortcut, &QShortcut::activated, this, [this]() {
        if (!m_contentSplitter) return;
        bool isVertical = (m_contentSplitter->orientation() == Qt::Vertical);
        m_contentSplitter->setOrientation(isVertical ? Qt::Horizontal : Qt::Vertical);
        QList<int> equal = {500, 500};
        m_contentSplitter->setSizes(equal);
        statusBar()->showMessage(isVertical ? "Layout: Side-by-side (Ctrl+Shift+L to toggle)" : "Layout: Stacked (Ctrl+Shift+L to toggle)", 2500);
    });

    // Status Bar & Variable Quick-Look / Telemetry Widgets
    auto* sb = statusBar();

    m_sessionTelemetryBtn = new QPushButton("⚡ 0 reqs · 📦 0 B · ⏱ 0 ms", this);
    m_sessionTelemetryBtn->setObjectName("sessionTelemetryBtn");
    m_sessionTelemetryBtn->setCursor(Qt::PointingHandCursor);
    m_sessionTelemetryBtn->setToolTip("Session Network Telemetry & Bandwidth (Click to inspect breakdown or reset)");
    connect(m_sessionTelemetryBtn, &QPushButton::clicked, this, &MainWindow::onShowSessionTelemetry);
    sb->addPermanentWidget(m_sessionTelemetryBtn);

    m_varQuickBtn = new QPushButton("Active Variables: 0", this);
    m_varQuickBtn->setObjectName("sessionTelemetryBtn");
    m_varQuickBtn->setCursor(Qt::PointingHandCursor);
    m_varQuickBtn->setToolTip("View all active variables across Environment, Folder, and Collection scopes (Click to inspect)");
    connect(m_varQuickBtn, &QPushButton::clicked, this, &MainWindow::onShowQuickVariables);
    sb->addPermanentWidget(m_varQuickBtn);

    updateTopEnvCombo();
    onMethodChanged(0);
}

void MainWindow::setupMenus() {
    auto* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("&New Request", QKeySequence::New, this, &MainWindow::onNewRequest);
    fileMenu->addAction("&Quick Open...", QKeySequence(Qt::CTRL | Qt::Key_P), this, &MainWindow::onQuickOpen);
    fileMenu->addAction("&Open Collection...", QKeySequence::Open, this, &MainWindow::onOpenCollection);
    m_recentCollectionsMenu = fileMenu->addMenu("Open &Recent");
    m_closeCollectionAction = fileMenu->addAction("&Close Collection", this, &MainWindow::onCloseCollection);
    fileMenu->addSeparator();
    fileMenu->addAction("&Import...", this, &MainWindow::onImport);
    auto* exportMenu = fileMenu->addMenu("&Export Collection");
    exportMenu->addAction("as &OpenAPI 3.0...", this, &MainWindow::onExportOpenApi);
    exportMenu->addAction("as &Postman Collection (v2.1)...", this, &MainWindow::onExportPostman);
    exportMenu->addAction("as &Insomnia Collection (v4)...", this, &MainWindow::onExportInsomnia);
    exportMenu->addAction("as &HTTP Archive (.har)...", this, &MainWindow::onExportHar);
    exportMenu->addAction("as &Markdown Runbook...", this, &MainWindow::onExportMarkdown);
    fileMenu->addAction("&Run Collection...", this, &MainWindow::onRunCollection);
    fileMenu->addAction("&Save Request", QKeySequence::Save, this, &MainWindow::onSaveRequest);
    fileMenu->addAction("&Close Tab", QKeySequence::Close, this, &MainWindow::onCloseCurrentTab);
    fileMenu->addSeparator();
    fileMenu->addAction("&Settings...", QKeySequence::Preferences, this, &MainWindow::onOpenSettings);
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", this, &QWidget::close);

    auto* editMenu = menuBar()->addMenu("&Edit");
    auto* cmdPaletteAct = editMenu->addAction("&Command Palette...", QKeySequence(Qt::CTRL | Qt::Key_K), this, &MainWindow::onOpenCommandPalette);
    cmdPaletteAct->setShortcuts({QKeySequence(Qt::CTRL | Qt::Key_K), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P)});
    editMenu->addAction("Find && &Replace in Collection...", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F), this, &MainWindow::onFindAndReplace);

    auto* envMenu = menuBar()->addMenu("&Environments");
    envMenu->addAction("&Manage Environments...", QKeySequence(Qt::CTRL | Qt::Key_E), this, &MainWindow::onManageEnvironments);

    auto* toolsMenu = menuBar()->addMenu("&Tools");
    toolsMenu->addAction("&Command Palette...", QKeySequence(Qt::CTRL | Qt::Key_K), this, &MainWindow::onOpenCommandPalette);
    toolsMenu->addAction("&Cookie Manager...", QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_C), this, &MainWindow::onManageCookies);
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
    toolsMenu->addAction("Find && &Replace in Collection...", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F), this, &MainWindow::onFindAndReplace);
    toolsMenu->addAction("Session &Network Telemetry...", this, &MainWindow::onShowSessionTelemetry);
    toolsMenu->addAction("Export Collection as &Markdown...", this, &MainWindow::onExportMarkdown);
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
    viewMenu->addAction("Toggle &Layout Split (Side-by-side / Stacked)", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L), this, [this]() {
        if (!m_contentSplitter) return;
        bool isVertical = (m_contentSplitter->orientation() == Qt::Vertical);
        m_contentSplitter->setOrientation(isVertical ? Qt::Horizontal : Qt::Vertical);
        m_contentSplitter->setSizes({500, 500});
    });

    auto* helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("&Keyboard Shortcuts...", QKeySequence(Qt::CTRL | Qt::Key_Slash), this, &MainWindow::onShowShortcuts);
    helpMenu->addSeparator();
    helpMenu->addAction("&About Poppy", this, [this]() {
        QMessageBox::about(this, "About Poppy",
            "<h3>Poppy API Client v1.4.5</h3>"
            "<p>A native, ultra-fast, local-first API client & test runner written in C++20 and Qt 6.</p>"
            "<p>Inspired by Bruno. Complete local ownership of your collections and environments.</p>");
    });
}

void MainWindow::onMethodChanged(int index) {
    auto method = static_cast<core::HttpMethod>(m_methodCombo->itemData(index).toInt());
    QColor c = Theme::methodColor(method);
    const bool dark = Theme::isDarkMode();
    m_methodCombo->setStyleSheet(QString(
        "QComboBox#methodCombo { color: %1; font-weight: 800; font-size: 13px; background: transparent; border: none; padding-left: 6px; }"
        "QComboBox#methodCombo:hover { background-color: %2; border-radius: 4px; }"
        "QComboBox#methodCombo::drop-down { border: none; width: 16px; }"
    ).arg(c.name(), dark ? "rgba(255, 255, 255, 0.06)" : "rgba(0, 0, 0, 0.05)"));
}

void MainWindow::updateRequestTabBadges() {
    if (!m_requestTabs || m_requestTabs->count() < 6) return;

    // 0: Params
    int paramCount = 0;
    for (const auto& p : m_currentRequest.queryParams) {
        if (p.enabled && !p.key.trimmed().isEmpty()) paramCount++;
    }
    for (const auto& p : m_currentRequest.pathParams) {
        if (p.enabled && !p.key.trimmed().isEmpty()) paramCount++;
    }
    m_requestTabs->setTabText(0, paramCount > 0 ? QString("Params (%1)").arg(paramCount) : "Params");

    // 1: Headers
    int headerCount = 0;
    for (const auto& h : m_currentRequest.headers) {
        if (h.enabled && !h.name.trimmed().isEmpty()) headerCount++;
    }
    m_requestTabs->setTabText(1, headerCount > 0 ? QString("Headers (%1)").arg(headerCount) : "Headers");

    // 2: Body
    QString bodyTitle = "Body";
    if (m_currentRequest.bodyType != core::BodyType::None && !m_currentRequest.bodyContent.trimmed().isEmpty()) {
        bodyTitle = QString("Body (%1)").arg(core::bodyTypeToString(m_currentRequest.bodyType));
    }
    m_requestTabs->setTabText(2, bodyTitle);

    // 3: Auth
    QString authTitle = "Auth";
    if (m_currentRequest.auth.type != core::AuthType::None) {
        authTitle = QString("Auth (%1)").arg(core::authTypeToString(m_currentRequest.auth.type));
    }
    m_requestTabs->setTabText(3, authTitle);

    // 4: Assertions
    int assertCount = m_currentRequest.assertions.size();
    m_requestTabs->setTabText(4, assertCount > 0 ? QString("Assertions (%1)").arg(assertCount) : "Assertions");

    // 5: Scripts
    int scriptCount = 0;
    if (!m_currentRequest.scripts.preRequestScript.trimmed().isEmpty()) scriptCount++;
    if (!m_currentRequest.scripts.postResponseScript.trimmed().isEmpty()) scriptCount++;
    if (!m_currentRequest.scripts.tests.trimmed().isEmpty()) scriptCount++;
    m_requestTabs->setTabText(5, scriptCount > 0 ? QString("Scripts (%1)").arg(scriptCount) : "Scripts & Tests");
}

void MainWindow::loadRequestIntoUi(const core::RequestModel& req) {
    m_loadingUi = true;
    m_currentRequest = req;

    // Breadcrumb path display
    QString reqDisplayName = req.name.isEmpty() ? "Untitled Request" : req.name;
    if (m_activeItem) {
        QStringList pathParts;
        for (auto* p = m_activeItem->parent(); p; p = p->parent()) {
            if (!p->name().isEmpty()) {
                pathParts.prepend(p->name());
            }
        }
        if (!pathParts.isEmpty()) {
            reqDisplayName = QString("<span style=\"color:#888ea0; font-weight:normal; font-size:12px;\">%1  ›  </span><b>%2</b>")
                .arg(pathParts.join("  ›  ").toHtmlEscaped(), (req.name.isEmpty() ? "Untitled Request" : req.name).toHtmlEscaped());
        }
    }
    m_requestNameLabel->setText(reqDisplayName);

    int idx = m_methodCombo->findData(static_cast<int>(req.method));
    if (idx >= 0) {
        m_methodCombo->setCurrentIndex(idx);
        onMethodChanged(idx);
    }

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
    updateRequestTabBadges();
    m_loadingUi = false;
}

void MainWindow::saveUiIntoRequest(core::RequestModel& req) {
    if (m_activeItem && m_activeItem->request()) {
        req.name = m_activeItem->request()->name;
    } else if (req.name.isEmpty()) {
        req.name = "Untitled Request";
    }
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
    if (m_loadingUi) return;
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
        if (m_openTabs[i].item == item || (!item->path().isEmpty() && m_openTabs[i].itemPath == item->path())) {
            m_openTabs[i].item = item;
            m_openTabs[i].itemPath = item->path();
            m_openRequestsTabBar->setCurrentIndex(i);
            return;
        }
    }

    // If only single tab open and it's the pristine Quick Request, reuse it
    if (m_openTabs.size() == 1 && m_openTabs[0].item == nullptr && !m_openTabs[0].isDirty && !m_openTabs[0].isPinned) {
        m_openTabs[0].item = item;
        m_openTabs[0].itemPath = item->path();
        m_openTabs[0].request = *item->request();
        m_openTabs[0].isDirty = false;
        m_openRequestsTabBar->setTabText(0, item->name());
        m_activeItem = item;
        loadRequestIntoUi(*item->request());
        m_openTabs[0].hasResponse = false;
        m_responseInspector->clear();
        return;
    }

    // Save current tab before creating new one
    if (m_currentTabIndex >= 0 && m_currentTabIndex < m_openTabs.size()) {
        saveUiIntoRequest(m_openTabs[m_currentTabIndex].request);
    }

    OpenTabInfo newTab;
    newTab.tabId = ++m_nextTabId;
    newTab.item = item;
    newTab.itemPath = item->path();
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

    // Save active tab, and flush a pending auto-save before leaving it.
    if (m_currentTabIndex >= 0 && m_currentTabIndex < m_openTabs.size()) {
        saveUiIntoRequest(m_openTabs[m_currentTabIndex].request);
        flushDirtyTab(m_currentTabIndex);
    }

    m_currentTabIndex = index;
    m_activeItem = m_openTabs[index].item;
    loadRequestIntoUi(m_openTabs[index].request);
    if (m_openTabs[index].hasResponse) {
        m_responseInspector->setResponse(m_openTabs[index].lastResponse, &m_openTabs[index].lastReport);
    } else {
        m_responseInspector->clear();
    }
}

void MainWindow::onTabMoved(int from, int to) {
    if (from < 0 || to < 0 || from >= m_openTabs.size() || to >= m_openTabs.size()) return;
    m_openTabs.move(from, to);
    m_currentTabIndex = m_openRequestsTabBar->currentIndex();
    if (m_currentTabIndex >= 0 && m_currentTabIndex < m_openTabs.size()) {
        m_activeItem = m_openTabs[m_currentTabIndex].item;
    }
}

void MainWindow::onItemAboutToBeDeleted(core::CollectionItem* item) {
    if (!item) return;
    auto isOrContains = [](core::CollectionItem* ancestor, core::CollectionItem* node) {
        for (auto* p = node; p; p = p->parent()) {
            if (p == ancestor) return true;
        }
        return false;
    };
    for (int i = 0; i < m_openTabs.size(); ++i) {
        if (isOrContains(item, m_openTabs[i].item)) {
            if (m_openTabs[i].item) {
                m_openTabs[i].itemPath = m_openTabs[i].item->path();
            }
            m_openTabs[i].item = nullptr;
            m_openTabs[i].isDirty = true;
            updateTabTitle(i);
        }
    }
    if (isOrContains(item, m_activeItem)) {
        m_activeItem = nullptr;
    }
}

void MainWindow::onCollectionAboutToReload() {
    for (auto& tab : m_openTabs) {
        if (tab.item) {
            tab.itemPath = tab.item->path();
        }
        tab.item = nullptr;
    }
    m_activeItem = nullptr;
}

void MainWindow::onCollectionLoaded() {
    rebindOpenTabs();
    updateTopEnvCombo();
    updateUrlVariableInspection();
}

void MainWindow::rebindOpenTabs() {
    for (int i = 0; i < m_openTabs.size(); ++i) {
        auto& tab = m_openTabs[i];
        if (tab.itemPath.isEmpty()) continue;
        auto* found = m_collectionModel.findItemByPath(tab.itemPath);
        tab.item = found;
        if (found) {
            tab.itemPath = found->path();
            if (!tab.isDirty && found->request()) {
                tab.request = *found->request();
                tab.hasResponse = false;
                tab.lastReport = {};
            }
        }
    }
    if (m_currentTabIndex >= 0 && m_currentTabIndex < m_openTabs.size()) {
        m_activeItem = m_openTabs[m_currentTabIndex].item;
        if (m_activeItem && !m_openTabs[m_currentTabIndex].isDirty) {
            loadRequestIntoUi(m_openTabs[m_currentTabIndex].request);
        }
    }
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

    const int previousCurrent = m_currentTabIndex;
    if (index == m_currentTabIndex) {
        saveUiIntoRequest(m_openTabs[index].request);
    }

    if (m_openTabs[index].isDirty) {
        auto res = QMessageBox::question(this, "Unsaved Changes",
            QString("Save changes to '%1' before closing?").arg(m_openTabs[index].request.name),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (res == QMessageBox::Cancel) return;
        if (res == QMessageBox::Save) {
            bool saved = false;
            if (m_openTabs[index].item && m_openTabs[index].item->request()) {
                *m_openTabs[index].item->request() = m_openTabs[index].request;
                saved = m_collectionModel.saveRequest(m_openTabs[index].item);
            } else if (m_collectionModel.rootItem()) {
                saved = m_collectionModel.addRequest(m_collectionModel.rootItem(),
                    m_openTabs[index].request.name, m_openTabs[index].request) != nullptr;
            } else {
                saved = true;
            }
            if (!saved) {
                QMessageBox::warning(this, "Save Failed",
                    "Could not write the request to disk. The tab was left open.");
                return;
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
        OpenTabInfo quickTab;
        quickTab.tabId = ++m_nextTabId;
        quickTab.request = quickReq;
        m_openTabs.append(quickTab);
        m_openRequestsTabBar->addTab("Quick Request");
        m_currentTabIndex = 0;
        m_activeItem = nullptr;
        loadRequestIntoUi(quickReq);
        return;
    }

    if (index == previousCurrent) {
        int nextIdx = qBound(0, index == 0 ? 0 : index - 1, m_openTabs.size() - 1);
        m_currentTabIndex = -1;
        m_openRequestsTabBar->setCurrentIndex(nextIdx);
        onTabChanged(nextIdx);
    } else {
        if (index < previousCurrent) {
            m_currentTabIndex = previousCurrent - 1;
        } else {
            m_currentTabIndex = previousCurrent;
        }
        m_openRequestsTabBar->blockSignals(true);
        m_openRequestsTabBar->setCurrentIndex(m_currentTabIndex);
        m_openRequestsTabBar->blockSignals(false);
        if (m_currentTabIndex >= 0 && m_currentTabIndex < m_openTabs.size()) {
            m_activeItem = m_openTabs[m_currentTabIndex].item;
        }
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
    tabInfo.tabId = ++m_nextTabId;
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

void MainWindow::onOpenCommandPalette() {
    CommandPaletteDialog dialog(this);

    // 1. Core Actions
    QList<PaletteEntry> actions = {
        {PaletteItemType::Action, "Send Request", "Execute current HTTP request", "Ctrl+Enter", "Request", core::HttpMethod::GET, "", nullptr, [this]() { onSendClicked(); }},
        {PaletteItemType::Action, "New Request", "Create a new blank request tab", "Ctrl+N", "Request", core::HttpMethod::GET, "", nullptr, [this]() { onNewRequest(); }},
        {PaletteItemType::Action, "Quick Open Request...", "Quickly search and jump to any request", "Ctrl+P", "Request", core::HttpMethod::GET, "", nullptr, [this]() { onQuickOpen(); }},
        {PaletteItemType::Action, "Save Request", "Save active request to collection disk", "Ctrl+S", "Request", core::HttpMethod::GET, "", nullptr, [this]() { onSaveRequest(); }},
        {PaletteItemType::Action, "Close Tab", "Close currently active request tab", "Ctrl+W", "Request", core::HttpMethod::GET, "", nullptr, [this]() { onCloseCurrentTab(); }},
        {PaletteItemType::Action, "Open Collection...", "Open a directory containing Bruno or Poppy collection", "Ctrl+O", "Collection", core::HttpMethod::GET, "", nullptr, [this]() { onOpenCollection(); }},
        {PaletteItemType::Action, "Copy as cURL", "Export active request as cURL command to clipboard", "", "Export", core::HttpMethod::GET, "", nullptr, [this]() { onCopyAsCurl(); }},
        {PaletteItemType::Action, "Show Code Snippets...", "Generate client code in Python, Node.js, Go, Rust, etc.", "", "Tools", core::HttpMethod::GET, "", nullptr, [this]() { onShowCodeSnippets(); }},
        {PaletteItemType::Action, "Toggle Dark / Light Theme", "Switch between Obsidian dark and crisp light mode", "Ctrl+T", "View", core::HttpMethod::GET, "", nullptr, [this]() { onToggleTheme(); }},
        {PaletteItemType::Action, "Toggle Layout Split", "Switch between horizontal side-by-side and vertical stacked layout", "Ctrl+Shift+L", "View", core::HttpMethod::GET, "", nullptr, [this]() {
            if (!m_contentSplitter) return;
            bool isVert = (m_contentSplitter->orientation() == Qt::Vertical);
            m_contentSplitter->setOrientation(isVert ? Qt::Horizontal : Qt::Vertical);
            m_contentSplitter->setSizes({500, 500});
        }},
        {PaletteItemType::Action, "Manage Environments...", "Edit environment variables, secrets, and configurations", "Ctrl+E", "Environments", core::HttpMethod::GET, "", nullptr, [this]() { onManageEnvironments(); }},
        {PaletteItemType::Action, "Cookie Manager...", "Inspect and edit domain cookies stored in jar", "Ctrl+Alt+C", "Tools", core::HttpMethod::GET, "", nullptr, [this]() { onManageCookies(); }},
        {PaletteItemType::Action, "Clear Cookie Jar", "Flush all session and persistent cookies", "", "Tools", core::HttpMethod::GET, "", nullptr, [this]() { onClearCookieJar(); }},
        {PaletteItemType::Action, "Clear Request History", "Clear all request execution history", "", "History", core::HttpMethod::GET, "", nullptr, [this]() {
            if (m_historyManager.count() > 0) {
                auto res = QMessageBox::question(this, "Clear History", "Clear all request execution history?", QMessageBox::Yes | QMessageBox::No);
                if (res == QMessageBox::Yes) {
                    m_historyManager.clear();
                }
            }
        }},
        {PaletteItemType::Action, "Find & Replace in Collection...", "Search and bulk replace strings across collection", "Ctrl+Shift+F", "Edit", core::HttpMethod::GET, "", nullptr, [this]() { onFindAndReplace(); }},
        {PaletteItemType::Action, "Run Collection Runner...", "Batch execute requests and view automated test results", "", "Collection", core::HttpMethod::GET, "", nullptr, [this]() { onRunCollection(); }},
        {PaletteItemType::Action, "Session Network Telemetry...", "View network roundtrip statistics and throughput", "", "Telemetry", core::HttpMethod::GET, "", nullptr, [this]() { onShowSessionTelemetry(); }},
        {PaletteItemType::Action, "Response Diff Viewer...", "Compare two response payloads side-by-side", "", "Tools", core::HttpMethod::GET, "", nullptr, [this]() { onOpenDiffViewer(); }},
        {PaletteItemType::Action, "WebSocket Client...", "Open interactive real-time WebSocket client", "", "Tools", core::HttpMethod::GET, "", nullptr, [this]() { onOpenWebSocket(); }},
        {PaletteItemType::Action, "gRPC Client...", "Invoke gRPC services with Protobuf reflection", "", "Tools", core::HttpMethod::GET, "", nullptr, [this]() { onOpenGrpc(); }},
        {PaletteItemType::Action, "Server-Sent Events (SSE)...", "Stream SSE event-source feeds in real-time", "", "Tools", core::HttpMethod::GET, "", nullptr, [this]() { onOpenSse(); }},
        {PaletteItemType::Action, "Mock Server...", "Launch local HTTP mock server for offline testing", "", "Tools", core::HttpMethod::GET, "", nullptr, [this]() { onOpenMockServer(); }},
        {PaletteItemType::Action, "Git Sync & Branches...", "Manage git commits, branches, and push/pull", "", "Tools", core::HttpMethod::GET, "", nullptr, [this]() { onOpenGitSync(); }},
        {PaletteItemType::Action, "Generate API Documentation...", "Create styled interactive HTML documentation", "", "Documentation", core::HttpMethod::GET, "", nullptr, [this]() { onGenerateDocumentation(); }},
        {PaletteItemType::Action, "Export Collection as OpenAPI 3.0...", "Generate OpenAPI v3 JSON specification", "", "Export", core::HttpMethod::GET, "", nullptr, [this]() { onExportOpenApi(); }},
        {PaletteItemType::Action, "Export Collection as Postman (v2.1)...", "Export to Postman collection format", "", "Export", core::HttpMethod::GET, "", nullptr, [this]() { onExportPostman(); }},
        {PaletteItemType::Action, "Export Collection as Insomnia (v4)...", "Export to Insomnia collection format", "", "Export", core::HttpMethod::GET, "", nullptr, [this]() { onExportInsomnia(); }},
        {PaletteItemType::Action, "Export Collection as HTTP Archive (.har)...", "Export collection as HAR archive", "", "Export", core::HttpMethod::GET, "", nullptr, [this]() { onExportHar(); }},
        {PaletteItemType::Action, "Export Collection as Markdown Runbook...", "Generate documentation markdown file", "", "Export", core::HttpMethod::GET, "", nullptr, [this]() { onExportMarkdown(); }},
        {PaletteItemType::Action, "Import Collection / OpenAPI / Postman...", "Import from Bruno, Postman, or OpenAPI file", "", "Import", core::HttpMethod::GET, "", nullptr, [this]() { onImport(); }},
        {PaletteItemType::Action, "Settings & Preferences...", "Configure SSL verification, proxies, and timeouts", "Ctrl+,", "Settings", core::HttpMethod::GET, "", nullptr, [this]() { onOpenSettings(); }},
        {PaletteItemType::Action, "Keyboard Shortcuts...", "Show cheat sheet of all Poppy shortcut keys", "Ctrl+/", "Help", core::HttpMethod::GET, "", nullptr, [this]() { onShowShortcuts(); }}
    };
    dialog.setActions(actions);

    // 2. Environments
    QStringList envNames;
    for (const auto& env : m_collectionModel.environments()) {
        envNames.append(env.name());
    }
    dialog.setEnvironments(envNames, m_activeEnvName);

    // 3. Requests
    dialog.setCollection(&m_collectionModel);

    if (dialog.exec() == QDialog::Accepted) {
        PaletteEntry chosen = dialog.selectedEntry();
        if (chosen.type == PaletteItemType::Action && chosen.action) {
            chosen.action();
        } else if (chosen.type == PaletteItemType::Environment) {
            onEnvironmentChanged(chosen.envName);
        } else if (chosen.type == PaletteItemType::Request && chosen.requestItem) {
            onRequestSelected(chosen.requestItem);
        }
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

bool MainWindow::openPath(const QString& path, bool remember) {
    if (path.isEmpty()) return false;
    QFileInfo fi(path);
    QString dirPath = fi.isDir() ? fi.canonicalFilePath() : fi.dir().canonicalPath();
    if (dirPath.isEmpty()) {
        dirPath = fi.isDir() ? path : fi.dir().path();
    }
    if (m_collectionModel.openDirectory(dirPath)) {
        m_sidebar->refreshTree();
        updateTopEnvCombo();
        updateUrlVariableInspection();

        QString collName = m_collectionModel.name();
        if (collName.isEmpty()) collName = QDir(dirPath).dirName();
        setWindowTitle(QString("%1 - Poppy").arg(collName));

        if (remember) {
            QSettings settings;
            settings.setValue("collection/lastPath", dirPath);

            QStringList recent = settings.value("collection/recentPaths").toStringList();
            recent.removeAll(dirPath);
            recent.prepend(dirPath);
            while (recent.size() > 10) recent.removeLast();
            settings.setValue("collection/recentPaths", recent);

            updateRecentCollectionsMenu();
            saveAppState();
        }

        if (fi.isFile()) {
            auto* item = m_collectionModel.findItemByPath(fi.canonicalFilePath());
            if (item) {
                onRequestSelected(item);
            }
        }
        return true;
    }
    return false;
}

void MainWindow::onOpenCollection() {
    QSettings settings;
    QString lastDir = settings.value("collection/lastPath").toString();
    if (lastDir.isEmpty() || !QDir(lastDir).exists()) {
        lastDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }
    QString dir = QFileDialog::getExistingDirectory(this, "Open Collection Directory", lastDir);
    if (!dir.isEmpty()) {
        if (!openPath(dir)) {
            QMessageBox::warning(this, "Error", "Failed to open collection directory.");
        }
    }
}

void MainWindow::onSaveRequest() {
    saveUiIntoRequest(m_currentRequest);
    if (m_currentTabIndex >= 0 && m_currentTabIndex < m_openTabs.size()) {
        m_openTabs[m_currentTabIndex].request = m_currentRequest;
    }
    core::CollectionItem* targetItem = m_activeItem;
    if (!targetItem && m_currentTabIndex >= 0 && m_currentTabIndex < m_openTabs.size()) {
        targetItem = m_openTabs[m_currentTabIndex].item;
    }
    if (targetItem) {
        if (targetItem->request()) {
            *targetItem->request() = m_currentRequest;
        }
        if (m_collectionModel.saveRequest(targetItem)) {
            if (m_currentTabIndex >= 0 && m_currentTabIndex < m_openTabs.size()) {
                m_openTabs[m_currentTabIndex].isDirty = false;
                updateTabTitle(m_currentTabIndex);
            }
            statusBar()->showMessage("Request saved successfully.", 3000);
        } else {
            statusBar()->showMessage("Failed to save request to disk.", 3000);
        }
    } else {
        if (m_currentTabIndex >= 0 && m_currentTabIndex < m_openTabs.size()) {
            m_openTabs[m_currentTabIndex].isDirty = false;
            updateTabTitle(m_currentTabIndex);
        }
        statusBar()->showMessage("Quick request updated.", 3000);
    }
}

void MainWindow::onCopyAsCurl() {
    saveUiIntoRequest(m_currentRequest);
    core::VariableResolver resolver = currentVariableResolver();
    core::RequestModel resolvedReq = resolver.resolveRequest(requestForExecution());
    QGuiApplication::clipboard()->setText(resolvedReq.toCurlCommand());
    statusBar()->showMessage("cURL command copied to clipboard!", 3000);
}

void MainWindow::onEnvironmentChanged(const QString& envName) {
    m_activeEnvName = envName;
    if (m_sidebar) {
        m_sidebar->setActiveEnvironment(envName);
    }
    updateTopEnvCombo();
    updateUrlVariableInspection();
    statusBar()->showMessage(envName.isEmpty() ? "No environment active." : ("Active environment: " + envName), 3000);
}

void MainWindow::onShowCodeSnippets() {
    saveUiIntoRequest(m_currentRequest);
    core::VariableResolver resolver = currentVariableResolver();
    core::RequestModel resolvedReq = resolver.resolveRequest(requestForExecution());

    CodeSnippetDialog dlg(resolvedReq, this);
    dlg.exec();
}

void MainWindow::onImport() {
    ImportDialog dlg(m_collectionModel.rootPath(), this);
    connect(&dlg, &ImportDialog::curlImported, this, [this](const core::RequestModel& req) {
        loadRequestIntoUi(req);
        if (m_currentTabIndex >= 0 && m_currentTabIndex < m_openTabs.size()) {
            m_openTabs[m_currentTabIndex].request = m_currentRequest;
            m_openTabs[m_currentTabIndex].hasResponse = false;
            m_openTabs[m_currentTabIndex].lastReport = {};
        }
        markCurrentTabDirty();
        statusBar()->showMessage("cURL command imported successfully!", 3000);
    });
    connect(&dlg, &ImportDialog::collectionImported, this, [this](const QString& dirPath) {
        if (openPath(dirPath)) {
            saveAppState();
            statusBar()->showMessage(QString("Collection imported and loaded from %1").arg(dirPath), 4000);
        } else {
            statusBar()->showMessage("Failed to open imported collection directory.", 4000);
        }
    });
    dlg.exec();
}

void MainWindow::onRunCollection() {
    network::CurlNetworkEngine runnerEngine;
    runnerEngine.setSslVerifyPeer(m_networkEngine.sslVerifyPeer());
    runnerEngine.setTimeoutMs(m_networkEngine.timeoutMs());
    runnerEngine.setProxy(m_networkEngine.proxy());
    runnerEngine.setCookieJarEnabled(m_networkEngine.cookieJarEnabled());
    runnerEngine.setCookieJarPath(QDir::temp().filePath(
        QStringLiteral("poppy_runner_%1.txt").arg(QUuid::createUuid().toString(QUuid::Id128))));
    runnerEngine.setClientCertPath(m_networkEngine.clientCertPath());
    runnerEngine.setClientCertType(m_networkEngine.clientCertType());
    runnerEngine.setClientKeyPath(m_networkEngine.clientKeyPath());
    runnerEngine.setClientKeyPassword(m_networkEngine.clientKeyPassword());
    CollectionRunnerDialog dlg(&m_collectionModel, &runnerEngine, &m_scriptRunner, m_activeEnvName, this);
    dlg.exec();
}

void MainWindow::onManageEnvironments() {
    EnvironmentDialog dlg(m_collectionModel.environments(), m_activeEnvName, m_collectionModel.rootPath(), this);
    connect(&dlg, &EnvironmentDialog::environmentsModified, this, &MainWindow::refreshEnvironmentUi);
    dlg.exec();
    refreshEnvironmentUi();
}

void MainWindow::onSendClicked() {
    saveUiIntoRequest(m_currentRequest);

    core::EnvironmentModel* liveEnv = mutableActiveEnvironment();
    core::EnvironmentModel scratch(m_activeEnvName);
    core::EnvironmentModel& activeEnv = liveEnv ? *liveEnv : scratch;

    core::VariableResolver resolver = currentVariableResolver();
    const core::RequestModel historyTemplate = m_currentRequest;
    QString historyPath;
    if (m_activeItem) {
        historyPath = m_activeItem->path();
    } else if (m_currentTabIndex >= 0 && m_currentTabIndex < m_openTabs.size()) {
        historyPath = m_openTabs[m_currentTabIndex].resolvePath;
    }
    core::RequestModel resolvedReq = resolver.resolveRequest(requestForExecution());
    QString scriptErr;
    if (!m_scriptRunner.runPreRequestScript(resolvedReq.scripts.preRequestScript, resolvedReq, activeEnv, &scriptErr)) {
        QMessageBox::warning(this, "Pre-request Script Error", scriptErr);
        return;
    }
    persistActiveEnvironment();
    resolver = currentVariableResolver();
    resolvedReq = resolver.resolveRequest(resolvedReq);

    m_networkEngine.cancelAll();

    m_sendBtn->setEnabled(false);
    m_sendBtn->setText("Sending...");

    const quint64 sendGen = ++m_sendGeneration;
    const quint64 sentTabId = (m_currentTabIndex >= 0 && m_currentTabIndex < m_openTabs.size())
        ? m_openTabs[m_currentTabIndex].tabId : 0;
    m_networkEngine.sendRequestAsync(resolvedReq, [this, resolvedReq, sendGen, sentTabId, historyTemplate, historyPath](const core::ResponseModel& res) mutable {
        QPointer<MainWindow> self(this);
        if (!self) return;

        if (sendGen == self->m_sendGeneration) {
            m_sendBtn->setEnabled(true);
            m_sendBtn->setText("Send");
        }
        if (sendGen != self->m_sendGeneration) return;

        core::EnvironmentModel* live = mutableActiveEnvironment();
        core::EnvironmentModel scratchEnv(m_activeEnvName);
        core::EnvironmentModel& envForScripts = live ? *live : scratchEnv;

        QString postErr;
        if (!m_scriptRunner.runPostResponseScript(resolvedReq.scripts.postResponseScript, resolvedReq, res, envForScripts, &postErr)) {
            if (postErr.isEmpty()) postErr = QStringLiteral("Post-response script failed");
        }
        persistActiveEnvironment();

        core::TestReport report = m_scriptRunner.runTests(resolvedReq.scripts.tests, resolvedReq, res, envForScripts);
        auto declResults = core::DeclarativeAssertionEvaluator::evaluateAll(resolvedReq.assertions, res);
        for (const auto& dr : declResults) {
            report.results.append(dr);
        }

        int targetTab = -1;
        for (int i = 0; i < m_openTabs.size(); ++i) {
            if (m_openTabs[i].tabId == sentTabId) {
                targetTab = i;
                break;
            }
        }
        if (targetTab >= 0) {
            m_openTabs[targetTab].lastResponse = res;
            m_openTabs[targetTab].lastReport = report;
            m_openTabs[targetTab].hasResponse = true;
            if (m_currentTabIndex == targetTab) {
                m_responseInspector->setResponse(res, &report);
            }
        } else if (m_currentTabIndex < 0) {
            m_responseInspector->setResponse(res, &report);
        }

        m_historyManager.addEntry(historyTemplate, res, historyPath);

        m_sessionReqCount++;
        qint64 bytes = res.sizeBytes > 0 ? res.sizeBytes : res.rawBody.size();
        m_sessionBytesReceived += bytes;
        m_sessionTotalLatencyMs += res.latencyMs;
        if (!res.errorString.isEmpty() || res.statusCode >= 400 || res.statusCode == 0) {
            m_sessionErrorCount++;
        }
        updateSessionTelemetryWidget();

        if (!postErr.isEmpty()) {
            QMessageBox::warning(this, "Post-response Script Error", postErr);
        }
    });
}

void MainWindow::onHistoryItemSelected(const core::HistoryItem& item) {
    if (m_currentTabIndex >= 0 && m_currentTabIndex < m_openTabs.size()) {
        saveUiIntoRequest(m_openTabs[m_currentTabIndex].request);
    }
    loadRequestIntoUi(item.request);
    m_currentRequest = item.request;
    m_activeItem = nullptr;
    if (m_currentTabIndex >= 0 && m_currentTabIndex < m_openTabs.size()) {
        m_openTabs[m_currentTabIndex].request = item.request;
        m_openTabs[m_currentTabIndex].item = nullptr;
        m_openTabs[m_currentTabIndex].itemPath.clear();
        m_openTabs[m_currentTabIndex].resolvePath = item.sourcePath;
        m_openTabs[m_currentTabIndex].isDirty = true;
        m_openTabs[m_currentTabIndex].lastResponse = item.toResponseModel();
        m_openTabs[m_currentTabIndex].lastReport = {};
        m_openTabs[m_currentTabIndex].hasResponse = true;
        updateTabTitle(m_currentTabIndex);
    }
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
        updateTabTitle(index);
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

void MainWindow::flushDirtyTab(int index) {
    if (index < 0 || index >= m_openTabs.size()) return;
    auto& tab = m_openTabs[index];
    if (!tab.isDirty || !tab.item) return;
    if (tab.item->request()) {
        *tab.item->request() = tab.request;
    }
    if (m_collectionModel.saveRequest(tab.item)) {
        tab.isDirty = false;
        updateTabTitle(index);
        statusBar()->showMessage("Auto-saved changes.", 2000);
    } else {
        statusBar()->showMessage("Auto-save failed.", 3000);
    }
}

void MainWindow::onAutoSaveTimerTimeout() {
    if (m_currentTabIndex >= 0 && m_currentTabIndex < m_openTabs.size()) {
        saveUiIntoRequest(m_openTabs[m_currentTabIndex].request);
    }
    flushDirtyTab(m_currentTabIndex);
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
    dlg->setNetworkEngine(&m_networkEngine);
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
    resolver.setGlobalVariables(m_sessionGlobals);
    core::EnvironmentModel activeEnv(m_activeEnvName);
    for (const auto& env : m_collectionModel.environments()) {
        if (env.name() == m_activeEnvName) {
            activeEnv = env;
            break;
        }
    }
    resolver.setEnvironment(activeEnv);
    if (m_collectionModel.rootItem()) {
        resolver.setCollectionVariables(m_collectionModel.rootItem()->variables());
    }
    if (core::CollectionItem* scope = scopeItem()) {
        resolver.setFolderVariables(scope->effectiveVariables());
    }
    return resolver;
}

core::CollectionItem* MainWindow::scopeItem() const {
    if (m_activeItem) return m_activeItem;
    if (m_currentTabIndex >= 0 && m_currentTabIndex < m_openTabs.size()) {
        const QString& path = m_openTabs[m_currentTabIndex].resolvePath;
        if (!path.isEmpty()) {
            return m_collectionModel.findItemByPath(path);
        }
    }
    return nullptr;
}

core::RequestModel MainWindow::requestForExecution() const {
    core::RequestModel toSend = m_currentRequest;
    if (core::CollectionItem* scope = scopeItem()) {
        if (toSend.auth.type == core::AuthType::Inherit) {
            toSend.auth = scope->effectiveAuth();
        }
        scope->applyInheritedHeaders(toSend);
    } else if (toSend.auth.type == core::AuthType::Inherit) {
        toSend.auth = {};
    }
    return toSend;
}

void MainWindow::refreshEnvironmentUi() {
    bool found = m_activeEnvName.isEmpty();
    for (const auto& env : m_collectionModel.environments()) {
        if (env.name() == m_activeEnvName) {
            found = true;
            break;
        }
    }
    if (!found) {
        m_activeEnvName.clear();
        m_sidebar->setActiveEnvironment(QString());
    } else {
        m_sidebar->setActiveEnvironment(m_activeEnvName);
    }
    m_sidebar->updateEnvironmentsCombo();
    updateTopEnvCombo();
    updateUrlVariableInspection();
}

core::EnvironmentModel* MainWindow::mutableActiveEnvironment() {
    if (m_activeEnvName.isEmpty()) return nullptr;
    for (auto& env : m_collectionModel.environments()) {
        if (env.name() == m_activeEnvName) {
            return &env;
        }
    }
    return nullptr;
}

void MainWindow::persistActiveEnvironment() {
    core::EnvironmentModel* live = mutableActiveEnvironment();
    if (!live) return;
    m_collectionModel.saveEnvironment(*live);
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

void MainWindow::applyRequestChrome() {
    const bool dark = Theme::isDarkMode();
    if (m_openRequestsTabBar) {
        if (dark) {
            m_openRequestsTabBar->setStyleSheet(
                "QTabBar { background: #111215; border-bottom: 1px solid #23242a; qproperty-drawBase: 0; }"
                "QTabBar::tab {"
                "    background: transparent;"
                "    color: #9496a1;"
                "    padding: 6px 12px 6px 14px;"
                "    margin-right: 3px;"
                "    margin-top: 2px;"
                "    border-top-left-radius: 6px;"
                "    border-top-right-radius: 6px;"
                "    border: 1px solid transparent;"
                "    border-bottom: 2px solid transparent;"
                "    font-size: 12px;"
                "    font-weight: 500;"
                "    min-width: 80px;"
                "    max-width: 220px;"
                "}"
                "QTabBar::tab:hover:!selected {"
                "    background: rgba(255, 255, 255, 0.04);"
                "    color: #e2e4ea;"
                "    border: 1px solid #1f2026;"
                "    border-bottom: 2px solid transparent;"
                "}"
                "QTabBar::tab:selected {"
                "    background: #18191e;"
                "    color: #ffffff;"
                "    font-weight: 600;"
                "    border: 1px solid #282932;"
                "    border-bottom: 2px solid #f59e0b;"
                "}"
                "QTabBar::close-button {"
                "    image: url(:/icons/close_tab.png);"
                "    subcontrol-position: right;"
                "    subcontrol-origin: padding;"
                "    margin-left: 8px;"
                "    margin-right: 2px;"
                "    padding: 3px;"
                "    border-radius: 4px;"
                "    background: transparent;"
                "}"
                "QTabBar::close-button:hover {"
                "    image: url(:/icons/close_tab_hover.png);"
                "    background: rgba(255, 255, 255, 0.12);"
                "}"
                "QTabBar::close-button:pressed {"
                "    background: rgba(255, 255, 255, 0.22);"
                "}"
            );
        } else {
            m_openRequestsTabBar->setStyleSheet(
                "QTabBar { background: #f8fafc; border-bottom: 1px solid #e2e8f0; qproperty-drawBase: 0; }"
                "QTabBar::tab {"
                "    background: transparent;"
                "    color: #64748b;"
                "    padding: 6px 12px 6px 14px;"
                "    margin-right: 3px;"
                "    margin-top: 2px;"
                "    border-top-left-radius: 6px;"
                "    border-top-right-radius: 6px;"
                "    border: 1px solid transparent;"
                "    border-bottom: 2px solid transparent;"
                "    font-size: 12px;"
                "    font-weight: 500;"
                "    min-width: 80px;"
                "    max-width: 220px;"
                "}"
                "QTabBar::tab:hover:!selected {"
                "    background: rgba(0, 0, 0, 0.04);"
                "    color: #0f172a;"
                "    border: 1px solid #e2e8f0;"
                "    border-bottom: 2px solid transparent;"
                "}"
                "QTabBar::tab:selected {"
                "    background: #ffffff;"
                "    color: #0f172a;"
                "    font-weight: 600;"
                "    border: 1px solid #cbd5e1;"
                "    border-bottom: 2px solid #d97706;"
                "}"
                "QTabBar::close-button {"
                "    image: url(:/icons/close_tab_light.png);"
                "    subcontrol-position: right;"
                "    subcontrol-origin: padding;"
                "    margin-left: 8px;"
                "    margin-right: 2px;"
                "    padding: 3px;"
                "    border-radius: 4px;"
                "    background: transparent;"
                "}"
                "QTabBar::close-button:hover {"
                "    image: url(:/icons/close_tab_light_hover.png);"
                "    background: rgba(0, 0, 0, 0.08);"
                "}"
                "QTabBar::close-button:pressed {"
                "    background: rgba(0, 0, 0, 0.15);"
                "}"
            );
        }
    }
    if (m_requestNameLabel) {
        m_requestNameLabel->setStyleSheet(QString("font-size: 16px; font-weight: 700; color: %1; padding-left: 2px;").arg(dark ? "#f9fafb" : "#0f172a"));
    }
}

void MainWindow::onToggleTheme() {
    bool dark = Theme::toggleTheme();
    if (auto* app = qobject_cast<QApplication*>(QApplication::instance())) {
        app->setStyleSheet(dark ? Theme::darkStyleSheet() : Theme::lightStyleSheet());
    }
    applyRequestChrome();
    if (m_methodCombo) {
        onMethodChanged(m_methodCombo->currentIndex());
    }
    updateTopEnvCombo();
    updateRequestTabBadges();
    m_sidebar->refreshTree();
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
    const core::EnvironmentModel* activeEnvModel = nullptr;
    for (const auto& env : m_collectionModel.environments()) {
        if (env.name() == m_activeEnvName) {
            activeEnvModel = &env;
            break;
        }
    }
    for (auto it = allVars.begin(); it != allVars.end(); ++it) {
        QString scope;
        QString val = resolver.lookupVariableWithScope(it.key(), &scope);
        if (activeEnvModel && activeEnvModel->isSecretVariable(it.key())) {
            val = QStringLiteral("••••••••");
        }
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
        if (auto* existingModel = qobject_cast<QStringListModel*>(m_urlCompleter->model())) {
            existingModel->setStringList(varTokens);
        } else {
            m_urlCompleter->setModel(new QStringListModel(varTokens, m_urlCompleter));
        }
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

    const core::EnvironmentModel* activeEnvModel = nullptr;
    for (const auto& env : m_collectionModel.environments()) {
        if (env.name() == m_activeEnvName) {
            activeEnvModel = &env;
            break;
        }
    }

    QString tooltip = "<div style='font-family: Consolas, monospace; font-size: 11px;'><b>Variable Hover Inspection:</b><br/>";
    while (matchIter.hasNext()) {
        auto match = matchIter.next();
        QString varName = match.captured(1).trimmed();
        QString scope;
        QString val = resolver.lookupVariableWithScope(varName, &scope);
        if (activeEnvModel && activeEnvModel->isSecretVariable(varName)) {
            val = QStringLiteral("••••••••");
        }
        if (!scope.isEmpty() && scope != "Unresolved") {
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

void MainWindow::updateTopEnvCombo() {
    if (!m_topEnvCombo) return;
    m_topEnvCombo->blockSignals(true);
    m_topEnvCombo->clear();
    m_topEnvCombo->addItem("⚪ No Environment", QString());

    int selectIdx = 0;
    for (const auto& env : m_collectionModel.environments()) {
        m_topEnvCombo->addItem("🟢 " + env.name(), env.name());
        if (env.name() == m_activeEnvName) {
            selectIdx = m_topEnvCombo->count() - 1;
        }
    }
    m_topEnvCombo->setCurrentIndex(selectIdx);
    m_topEnvCombo->blockSignals(false);
    if (selectIdx == 0 && !m_activeEnvName.isEmpty()) {
        bool found = false;
        for (const auto& env : m_collectionModel.environments()) {
            if (env.name() == m_activeEnvName) {
                found = true;
                break;
            }
        }
        if (!found) {
            m_activeEnvName.clear();
            m_sidebar->setActiveEnvironment(QString());
        }
    }
}

static QString formatTelemetryBytes(qint64 bytes) {
    if (bytes < 1024) {
        return QString("%1 B").arg(bytes);
    } else if (bytes < 1024 * 1024) {
        return QString("%1 KB").arg(QString::number(bytes / 1024.0, 'f', 1));
    } else if (bytes < 1024LL * 1024 * 1024) {
        return QString("%1 MB").arg(QString::number(bytes / (1024.0 * 1024.0), 'f', 2));
    } else {
        return QString("%1 GB").arg(QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2));
    }
}

void MainWindow::updateSessionTelemetryWidget() {
    if (!m_sessionTelemetryBtn) return;
    qint64 avgLatency = m_sessionReqCount > 0 ? (m_sessionTotalLatencyMs / m_sessionReqCount) : 0;
    QString txt = QString("⚡ %1 req%2 | 📦 %3 | ⏱ avg %4 ms")
                      .arg(m_sessionReqCount)
                      .arg(m_sessionReqCount == 1 ? "" : "s")
                      .arg(formatTelemetryBytes(m_sessionBytesReceived))
                      .arg(avgLatency);
    m_sessionTelemetryBtn->setText(txt);
}

void MainWindow::onShowSessionTelemetry() {
    QDialog dlg(this);
    dlg.setWindowTitle("Session Network Telemetry");
    dlg.resize(500, 400);

    auto* layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(14);

    auto* titleLabel = new QLabel("<h3>📈 Session Network Telemetry</h3>", &dlg);
    layout->addWidget(titleLabel);

    auto* desc = new QLabel("Real-time network throughput and latency metrics accumulated during this application session:", &dlg);
    desc->setStyleSheet("color: #a1a1aa; font-size: 12px;");
    desc->setWordWrap(true);
    layout->addWidget(desc);

    auto* table = new QTableWidget(&dlg);
    table->setColumnCount(2);
    table->setHorizontalHeaderLabels({"Metric", "Value"});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->setColumnWidth(0, 230);
    table->verticalHeader()->setVisible(false);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    int successes = m_sessionReqCount - m_sessionErrorCount;
    double successRate = m_sessionReqCount > 0 ? (successes * 100.0 / m_sessionReqCount) : 100.0;
    qint64 avgLatency = m_sessionReqCount > 0 ? (m_sessionTotalLatencyMs / m_sessionReqCount) : 0;

    struct MetricRow { QString name; QString val; };
    QList<MetricRow> metrics = {
        {"Total Requests Executed", QString::number(m_sessionReqCount)},
        {"Successful Responses (2xx/3xx)", QString::number(successes)},
        {"Failed / Error Responses", QString::number(m_sessionErrorCount)},
        {"Session Success Rate", QString("%1%").arg(QString::number(successRate, 'f', 1))},
        {"Total Bandwidth Received", formatTelemetryBytes(m_sessionBytesReceived)},
        {"Average Request Latency", QString("%1 ms").arg(avgLatency)},
        {"Total Cumulative Latency", QString("%1 ms (%2 s)").arg(m_sessionTotalLatencyMs).arg(QString::number(m_sessionTotalLatencyMs / 1000.0, 'f', 2))}
    };

    table->setRowCount(metrics.size());
    for (int i = 0; i < metrics.size(); ++i) {
        auto* item0 = new QTableWidgetItem(metrics[i].name);
        auto* item1 = new QTableWidgetItem(metrics[i].val);
        QFont monoFont("Consolas", 10);
        item1->setFont(monoFont);
        table->setItem(i, 0, item0);
        table->setItem(i, 1, item1);
    }
    layout->addWidget(table, 1);

    auto* btnBox = new QHBoxLayout();
    auto* resetBtn = new QPushButton("Reset Session Stats", &dlg);
    resetBtn->setToolTip("Clear session counters back to 0");
    connect(resetBtn, &QPushButton::clicked, this, [this, &dlg]() {
        m_sessionReqCount = 0;
        m_sessionBytesReceived = 0;
        m_sessionTotalLatencyMs = 0;
        m_sessionErrorCount = 0;
        updateSessionTelemetryWidget();
        dlg.accept();
        statusBar()->showMessage("Session telemetry statistics reset.", 3000);
    });
    btnBox->addWidget(resetBtn);
    btnBox->addStretch();

    auto* closeBtn = new QPushButton("Close", &dlg);
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnBox->addWidget(closeBtn);

    layout->addLayout(btnBox);
    dlg.exec();
}

void MainWindow::onExportMarkdown() {
    auto requests = m_collectionModel.allRequests();
    if (requests.isEmpty()) {
        saveUiIntoRequest(m_currentRequest);
        requests.append(m_currentRequest);
    }

    QString defaultName = m_collectionModel.name().isEmpty() ? "API_RUNBOOK.md" : (m_collectionModel.name().toLower().replace(' ', '_') + "_runbook.md");
    QString savePath = QFileDialog::getSaveFileName(this, "Export Collection as Markdown Runbook", defaultName, "Markdown (*.md);;All Files (*.*)");
    if (savePath.isEmpty()) return;

    QString error;
    QString colName = m_collectionModel.name().isEmpty() ? "Poppy Collection" : m_collectionModel.name();
    if (core::MarkdownExporter::exportToFile(savePath, requests, colName, &error)) {
        QMessageBox::information(this, "Export Succeeded", QString("Collection exported as Markdown Runbook successfully to:\n%1").arg(savePath));
    } else {
        QMessageBox::warning(this, "Export Failed", QString("Could not export collection:\n%1").arg(error));
    }
}

void MainWindow::onFindAndReplace() {
    saveUiIntoRequest(m_currentRequest);
    if (m_currentTabIndex >= 0 && m_currentTabIndex < m_openTabs.size()) {
        auto& tab = m_openTabs[m_currentTabIndex];
        tab.request = m_currentRequest;
        if (tab.item) tab.isDirty = true;
    }
    for (int i = 0; i < m_openTabs.size(); ++i) {
        flushDirtyTab(i);
    }
    FindReplaceDialog dlg(&m_collectionModel, this);
    connect(&dlg, &FindReplaceDialog::requestSelected, this, [this](core::CollectionItem* item) {
        onRequestSelected(item);
    });
    connect(&dlg, &FindReplaceDialog::collectionModified, this, [this]() {
        m_sidebar->refreshTree();
        for (int i = 0; i < m_openTabs.size(); ++i) {
            auto& tab = m_openTabs[i];
            if (!tab.item) {
                tab.item = m_collectionModel.findItemByPath(tab.itemPath);
            }
            if (tab.item && tab.item->request()) {
                tab.request = *tab.item->request();
                tab.isDirty = false;
                tab.hasResponse = false;
                tab.lastReport = {};
                updateTabTitle(i);
            }
        }
        if (m_currentTabIndex >= 0 && m_currentTabIndex < m_openTabs.size()) {
            m_activeItem = m_openTabs[m_currentTabIndex].item;
            loadRequestIntoUi(m_openTabs[m_currentTabIndex].request);
        }
    });

    if (m_urlEdit && !m_urlEdit->selectedText().isEmpty()) {
        dlg.setInitialFindText(m_urlEdit->selectedText());
    }

    dlg.exec();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (m_currentTabIndex >= 0 && m_currentTabIndex < m_openTabs.size()) {
        saveUiIntoRequest(m_openTabs[m_currentTabIndex].request);
        m_currentRequest = m_openTabs[m_currentTabIndex].request;
    }

    QStringList dirtyNames;
    for (const auto& tab : m_openTabs) {
        if (tab.isDirty) {
            dirtyNames.append(tab.request.name.isEmpty() ? QString("Untitled") : tab.request.name);
        }
    }

    if (!dirtyNames.isEmpty()) {
        auto res = QMessageBox::question(this, "Unsaved Changes",
            QString("You have unsaved changes in:\n%1\n\nSave before quitting?").arg(dirtyNames.join("\n")),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (res == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
        if (res == QMessageBox::Save) {
            bool allSaved = true;
            for (auto& tab : m_openTabs) {
                if (!tab.isDirty) continue;
                bool saved = false;
                if (tab.item && tab.item->request()) {
                    *tab.item->request() = tab.request;
                    saved = m_collectionModel.saveRequest(tab.item);
                } else if (m_collectionModel.rootItem()) {
                    saved = m_collectionModel.addRequest(m_collectionModel.rootItem(), tab.request.name, tab.request) != nullptr;
                } else {
                    saved = true;
                }
                if (!saved) allSaved = false;
            }
            if (!allSaved) {
                QMessageBox::warning(this, "Save Failed",
                    "One or more requests could not be written to disk.");
                event->ignore();
                return;
            }
        }
    }

    saveAppState();
    m_networkEngine.cancelAll();
    event->accept();
}

void MainWindow::saveAppState() {
    QSettings settings;
    settings.setValue("window/geometry", saveGeometry());
    settings.setValue("window/state", saveState());
    if (m_contentSplitter) {
        settings.setValue("window/contentSplitter", m_contentSplitter->saveState());
        settings.setValue("window/contentOrientation", static_cast<int>(m_contentSplitter->orientation()));
    }
    if (m_mainSplitter) {
        settings.setValue("window/mainSplitter", m_mainSplitter->saveState());
    }

    if (!m_collectionModel.rootPath().isEmpty()) {
        settings.setValue("collection/lastPath", m_collectionModel.rootPath());
        settings.setValue("collection/lastActiveEnvironment", m_activeEnvName);

        QStringList openTabPaths;
        for (const auto& tab : m_openTabs) {
            if (!tab.itemPath.isEmpty()) {
                openTabPaths.append(tab.itemPath);
            }
        }
        settings.setValue("collection/openTabPaths", openTabPaths);
        settings.setValue("collection/currentTabIndex", m_currentTabIndex);
    }
}

void MainWindow::restoreAppState() {
    QSettings settings;
    if (settings.contains("window/geometry")) {
        restoreGeometry(settings.value("window/geometry").toByteArray());
    }
    if (settings.contains("window/state")) {
        restoreState(settings.value("window/state").toByteArray());
    }
    if (m_mainSplitter && settings.contains("window/mainSplitter")) {
        m_mainSplitter->restoreState(settings.value("window/mainSplitter").toByteArray());
    }
    if (m_contentSplitter && settings.contains("window/contentSplitter")) {
        int orientation = settings.value("window/contentOrientation", static_cast<int>(Qt::Vertical)).toInt();
        m_contentSplitter->setOrientation(static_cast<Qt::Orientation>(orientation));
        m_contentSplitter->restoreState(settings.value("window/contentSplitter").toByteArray());
    }

    updateRecentCollectionsMenu();

    QString lastPath = settings.value("collection/lastPath").toString();
    if (!lastPath.isEmpty() && QDir(lastPath).exists()) {
        if (openPath(lastPath, false)) {
            QString lastEnv = settings.value("collection/lastActiveEnvironment").toString();
            if (!lastEnv.isEmpty()) {
                onEnvironmentChanged(lastEnv);
            }

            QStringList openTabPaths = settings.value("collection/openTabPaths").toStringList();
            for (const QString& tabPath : openTabPaths) {
                auto* item = m_collectionModel.findItemByPath(tabPath);
                if (item) {
                    onRequestSelected(item);
                }
            }

            int savedIndex = settings.value("collection/currentTabIndex", -1).toInt();
            if (savedIndex >= 0 && savedIndex < m_openTabs.size() && m_openRequestsTabBar) {
                m_openRequestsTabBar->setCurrentIndex(savedIndex);
            }
        }
    }
}

void MainWindow::onCloseCollection() {
    if (m_collectionModel.rootPath().isEmpty()) return;

    // Check unsaved changes
    bool hasDirty = false;
    for (const auto& tab : m_openTabs) {
        if (tab.isDirty) { hasDirty = true; break; }
    }
    if (hasDirty) {
        auto res = QMessageBox::question(this, "Unsaved Changes",
            "You have unsaved changes in the collection. Save before closing?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (res == QMessageBox::Cancel) return;
        if (res == QMessageBox::Save) onSaveRequest();
    }

    // Reset open tabs
    while (m_openRequestsTabBar->count() > 0) {
        m_openRequestsTabBar->removeTab(0);
    }
    m_openTabs.clear();

    m_currentRequest = core::RequestModel{};
    m_currentRequest.name = "Quick Request";
    m_currentRequest.method = core::HttpMethod::GET;
    m_currentRequest.url = "https://httpbin.org/get";
    OpenTabInfo initTab;
    initTab.tabId = ++m_nextTabId;
    initTab.request = m_currentRequest;
    m_openTabs.append(initTab);
    m_openRequestsTabBar->addTab("Quick Request");
    m_currentTabIndex = 0;
    m_activeItem = nullptr;
    loadRequestIntoUi(m_currentRequest);

    m_collectionModel.closeCollection();
    m_activeEnvName.clear();
    m_sidebar->refreshTree();
    updateTopEnvCombo();
    updateUrlVariableInspection();
    setWindowTitle("Poppy - Native API Client");

    QSettings settings;
    settings.remove("collection/lastPath");
    settings.remove("collection/lastActiveEnvironment");
    settings.remove("collection/openTabPaths");
    settings.remove("collection/currentTabIndex");

    statusBar()->showMessage("Collection closed.", 3000);
}

void MainWindow::updateRecentCollectionsMenu() {
    if (!m_recentCollectionsMenu) return;
    m_recentCollectionsMenu->clear();

    QSettings settings;
    QStringList recent = settings.value("collection/recentPaths").toStringList();
    QStringList validRecent;

    for (const QString& path : recent) {
        if (QDir(path).exists()) {
            validRecent.append(path);
            QString name = QDir(path).dirName();
            auto* act = m_recentCollectionsMenu->addAction(QString("%1 (%2)").arg(name, path));
            connect(act, &QAction::triggered, this, [this, path]() {
                openPath(path);
            });
        }
    }

    if (validRecent.size() != recent.size()) {
        settings.setValue("collection/recentPaths", validRecent);
    }

    if (validRecent.isEmpty()) {
        auto* emptyAct = m_recentCollectionsMenu->addAction("No Recent Collections");
        emptyAct->setEnabled(false);
    } else {
        m_recentCollectionsMenu->addSeparator();
        auto* clearAct = m_recentCollectionsMenu->addAction("Clear Recent Collections");
        connect(clearAct, &QAction::triggered, this, &MainWindow::clearRecentCollections);
    }
}

void MainWindow::clearRecentCollections() {
    QSettings settings;
    settings.remove("collection/recentPaths");
    updateRecentCollectionsMenu();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    const auto urls = event->mimeData()->urls();
    if (!urls.isEmpty()) {
        QString localPath = urls.first().toLocalFile();
        if (!localPath.isEmpty()) {
            openPath(localPath);
            event->acceptProposedAction();
        }
    }
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_urlEdit) {
        if (event->type() == QEvent::ToolTip) {
            auto* helpEvent = static_cast<QHelpEvent*>(event);
            const QString text = m_urlEdit->text();
            static const QRegularExpression varRegex(R"(\{\{([^}]+)\}\})");
            auto matchIter = varRegex.globalMatch(text);
            if (!matchIter.hasNext()) {
                return false;
            }

            int cursorPos = m_urlEdit->cursorPositionAt(helpEvent->pos());
            QString targetVar;
            while (matchIter.hasNext()) {
                auto match = matchIter.next();
                if (cursorPos >= (match.capturedStart() - 1) && cursorPos <= (match.capturedEnd() + 1)) {
                    targetVar = match.captured(1).trimmed();
                    break;
                }
            }

            if (!targetVar.isEmpty() && m_variableHoverPopup) {
                core::VariableResolver resolver = currentVariableResolver();
                m_variableHoverPopup->showForVariable(targetVar, helpEvent->globalPos(), &resolver, &m_collectionModel, m_activeEnvName);
                return true; // Suppress default static tooltip so user can interact with hover card
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                int cursorPos = m_urlEdit->cursorPositionAt(mouseEvent->pos());
                static const QRegularExpression varRegex(R"(\{\{([^}]+)\}\})");
                auto matchIter = varRegex.globalMatch(m_urlEdit->text());
                while (matchIter.hasNext()) {
                    auto match = matchIter.next();
                    if (cursorPos >= (match.capturedStart() - 1) && cursorPos <= (match.capturedEnd() + 1)) {
                        QString targetVar = match.captured(1).trimmed();
                        if (m_variableHoverPopup) {
                            core::VariableResolver resolver = currentVariableResolver();
                            m_variableHoverPopup->showForVariable(targetVar, mouseEvent->globalPosition().toPoint(), &resolver, &m_collectionModel, m_activeEnvName);
                        }
                        break;
                    }
                }
            }
        } else if (event->type() == QEvent::Leave) {
            if (m_variableHoverPopup) {
                m_variableHoverPopup->scheduleHide(350);
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::onVariableSaved(const QString& name, const QString& value, const QString& scope, bool isSecret) {
    if (name.isEmpty()) return;

    if (scope == "env") {
        if (m_activeEnvName.isEmpty()) {
            if (!m_collectionModel.environments().isEmpty()) {
                m_activeEnvName = m_collectionModel.environments().first().name();
            } else {
                core::EnvironmentModel devEnv("dev");
                m_collectionModel.environments().append(devEnv);
                m_activeEnvName = "dev";
            }
        }
        auto* env = mutableActiveEnvironment();
        if (env) {
            env->addOrUpdateVariable(name, value, isSecret, true);
            persistActiveEnvironment();
        }
    } else if (scope == "collection") {
        if (m_collectionModel.rootItem()) {
            m_collectionModel.rootItem()->setVariable(name, value);
            if (!m_collectionModel.rootPath().isEmpty()) {
                m_collectionModel.saveFolderVariables(m_collectionModel.rootItem());
            }
        } else {
            m_sessionGlobals[name] = value;
        }
    } else { // "global"
        m_sessionGlobals[name] = value;
    }

    refreshEnvironmentUi();
    updateUrlVariableInspection();
    updateRequestTabBadges();
    statusBar()->showMessage(QString("Variable {{%1}} saved successfully!").arg(name), 3000);
}

} // namespace poppy::gui
