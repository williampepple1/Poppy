#include "MockServerDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QTabWidget>

namespace poppy::gui {

MockServerDialog::MockServerDialog(core::CollectionModel* collectionModel, QWidget* parent)
    : QDialog(parent)
    , m_server(new network::MockServer(this))
    , m_collectionModel(collectionModel)
{
    setWindowTitle("Embedded Mock Server - Poppy");
    resize(980, 650);

    setupUi();

    connect(m_server, &network::MockServer::serverStarted, this, &MockServerDialog::updateServerStatus);
    connect(m_server, &network::MockServer::serverStopped, this, &MockServerDialog::updateServerStatus);
    connect(m_server, &network::MockServer::requestReceived, this, &MockServerDialog::onRequestReceived);

    // Add initial default mock route
    network::MockRoute defaultRoute;
    defaultRoute.method = "GET";
    defaultRoute.path = "/api/hello";
    defaultRoute.statusCode = 200;
    defaultRoute.contentType = "application/json";
    defaultRoute.responseBody = "{\n  \"message\": \"Hello from Poppy Mock Server!\",\n  \"status\": \"ok\"\n}";
    m_server->addRoute(defaultRoute);

    refreshRoutesTable();
    updateServerStatus();
}

MockServerDialog::~MockServerDialog() {
    if (m_server) {
        m_server->stop();
    }
}

void MockServerDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    // 1. Server Control Bar
    auto* ctrlRow = new QHBoxLayout();
    ctrlRow->setSpacing(10);

