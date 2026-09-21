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
#include "editors/AssertionsEditor.h"
#include <core/assertions/DeclarativeAssertion.h>

namespace poppy::gui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Poppy - Native API Client");
    resize(1200, 750);
    setMinimumSize(800, 500);

    setupUi();
    setupMenus();

    // Set default initial request
    m_currentRequest.name = "Quick Request";
    m_currentRequest.method = core::HttpMethod::GET;
    m_currentRequest.url = "https://httpbin.org/get";
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
    reqLayout->setSpacing(10);

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
    urlBarLayout->addWidget(m_methodCombo);

    m_urlEdit = new QLineEdit(this);
    m_urlEdit->setPlaceholderText("Enter request URL or {{baseUrl}}/path...");
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
    fileMenu->addAction("&Open Collection...", this, &MainWindow::onOpenCollection, QKeySequence::Open);
    fileMenu->addAction("&Import...", this, &MainWindow::onImport);
    fileMenu->addAction("&Run Collection...", this, &MainWindow::onRunCollection);
    fileMenu->addAction("&Save Request", this, &MainWindow::onSaveRequest, QKeySequence::Save);
    fileMenu->addSeparator();
    fileMenu->addAction("&Settings...", this, &MainWindow::onOpenSettings, QKeySequence::Preferences);
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", this, &QWidget::close);

    auto* envMenu = menuBar()->addMenu("&Environments");
    envMenu->addAction("&Manage Environments...", this, &MainWindow::onManageEnvironments, QKeySequence(Qt::CTRL | Qt::Key_E));

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

void MainWindow::onRequestSelected(core::CollectionItem* item) {
    if (m_activeItem && m_activeItem->request()) {
        saveUiIntoRequest(*m_activeItem->request());
        m_collectionModel.saveRequest(m_activeItem);
    }

    m_activeItem = item;
    if (item && item->request()) {
        loadRequestIntoUi(*item->request());
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
        statusBar()->showMessage("No collection item active to save.", 3000);
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
