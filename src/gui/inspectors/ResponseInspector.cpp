#include "ResponseInspector.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QClipboard>
#include <QGuiApplication>
#include <QTextBrowser>
#include <Theme.h>

namespace poppy::gui {

ResponseInspector::ResponseInspector(QWidget* parent) : QWidget(parent) {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // 1. Top Telemetry Bar
    auto* topBar = new QHBoxLayout();
    topBar->setSpacing(10);

    m_statusBadge = new QLabel(this);
    m_statusBadge->setStyleSheet("background-color: #27272a; color: #a1a1aa; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px;");
    m_statusBadge->setText("STATUS: ---");
    topBar->addWidget(m_statusBadge);

    m_latencyBadge = new QLabel(this);
    m_latencyBadge->setStyleSheet("background-color: #27272a; color: #a1a1aa; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px;");
    m_latencyBadge->setText("TIME: ---");
    topBar->addWidget(m_latencyBadge);

    m_sizeBadge = new QLabel(this);
    m_sizeBadge->setStyleSheet("background-color: #27272a; color: #a1a1aa; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px;");
    m_sizeBadge->setText("SIZE: ---");
    topBar->addWidget(m_sizeBadge);

    m_timingDetails = new QLabel(this);
    m_timingDetails->setStyleSheet("color: #71717a; font-size: 11px;");
    topBar->addWidget(m_timingDetails);

    topBar->addStretch();

    m_prettyRawToggleBtn = new QPushButton("Raw", this);
    connect(m_prettyRawToggleBtn, &QPushButton::clicked, this, &ResponseInspector::togglePrettyRaw);
    topBar->addWidget(m_prettyRawToggleBtn);

    m_copyBtn = new QPushButton("Copy Body", this);
    connect(m_copyBtn, &QPushButton::clicked, this, &ResponseInspector::copyBodyToClipboard);
    topBar->addWidget(m_copyBtn);

    mainLayout->addLayout(topBar);

    // 2. Tabs
    m_tabWidget = new QTabWidget(this);

    // Tab 0: Body
    m_bodyTab = new QWidget(this);
    auto* bLayout = new QVBoxLayout(m_bodyTab);
    bLayout->setContentsMargins(0, 4, 0, 0);

    m_bodySearchFilter = new QLineEdit(m_bodyTab);
    m_bodySearchFilter->setPlaceholderText("Search in response body...");
    connect(m_bodySearchFilter, &QLineEdit::textChanged, this, [this](const QString& q) {
        if (q.isEmpty()) {
            m_bodyViewer->find("", QTextDocument::FindFlags{});
        } else {
            m_bodyViewer->find(q);
        }
    });
    bLayout->addWidget(m_bodySearchFilter);

    m_bodyViewer = new QPlainTextEdit(m_bodyTab);
    m_bodyViewer->setReadOnly(true);
    QFont codeFont("Consolas", 10);
    if (!codeFont.exactMatch()) codeFont = QFont("Courier New", 10);
    m_bodyViewer->setFont(codeFont);
    m_jsonHighlighter = new JsonSyntaxHighlighter(m_bodyViewer->document());
    bLayout->addWidget(m_bodyViewer);

    m_tabWidget->addTab(m_bodyTab, "Response Body");

    // Tab 1: HTML Preview
    m_previewBrowser = new QTextBrowser(this);
    m_previewBrowser->setOpenExternalLinks(false);
    m_tabWidget->addTab(m_previewBrowser, "Preview (HTML)");

    // Tab 1: Headers
    m_headersTab = new QWidget(this);
    auto* hLayout = new QVBoxLayout(m_headersTab);
    hLayout->setContentsMargins(0, 4, 0, 0);

    m_headersFilter = new QLineEdit(m_headersTab);
    m_headersFilter->setPlaceholderText("Filter headers...");
    connect(m_headersFilter, &QLineEdit::textChanged, this, &ResponseInspector::filterHeaders);
    hLayout->addWidget(m_headersFilter);

    m_headersTable = new QTableWidget(m_headersTab);
    m_headersTable->setColumnCount(2);
    m_headersTable->setHorizontalHeaderLabels({"Header", "Value"});
    m_headersTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_headersTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_headersTable->setColumnWidth(0, 220);
    m_headersTable->verticalHeader()->setVisible(false);
    m_headersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    hLayout->addWidget(m_headersTable);

    m_tabWidget->addTab(m_headersTab, "Headers");

    // Tab 2: Test Results
    m_testsTab = new QWidget(this);
    auto* tLayout = new QVBoxLayout(m_testsTab);
    tLayout->setContentsMargins(0, 4, 0, 0);

    m_testSummaryLabel = new QLabel("No tests run", m_testsTab);
    m_testSummaryLabel->setStyleSheet("font-weight: bold; color: #a1a1aa; padding: 4px;");
    tLayout->addWidget(m_testSummaryLabel);

    m_testsTable = new QTableWidget(m_testsTab);
    m_testsTable->setColumnCount(3);
    m_testsTable->setHorizontalHeaderLabels({"Status", "Test Case", "Message / Time"});
    m_testsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_testsTable->setColumnWidth(0, 80);
    m_testsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_testsTable->setColumnWidth(1, 250);
    m_testsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_testsTable->verticalHeader()->setVisible(false);
    m_testsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tLayout->addWidget(m_testsTable);

    m_tabWidget->addTab(m_testsTab, "Tests (0)");

    mainLayout->addWidget(m_tabWidget);
}

void ResponseInspector::clear() {
    m_currentResponse = core::ResponseModel{};
    m_statusBadge->setStyleSheet("background-color: #27272a; color: #a1a1aa; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px;");
    m_statusBadge->setText("STATUS: ---");
    m_latencyBadge->setText("TIME: ---");
    m_sizeBadge->setText("SIZE: ---");
    m_timingDetails->clear();
    m_bodyViewer->clear();
    m_previewBrowser->clear();
    m_bodySearchFilter->clear();
    m_headersTable->setRowCount(0);
    m_testsTable->setRowCount(0);
    m_testSummaryLabel->setText("No tests run");
    m_tabWidget->setTabText(3, "Tests (0)");
}

void ResponseInspector::setResponse(const core::ResponseModel& res, const core::TestReport* testReport) {
    m_currentResponse = res;
    updateTelemetryBar(res);

    // Body viewer & HTML Preview
    m_isPretty = true;
    m_prettyRawToggleBtn->setText("Raw");
    if (res.isJson()) {
        m_bodyViewer->setPlainText(res.formattedJson());
        m_previewBrowser->setHtml("<pre style=\"font-family: Consolas, monospace; color: #f4f4f5; background-color: #18181b;\">" + res.formattedJson().toHtmlEscaped() + "</pre>");
    } else {
        m_bodyViewer->setPlainText(res.bodyAsString());
        m_previewBrowser->setHtml(res.bodyAsString());
    }

    // Headers table
    updateHeadersTable(res);

    // Tests
    updateTestsTab(testReport);
}

void ResponseInspector::updateTelemetryBar(const core::ResponseModel& res) {
    if (!res.errorString.isEmpty() && res.statusCode == 0) {
        m_statusBadge->setStyleSheet("background-color: #ef4444; color: #ffffff; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px;");
        m_statusBadge->setText("ERROR");
        m_bodyViewer->setPlainText("Network Error:\n" + res.errorString);
    } else {
        QColor sc = Theme::statusColor(res.statusCode);
        m_statusBadge->setStyleSheet(QString("background-color: %1; color: #ffffff; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px;").arg(sc.name()));
        m_statusBadge->setText(QString("%1 %2").arg(res.statusCode).arg(res.statusText));
    }

    m_latencyBadge->setText(QString("%1 ms").arg(res.latencyMs));

    // Size formatting
    QString sizeStr;
    if (res.sizeBytes < 1024) {
        sizeStr = QString("%1 B").arg(res.sizeBytes);
    } else if (res.sizeBytes < 1024 * 1024) {
        sizeStr = QString("%1 KB").arg(res.sizeBytes / 1024.0, 0, 'f', 1);
    } else {
        sizeStr = QString("%1 MB").arg(res.sizeBytes / (1024.0 * 1024.0), 0, 'f', 2);
    }
    m_sizeBadge->setText(sizeStr);

    // Timing breakdown
    if (res.dnsTimeMs > 0 || res.connectTimeMs > 0 || res.sslHandshakeTimeMs > 0) {
        m_timingDetails->setText(QString("DNS: %1ms | TCP: %2ms | SSL: %3ms | TTFB: %4ms")
            .arg(static_cast<int>(res.dnsTimeMs))
            .arg(static_cast<int>(res.connectTimeMs))
            .arg(static_cast<int>(res.sslHandshakeTimeMs))
            .arg(static_cast<int>(res.ttfbMs)));
    } else {
        m_timingDetails->clear();
    }
}

void ResponseInspector::updateHeadersTable(const core::ResponseModel& res) {
    m_headersTable->setRowCount(0);
    for (const auto& h : res.headers) {
        int r = m_headersTable->rowCount();
        m_headersTable->insertRow(r);
        m_headersTable->setItem(r, 0, new QTableWidgetItem(h.name));
        m_headersTable->setItem(r, 1, new QTableWidgetItem(h.value));
    }
}

void ResponseInspector::filterHeaders(const QString& text) {
    for (int r = 0; r < m_headersTable->rowCount(); ++r) {
        auto* nameItem = m_headersTable->item(r, 0);
        auto* valItem = m_headersTable->item(r, 1);
        bool match = (nameItem && nameItem->text().contains(text, Qt::CaseInsensitive)) ||
                     (valItem && valItem->text().contains(text, Qt::CaseInsensitive));
        m_headersTable->setRowHidden(r, !match);
    }
}

void ResponseInspector::updateTestsTab(const core::TestReport* testReport) {
    if (!testReport || testReport->results.isEmpty()) {
        m_testSummaryLabel->setText("No tests registered in request.");
        m_testsTable->setRowCount(0);
        m_tabWidget->setTabText(3, "Tests (0)");
        return;
    }

    m_tabWidget->setTabText(3, QString("Tests (%1/%2)").arg(testReport->passedCount()).arg(testReport->totalCount()));
    QString summaryColor = (testReport->failedCount() == 0) ? "#10b981" : "#ef4444";
    m_testSummaryLabel->setStyleSheet(QString("font-weight: bold; color: %1; padding: 4px;").arg(summaryColor));
    m_testSummaryLabel->setText(QString("Tests Passed: %1 / %2 (Total Time: %3 ms)")
        .arg(testReport->passedCount())
        .arg(testReport->totalCount())
        .arg(testReport->totalDurationMs));

    m_testsTable->setRowCount(0);
    for (const auto& r : testReport->results) {
        int row = m_testsTable->rowCount();
        m_testsTable->insertRow(row);

        auto* statusItem = new QTableWidgetItem(r.passed ? "PASS" : "FAIL");
        statusItem->setForeground(r.passed ? QColor("#10b981") : QColor("#ef4444"));
        statusItem->setFont(QFont(statusItem->font().family(), -1, QFont::Bold));
        m_testsTable->setItem(row, 0, statusItem);

        m_testsTable->setItem(row, 1, new QTableWidgetItem(r.name));

        QString msg = r.passed ? QString("%1 ms").arg(r.durationMs) : r.errorMessage;
        auto* msgItem = new QTableWidgetItem(msg);
        if (!r.passed) msgItem->setForeground(QColor("#f87171"));
        m_testsTable->setItem(row, 2, msgItem);
    }
}

void ResponseInspector::togglePrettyRaw() {
    m_isPretty = !m_isPretty;
    if (m_isPretty) {
        m_prettyRawToggleBtn->setText("Raw");
        if (m_currentResponse.isJson()) {
            m_bodyViewer->setPlainText(m_currentResponse.formattedJson());
        } else {
            m_bodyViewer->setPlainText(m_currentResponse.bodyAsString());
        }
    } else {
        m_prettyRawToggleBtn->setText("Pretty");
        m_bodyViewer->setPlainText(m_currentResponse.bodyAsString());
    }
}

void ResponseInspector::copyBodyToClipboard() {
    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setText(m_bodyViewer->toPlainText());
}

} // namespace poppy::gui