    ctrlRow->addWidget(new QLabel("Port:", this));
    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1024, 65535);
    m_portSpin->setValue(8080);
    ctrlRow->addWidget(m_portSpin);

    m_toggleServerBtn = new QPushButton("Start Server", this);
    m_toggleServerBtn->setStyleSheet("background-color: #10b981; color: white; font-weight: bold; padding: 6px 16px; border-radius: 4px;");
    connect(m_toggleServerBtn, &QPushButton::clicked, this, &MockServerDialog::onToggleServer);
    ctrlRow->addWidget(m_toggleServerBtn);

    m_statusBadge = new QLabel(this);
    ctrlRow->addWidget(m_statusBadge);

    ctrlRow->addStretch();

    m_importCollectionBtn = new QPushButton("Import from Collection", this);
    connect(m_importCollectionBtn, &QPushButton::clicked, this, &MockServerDialog::onImportCollection);
    ctrlRow->addWidget(m_importCollectionBtn);

    mainLayout->addLayout(ctrlRow);

    // 2. Main Tabs (Routes / Live Request Log)
    auto* mainTabs = new QTabWidget(this);

    // Tab 1: Mock Routes
    auto* routesWidget = new QWidget(this);
    auto* routesLayout = new QVBoxLayout(routesWidget);
    routesLayout->setContentsMargins(6, 6, 6, 6);

    auto* routesSplitter = new QSplitter(Qt::Horizontal, routesWidget);

    // Left pane: Routes Table + Add/Remove buttons
    auto* leftBox = new QWidget(routesSplitter);
    auto* lLayout = new QVBoxLayout(leftBox);
    lLayout->setContentsMargins(0, 0, 0, 0);

    auto* tblHeader = new QHBoxLayout();
    tblHeader->addWidget(new QLabel("<b>Configured Mock Endpoints</b>", leftBox));
    tblHeader->addStretch();

    auto* addBtn = new QPushButton("+ Add Route", leftBox);
    connect(addBtn, &QPushButton::clicked, this, &MockServerDialog::onAddRoute);
    tblHeader->addWidget(addBtn);

    auto* removeBtn = new QPushButton("Delete", leftBox);
    connect(removeBtn, &QPushButton::clicked, this, &MockServerDialog::onRemoveRoute);
    tblHeader->addWidget(removeBtn);

    lLayout->addLayout(tblHeader);

    m_routesTable = new QTableWidget(leftBox);
    m_routesTable->setColumnCount(4);
    m_routesTable->setHorizontalHeaderLabels({"Method", "Path", "Status", "Delay"});
    m_routesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_routesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_routesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_routesTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_routesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_routesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_routesTable->verticalHeader()->setVisible(false);
    connect(m_routesTable, &QTableWidget::itemSelectionChanged, this, &MockServerDialog::onRouteSelected);
    lLayout->addWidget(m_routesTable);

    routesSplitter->addWidget(leftBox);

    // Right pane: Route Editor
    auto* rightBox = new QWidget(routesSplitter);
    auto* rLayout = new QVBoxLayout(rightBox);
    rLayout->setContentsMargins(6, 0, 0, 0);

    auto* form = new QFormLayout();
    m_methodCombo = new QComboBox(rightBox);
    m_methodCombo->addItems({"GET", "POST", "PUT", "DELETE", "PATCH", "*"});
    connect(m_methodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MockServerDialog::onRouteDataChanged);
    form->addRow("HTTP Method:", m_methodCombo);

    m_pathEdit = new QLineEdit(rightBox);
    connect(m_pathEdit, &QLineEdit::textChanged, this, &MockServerDialog::onRouteDataChanged);
    form->addRow("Endpoint Path:", m_pathEdit);

    m_statusCodeSpin = new QSpinBox(rightBox);
    m_statusCodeSpin->setRange(100, 599);
    m_statusCodeSpin->setValue(200);
    connect(m_statusCodeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MockServerDialog::onRouteDataChanged);
    form->addRow("Status Code:", m_statusCodeSpin);

    m_delaySpin = new QSpinBox(rightBox);
    m_delaySpin->setRange(0, 10000);
    m_delaySpin->setSuffix(" ms");
    connect(m_delaySpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MockServerDialog::onRouteDataChanged);
    form->addRow("Simulated Delay:", m_delaySpin);

    m_contentTypeEdit = new QLineEdit(rightBox);
    connect(m_contentTypeEdit, &QLineEdit::textChanged, this, &MockServerDialog::onRouteDataChanged);
    form->addRow("Content-Type:", m_contentTypeEdit);

    rLayout->addLayout(form);

    rLayout->addWidget(new QLabel("<b>Mock Response Body:</b>", rightBox));
    m_responseBodyEdit = new QPlainTextEdit(rightBox);
    connect(m_responseBodyEdit, &QPlainTextEdit::textChanged, this, &MockServerDialog::onRouteDataChanged);
    rLayout->addWidget(m_responseBodyEdit, 1);

    routesSplitter->addWidget(rightBox);
    routesSplitter->setSizes({460, 480});
    routesLayout->addWidget(routesSplitter);

    mainTabs->addTab(routesWidget, "Mock Routes");

    // Tab 2: Live Request Log
    auto* logsWidget = new QWidget(this);
    auto* logsLayout = new QVBoxLayout(logsWidget);
    logsLayout->setContentsMargins(6, 6, 6, 6);

    auto* logHeader = new QHBoxLayout();
    logHeader->addWidget(new QLabel("<b>Incoming Request History</b>", logsWidget));
    logHeader->addStretch();
    auto* clearLogBtn = new QPushButton("Clear History", logsWidget);
    connect(clearLogBtn, &QPushButton::clicked, this, &MockServerDialog::onClearLog);
    logHeader->addWidget(clearLogBtn);
    logsLayout->addLayout(logHeader);

    auto* logSplitter = new QSplitter(Qt::Horizontal, logsWidget);

    m_logsTable = new QTableWidget(logSplitter);
    m_logsTable->setColumnCount(4);
    m_logsTable->setHorizontalHeaderLabels({"Time", "Method", "Path", "Response"});
    m_logsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_logsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_logsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_logsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_logsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_logsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_logsTable->verticalHeader()->setVisible(false);
    connect(m_logsTable, &QTableWidget::itemSelectionChanged, this, &MockServerDialog::onLogSelected);
    logSplitter->addWidget(m_logsTable);

    m_logDetailViewer = new QPlainTextEdit(logSplitter);
    m_logDetailViewer->setReadOnly(true);
    m_logDetailViewer->setPlaceholderText("Select an incoming request to view headers and body...");
    logSplitter->addWidget(m_logDetailViewer);

    logSplitter->setSizes({500, 440});
    logsLayout->addWidget(logSplitter, 1);

    mainTabs->addTab(logsWidget, "Live Request Logs");

    mainLayout->addWidget(mainTabs, 1);
}

