#include "ResponseInspector.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QClipboard>
#include <QGuiApplication>
#include <QTextBrowser>
#include <QFileDialog>
#include <QFile>
#include <QMessageBox>
#include <QShortcut>
#include <QRegularExpression>
#include <core/JsonPathEvaluator.h>
#include <dialogs/DiffViewerDialog.h>
#include <Theme.h>

namespace poppy::gui {

static QString formatHexDump(const QByteArray& data) {
    QString result;
    const int bytesPerLine = 16;
    int limit = qMin(data.size(), 65536);
    for (int i = 0; i < limit; i += bytesPerLine) {
        result += QString("%1  ").arg(i, 8, 16, QChar('0')).toUpper();
        QString hexPart;
        QString asciiPart;
        for (int j = 0; j < bytesPerLine; ++j) {
            if (i + j < limit) {
                unsigned char c = static_cast<unsigned char>(data.at(i + j));
                hexPart += QString("%1 ").arg(c, 2, 16, QChar('0')).toUpper();
                asciiPart += (c >= 32 && c <= 126) ? QChar(c) : '.';
            } else {
                hexPart += "   ";
            }
            if (j == 7) hexPart += " ";
        }
        result += hexPart + " |" + asciiPart + "|\n";
    }
    if (data.size() > limit) {
        result += QString("\n... (%1 bytes remaining truncated in hex view) ...\n").arg(data.size() - limit);
    }
    return result;
}

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

    m_wordWrapBtn = new QPushButton("Wrap", this);
    m_wordWrapBtn->setCheckable(true);
    m_wordWrapBtn->setToolTip("Toggle word wrap in response body");
    connect(m_wordWrapBtn, &QPushButton::clicked, this, &ResponseInspector::toggleWordWrap);
    topBar->addWidget(m_wordWrapBtn);

    m_copyBtn = new QPushButton("Copy Body", this);
    connect(m_copyBtn, &QPushButton::clicked, this, &ResponseInspector::copyBodyToClipboard);
    topBar->addWidget(m_copyBtn);

    m_saveToFileBtn = new QPushButton("Save...", this);
    connect(m_saveToFileBtn, &QPushButton::clicked, this, &ResponseInspector::saveBodyToFile);
    topBar->addWidget(m_saveToFileBtn);

    m_diffBtn = new QPushButton("Compare...", this);
    m_diffBtn->setToolTip("Compare this response with another response or file");
    connect(m_diffBtn, &QPushButton::clicked, this, &ResponseInspector::onCompareDiffClicked);
    topBar->addWidget(m_diffBtn);

    mainLayout->addLayout(topBar);

    // 2. Tabs
    m_tabWidget = new QTabWidget(this);

    // Tab 0: Body
    m_bodyTab = new QWidget(this);
    auto* bLayout = new QVBoxLayout(m_bodyTab);
    bLayout->setContentsMargins(0, 4, 0, 0);
    bLayout->setSpacing(4);

    // Search / Filter Toolbar
    m_searchToolbar = new QWidget(m_bodyTab);
    auto* sLayout = new QHBoxLayout(m_searchToolbar);
    sLayout->setContentsMargins(2, 2, 2, 2);
    sLayout->setSpacing(6);

