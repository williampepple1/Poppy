#include "WebSocketDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTabWidget>
#include <QMessageBox>

namespace poppy::gui {

WebSocketDialog::WebSocketDialog(QWidget* parent)
    : QDialog(parent)
    , m_client(new network::WebSocketClient(this))
{
    setWindowTitle("WebSocket Client - Poppy");
    resize(950, 650);

    setupUi();

    connect(m_client, &network::WebSocketClient::connected, this, &WebSocketDialog::onWsConnected);
    connect(m_client, &network::WebSocketClient::disconnected, this, &WebSocketDialog::onWsDisconnected);
    connect(m_client, &network::WebSocketClient::stateChanged, this, &WebSocketDialog::onWsStateChanged);
    connect(m_client, &network::WebSocketClient::textMessageReceived, this, &WebSocketDialog::onWsTextMessageReceived);
    connect(m_client, &network::WebSocketClient::binaryMessageReceived, this, &WebSocketDialog::onWsBinaryMessageReceived);
    connect(m_client, &network::WebSocketClient::frameSent, this, &WebSocketDialog::onWsFrameSent);
    connect(m_client, &network::WebSocketClient::frameReceived, this, &WebSocketDialog::onWsFrameReceived);
    connect(m_client, &network::WebSocketClient::errorOccurred, this, &WebSocketDialog::onErrorOccurred);

    updateStatusBadge("DISCONNECTED", "#71717a");
}

WebSocketDialog::~WebSocketDialog() {
    if (m_client) {
        m_client->close();
    }
}

void WebSocketDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    // 1. Connection Bar (URL + Connect/Disconnect + Status)
    auto* connLayout = new QHBoxLayout();
    connLayout->setSpacing(8);

    auto* urlLabel = new QLabel("URL:", this);
    urlLabel->setStyleSheet("font-weight: bold;");
    connLayout->addWidget(urlLabel);

    m_urlEdit = new QLineEdit(this);
    m_urlEdit->setPlaceholderText("ws://localhost:8080 or wss://echo.websocket.events");
    m_urlEdit->setText("wss://echo.websocket.events");
    connLayout->addWidget(m_urlEdit, 1);

    m_statusBadge = new QLabel(this);
    m_statusBadge->setStyleSheet("background-color: #27272a; color: #a1a1aa; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px;");
    connLayout->addWidget(m_statusBadge);

    m_connectBtn = new QPushButton("Connect", this);
    m_connectBtn->setStyleSheet("background-color: #10b981; color: white; font-weight: bold; padding: 6px 14px; border-radius: 4px;");
    connect(m_connectBtn, &QPushButton::clicked, this, &WebSocketDialog::onConnectClicked);
    connLayout->addWidget(m_connectBtn);

    mainLayout->addLayout(connLayout);

    // 2. Middle Splitter: Tabs (Handshake Headers / Send Payload) on Top/Left, Frames log on Right
    auto* splitter = new QSplitter(Qt::Vertical, this);

    auto* topTabs = new QTabWidget(this);

    // Send Tab
    auto* sendWidget = new QWidget(this);
    auto* sendLayout = new QVBoxLayout(sendWidget);
    sendLayout->setContentsMargins(6, 6, 6, 6);
    sendLayout->setSpacing(6);

    m_sendTextEdit = new QPlainTextEdit(this);
    m_sendTextEdit->setPlaceholderText("Enter text or JSON to send across the WebSocket connection...");
    m_sendTextEdit->setPlainText("{\n  \"action\": \"ping\",\n  \"timestamp\": 123456789\n}");
    sendLayout->addWidget(m_sendTextEdit);

    auto* sendBtnRow = new QHBoxLayout();
    sendBtnRow->addStretch();

    m_pingBtn = new QPushButton("Send Ping", this);
    m_pingBtn->setEnabled(false);
    connect(m_pingBtn, &QPushButton::clicked, this, &WebSocketDialog::onSendPingClicked);
    sendBtnRow->addWidget(m_pingBtn);

    m_sendBtn = new QPushButton("Send Message", this);
    m_sendBtn->setStyleSheet("background-color: #3b82f6; color: white; font-weight: bold; padding: 6px 16px; border-radius: 4px;");
    m_sendBtn->setEnabled(false);
    connect(m_sendBtn, &QPushButton::clicked, this, &WebSocketDialog::onSendClicked);
    sendBtnRow->addWidget(m_sendBtn);

    sendLayout->addLayout(sendBtnRow);
    topTabs->addTab(sendWidget, "Compose Message");

