#include "GrpcDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QFile>
#include <QMessageBox>
#include <QHeaderView>
#include <QTabWidget>

namespace poppy::gui {

static const char* kDefaultProto = R"(syntax = "proto3";

package helloworld;

service Greeter {
  rpc SayHello (HelloRequest) returns (HelloReply);
  rpc SayHelloAgain (HelloRequest) returns (HelloReply);
}

message HelloRequest {
  string name = 1;
}

message HelloReply {
  string message = 1;
}
)";

GrpcDialog::GrpcDialog(QWidget* parent)
    : QDialog(parent)
    , m_client(new network::GrpcClient(this))
{
    setWindowTitle("gRPC Client - Poppy");
    resize(1000, 680);

    setupUi();

    connect(m_client, &network::GrpcClient::callStarted, this, &GrpcDialog::onCallStarted);
    connect(m_client, &network::GrpcClient::callFinished, this, &GrpcDialog::onCallFinished);

    // Pre-populate with default sample proto
    m_protoViewer->setPlainText(kDefaultProto);
    m_protoDef = network::GrpcClient::parseProto(kDefaultProto);
    m_serviceCombo->clear();
    for (const auto& s : m_protoDef.services) {
        m_serviceCombo->addItem(s.fullName, s.fullName);
    }
    updateMethodCombo();
}

void GrpcDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    // 1. Endpoint row
    auto* endpointRow = new QHBoxLayout();
    endpointRow->setSpacing(8);

    auto* epLabel = new QLabel("Server / Host:", this);
    epLabel->setStyleSheet("font-weight: bold;");
    endpointRow->addWidget(epLabel);

    m_endpointEdit = new QLineEdit(this);
    m_endpointEdit->setPlaceholderText("localhost:50051 or grpc.example.com:443");
    m_endpointEdit->setText("localhost:50051");
    endpointRow->addWidget(m_endpointEdit, 1);

    m_tlsCheck = new QCheckBox("Use TLS (HTTPS)", this);
    m_tlsCheck->setChecked(false);
    endpointRow->addWidget(m_tlsCheck);

    m_invokeBtn = new QPushButton("Invoke RPC", this);
    m_invokeBtn->setStyleSheet("background-color: #3b82f6; color: white; font-weight: bold; padding: 6px 16px; border-radius: 4px;");
    connect(m_invokeBtn, &QPushButton::clicked, this, &GrpcDialog::onInvokeClicked);
    endpointRow->addWidget(m_invokeBtn);

    mainLayout->addLayout(endpointRow);

    // 2. Service & Method row
    auto* rpcRow = new QHBoxLayout();
    rpcRow->setSpacing(8);

    m_loadProtoBtn = new QPushButton("Load .proto...", this);
    connect(m_loadProtoBtn, &QPushButton::clicked, this, &GrpcDialog::onLoadProtoClicked);
    rpcRow->addWidget(m_loadProtoBtn);

    rpcRow->addWidget(new QLabel("Service:", this));
    m_serviceCombo = new QComboBox(this);
    m_serviceCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    connect(m_serviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GrpcDialog::onServiceChanged);
    rpcRow->addWidget(m_serviceCombo);

    rpcRow->addWidget(new QLabel("Method:", this));
    m_methodCombo = new QComboBox(this);
    m_methodCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    connect(m_methodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GrpcDialog::onMethodChanged);
    rpcRow->addWidget(m_methodCombo, 1);

    m_examplePayloadBtn = new QPushButton("Example Payload", this);
    connect(m_examplePayloadBtn, &QPushButton::clicked, this, &GrpcDialog::onGenerateExampleClicked);
    rpcRow->addWidget(m_examplePayloadBtn);

    mainLayout->addLayout(rpcRow);

    // 3. Central Splitter (Left: Request tabs, Right: Response panel)
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // Left Tabs
    auto* reqTabs = new QTabWidget(this);

    // Tab: Payload
    m_requestPayloadEdit = new QPlainTextEdit(this);
    m_requestPayloadEdit->setPlaceholderText("{\n  \"name\": \"World\"\n}");
    m_requestPayloadEdit->setPlainText("{\n  \"name\": \"World\"\n}");
    reqTabs->addTab(m_requestPayloadEdit, "JSON Payload");

    // Tab: Metadata
    m_metadataTable = new KeyValueTable(false, this);
    reqTabs->addTab(m_metadataTable, "Metadata Headers");

    // Tab: Proto Schema
    m_protoViewer = new QPlainTextEdit(this);
    reqTabs->addTab(m_protoViewer, "Proto Definition");

    splitter->addWidget(reqTabs);

    // Right Response Panel
    auto* respWidget = new QWidget(this);
    auto* respLayout = new QVBoxLayout(respWidget);
    respLayout->setContentsMargins(0, 0, 0, 0);
    respLayout->setSpacing(6);

    auto* teleBar = new QHBoxLayout();
    teleBar->setSpacing(8);

    m_statusBadge = new QLabel("STATUS: ---", this);
    m_statusBadge->setStyleSheet("background-color: #27272a; color: #a1a1aa; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px;");
    teleBar->addWidget(m_statusBadge);

    m_latencyBadge = new QLabel("TIME: ---", this);
    m_latencyBadge->setStyleSheet("background-color: #27272a; color: #a1a1aa; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px;");
    teleBar->addWidget(m_latencyBadge);

    teleBar->addStretch();
    respLayout->addLayout(teleBar);

    auto* respTabs = new QTabWidget(this);

    m_responseViewer = new QPlainTextEdit(this);
    m_responseViewer->setReadOnly(true);
    m_responseViewer->setPlaceholderText("gRPC response will appear here after invocation...");
    respTabs->addTab(m_responseViewer, "Response Message");

    m_responseHeadersTable = new QTableWidget(this);
    m_responseHeadersTable->setColumnCount(2);
    m_responseHeadersTable->setHorizontalHeaderLabels({"Header / Trailer", "Value"});
    m_responseHeadersTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_responseHeadersTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_responseHeadersTable->verticalHeader()->setVisible(false);
    respTabs->addTab(m_responseHeadersTable, "Headers & Trailers");

    respLayout->addWidget(respTabs, 1);
    splitter->addWidget(respWidget);

    splitter->setSizes({500, 480});
    mainLayout->addWidget(splitter, 1);
}