    m_searchEdit = new QLineEdit(m_searchToolbar);
    m_searchEdit->setPlaceholderText("Find in response (Ctrl+F) or type $.jsonpath...");
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ResponseInspector::onSearchTextChanged);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &ResponseInspector::findNext);
    sLayout->addWidget(m_searchEdit, 1);

    m_findPrevBtn = new QPushButton("▲", m_searchToolbar);
    m_findPrevBtn->setToolTip("Previous Match (Shift+Enter)");
    m_findPrevBtn->setFixedWidth(28);
    connect(m_findPrevBtn, &QPushButton::clicked, this, &ResponseInspector::findPrevious);
    sLayout->addWidget(m_findPrevBtn);

    m_findNextBtn = new QPushButton("▼", m_searchToolbar);
    m_findNextBtn->setToolTip("Next Match (Enter)");
    m_findNextBtn->setFixedWidth(28);
    connect(m_findNextBtn, &QPushButton::clicked, this, &ResponseInspector::findNext);
    sLayout->addWidget(m_findNextBtn);

    m_matchCountLabel = new QLabel(m_searchToolbar);
    m_matchCountLabel->setStyleSheet("color: #a1a1aa; font-size: 11px; padding: 0 4px;");
    sLayout->addWidget(m_matchCountLabel);

    m_caseSensitiveBtn = new QPushButton("Aa", m_searchToolbar);
    m_caseSensitiveBtn->setToolTip("Match Case");
    m_caseSensitiveBtn->setCheckable(true);
    m_caseSensitiveBtn->setFixedWidth(30);
    connect(m_caseSensitiveBtn, &QPushButton::clicked, this, &ResponseInspector::toggleCaseSensitive);
    sLayout->addWidget(m_caseSensitiveBtn);

    m_regexBtn = new QPushButton(".*", m_searchToolbar);
    m_regexBtn->setToolTip("Regular Expression");
    m_regexBtn->setCheckable(true);
    m_regexBtn->setFixedWidth(30);
    connect(m_regexBtn, &QPushButton::clicked, this, &ResponseInspector::toggleRegex);
    sLayout->addWidget(m_regexBtn);

    m_jsonPathModeBtn = new QPushButton("{ } JSONPath", m_searchToolbar);
    m_jsonPathModeBtn->setToolTip("Toggle JSONPath Query Filter");
    m_jsonPathModeBtn->setCheckable(true);
    connect(m_jsonPathModeBtn, &QPushButton::clicked, this, &ResponseInspector::toggleJsonPathMode);
    sLayout->addWidget(m_jsonPathModeBtn);

    m_resetFilterBtn = new QPushButton("Reset View", m_searchToolbar);
    m_resetFilterBtn->setToolTip("Restore full response body");
    m_resetFilterBtn->setVisible(false);
    m_resetFilterBtn->setStyleSheet("background-color: #2563eb; color: #ffffff; border-radius: 4px; font-weight: bold; padding: 4px 8px;");
    connect(m_resetFilterBtn, &QPushButton::clicked, this, &ResponseInspector::resetFilter);
    sLayout->addWidget(m_resetFilterBtn);

    bLayout->addWidget(m_searchToolbar);

    auto* searchShortcut = new QShortcut(QKeySequence::Find, this);
    connect(searchShortcut, &QShortcut::activated, this, &ResponseInspector::openSearch);

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

    // Tab 2: Hex View
    m_hexViewer = new QPlainTextEdit(this);
    m_hexViewer->setReadOnly(true);
    m_hexViewer->setFont(codeFont);
    m_tabWidget->addTab(m_hexViewer, "Hex");

    // Tab 3: Headers
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

    // Tab 4: Test Results
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

    // Tab 5: SSL / TLS Certificate Chain
    m_sslTab = new QWidget(this);
    auto* sslLayout = new QVBoxLayout(m_sslTab);
    sslLayout->setContentsMargins(0, 4, 0, 0);
    m_sslCertViewer = new QPlainTextEdit(m_sslTab);
    m_sslCertViewer->setReadOnly(true);
    m_sslCertViewer->setFont(codeFont);
    m_sslCertViewer->setPlaceholderText("No SSL/TLS certificate details for this request.");
    sslLayout->addWidget(m_sslCertViewer);
    m_tabWidget->addTab(m_sslTab, "SSL / TLS");

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
    m_hexViewer->clear();
    m_searchEdit->clear();
    m_searchMatches.clear();
    m_currentMatchIndex = -1;
    m_isFiltered = false;
    m_resetFilterBtn->setVisible(false);
    m_matchCountLabel->clear();
    m_headersTable->setRowCount(0);
    m_testsTable->setRowCount(0);
    m_sslCertViewer->clear();
    m_testSummaryLabel->setText("No tests run");
    m_tabWidget->setTabText(4, "Tests (0)");
    m_tabWidget->setTabText(5, "SSL / TLS");
}

void ResponseInspector::setTheme(bool isDark) {
    if (m_jsonHighlighter) {
        m_jsonHighlighter->setDarkTheme(isDark);
    }
}