    // Handshake Headers Tab
    m_headersTable = new KeyValueTable(false, this);
    topTabs->addTab(m_headersTable, "Handshake Headers");

    splitter->addWidget(topTabs);

    // Messages Log Panel
    auto* logWidget = new QWidget(this);
    auto* logLayout = new QVBoxLayout(logWidget);
    logLayout->setContentsMargins(0, 0, 0, 0);
    logLayout->setSpacing(6);

    auto* logHeader = new QHBoxLayout();
    auto* logTitle = new QLabel("Messages & Frames Timeline", this);
    logTitle->setStyleSheet("font-weight: bold; font-size: 12px;");
    logHeader->addWidget(logTitle);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText("Filter messages...");
    connect(m_filterEdit, &QLineEdit::textChanged, this, [this](const QString& filter) {
        for (int row = 0; row < m_logTable->rowCount(); ++row) {
            bool matches = filter.isEmpty() ||
                           m_logTable->item(row, 2)->text().contains(filter, Qt::CaseInsensitive) ||
                           m_logTable->item(row, 3)->text().contains(filter, Qt::CaseInsensitive);
            m_logTable->setRowHidden(row, !matches);
        }
    });
    logHeader->addWidget(m_filterEdit, 1);

    m_autoScrollCheck = new QCheckBox("Auto-scroll", this);
    m_autoScrollCheck->setChecked(true);
    logHeader->addWidget(m_autoScrollCheck);

    auto* clearBtn = new QPushButton("Clear", this);
    connect(clearBtn, &QPushButton::clicked, this, &WebSocketDialog::onClearLogClicked);
    logHeader->addWidget(clearBtn);

    logLayout->addLayout(logHeader);

    auto* logSplitter = new QSplitter(Qt::Horizontal, this);

    m_logTable = new QTableWidget(this);
    m_logTable->setColumnCount(4);
    m_logTable->setHorizontalHeaderLabels({"Time", "Direction", "Type", "Summary"});
    m_logTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_logTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_logTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_logTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_logTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_logTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_logTable->verticalHeader()->setVisible(false);
    connect(m_logTable, &QTableWidget::itemSelectionChanged, this, &WebSocketDialog::onLogSelectionChanged);
    logSplitter->addWidget(m_logTable);

    m_payloadViewer = new QPlainTextEdit(this);
    m_payloadViewer->setReadOnly(true);
    m_payloadViewer->setPlaceholderText("Select a message to view full payload...");
    logSplitter->addWidget(m_payloadViewer);

    logSplitter->setSizes({480, 420});
    logLayout->addWidget(logSplitter);

    splitter->addWidget(logWidget);
    splitter->setSizes({220, 380});

    mainLayout->addWidget(splitter, 1);
}

void WebSocketDialog::updateStatusBadge(const QString& status, const QString& color) {
    m_statusBadge->setText(status);
    m_statusBadge->setStyleSheet(QString(
        "background-color: %1; color: #ffffff; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px;"
    ).arg(color));
}

void WebSocketDialog::onConnectClicked() {
    if (m_client->isConnected() || m_client->state() == network::WebSocketClient::State::Connecting) {
        m_client->close();
        return;
    }

    QUrl url(m_urlEdit->text().trimmed());
    if (!url.isValid() || (!url.scheme().startsWith("ws") && !url.scheme().startsWith("http"))) {
        QMessageBox::warning(this, "Invalid URL", "Please enter a valid WebSocket URL (e.g. ws:// or wss://).");
        return;
    }

    QMap<QString, QString> headers;
    for (const auto& h : m_headersTable->headers()) {
        if (h.enabled && !h.name.isEmpty()) {
            headers[h.name] = h.value;
        }
    }

    updateStatusBadge("CONNECTING...", "#f59e0b");
    m_connectBtn->setText("Disconnect");
    m_connectBtn->setStyleSheet("background-color: #ef4444; color: white; font-weight: bold; padding: 6px 14px; border-radius: 4px;");

    m_client->open(url, headers);
}

void WebSocketDialog::onWsConnected() {
    updateStatusBadge("CONNECTED", "#10b981");
    m_connectBtn->setText("Disconnect");
    m_connectBtn->setStyleSheet("background-color: #ef4444; color: white; font-weight: bold; padding: 6px 14px; border-radius: 4px;");
    m_sendBtn->setEnabled(true);
    m_pingBtn->setEnabled(true);
}

