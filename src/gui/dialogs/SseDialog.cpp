#include "SseDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QJsonDocument>
#include <QTabWidget>

namespace poppy::gui {

SseDialog::SseDialog(const QString& initialUrl, QWidget* parent)
    : QDialog(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    setWindowTitle("Server-Sent Events (SSE) Stream Inspector - Poppy");
    resize(1000, 680);

    setupUi();

    if (!initialUrl.isEmpty()) {
        m_urlEdit->setText(initialUrl);
    }
}

SseDialog::~SseDialog() {
    onDisconnectClicked();
}

void SseDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    // 1. Connection row
    auto* connRow = new QHBoxLayout();
    connRow->setSpacing(8);

    auto* urlLabel = new QLabel("SSE Endpoint URL:", this);
    urlLabel->setStyleSheet("font-weight: bold;");
    connRow->addWidget(urlLabel);

    m_urlEdit = new QLineEdit(this);
    m_urlEdit->setPlaceholderText("https://api.openai.com/v1/chat/completions or http://localhost:8000/events");
    connRow->addWidget(m_urlEdit, 1);

    m_statusBadge = new QLabel("DISCONNECTED", this);
    m_statusBadge->setStyleSheet("background-color: #27272a; color: #a1a1aa; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px;");
    connRow->addWidget(m_statusBadge);

    m_eventCountBadge = new QLabel("Events: 0", this);
    m_eventCountBadge->setStyleSheet("color: #71717a; font-size: 11px;");
    connRow->addWidget(m_eventCountBadge);

    m_connectBtn = new QPushButton("Listen Stream", this);
    m_connectBtn->setStyleSheet("background-color: #10b981; color: white; font-weight: bold; padding: 6px 14px; border-radius: 4px;");
    connect(m_connectBtn, &QPushButton::clicked, this, &SseDialog::onConnectClicked);
    connRow->addWidget(m_connectBtn);

    mainLayout->addLayout(connRow);

    // 2. Middle Splitter
    auto* splitter = new QSplitter(Qt::Vertical, this);

    // Top: Headers & Options Tabs
    auto* topTabs = new QTabWidget(this);
    m_headersTable = new KeyValueTable(false, this);
    topTabs->addTab(m_headersTable, "Request Headers");
    splitter->addWidget(topTabs);

    // Bottom: Events Timeline & Accumulator
    auto* eventsContainer = new QWidget(this);
    auto* evLayout = new QVBoxLayout(eventsContainer);
    evLayout->setContentsMargins(0, 0, 0, 0);
    evLayout->setSpacing(6);

    // Toolbar
    auto* tb = new QHBoxLayout();
    tb->addWidget(new QLabel("<b>Live Stream Timeline</b>", this));

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText("Filter events...");
    connect(m_filterEdit, &QLineEdit::textChanged, this, [this](const QString& filter) {
        for (int r = 0; r < m_eventsTable->rowCount(); ++r) {
            bool matches = filter.isEmpty() ||
                           m_eventsTable->item(r, 2)->text().contains(filter, Qt::CaseInsensitive) ||
                           m_eventsTable->item(r, 4)->text().contains(filter, Qt::CaseInsensitive);
            m_eventsTable->setRowHidden(r, !matches);
        }
    });
    tb->addWidget(m_filterEdit, 1);

    m_llmModeCheck = new QCheckBox("Auto-accumulate LLM stream deltas", this);
    m_llmModeCheck->setChecked(true);
    tb->addWidget(m_llmModeCheck);

    m_autoScrollCheck = new QCheckBox("Auto-scroll", this);
    m_autoScrollCheck->setChecked(true);
    tb->addWidget(m_autoScrollCheck);

    auto* clearBtn = new QPushButton("Clear", this);
    connect(clearBtn, &QPushButton::clicked, this, &SseDialog::onClearClicked);
    tb->addWidget(clearBtn);

    evLayout->addLayout(tb);

    auto* evSplitter = new QSplitter(Qt::Horizontal, this);

    m_eventsTable = new QTableWidget(this);
    m_eventsTable->setColumnCount(5);
    m_eventsTable->setHorizontalHeaderLabels({"#", "Time", "Event", "ID", "Data"});
    m_eventsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_eventsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_eventsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_eventsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_eventsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_eventsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_eventsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_eventsTable->verticalHeader()->setVisible(false);
    connect(m_eventsTable, &QTableWidget::itemSelectionChanged, this, &SseDialog::onEventSelected);
    evSplitter->addWidget(m_eventsTable);

    // Right pane: Tabs for Live Stream Text Accumulator & Event Payload Detail
    auto* rightTabs = new QTabWidget(this);

    m_streamAccumulator = new QPlainTextEdit(this);
    m_streamAccumulator->setReadOnly(true);
    m_streamAccumulator->setPlaceholderText("Reconstructed stream text / tokens will stream here...");
    rightTabs->addTab(m_streamAccumulator, "Accumulated Stream Text");

    m_eventDetailViewer = new QPlainTextEdit(this);
    m_eventDetailViewer->setReadOnly(true);
    m_eventDetailViewer->setPlaceholderText("Select an event to view full data payload...");
    rightTabs->addTab(m_eventDetailViewer, "Event Payload (Raw/JSON)");

    evSplitter->addWidget(rightTabs);
    evSplitter->setSizes({500, 480});

    evLayout->addWidget(evSplitter, 1);
    splitter->addWidget(eventsContainer);