void ResponseInspector::setResponse(const core::ResponseModel& res, const core::TestReport* testReport) {
    m_currentResponse = res;
    updateTelemetryBar(res);

    // Body size warning: don't auto-render bodies > 5 MB
    constexpr qint64 kWarnThresholdBytes = 5 * 1024 * 1024;
    if (res.rawBody.size() > kWarnThresholdBytes) {
        auto choice = QMessageBox::question(this, "Large Response",
            QString("The response body is %1 MB. Rendering may be slow.\nShow it anyway?")
                .arg(res.rawBody.size() / (1024.0 * 1024.0), 0, 'f', 1),
            QMessageBox::Yes | QMessageBox::No);
        if (choice == QMessageBox::No) {
            m_bodyViewer->setPlainText(QString("[Body too large to display: %1 MB — use Save... to write to file]")
                .arg(res.rawBody.size() / (1024.0 * 1024.0), 0, 'f', 1));
            updateHeadersTable(res);
            updateTestsTab(testReport);
            return;
        }
    }

    // Apply word-wrap setting
    m_bodyViewer->setLineWrapMode(m_wordWrap ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);

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

    // Hex view
    m_hexViewer->setPlainText(formatHexDump(res.rawBody));

    // Headers table
    updateHeadersTable(res);

    // Tests
    updateTestsTab(testReport);

    // SSL / TLS Certificate Info
    if (!res.certDetails.isEmpty()) {
        QString certReport = QString("=== Protocol: %1 ===\n\n").arg(res.protocol.isEmpty() ? "HTTPS" : res.protocol);
        for (int i = 0; i < res.certDetails.size(); ++i) {
            certReport += QString("--- Certificate #%1 ---\n%2\n\n").arg(i + 1).arg(res.certDetails[i]);
        }
        m_sslCertViewer->setPlainText(certReport.trimmed());
        m_tabWidget->setTabText(5, QString("SSL / TLS (%1)").arg(res.certDetails.size()));
    } else {
        m_sslCertViewer->setPlainText(!res.protocol.isEmpty() ? QString("Protocol: %1\nNo certificate chain available or connection was unencrypted (HTTP).").arg(res.protocol) : "No SSL/TLS certificate details available for this request.");
        m_tabWidget->setTabText(5, "SSL / TLS");
    }
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
        m_tabWidget->setTabText(4, "Tests (0)");
        return;
    }

    m_tabWidget->setTabText(4, QString("Tests (%1/%2)").arg(testReport->passedCount()).arg(testReport->totalCount()));
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

void ResponseInspector::toggleWordWrap() {
    m_wordWrap = m_wordWrapBtn->isChecked();
    m_bodyViewer->setLineWrapMode(m_wordWrap ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    m_wordWrapBtn->setStyleSheet(m_wordWrap
        ? "background-color: #3b82f6; color: #ffffff; font-weight: bold;"
        : "");
}

void ResponseInspector::copyBodyToClipboard() {
    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setText(m_bodyViewer->toPlainText());
}

void ResponseInspector::saveBodyToFile() {
    if (m_currentResponse.rawBody.isEmpty()) {
        QMessageBox::information(this, "Empty Body", "There is no response body to save.");
        return;
    }

    QString defaultName = "response";
    if (m_currentResponse.isJson()) defaultName += ".json";
    else if (m_currentResponse.contentType().contains("html", Qt::CaseInsensitive)) defaultName += ".html";
    else if (m_currentResponse.contentType().contains("xml", Qt::CaseInsensitive)) defaultName += ".xml";
    else defaultName += ".bin";

    QString filePath = QFileDialog::getSaveFileName(this, "Save Response Body", defaultName, "All Files (*.*)");
    if (!filePath.isEmpty()) {
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(m_currentResponse.rawBody);
            file.close();
            QMessageBox::information(this, "Saved", "Response body saved successfully.");
        } else {
            QMessageBox::warning(this, "Error", "Could not write to file: " + file.errorString());
        }
    }
}

