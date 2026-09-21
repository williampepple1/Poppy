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
#include <Theme.h>
#include "dialogs/EnvironmentDialog.h"
#include "dialogs/CodeSnippetDialog.h"
#include "dialogs/ImportDialog.h"
#include "dialogs/CollectionRunnerDialog.h"
#include "dialogs/SettingsDialog.h"
#include "dialogs/QuickOpenDialog.h"
#include "editors/AssertionsEditor.h"
#include <core/assertions/DeclarativeAssertion.h>
#include <core/exporters/OpenApiExporter.h>
#include <QTabBar>

namespace poppy::gui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Poppy - Native API Client");
    setWindowIcon(QIcon(":/icons/app_icon.png"));
    resize(1200, 750);
    setMinimumSize(800, 500);

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
    m_sidebar = new CollectionSidebar(&m_collectionModel, this);
    connect(m_sidebar, &CollectionSidebar::requestSelected, this, &MainWindow::onRequestSelected);
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
    connect(m_openRequestsTabBar, &QTabBar::currentChanged, this, &MainWindow::onTabChanged);
    connect(m_openRequestsTabBar, &QTabBar::tabCloseRequested, this, &MainWindow::onTabCloseRequested);
    reqLayout->addWidget(m_openRequestsTabBar);

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
    m_urlEdit->setPlaceholderText("Enter request URL or {{baseUrl}}/path...");
    connect(m_urlEdit, &QLineEdit::textChanged, this, &MainWindow::markCurrentTabDirty);
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

    onMethodChanged(0);
}

void MainWindow::setupMenus() {
    auto* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("&New Request", QKeySequence::New, this, &MainWindow::onNewRequest);
    fileMenu->addAction("&Quick Open...", QKeySequence(Qt::CTRL | Qt::Key_P), this, &MainWindow::onQuickOpen);
    fileMenu->addAction("&Open Collection...", QKeySequence::Open, this, &MainWindow::onOpenCollection);
    fileMenu->addAction("&Import...", this, &MainWindow::onImport);
    fileMenu->addAction("&Export Collection as OpenAPI 3.0...", this, &MainWindow::onExportOpenApi);
    fileMenu->addAction("&Run Collection...", this, &MainWindow::onRunCollection);
    fileMenu->addAction("&Save Request", QKeySequence::Save, this, &MainWindow::onSaveRequest);
    fileMenu->addAction("&Close Tab", QKeySequence::Close, this, &MainWindow::onCloseCurrentTab);
    fileMenu->addSeparator();
    fileMenu->addAction("&Settings...", QKeySequence::Preferences, this, &MainWindow::onOpenSettings);
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", this, &QWidget::close);

    auto* envMenu = menuBar()->addMenu("&Environments");
    envMenu->addAction("&Manage Environments...", QKeySequence(Qt::CTRL | Qt::Key_E), this, &MainWindow::onManageEnvironments);

    auto* helpMenu = menuBar()->addMenu("&Help");
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

    m_responseInspector->clear();
}

void MainWindow::saveUiIntoRequest(core::RequestModel& req) {
    req.name = m_requestNameLabel->text();
    req.method = static_cast<core::HttpMethod>(m_methodCombo->currentData().toInt());
    req.url = m_urlEdit->text().trimmed();

    m_paramsEditor->saveToRequest(req);
    m_headersEditor->saveToRequest(req);
    m_bodyEditor->saveToRequest(req);
    m_authEditor->saveToRequest(req);
    m_assertionsEditor->saveToRequest(req);
    m_scriptEditor->saveToRequest(req);
}

void MainWindow::markCurrentTabDirty() {
    if (m_currentTabIndex >= 0 && m_currentTabIndex < m_openTabs.size()) {
        if (!m_openTabs[m_currentTabIndex].isDirty) {
            m_openTabs[m_currentTabIndex].isDirty = true;
            QString title = m_openTabs[m_currentTabIndex].request.name.isEmpty() ? "Untitled" : m_openTabs[m_currentTabIndex].request.name;
            m_openRequestsTabBar->setTabText(m_currentTabIndex, "* " + title);
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
    if (m_openTabs.size() == 1 && m_openTabs[0].item == nullptr && !m_openTabs[0].isDirty) {
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

void MainWindow::onOpenCollection() {
    QString dir = QFileDialog::getExistingDirectory(this, "Open Collection Directory", QString());
    if (!dir.isEmpty()) {
        if (m_collectionModel.openDirectory(dir)) {
            m_sidebar->refreshTree();
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
    statusBar()->showMessage(envName.isEmpty() ? "No environment active." : ("Active environment: " + envName), 3000);
}

void MainWindow::onShowCodeSnippets() {
    saveUiIntoRequest(m_currentRequest);
    core::VariableResolver resolver;
    core::EnvironmentModel activeEnv(m_activeEnvName);
    for (const auto& env : m_collectionModel.environments()) {
        if (env.name() == m_activeEnvName) {
            activeEnv = env;
            break;
        }
    }
    resolver.setEnvironment(activeEnv);
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
    core::VariableResolver resolver;
    resolver.setEnvironment(activeEnv);
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
    });
}

void MainWindow::onOpenSettings() {
    SettingsDialog dlg(&m_networkEngine, this);
    dlg.exec();
}

} // namespace poppy::gui
