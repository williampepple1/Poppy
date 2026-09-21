#pragma once

#include <QWidget>
#include <QLabel>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QPushButton>
#include <QLineEdit>
#include <components/JsonSyntaxHighlighter.h>
#include <core/ResponseModel.h>
#include <core/ScriptRunner.h>

namespace poppy::gui {

class ResponseInspector : public QWidget {
    Q_OBJECT
public:
    explicit ResponseInspector(QWidget* parent = nullptr);

    void setResponse(const core::ResponseModel& res, const core::TestReport* testReport = nullptr);
    void clear();

private slots:
    void copyBodyToClipboard();
    void togglePrettyRaw();
    void filterHeaders(const QString& text);

private:
    void updateTelemetryBar(const core::ResponseModel& res);
    void updateHeadersTable(const core::ResponseModel& res);
    void updateTestsTab(const core::TestReport* testReport);

    // Top Telemetry Bar
    QLabel* m_statusBadge;
    QLabel* m_latencyBadge;
    QLabel* m_sizeBadge;
    QLabel* m_timingDetails;
    QPushButton* m_prettyRawToggleBtn;
    QPushButton* m_copyBtn;

    // Tabs
    QTabWidget* m_tabWidget;

    // Body Tab
    QPlainTextEdit* m_bodyViewer;
    JsonSyntaxHighlighter* m_jsonHighlighter;
    bool m_isPretty{true};
    core::ResponseModel m_currentResponse;

    // Headers Tab
    QWidget* m_headersTab;
    QLineEdit* m_headersFilter;
    QTableWidget* m_headersTable;

    // Tests Tab
    QWidget* m_testsTab;
    QLabel* m_testSummaryLabel;
    QTableWidget* m_testsTable;
};

} // namespace poppy::gui