void ResponseInspector::onCompareDiffClicked() {
    auto dlg = new DiffViewerDialog(m_currentResponse.bodyAsString(), QString(), nullptr, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

void ResponseInspector::openSearch() {
    m_tabWidget->setCurrentIndex(0);
    m_searchEdit->setFocus();
    m_searchEdit->selectAll();
}

void ResponseInspector::onSearchTextChanged(const QString& text) {
    QString q = text.trimmed();
    if (q.isEmpty()) {
        if (m_isFiltered) {
            restoreOriginalBody();
        }
        m_searchMatches.clear();
        m_currentMatchIndex = -1;
        m_bodyViewer->setExtraSelections({});
        m_matchCountLabel->clear();
        return;
    }

    // JSONPath mode
    if (m_isJsonPathMode || q.startsWith("$.") || q.startsWith("$[")) {
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(m_currentResponse.rawBody, &err);
        if (err.error == QJsonParseError::NoError && !doc.isNull()) {
            QString res = core::JsonPathEvaluator::evaluateToString(doc, q, true);
            if (!res.isEmpty()) {
                m_isFiltered = true;
                m_bodyViewer->setPlainText(res);
                m_resetFilterBtn->setVisible(true);
                m_matchCountLabel->setStyleSheet("color: #10b981; font-weight: bold; font-size: 11px;");
                m_matchCountLabel->setText("JSONPath Match");
                m_bodyViewer->setExtraSelections({});
                return;
            } else {
                m_matchCountLabel->setStyleSheet("color: #ef4444; font-size: 11px;");
                m_matchCountLabel->setText("No JSONPath match");
                return;
            }
        }
    }

    // Normal text search mode
    if (m_isFiltered) {
        restoreOriginalBody();
    }

    m_searchMatches.clear();
    m_currentMatchIndex = -1;

    QTextDocument::FindFlags flags;
    if (m_isCaseSensitive) {
        flags |= QTextDocument::FindCaseSensitively;
    }

    if (m_isRegex) {
        auto patternOptions = m_isCaseSensitive ? QRegularExpression::NoPatternOption : QRegularExpression::CaseInsensitiveOption;
        QRegularExpression regex(q, patternOptions);
        if (regex.isValid()) {
            QTextCursor cur = m_bodyViewer->document()->find(regex, 0);
            while (!cur.isNull()) {
                m_searchMatches.append(cur);
                cur = m_bodyViewer->document()->find(regex, cur.position());
            }
        }
    } else {
        QTextCursor cur = m_bodyViewer->document()->find(q, 0, flags);
        while (!cur.isNull()) {
            m_searchMatches.append(cur);
            cur = m_bodyViewer->document()->find(q, cur.position(), flags);
        }
    }

    if (!m_searchMatches.isEmpty()) {
        m_currentMatchIndex = 0;
    }
    updateSearchHighlights();
}

void ResponseInspector::findNext() {
    if (m_searchMatches.isEmpty()) return;
    m_currentMatchIndex = (m_currentMatchIndex + 1) % m_searchMatches.size();
    updateSearchHighlights();
}

void ResponseInspector::findPrevious() {
    if (m_searchMatches.isEmpty()) return;
    m_currentMatchIndex = (m_currentMatchIndex - 1 + m_searchMatches.size()) % m_searchMatches.size();
    updateSearchHighlights();
}

void ResponseInspector::toggleCaseSensitive() {
    m_isCaseSensitive = m_caseSensitiveBtn->isChecked();
    if (m_isCaseSensitive) {
        m_caseSensitiveBtn->setStyleSheet("background-color: #3b82f6; color: #ffffff; font-weight: bold;");
    } else {
        m_caseSensitiveBtn->setStyleSheet("");
    }
    onSearchTextChanged(m_searchEdit->text());
}

void ResponseInspector::toggleRegex() {
    m_isRegex = m_regexBtn->isChecked();
    if (m_isRegex) {
        m_regexBtn->setStyleSheet("background-color: #3b82f6; color: #ffffff; font-weight: bold;");
    } else {
        m_regexBtn->setStyleSheet("");
    }
    onSearchTextChanged(m_searchEdit->text());
}

void ResponseInspector::toggleJsonPathMode() {
    m_isJsonPathMode = m_jsonPathModeBtn->isChecked();
    if (m_isJsonPathMode) {
        m_jsonPathModeBtn->setStyleSheet("background-color: #10b981; color: #ffffff; font-weight: bold;");
    } else {
        m_jsonPathModeBtn->setStyleSheet("");
        if (m_isFiltered) {
            restoreOriginalBody();
        }
    }
    onSearchTextChanged(m_searchEdit->text());
}

void ResponseInspector::resetFilter() {
    m_searchEdit->clear();
    restoreOriginalBody();
    m_matchCountLabel->clear();
}

void ResponseInspector::restoreOriginalBody() {
    m_isFiltered = false;
    m_resetFilterBtn->setVisible(false);
    if (m_currentResponse.isJson()) {
        m_bodyViewer->setPlainText(m_isPretty ? m_currentResponse.formattedJson() : m_currentResponse.bodyAsString());
    } else {
        m_bodyViewer->setPlainText(m_currentResponse.bodyAsString());
    }
}

void ResponseInspector::updateSearchHighlights() {
    QList<QTextEdit::ExtraSelection> extraSelections;
    for (int i = 0; i < m_searchMatches.size(); ++i) {
        QTextEdit::ExtraSelection sel;
        sel.cursor = m_searchMatches[i];
        if (i == m_currentMatchIndex) {
            sel.format.setBackground(QColor("#f59e0b")); // Active match (amber/orange)
            sel.format.setForeground(QColor("#000000"));
        } else {
            sel.format.setBackground(QColor("#6b4f10")); // Subtle yellow
            sel.format.setForeground(QColor("#fef08a"));
        }
        extraSelections.append(sel);
    }
    m_bodyViewer->setExtraSelections(extraSelections);

    if (m_searchMatches.isEmpty()) {
        if (!m_searchEdit->text().trimmed().isEmpty()) {
            m_matchCountLabel->setStyleSheet("color: #ef4444; font-size: 11px;");
            m_matchCountLabel->setText("No matches");
        } else {
            m_matchCountLabel->clear();
        }
    } else {
        m_matchCountLabel->setStyleSheet("color: #a1a1aa; font-size: 11px;");
        m_matchCountLabel->setText(QString("%1 of %2").arg(m_currentMatchIndex + 1).arg(m_searchMatches.size()));
        m_bodyViewer->setTextCursor(m_searchMatches[m_currentMatchIndex]);
        m_bodyViewer->centerCursor();
    }
}

} // namespace poppy::gui