    splitter->setSizes({160, 480});
    mainLayout->addWidget(splitter, 1);
}

void SseDialog::updateStatusBadge(const QString& status, const QString& color) {
    m_statusBadge->setText(status);
    m_statusBadge->setStyleSheet(QString(
        "background-color: %1; color: #ffffff; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px;"
    ).arg(color));
}

void SseDialog::onConnectClicked() {
    if (m_reply) {
        onDisconnectClicked();
        return;
    }

    QUrl url(m_urlEdit->text().trimmed());
    if (!url.isValid() || !url.scheme().startsWith("http")) {
        QMessageBox::warning(this, "Invalid URL", "Please specify a valid HTTP or HTTPS endpoint.");
        return;
    }

    QNetworkRequest req(url);
    req.setRawHeader("Accept", "text/event-stream");
    req.setRawHeader("Cache-Control", "no-cache");

    for (const auto& h : m_headersTable->headers()) {
        if (h.enabled && !h.name.isEmpty()) {
            req.setRawHeader(h.name.toUtf8(), h.value.toUtf8());
        }
    }

    m_parser.reset();
    m_reply = m_nam->get(req);
    connect(m_reply, &QNetworkReply::readyRead, this, &SseDialog::onReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &SseDialog::onReplyFinished);
    connect(m_reply, &QNetworkReply::errorOccurred, this, &SseDialog::onReplyError);

    updateStatusBadge("STREAMING...", "#10b981");
    m_connectBtn->setText("Stop Listening");
    m_connectBtn->setStyleSheet("background-color: #ef4444; color: white; font-weight: bold; padding: 6px 14px; border-radius: 4px;");
}

void SseDialog::onDisconnectClicked() {
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    updateStatusBadge("DISCONNECTED", "#71717a");
    m_connectBtn->setText("Listen Stream");
    m_connectBtn->setStyleSheet("background-color: #10b981; color: white; font-weight: bold; padding: 6px 14px; border-radius: 4px;");
}

void SseDialog::onReadyRead() {
    if (!m_reply) return;
    QByteArray chunk = m_reply->readAll();
    auto events = m_parser.feed(chunk);

    for (const auto& ev : events) {
        m_events.append(ev);

        int row = m_eventsTable->rowCount();
        m_eventsTable->insertRow(row);

        auto* idxItem = new QTableWidgetItem(QString::number(row + 1));
        idxItem->setForeground(QColor("#a1a1aa"));
        m_eventsTable->setItem(row, 0, idxItem);

        auto* timeItem = new QTableWidgetItem(ev.timestamp.toString("HH:mm:ss.zzz"));
        timeItem->setForeground(QColor("#a1a1aa"));
        m_eventsTable->setItem(row, 1, timeItem);

        auto* evItem = new QTableWidgetItem(ev.event);
        evItem->setForeground(QColor("#3b82f6"));
        evItem->setFont(QFont(evItem->font().family(), -1, QFont::Bold));
        m_eventsTable->setItem(row, 2, evItem);

        auto* idItem = new QTableWidgetItem(ev.id);
        idItem->setForeground(QColor("#71717a"));
        m_eventsTable->setItem(row, 3, idItem);

        auto* dataItem = new QTableWidgetItem(ev.data.left(120).replace('\n', ' '));
        m_eventsTable->setItem(row, 4, dataItem);

        if (m_autoScrollCheck->isChecked()) {
            m_eventsTable->scrollToBottom();
        }

        // LLM streaming text accumulator
        if (m_llmModeCheck->isChecked()) {
            QString delta = core::SseParser::extractLlmStreamDelta(ev.data);
            if (!delta.isEmpty()) {
                m_streamAccumulator->insertPlainText(delta);
                if (m_autoScrollCheck->isChecked()) {
                    m_streamAccumulator->ensureCursorVisible();
                }
            } else if (ev.data.trimmed() != "[DONE]" && !ev.data.startsWith('{')) {
                // If plaintext SSE stream, append directly
                m_streamAccumulator->appendPlainText(ev.data);
            }
        }
    }

    m_eventCountBadge->setText(QString("Events: %1").arg(m_events.size()));
}

void SseDialog::onReplyFinished() {
    updateStatusBadge("FINISHED", "#3b82f6");
    if (m_reply) {
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_connectBtn->setText("Listen Stream");
    m_connectBtn->setStyleSheet("background-color: #10b981; color: white; font-weight: bold; padding: 6px 14px; border-radius: 4px;");
}

void SseDialog::onReplyError(QNetworkReply::NetworkError error) {
    Q_UNUSED(error);
    if (m_reply) {
        updateStatusBadge("ERROR", "#ef4444");
    }
}

void SseDialog::onEventSelected() {
    int row = m_eventsTable->currentRow();
    if (row >= 0 && row < m_events.size()) {
        const auto& ev = m_events[row];
        QJsonParseError err;
        auto doc = QJsonDocument::fromJson(ev.data.toUtf8(), &err);
        if (err.error == QJsonParseError::NoError && (doc.isObject() || doc.isArray())) {
            m_eventDetailViewer->setPlainText(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
        } else {
            m_eventDetailViewer->setPlainText(ev.data);
        }
    }
}

void SseDialog::onClearClicked() {
    m_eventsTable->setRowCount(0);
    m_events.clear();
    m_eventDetailViewer->clear();
    m_streamAccumulator->clear();
    m_eventCountBadge->setText("Events: 0");
}

} // namespace poppy::gui