void MockServerDialog::updateServerStatus() {
    if (m_server->isRunning()) {
        m_statusBadge->setText(QString("RUNNING on %1").arg(m_server->serverUrl()));
        m_statusBadge->setStyleSheet("background-color: #10b981; color: white; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px;");
        m_toggleServerBtn->setText("Stop Server");
        m_toggleServerBtn->setStyleSheet("background-color: #ef4444; color: white; font-weight: bold; padding: 6px 16px; border-radius: 4px;");
        m_portSpin->setEnabled(false);
    } else {
        m_statusBadge->setText("STOPPED");
        m_statusBadge->setStyleSheet("background-color: #27272a; color: #a1a1aa; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px;");
        m_toggleServerBtn->setText("Start Server");
        m_toggleServerBtn->setStyleSheet("background-color: #10b981; color: white; font-weight: bold; padding: 6px 16px; border-radius: 4px;");
        m_portSpin->setEnabled(true);
    }
}

void MockServerDialog::onToggleServer() {
    if (m_server->isRunning()) {
        m_server->stop();
    } else {
        if (!m_server->start(static_cast<quint16>(m_portSpin->value()))) {
            QMessageBox::warning(this, "Server Error", "Could not start mock server on selected port.");
        }
    }
}

void MockServerDialog::onImportCollection() {
    if (!m_collectionModel || m_collectionModel->allRequests().isEmpty()) {
        QMessageBox::information(this, "Empty Collection", "No requests available to import in the active collection.");
        return;
    }

    m_server->importFromRequests(m_collectionModel->allRequests());
    refreshRoutesTable();
    QMessageBox::information(this, "Import Complete", QString("Imported %1 endpoints from the collection.").arg(m_collectionModel->allRequests().size()));
}

void MockServerDialog::refreshRoutesTable() {
    m_routesTable->setRowCount(0);
    const auto& routes = m_server->routes();
    for (int i = 0; i < routes.size(); ++i) {
        const auto& r = routes[i];
        m_routesTable->insertRow(i);

        auto* mItem = new QTableWidgetItem(r.method);
        mItem->setFont(QFont(mItem->font().family(), -1, QFont::Bold));
        if (r.method == "GET") mItem->setForeground(QColor("#10b981"));
        else if (r.method == "POST") mItem->setForeground(QColor("#3b82f6"));
        else if (r.method == "DELETE") mItem->setForeground(QColor("#ef4444"));
        m_routesTable->setItem(i, 0, mItem);

        m_routesTable->setItem(i, 1, new QTableWidgetItem(r.path));
        m_routesTable->setItem(i, 2, new QTableWidgetItem(QString::number(r.statusCode)));
        m_routesTable->setItem(i, 3, new QTableWidgetItem(r.delayMs > 0 ? QString("%1ms").arg(r.delayMs) : "0"));
    }

    if (m_selectedRouteIndex >= 0 && m_selectedRouteIndex < m_server->routes().size()) {
        m_routesTable->selectRow(m_selectedRouteIndex);
    }
}

void MockServerDialog::onAddRoute() {
    network::MockRoute r;
    r.method = "GET";
    r.path = "/api/new-route";
    r.statusCode = 200;
    r.contentType = "application/json";
    r.responseBody = "{\n  \"message\": \"Mock response\"\n}";
    m_server->addRoute(r);
    refreshRoutesTable();
    m_routesTable->selectRow(m_server->routes().size() - 1);
}

void MockServerDialog::onRemoveRoute() {
    int row = m_routesTable->currentRow();
    if (row >= 0 && row < m_server->routes().size()) {
        m_server->removeRoute(row);
        m_selectedRouteIndex = -1;
        refreshRoutesTable();
    }
}

