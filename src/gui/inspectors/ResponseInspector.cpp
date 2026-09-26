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
#include <QTimer>
#include <QRegularExpression>
#include <core/JsonPathEvaluator.h>
#include <dialogs/DiffViewerDialog.h>
#include <Theme.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QMenu>
#include <QAction>

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
    topBar->setSpacing(8);

    m_statusBadge = new QLabel(this);
    topBar->addWidget(m_statusBadge);

    m_latencyBadge = new QLabel(this);
    topBar->addWidget(m_latencyBadge);

    m_sizeBadge = new QLabel(this);
    topBar->addWidget(m_sizeBadge);

    m_timingDetails = new QLabel(this);
    topBar->addWidget(m_timingDetails);

    applyDefaultBadgeStyles(Theme::isDarkMode());

    topBar->addStretch();

    m_prettyRawToggleBtn = new QPushButton("Raw", this);
    m_prettyRawToggleBtn->setToolTip("Toggle formatted JSON vs Raw body");
    connect(m_prettyRawToggleBtn, &QPushButton::clicked, this, &ResponseInspector::togglePrettyRaw);
    topBar->addWidget(m_prettyRawToggleBtn);

    m_wordWrapBtn = new QPushButton("Wrap", this);
    m_wordWrapBtn->setCheckable(true);
    m_wordWrapBtn->setToolTip("Toggle word wrap in response body");
    connect(m_wordWrapBtn, &QPushButton::clicked, this, &ResponseInspector::toggleWordWrap);
    topBar->addWidget(m_wordWrapBtn);

    m_copyBtn = new QPushButton("📋 Copy", this);
    m_copyBtn->setToolTip("Copy response body to clipboard");
    connect(m_copyBtn, &QPushButton::clicked, this, &ResponseInspector::copyBodyToClipboard);
    topBar->addWidget(m_copyBtn);

    m_saveToFileBtn = new QPushButton("💾 Save...", this);
    m_saveToFileBtn->setToolTip("Save response body to disk");
    connect(m_saveToFileBtn, &QPushButton::clicked, this, &ResponseInspector::saveBodyToFile);
    topBar->addWidget(m_saveToFileBtn);

    m_diffBtn = new QPushButton("🔄 Compare...", this);
    m_diffBtn->setToolTip("Compare this response with another response or file");
    connect(m_diffBtn, &QPushButton::clicked, this, &ResponseInspector::onCompareDiffClicked);
    topBar->addWidget(m_diffBtn);

    mainLayout->addLayout(topBar);

    m_waterfallWidget = new NetworkWaterfallWidget(this);
    m_waterfallWidget->setVisible(false);
    mainLayout->addWidget(m_waterfallWidget);

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

    m_bodyViewer->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_bodyViewer, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu menu(this);
        auto* copyAct = menu.addAction("Copy");
        connect(copyAct, &QAction::triggered, m_bodyViewer, &QPlainTextEdit::copy);

        QString selected = m_bodyViewer->textCursor().selectedText().trimmed();
        if (!selected.isEmpty()) {
            menu.addSeparator();
            auto* storeAct = menu.addAction("Set Selection as Environment Variable...");
            connect(storeAct, &QAction::triggered, this, [this, selected]() {
                emit storeVariableRequested(selected);
            });
        }
        menu.exec(m_bodyViewer->mapToGlobal(pos));
    });

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

    // Tab 6: Visualize (JSON Table & Charts)
    m_visualizeTab = new QWidget(this);
    auto* vLayout = new QVBoxLayout(m_visualizeTab);
    vLayout->setContentsMargins(0, 4, 0, 0);

    auto* vTop = new QHBoxLayout();
    m_visualizeFilter = new QLineEdit(m_visualizeTab);
    m_visualizeFilter->setPlaceholderText("Filter visualized table rows...");
    connect(m_visualizeFilter, &QLineEdit::textChanged, this, [this](const QString& q) {
        for (int r = 0; r < m_visualizeTable->rowCount(); ++r) {
            bool matches = q.isEmpty();
            for (int c = 0; !matches && c < m_visualizeTable->columnCount(); ++c) {
                auto* item = m_visualizeTable->item(r, c);
                if (item && item->text().contains(q, Qt::CaseInsensitive)) {
                    matches = true;
                }
            }
            m_visualizeTable->setRowHidden(r, !matches);
        }
    });
    vTop->addWidget(m_visualizeFilter, 1);

    m_chartToggleBtn = new QPushButton("Toggle Chart", m_visualizeTab);
    connect(m_chartToggleBtn, &QPushButton::clicked, this, &ResponseInspector::toggleChartView);
    vTop->addWidget(m_chartToggleBtn);

    m_visualizeStats = new QLabel("No data visualized", m_visualizeTab);
    m_visualizeStats->setStyleSheet("color: #71717a; font-size: 11px;");
    vTop->addWidget(m_visualizeStats);
    vLayout->addLayout(vTop);

    m_visualizeTable = new QTableWidget(m_visualizeTab);
    m_visualizeTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_visualizeTable->setSortingEnabled(true);
    vLayout->addWidget(m_visualizeTable);

    m_chartViewer = new QPlainTextEdit(m_visualizeTab);
    m_chartViewer->setReadOnly(true);
    m_chartViewer->setFont(codeFont);
    m_chartViewer->setVisible(false);
    vLayout->addWidget(m_chartViewer);

    m_tabWidget->addTab(m_visualizeTab, "Visualize");

    // Tab 7: Cookies (Set-Cookie)
    m_cookiesTab = new QWidget(this);
    auto* cLayout = new QVBoxLayout(m_cookiesTab);
    cLayout->setContentsMargins(0, 4, 0, 0);

    m_cookiesTable = new QTableWidget(m_cookiesTab);
    m_cookiesTable->setColumnCount(7);
    m_cookiesTable->setHorizontalHeaderLabels({"Name", "Value", "Domain", "Path", "Expires / Max-Age", "Secure", "HttpOnly"});
    m_cookiesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_cookiesTable->setColumnWidth(0, 150);
    m_cookiesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_cookiesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_cookiesTable->setColumnWidth(2, 120);
    m_cookiesTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    m_cookiesTable->setColumnWidth(3, 80);
    m_cookiesTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Interactive);
    m_cookiesTable->setColumnWidth(4, 140);
    m_cookiesTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    m_cookiesTable->setColumnWidth(5, 75);
    m_cookiesTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Fixed);
    m_cookiesTable->setColumnWidth(6, 75);
    m_cookiesTable->verticalHeader()->setVisible(false);
    m_cookiesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    cLayout->addWidget(m_cookiesTable);

    m_tabWidget->addTab(m_cookiesTab, "Cookies (0)");

    // Tab 8: JSON Tree
    m_jsonTreeTab = new QWidget(this);
    auto* jtLayout = new QVBoxLayout(m_jsonTreeTab);
    jtLayout->setContentsMargins(0, 4, 0, 0);
    jtLayout->setSpacing(4);

    m_jsonTreeSearch = new QLineEdit(m_jsonTreeTab);
    m_jsonTreeSearch->setPlaceholderText("Search keys or values...");
    m_jsonTreeSearch->setClearButtonEnabled(true);
    jtLayout->addWidget(m_jsonTreeSearch);

    m_jsonTreeWidget = new QTreeWidget(m_jsonTreeTab);
    m_jsonTreeWidget->setHeaderLabels({"Key", "Value", "Type"});
    m_jsonTreeWidget->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_jsonTreeWidget->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_jsonTreeWidget->header()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_jsonTreeWidget->setColumnWidth(0, 200);
    m_jsonTreeWidget->setColumnWidth(2, 70);
    m_jsonTreeWidget->setAlternatingRowColors(true);
    m_jsonTreeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_jsonTreeWidget, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        auto* item = m_jsonTreeWidget->itemAt(pos);
        if (!item) return;
        QMenu menu(this);
        auto* copyKey = menu.addAction("Copy Key");
        auto* copyVal = menu.addAction("Copy Value");
        auto* copyPath = menu.addAction("Copy JSONPath");
        connect(copyKey, &QAction::triggered, this, [item]() {
            QGuiApplication::clipboard()->setText(item->text(0));
        });
        connect(copyVal, &QAction::triggered, this, [item]() {
            QGuiApplication::clipboard()->setText(item->text(1));
        });
        connect(copyPath, &QAction::triggered, this, [item]() {
            QStringList parts;
            const QTreeWidgetItem* cur = item;
            while (cur && cur->parent()) {
                const QString key = cur->text(0);
                const bool inArray = cur->parent()->text(2) == QLatin1String("array");
                if (inArray) {
                    parts.prepend(QString("[%1]").arg(key));
                } else {
                    static const QRegularExpression ident(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
                    if (ident.match(key).hasMatch()) {
                        parts.prepend(QString(".%1").arg(key));
                    } else {
                        QString escaped = key;
                        escaped.replace('\\', "\\\\");
                        escaped.replace('\'', "\\'");
                        parts.prepend(QString("['%1']").arg(escaped));
                    }
                }
                cur = cur->parent();
            }
            QGuiApplication::clipboard()->setText("$" + parts.join(""));
        });
        menu.exec(m_jsonTreeWidget->mapToGlobal(pos));
    });
    jtLayout->addWidget(m_jsonTreeWidget);

    connect(m_jsonTreeSearch, &QLineEdit::textChanged, this, [this](const QString& q) {
        for (int i = 0; i < m_jsonTreeWidget->topLevelItemCount(); ++i) {
            filterJsonTree(m_jsonTreeWidget->topLevelItem(i), q);
        }
    });

    m_tabWidget->addTab(m_jsonTreeTab, "JSON Tree");

    mainLayout->addWidget(m_tabWidget);
}