void WebSocketDialog::onWsDisconnected() {
    updateStatusBadge("DISCONNECTED", "#71717a");
    m_connectBtn->setText("Connect");
    m_connectBtn->setStyleSheet("background-color: #10b981; color: white; font-weight: bold; padding: 6px 14px; border-radius: 4px;");
    m_sendBtn->setEnabled(false);
    m_pingBtn->setEnabled(false);
}

void WebSocketDialog::onWsStateChanged(network::WebSocketClient::State state) {
    switch (state) {
        case network::WebSocketClient::State::Connecting:
            updateStatusBadge("CONNECTING", "#f59e0b");
            break;
        case network::WebSocketClient::State::Connected:
            updateStatusBadge("CONNECTED", "#10b981");
            break;
        case network::WebSocketClient::State::Closing:
            updateStatusBadge("CLOSING", "#f59e0b");
            break;
        case network::WebSocketClient::State::Disconnected:
            updateStatusBadge("DISCONNECTED", "#71717a");
            break;
    }
}

void WebSocketDialog::onSendClicked() {
    QString text = m_sendTextEdit->toPlainText();
    if (text.isEmpty()) return;

    m_client->sendTextMessage(text);
    appendLog("▲ SEND", "TEXT", QString("%1 bytes").arg(text.toUtf8().size()), text);
}

void WebSocketDialog::onSendPingClicked() {
    m_client->sendPing("ping");
    appendLog("▲ SEND", "PING", "ping", "ping");
}

void WebSocketDialog::onWsTextMessageReceived(const QString& message) {
    appendLog("▼ RECV", "TEXT", QString("%1 bytes").arg(message.toUtf8().size()), message);
}

void WebSocketDialog::onWsBinaryMessageReceived(const QByteArray& data) {
    appendLog("▼ RECV", "BINARY", QString("%1 bytes").arg(data.size()), QString::fromLatin1(data.toHex(' ')));
}

void WebSocketDialog::onWsFrameSent(const QString& type, const QString& summary) {
    if (type == "CLOSE" || type == "PONG") {
        appendLog("▲ SEND", type, summary, summary);
    }
}

void WebSocketDialog::onWsFrameReceived(const QString& type, const QString& summary) {
    if (type == "CLOSE" || type == "PING" || type == "PONG") {
        appendLog("▼ RECV", type, summary, summary);
    }
}

void WebSocketDialog::onErrorOccurred(const QString& error) {
    appendLog("⚠ ERR", "ERROR", error, error);
}

void WebSocketDialog::appendLog(const QString& direction, const QString& type, const QString& summary, const QString& fullPayload) {
    QString timeStr = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    m_entries.append({timeStr, direction, type, fullPayload});

    int row = m_logTable->rowCount();
    m_logTable->insertRow(row);

    auto* timeItem = new QTableWidgetItem(timeStr);
    timeItem->setForeground(QColor("#a1a1aa"));
    m_logTable->setItem(row, 0, timeItem);

    auto* dirItem = new QTableWidgetItem(direction);
    if (direction.contains("SEND")) {
        dirItem->setForeground(QColor("#3b82f6")); // Blue
    } else if (direction.contains("RECV")) {
        dirItem->setForeground(QColor("#10b981")); // Green
    } else {
        dirItem->setForeground(QColor("#ef4444")); // Red
    }
    dirItem->setFont(QFont(dirItem->font().family(), -1, QFont::Bold));
    m_logTable->setItem(row, 1, dirItem);

    auto* typeItem = new QTableWidgetItem(type);
    typeItem->setForeground(QColor("#e4e4e7"));
    m_logTable->setItem(row, 2, typeItem);

    auto* summaryItem = new QTableWidgetItem(summary.left(100));
    m_logTable->setItem(row, 3, summaryItem);

    if (m_autoScrollCheck->isChecked()) {
        m_logTable->scrollToBottom();
    }
}

void WebSocketDialog::onLogSelectionChanged() {
    int row = m_logTable->currentRow();
    if (row >= 0 && row < m_entries.size()) {
        const auto& entry = m_entries[row];
        // If JSON, format nicely
        QJsonParseError parseErr;
        auto doc = QJsonDocument::fromJson(entry.payload.toUtf8(), &parseErr);
        if (parseErr.error == QJsonParseError::NoError && (doc.isObject() || doc.isArray())) {
            m_payloadViewer->setPlainText(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
        } else {
            m_payloadViewer->setPlainText(entry.payload);
        }
    }
}

void WebSocketDialog::onClearLogClicked() {
    m_logTable->setRowCount(0);
    m_entries.clear();
    m_payloadViewer->clear();
}

} // namespace poppy::gui