void GrpcDialog::onLoadProtoClicked() {
    QString path = QFileDialog::getOpenFileName(this, "Open Protocol Buffer (.proto)", QString(), "Proto Files (*.proto);;All Files (*.*)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Could not open proto file: " + file.errorString());
        return;
    }

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    m_protoViewer->setPlainText(content);
    m_protoDef = network::GrpcClient::parseProto(content);

    m_serviceCombo->clear();
    for (const auto& s : m_protoDef.services) {
        m_serviceCombo->addItem(s.fullName, s.fullName);
    }
    updateMethodCombo();
}

void GrpcDialog::onServiceChanged(int index) {
    Q_UNUSED(index);
    updateMethodCombo();
}

void GrpcDialog::updateMethodCombo() {
    m_methodCombo->clear();
    int sIdx = m_serviceCombo->currentIndex();
    if (sIdx >= 0 && sIdx < m_protoDef.services.size()) {
        const auto& srv = m_protoDef.services[sIdx];
        for (const auto& m : srv.methods) {
            QString label = QString("%1 (%2 -> %3)").arg(m.name, m.inputType, m.outputType);
            m_methodCombo->addItem(label, m.name);
        }
    }
}

void GrpcDialog::onMethodChanged(int index) {
    Q_UNUSED(index);
}

void GrpcDialog::onGenerateExampleClicked() {
    int sIdx = m_serviceCombo->currentIndex();
    int mIdx = m_methodCombo->currentIndex();
    if (sIdx >= 0 && sIdx < m_protoDef.services.size()) {
        const auto& srv = m_protoDef.services[sIdx];
        if (mIdx >= 0 && mIdx < srv.methods.size()) {
            const auto& m = srv.methods[mIdx];
            QString json = network::GrpcClient::generateSampleJsonForMessage(m.inputType, m_protoDef);
            m_requestPayloadEdit->setPlainText(json);
        }
    }
}

void GrpcDialog::updateStatusBadge(int code, const QString& name) {
    QString color = (code == 0) ? "#10b981" : "#ef4444";
    m_statusBadge->setText(QString("STATUS: %1 (%2)").arg(code).arg(name));
    m_statusBadge->setStyleSheet(QString(
        "background-color: %1; color: #ffffff; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px;"
    ).arg(color));
}

void GrpcDialog::onInvokeClicked() {
    QString endpoint = m_endpointEdit->text().trimmed();
    if (endpoint.isEmpty()) {
        QMessageBox::warning(this, "Empty Endpoint", "Please specify a gRPC server endpoint.");
        return;
    }

    int sIdx = m_serviceCombo->currentIndex();
    int mIdx = m_methodCombo->currentIndex();
    if (sIdx < 0 || sIdx >= m_protoDef.services.size() || mIdx < 0) {
        QMessageBox::warning(this, "Invalid Method", "Please select a valid gRPC service and method.");
        return;
    }

    const auto& srv = m_protoDef.services[sIdx];
    const auto& m = srv.methods[mIdx];

    QString fullMethodPath = QString("/%1/%2").arg(srv.fullName, m.name);
    QString payload = m_requestPayloadEdit->toPlainText();

    QMap<QString, QString> metadata;
    for (const auto& h : m_metadataTable->headers()) {
        if (h.enabled && !h.name.isEmpty()) {
            metadata[h.name] = h.value;
        }
    }

    bool useTls = m_tlsCheck->isChecked();

    m_client->invokeUnary(endpoint, fullMethodPath, payload, metadata, useTls);
}

void GrpcDialog::onCallStarted() {
    m_invokeBtn->setEnabled(false);
    m_invokeBtn->setText("Invoking...");
    m_statusBadge->setText("CALLING...");
    m_statusBadge->setStyleSheet("background-color: #f59e0b; color: #ffffff; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px;");
    m_latencyBadge->setText("TIME: ---");
}

void GrpcDialog::onCallFinished(const network::GrpcResponse& res) {
    m_invokeBtn->setEnabled(true);
    m_invokeBtn->setText("Invoke RPC");

    updateStatusBadge(res.statusCode, res.statusName);
    m_latencyBadge->setText(QString("TIME: %1 ms").arg(res.latencyMs));

    if (!res.errorMessage.isEmpty()) {
        m_responseViewer->setPlainText(QString("ERROR:\n%1\n\n%2").arg(res.errorMessage, res.responseBody));
    } else {
        m_responseViewer->setPlainText(res.responseBody);
    }

    m_responseHeadersTable->setRowCount(0);
    for (auto it = res.responseHeaders.cbegin(); it != res.responseHeaders.cend(); ++it) {
        int r = m_responseHeadersTable->rowCount();
        m_responseHeadersTable->insertRow(r);
        m_responseHeadersTable->setItem(r, 0, new QTableWidgetItem(it.key()));
        m_responseHeadersTable->setItem(r, 1, new QTableWidgetItem(it.value()));
    }
}

} // namespace poppy::gui
