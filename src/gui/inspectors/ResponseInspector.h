#pragma once

#include <QWidget>
#include <QLabel>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QTextBrowser>
#include <QTreeWidget>
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
    void openSearch();
    void setTheme(bool isDark);
    QString currentBody() const { return m_currentResponse.bodyAsString(); }

private slots:
    void copyBodyToClipboard();
    void saveBodyToFile();
    void togglePrettyRaw();
    void toggleWordWrap();
    void onCompareDiffClicked();
    void filterHeaders(const QString& text);
    void onSearchTextChanged(const QString& text);
    void findNext();
    void findPrevious();
    void toggleCaseSensitive();
    void toggleRegex();
    void toggleJsonPathMode();
    void resetFilter();
    void toggleChartView();

signals:
    void storeVariableRequested(const QString& selectedValue);

private:
    void updateTelemetryBar(const core::ResponseModel& res);
    void updateHeadersTable(const core::ResponseModel& res);
    void updateTestsTab(const core::TestReport* testReport);
    void updateSearchHighlights();
    void restoreOriginalBody();

    // Top Telemetry Bar
    QLabel* m_statusBadge;
    QLabel* m_latencyBadge;
    QLabel* m_sizeBadge;
    QLabel* m_timingDetails;
    QPushButton* m_prettyRawToggleBtn;
    QPushButton* m_wordWrapBtn;
    QPushButton* m_copyBtn;
    QPushButton* m_saveToFileBtn;
    QPushButton* m_diffBtn;

    // Tabs
    QTabWidget* m_tabWidget;

    // Body Tab
    QWidget* m_bodyTab;
    QWidget* m_searchToolbar;
    QLineEdit* m_searchEdit;
    QPushButton* m_findPrevBtn;
    QPushButton* m_findNextBtn;
    QLabel* m_matchCountLabel;
    QPushButton* m_caseSensitiveBtn;
    QPushButton* m_regexBtn;
    QPushButton* m_jsonPathModeBtn;
    QPushButton* m_resetFilterBtn;

    QPlainTextEdit* m_bodyViewer;
    JsonSyntaxHighlighter* m_jsonHighlighter;
    bool m_isPretty{true};
    bool m_wordWrap{false};
    core::ResponseModel m_currentResponse;

    // Search & Filter State
    QList<QTextCursor> m_searchMatches;
    int m_currentMatchIndex{-1};
    bool m_isJsonPathMode{false};
    bool m_isCaseSensitive{false};
    bool m_isRegex{false};
    bool m_isFiltered{false};

    // Preview Tab
    QTextBrowser* m_previewBrowser;

    // Hex Tab
    QPlainTextEdit* m_hexViewer;

    // Headers Tab
    QWidget* m_headersTab;
    QLineEdit* m_headersFilter;
    QTableWidget* m_headersTable;

    // Tests Tab
    QWidget* m_testsTab;
    QLabel* m_testSummaryLabel;
    QTableWidget* m_testsTable;

    // SSL / TLS Certificate Tab
    QWidget* m_sslTab;
    QPlainTextEdit* m_sslCertViewer;

    // Visualize Tab
    QWidget* m_visualizeTab{nullptr};
    QLineEdit* m_visualizeFilter{nullptr};
    QLabel* m_visualizeStats{nullptr};
    QPushButton* m_chartToggleBtn{nullptr};
    QTableWidget* m_visualizeTable{nullptr};
    QPlainTextEdit* m_chartViewer{nullptr};
    bool m_showingChart{false};

    // Cookies Tab
    QWidget* m_cookiesTab{nullptr};
    QTableWidget* m_cookiesTable{nullptr};

    // JSON Tree Tab
    QWidget* m_jsonTreeTab{nullptr};
    QLineEdit* m_jsonTreeSearch{nullptr};
    QTreeWidget* m_jsonTreeWidget{nullptr};

    void updateVisualizeTab(const core::ResponseModel& res);
    void updateCookiesTab(const core::ResponseModel& res);
    void updateJsonTreeTab(const core::ResponseModel& res);
    static void populateJsonTree(QTreeWidgetItem* parent, const QJsonValue& val, const QString& key = {});
    void filterJsonTree(QTreeWidgetItem* item, const QString& query);
    static QString httpStatusExplanation(int code);
};

} // namespace poppy::gui