void MockServerDialog::onRouteSelected() {
    int row = m_routesTable->currentRow();
    if (row < 0 || row >= m_server->routes().size()) return;

    m_selectedRouteIndex = row;
    const auto& r = m_server->routes()[row];

    m_updatingForm = true;
    int mIdx = m_methodCombo->findText(r.method);
    if (mIdx != -1) m_methodCombo->setCurrentIndex(mIdx);

    m_pathEdit->setText(r.path);
    m_statusCodeSpin->setValue(r.statusCode);
    m_delaySpin->setValue(r.delayMs);
    m_contentTypeEdit->setText(r.contentType);
    m_responseBodyEdit->setPlainText(r.responseBody);
    m_updatingForm = false;
}

void MockServerDialog::onRouteDataChanged() {
    if (m_updatingForm || m_selectedRouteIndex < 0 || m_selectedRouteIndex >= m_server->routes().size()) return;

    network::MockRoute r = m_server->routes()[m_selectedRouteIndex];
    r.method = m_methodCombo->currentText();
    r.path = m_pathEdit->text().trimmed();
    r.statusCode = m_statusCodeSpin->value();
    r.delayMs = m_delaySpin->value();
    r.contentType = m_contentTypeEdit->text().trimmed();
    r.responseBody = m_responseBodyEdit->toPlainText();

    m_server->updateRoute(m_selectedRouteIndex, r);

    // Update row items
    m_routesTable->item(m_selectedRouteIndex, 0)->setText(r.method);
    m_routesTable->item(m_selectedRouteIndex, 1)->setText(r.path);
    m_routesTable->item(m_selectedRouteIndex, 2)->setText(QString::number(r.statusCode));
    m_routesTable->item(m_selectedRouteIndex, 3)->setText(r.delayMs > 0 ? QString("%1ms").arg(r.delayMs) : "0");
}

void MockServerDialog::onRequestReceived(const network::MockRequestLog& log) {
    int row = m_logsTable->rowCount();
    m_logsTable->insertRow(row);

    m_logsTable->setItem(row, 0, new QTableWidgetItem(log.timestamp.toString("HH:mm:ss.zzz")));

    auto* mItem = new QTableWidgetItem(log.method);
    mItem->setFont(QFont(mItem->font().family(), -1, QFont::Bold));
    m_logsTable->setItem(row, 1, mItem);

    QString fullUri = log.path;
    if (!log.queryString.isEmpty()) fullUri += "?" + log.queryString;
    m_logsTable->setItem(row, 2, new QTableWidgetItem(fullUri));

    auto* respItem = new QTableWidgetItem(QString::number(log.responseCode));
    respItem->setForeground(log.responseCode < 400 ? QColor("#10b981") : QColor("#ef4444"));
    m_logsTable->setItem(row, 3, respItem);

    m_logsTable->scrollToBottom();
}

void MockServerDialog::onLogSelected() {
    int row = m_logsTable->currentRow();
    if (row >= 0 && row < m_server->logs().size()) {
        const auto& log = m_server->logs()[row];
        QString detail;
        detail += QString("=== Incoming %1 %2 ===\n").arg(log.method, log.path);
        detail += QString("Time: %1\n").arg(log.timestamp.toString(Qt::ISODate));
        detail += QString("Response Status: %1\n\n").arg(log.responseCode);
        detail += "--- Headers ---\n";
        for (auto it = log.headers.cbegin(); it != log.headers.cend(); ++it) {
            detail += QString("%1: %2\n").arg(it.key(), it.value());
        }
        detail += "\n--- Body ---\n";
        detail += log.body.isEmpty() ? "(Empty Body)" : log.body;
        m_logDetailViewer->setPlainText(detail);
    }
}

void MockServerDialog::onClearLog() {
    m_server->clearLogs();
    m_logsTable->setRowCount(0);
    m_logDetailViewer->clear();
}

} // namespace poppy::gui