void ResponseInspector::applyDefaultBadgeStyles(bool isDark) {
    QString badgeStyle = isDark
        ? QStringLiteral("background-color: #161820; color: #6b7280; border: 1px solid #252834; border-radius: 6px; padding: 4px 10px; font-weight: 600; font-size: 11px;")
        : QStringLiteral("background-color: #f1f5f9; color: #94a3b8; border: 1px solid #cbd5e1; border-radius: 6px; padding: 4px 10px; font-weight: 600; font-size: 11px;");
    m_statusBadge->setStyleSheet(badgeStyle);
    m_statusBadge->setText("● STATUS: ---");
    m_latencyBadge->setStyleSheet(badgeStyle);
    m_latencyBadge->setText("⏱ TIME: ---");
    m_sizeBadge->setStyleSheet(badgeStyle);
    m_sizeBadge->setText("📦 SIZE: ---");
    m_timingDetails->setStyleSheet(isDark ? "color: #71717a; font-size: 11px;" : "color: #94a3b8; font-size: 11px;");
}

void ResponseInspector::clear() {
    m_currentResponse = core::ResponseModel{};
    applyDefaultBadgeStyles(Theme::isDarkMode());
    m_timingDetails->clear();
    if (m_waterfallWidget) {
        m_waterfallWidget->clear();
        m_waterfallWidget->setVisible(false);
    }
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
    m_visualizeTable->setRowCount(0);
    m_visualizeTable->setColumnCount(0);
    m_chartViewer->clear();
    m_cookiesTable->setRowCount(0);
    m_statusBadge->setToolTip(QString());
    m_visualizeStats->setText("No data visualized");
    m_testSummaryLabel->setText("No tests run");
    m_tabWidget->setTabText(4, "Tests (0)");
    m_tabWidget->setTabText(5, "SSL / TLS");
    m_tabWidget->setTabText(7, "Cookies (0)");
    m_jsonTreeWidget->clear();
    m_jsonTreeSearch->clear();
    m_tabWidget->setTabText(8, "JSON Tree");
}

void ResponseInspector::setTheme(bool isDark) {
    if (m_jsonHighlighter) {
        m_jsonHighlighter->setDarkTheme(isDark);
    }
    if (m_currentResponse.statusCode > 0 || !m_currentResponse.errorString.isEmpty()) {
        updateTelemetryBar(m_currentResponse);
    } else {
        applyDefaultBadgeStyles(isDark);
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
    if (!res.errorString.isEmpty() && res.statusCode == 0) {
        const QString errorText = QStringLiteral("Network Error:\n") + res.errorString;
        m_bodyViewer->setPlainText(errorText);
        m_previewBrowser->setHtml("<pre style=\"font-family: Consolas, monospace; color: #f4f4f5; background-color: #18181b;\">" + errorText.toHtmlEscaped() + "</pre>");
    } else if (res.isJson()) {
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

    // Tab 6: Visualize
    updateVisualizeTab(res);

    // Tab 7: Cookies
    updateCookiesTab(res);

    // Tab 8: JSON Tree
    updateJsonTreeTab(res);
}

void ResponseInspector::updateTelemetryBar(const core::ResponseModel& res) {
    const bool dark = Theme::isDarkMode();
    if (!res.errorString.isEmpty() && res.statusCode == 0) {
        m_statusBadge->setStyleSheet(
            "background-color: rgba(239, 68, 68, 0.16); color: #ef4444; border: 1px solid rgba(239, 68, 68, 0.40); border-radius: 6px; padding: 4px 10px; font-weight: 700; font-size: 11px;"
        );
        m_statusBadge->setText("● ERROR");
        m_statusBadge->setToolTip(res.errorString);
    } else {
        QColor sc = Theme::statusColor(res.statusCode);
        QString bgRgba = QString("rgba(%1, %2, %3, 0.16)").arg(sc.red()).arg(sc.green()).arg(sc.blue());
        QString borderRgba = QString("rgba(%1, %2, %3, 0.40)").arg(sc.red()).arg(sc.green()).arg(sc.blue());
        m_statusBadge->setStyleSheet(QString(
            "background-color: %1; color: %2; border: 1px solid %3; border-radius: 6px; padding: 4px 10px; font-weight: 700; font-size: 11px;"
        ).arg(bgRgba, sc.name(), borderRgba));
        m_statusBadge->setText(QString("● %1 %2").arg(res.statusCode).arg(res.statusText));
        m_statusBadge->setToolTip(httpStatusExplanation(res.statusCode));
    }

    // Color-scale latency
    QColor latColor = (res.latencyMs < 200) ? QColor("#10b981") : (res.latencyMs < 600 ? QColor("#f59e0b") : QColor("#ef4444"));
    QString latBg = QString("rgba(%1, %2, %3, 0.14)").arg(latColor.red()).arg(latColor.green()).arg(latColor.blue());
    QString latBorder = QString("rgba(%1, %2, %3, 0.35)").arg(latColor.red()).arg(latColor.green()).arg(latColor.blue());
    m_latencyBadge->setStyleSheet(QString(
        "background-color: %1; color: %2; border: 1px solid %3; border-radius: 6px; padding: 4px 10px; font-weight: 600; font-size: 11px;"
    ).arg(latBg, latColor.name(), latBorder));
    m_latencyBadge->setText(QString("⏱ %1 ms").arg(res.latencyMs));

    // Size formatting
    QString sizeStr;
    if (res.sizeBytes < 1024) {
        sizeStr = QString("%1 B").arg(res.sizeBytes);
    } else if (res.sizeBytes < 1024 * 1024) {
        sizeStr = QString("%1 KB").arg(res.sizeBytes / 1024.0, 0, 'f', 1);
    } else {
        sizeStr = QString("%1 MB").arg(res.sizeBytes / (1024.0 * 1024.0), 0, 'f', 2);
    }
    m_sizeBadge->setStyleSheet(dark
        ? "background-color: #161820; color: #f3f4f6; border: 1px solid #282a35; border-radius: 6px; padding: 4px 10px; font-weight: 600; font-size: 11px;"
        : "background-color: #ffffff; color: #0f172a; border: 1px solid #cbd5e1; border-radius: 6px; padding: 4px 10px; font-weight: 600; font-size: 11px;");
    m_sizeBadge->setText(QString("📦 %1").arg(sizeStr));

    // Timing breakdown
    if (res.statusCode > 0 && (res.dnsTimeMs > 0 || res.connectTimeMs > 0 || res.sslHandshakeTimeMs > 0 || res.ttfbMs > 0 || res.latencyMs > 0)) {
        m_timingDetails->setText(QString("DNS: %1ms · TCP: %2ms · SSL: %3ms · TTFB: %4ms")
            .arg(static_cast<int>(res.dnsTimeMs))
            .arg(static_cast<int>(res.connectTimeMs))
            .arg(static_cast<int>(res.sslHandshakeTimeMs))
            .arg(static_cast<int>(res.ttfbMs)));
        if (m_waterfallWidget) {
            m_waterfallWidget->setTimings(res.dnsTimeMs, res.connectTimeMs, res.sslHandshakeTimeMs, res.ttfbMs, static_cast<double>(res.latencyMs));
            m_waterfallWidget->setVisible(m_waterfallWidget->hasData());
        }
    } else {
        m_timingDetails->clear();
        if (m_waterfallWidget) {
            m_waterfallWidget->clear();
            m_waterfallWidget->setVisible(false);
        }
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
    if (m_copyBtn) {
        m_copyBtn->setText("✓ Copied!");
        QTimer::singleShot(1400, this, [this]() {
            if (m_copyBtn) m_copyBtn->setText("📋 Copy");
        });
    }
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

void ResponseInspector::toggleChartView() {
    m_showingChart = !m_showingChart;
    m_visualizeTable->setVisible(!m_showingChart);
    m_chartViewer->setVisible(m_showingChart);
    m_chartToggleBtn->setText(m_showingChart ? "Show Table" : "Toggle Chart");
}

void ResponseInspector::updateVisualizeTab(const core::ResponseModel& res) {
    m_visualizeTable->setSortingEnabled(false);
    m_visualizeTable->setRowCount(0);
    m_visualizeTable->setColumnCount(0);
    m_chartViewer->clear();
    m_showingChart = false;
    m_visualizeTable->setVisible(true);
    m_chartViewer->setVisible(false);
    m_chartToggleBtn->setText("Toggle Chart");

    if (!res.isJson() || res.rawBody.trimmed().isEmpty()) {
        m_visualizeStats->setText("Visualizer requires JSON response");
        m_chartToggleBtn->setEnabled(false);
        return;
    }

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(res.rawBody, &parseErr);
    if (parseErr.error != QJsonParseError::NoError) {
        m_visualizeStats->setText("Invalid JSON");
        m_chartToggleBtn->setEnabled(false);
        return;
    }

    QJsonArray array;
    if (doc.isArray()) {
        array = doc.array();
    } else if (doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj.contains("data") && obj["data"].isArray()) {
            array = obj["data"].toArray();
        } else if (obj.contains("items") && obj["items"].isArray()) {
            array = obj["items"].toArray();
        } else if (obj.contains("results") && obj["results"].isArray()) {
            array = obj["results"].toArray();
        }
    }

    if (!array.isEmpty() && array.first().isObject()) {
        // Collect all column keys
        QStringList headers;
        for (const auto& val : array) {
            if (val.isObject()) {
                for (const QString& key : val.toObject().keys()) {
                    if (!headers.contains(key)) {
                        headers.append(key);
                    }
                }
            }
        }

        m_visualizeTable->setColumnCount(headers.size());
        m_visualizeTable->setHorizontalHeaderLabels(headers);
        m_visualizeTable->setRowCount(array.size());

        QString labelKey;
        QString numericKey;

        // Determine best label and numeric column for chart
        for (const QString& h : headers) {
            QString lower = h.toLower();
            if (labelKey.isEmpty() && (lower.contains("name") || lower.contains("title") || lower.contains("id") || lower.contains("label") || lower.contains("key"))) {
                labelKey = h;
            }
        }
        if (labelKey.isEmpty() && !headers.isEmpty()) labelKey = headers.first();

        for (const QString& h : headers) {
            if (h != labelKey) {
                bool isNum = true;
                int checkCount = qMin(array.size(), 10);
                for (int i = 0; i < checkCount; ++i) {
                    QJsonValue v = array[i].toObject().value(h);
                    if (!v.isDouble()) { isNum = false; break; }
                }
                if (isNum) {
                    numericKey = h;
                    break;
                }
            }
        }

        double maxVal = 0.0;
        QList<QPair<QString, double>> chartData;

        for (int r = 0; r < array.size(); ++r) {
            QJsonObject rowObj = array[r].toObject();
            for (int c = 0; c < headers.size(); ++c) {
                const QString& key = headers[c];
                QJsonValue val = rowObj.value(key);
                QString displayStr;
                if (val.isObject()) {
                    displayStr = QJsonDocument(val.toObject()).toJson(QJsonDocument::Compact);
                } else if (val.isArray()) {
                    displayStr = QJsonDocument(val.toArray()).toJson(QJsonDocument::Compact);
                } else {
                    displayStr = val.toVariant().toString();
                }
                auto* item = new QTableWidgetItem(displayStr);
                m_visualizeTable->setItem(r, c, item);
            }

            if (!numericKey.isEmpty()) {
                QString lbl = rowObj.value(labelKey).toVariant().toString();
                double val = rowObj.value(numericKey).toDouble();
                if (val > maxVal) maxVal = val;
                chartData.append({lbl, val});
            }
        }

        m_visualizeStats->setText(QString("%1 rows × %2 columns").arg(array.size()).arg(headers.size()));
        m_chartToggleBtn->setEnabled(!numericKey.isEmpty());

        if (!chartData.isEmpty() && maxVal > 0.0) {
            QString chart;
            chart += QString("=== Visual Bar Chart (%1 vs %2) ===\n\n").arg(labelKey, numericKey);
            const int maxBarWidth = 40;
            for (const auto& pair : chartData) {
                int barLen = static_cast<int>((pair.second / maxVal) * maxBarWidth);
                if (barLen < 1 && pair.second > 0) barLen = 1;
                QString bar = QString("█").repeated(barLen);
                chart += QString("%1 | %2 %3\n").arg(pair.first.leftJustified(18, ' ').left(18), bar, QString::number(pair.second, 'f', 2));
            }
            m_chartViewer->setPlainText(chart);
        } else {
            m_chartViewer->setPlainText("No numeric columns found to plot.");
        }
    } else if (doc.isObject()) {
        // Single JSON Object: Display Property / Value / Type
        QJsonObject obj = doc.object();
        QStringList keys = obj.keys();
        m_visualizeTable->setColumnCount(3);
        m_visualizeTable->setHorizontalHeaderLabels({"Property", "Value", "Type"});
        m_visualizeTable->setRowCount(keys.size());

        for (int r = 0; r < keys.size(); ++r) {
            const QString& k = keys[r];
            QJsonValue v = obj.value(k);
            QString typeName = "string";
            QString valStr;
            if (v.isBool()) { typeName = "boolean"; valStr = v.toBool() ? "true" : "false"; }
            else if (v.isDouble()) { typeName = "number"; valStr = QString::number(v.toDouble()); }
            else if (v.isArray()) { typeName = QString("array [%1]").arg(v.toArray().size()); valStr = QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact); }
            else if (v.isObject()) { typeName = "object"; valStr = QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact); }
            else if (v.isNull()) { typeName = "null"; valStr = "null"; }
            else { valStr = v.toString(); }

            m_visualizeTable->setItem(r, 0, new QTableWidgetItem(k));
            m_visualizeTable->setItem(r, 1, new QTableWidgetItem(valStr));
            m_visualizeTable->setItem(r, 2, new QTableWidgetItem(typeName));
        }
        m_visualizeStats->setText(QString("%1 properties").arg(keys.size()));
        m_chartToggleBtn->setEnabled(false);
        m_chartViewer->setPlainText("Chart view requires an array of numeric records.");
    } else {
        m_visualizeStats->setText("Array of primitive values");
        m_chartToggleBtn->setEnabled(false);
    }

    m_visualizeTable->setSortingEnabled(true);
    m_visualizeTable->resizeColumnsToContents();
}

void ResponseInspector::updateCookiesTab(const core::ResponseModel& res) {
    m_cookiesTable->setRowCount(0);
    int cookieCount = 0;

    for (const auto& h : res.headers) {
        if (h.name.compare("Set-Cookie", Qt::CaseInsensitive) == 0) {
            const QStringList parts = h.value.split(';', Qt::SkipEmptyParts);
            if (parts.isEmpty()) continue;

            QString name, value, domain, path, expires;
            bool secure = false;
            bool httpOnly = false;

            const QString first = parts.first().trimmed();
            int eqIdx = first.indexOf('=');
            if (eqIdx != -1) {
                name = first.left(eqIdx).trimmed();
                value = first.mid(eqIdx + 1).trimmed();
            } else {
                name = first;
            }

            for (int i = 1; i < parts.size(); ++i) {
                QString attr = parts[i].trimmed();
                int aEq = attr.indexOf('=');
                QString attrName = (aEq != -1 ? attr.left(aEq) : attr).trimmed();
                QString attrVal = (aEq != -1 ? attr.mid(aEq + 1) : QString()).trimmed();

                if (attrName.compare("Domain", Qt::CaseInsensitive) == 0) {
                    domain = attrVal;
                } else if (attrName.compare("Path", Qt::CaseInsensitive) == 0) {
                    path = attrVal;
                } else if (attrName.compare("Expires", Qt::CaseInsensitive) == 0) {
                    expires = attrVal;
                } else if (attrName.compare("Max-Age", Qt::CaseInsensitive) == 0) {
                    if (expires.isEmpty()) expires = attrVal + " s";
                    else expires += " (" + attrVal + "s)";
                } else if (attrName.compare("Secure", Qt::CaseInsensitive) == 0) {
                    secure = true;
                } else if (attrName.compare("HttpOnly", Qt::CaseInsensitive) == 0) {
                    httpOnly = true;
                }
            }

            int row = m_cookiesTable->rowCount();
            m_cookiesTable->insertRow(row);
            m_cookiesTable->setItem(row, 0, new QTableWidgetItem(name));
            m_cookiesTable->setItem(row, 1, new QTableWidgetItem(value));
            m_cookiesTable->setItem(row, 2, new QTableWidgetItem(domain.isEmpty() ? "—" : domain));
            m_cookiesTable->setItem(row, 3, new QTableWidgetItem(path.isEmpty() ? "/" : path));
            m_cookiesTable->setItem(row, 4, new QTableWidgetItem(expires.isEmpty() ? "Session" : expires));

            auto* secItem = new QTableWidgetItem(secure ? "✓" : "✗");
            secItem->setTextAlignment(Qt::AlignCenter);
            secItem->setForeground(secure ? QColor("#10b981") : QColor("#71717a"));
            m_cookiesTable->setItem(row, 5, secItem);

            auto* httpItem = new QTableWidgetItem(httpOnly ? "✓" : "✗");
            httpItem->setTextAlignment(Qt::AlignCenter);
            httpItem->setForeground(httpOnly ? QColor("#10b981") : QColor("#71717a"));
            m_cookiesTable->setItem(row, 6, httpItem);

            cookieCount++;
        }
    }

    m_tabWidget->setTabText(7, QString("Cookies (%1)").arg(cookieCount));
}

QString ResponseInspector::httpStatusExplanation(int code) {
    switch (code) {
        // 2xx Success
        case 200: return "200 OK — The request succeeded and the server returned the requested payload.";
        case 201: return "201 Created — The request succeeded and a new resource was created.";
        case 202: return "202 Accepted — The request has been accepted for processing, but processing is not complete.";
        case 204: return "204 No Content — The server successfully processed the request, but is not returning any content.";
        // 3xx Redirection
        case 301: return "301 Moved Permanently — This and all future requests should be directed to the given URI.";
        case 302: return "302 Found — The resource temporarily resides under a different URI.";
        case 304: return "304 Not Modified — Resource has not been modified since the version specified in request headers.";
        case 307: return "307 Temporary Redirect — The request should be repeated with another URI, but future requests should still use original URI.";
        case 308: return "308 Permanent Redirect — The request and all future requests should be repeated using another URI.";
        // 4xx Client Errors
        case 400: return "400 Bad Request — The server cannot or will not process the request due to perceived client error.";
        case 401: return "401 Unauthorized — Authentication is required and has failed or has not been provided.";
        case 403: return "403 Forbidden — The request was valid, but the server is refusing action (insufficient permissions).";
        case 404: return "404 Not Found — The requested resource could not be found on the server.";
        case 405: return "405 Method Not Allowed — A request method is not supported for the requested resource.";
        case 408: return "408 Request Timeout — The server timed out waiting for the request.";
        case 409: return "409 Conflict — The request could not be processed because of conflict in current state of resource.";
        case 410: return "410 Gone — The resource requested is no longer available and will not be available again.";
        case 413: return "413 Payload Too Large — The request is larger than the server is willing or able to process.";
        case 415: return "415 Unsupported Media Type — The payload format is in an unsupported format.";
        case 422: return "422 Unprocessable Content — The request was well-formed but was unable to be followed due to semantic errors.";
        case 429: return "429 Too Many Requests — The user has sent too many requests in a given amount of time (rate limited).";
        // 5xx Server Errors
        case 500: return "500 Internal Server Error — A generic error message given when an unexpected condition was encountered.";
        case 501: return "501 Not Implemented — The server either does not recognize the request method, or lacks ability to fulfill it.";
        case 502: return "502 Bad Gateway — The server was acting as a gateway or proxy and received an invalid response from upstream.";
        case 503: return "503 Service Unavailable — The server cannot handle the request (usually overloaded or down for maintenance).";
        case 504: return "504 Gateway Timeout — The server was acting as a gateway or proxy and did not receive a timely response from upstream.";
        default:
            if (code >= 200 && code < 300) return QString("%1 Success").arg(code);
            if (code >= 300 && code < 400) return QString("%1 Redirection").arg(code);
            if (code >= 400 && code < 500) return QString("%1 Client Error").arg(code);
            if (code >= 500 && code < 600) return QString("%1 Server Error").arg(code);
            return QString("HTTP %1").arg(code);
    }
}

void ResponseInspector::updateJsonTreeTab(const core::ResponseModel& res) {
    m_jsonTreeWidget->clear();
    m_jsonTreeSearch->clear();

    const int treeIndex = m_tabWidget->indexOf(m_jsonTreeTab);
    if (!res.isJson()) {
        if (treeIndex >= 0) m_tabWidget->setTabText(treeIndex, "JSON Tree");
        auto* placeholder = new QTreeWidgetItem(m_jsonTreeWidget);
        placeholder->setText(0, "(not JSON)");
        placeholder->setForeground(0, QBrush(QColor("#71717a")));
        return;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(res.rawBody, &err);
    if (doc.isNull()) {
        if (treeIndex >= 0) m_tabWidget->setTabText(treeIndex, "JSON Tree");
        auto* placeholder = new QTreeWidgetItem(m_jsonTreeWidget);
        placeholder->setText(0, "Parse error: " + err.errorString());
        placeholder->setForeground(0, QBrush(QColor("#ef4444")));
        return;
    }

    QJsonValue root;
    if (doc.isObject()) root = doc.object();
    else root = doc.array();

    auto* rootItem = new QTreeWidgetItem(m_jsonTreeWidget);
    rootItem->setText(0, doc.isObject() ? "{}" : "[]");
    rootItem->setText(2, doc.isObject() ? "object" : "array");
    rootItem->setForeground(2, QBrush(QColor("#f59e0b")));
    populateJsonTree(rootItem, root);
    m_jsonTreeWidget->expandToDepth(1);

    int count = 0;
    if (doc.isObject()) count = doc.object().size();
    else count = doc.array().size();
    if (treeIndex >= 0) m_tabWidget->setTabText(treeIndex, QString("JSON Tree (%1)").arg(count));
}

/*static*/ void ResponseInspector::populateJsonTree(QTreeWidgetItem* parent, const QJsonValue& val, const QString& key) {
    if (val.isObject()) {
        QJsonObject obj = val.toObject();
        for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
            auto* child = new QTreeWidgetItem(parent);
            child->setText(0, it.key());
            if (it.value().isObject()) {
                child->setText(1, QString("{%1 keys}").arg(it.value().toObject().size()));
                child->setText(2, "object");
                child->setForeground(2, QBrush(QColor("#f59e0b")));
                populateJsonTree(child, it.value(), it.key());
            } else if (it.value().isArray()) {
                child->setText(1, QString("[%1 items]").arg(it.value().toArray().size()));
                child->setText(2, "array");
                child->setForeground(2, QBrush(QColor("#f59e0b")));
                populateJsonTree(child, it.value(), it.key());
            } else if (it.value().isString()) {
                child->setText(1, it.value().toString());
                child->setText(2, "string");
                child->setForeground(2, QBrush(QColor("#4ade80")));
            } else if (it.value().isDouble()) {
                double d = it.value().toDouble();
                child->setText(1, (d == static_cast<qint64>(d))
                    ? QString::number(static_cast<qint64>(d))
                    : QString::number(d, 'g', 15));
                child->setText(2, "number");
                child->setForeground(2, QBrush(QColor("#60a5fa")));
            } else if (it.value().isBool()) {
                child->setText(1, it.value().toBool() ? "true" : "false");
                child->setText(2, "bool");
                child->setForeground(2, QBrush(QColor("#c084fc")));
            } else if (it.value().isNull()) {
                child->setText(1, "null");
                child->setText(2, "null");
                child->setForeground(2, QBrush(QColor("#71717a")));
            }
        }
    } else if (val.isArray()) {
        QJsonArray arr = val.toArray();
        for (int i = 0; i < arr.size(); ++i) {
            auto* child = new QTreeWidgetItem(parent);
            child->setText(0, QString::number(i));
            const QJsonValue& elem = arr.at(i);
            if (elem.isObject()) {
                child->setText(1, QString("{%1 keys}").arg(elem.toObject().size()));
                child->setText(2, "object");
                child->setForeground(2, QBrush(QColor("#f59e0b")));
                populateJsonTree(child, elem, QString::number(i));
            } else if (elem.isArray()) {
                child->setText(1, QString("[%1 items]").arg(elem.toArray().size()));
                child->setText(2, "array");
                child->setForeground(2, QBrush(QColor("#f59e0b")));
                populateJsonTree(child, elem, QString::number(i));
            } else if (elem.isString()) {
                child->setText(1, elem.toString());
                child->setText(2, "string");
                child->setForeground(2, QBrush(QColor("#4ade80")));
            } else if (elem.isDouble()) {
                double d = elem.toDouble();
                child->setText(1, (d == static_cast<qint64>(d))
                    ? QString::number(static_cast<qint64>(d))
                    : QString::number(d, 'g', 15));
                child->setText(2, "number");
                child->setForeground(2, QBrush(QColor("#60a5fa")));
            } else if (elem.isBool()) {
                child->setText(1, elem.toBool() ? "true" : "false");
                child->setText(2, "bool");
                child->setForeground(2, QBrush(QColor("#c084fc")));
            } else if (elem.isNull()) {
                child->setText(1, "null");
                child->setText(2, "null");
                child->setForeground(2, QBrush(QColor("#71717a")));
            }
        }
    }
    Q_UNUSED(key)
}

bool ResponseInspector::filterJsonTree(QTreeWidgetItem* item, const QString& query, bool ancestorMatch) {
    if (!item) return false;
    const bool selfMatch = query.isEmpty()
        || item->text(0).contains(query, Qt::CaseInsensitive)
        || item->text(1).contains(query, Qt::CaseInsensitive);
    const bool showDescendants = ancestorMatch || selfMatch;

    bool childMatch = false;
    for (int i = 0; i < item->childCount(); ++i) {
        if (filterJsonTree(item->child(i), query, showDescendants)) childMatch = true;
    }

    const bool visible = query.isEmpty() || selfMatch || childMatch || ancestorMatch;
    item->setHidden(!visible);
    if (!query.isEmpty() && visible && (selfMatch || childMatch)) item->setExpanded(true);
    return selfMatch || childMatch;
}

} // namespace poppy::gui
