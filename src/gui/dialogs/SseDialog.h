#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QTableWidget>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QCheckBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <core/SseParser.h>
#include <components/KeyValueTable.h>

namespace poppy::gui {

class SseDialog : public QDialog {
    Q_OBJECT
public:
    explicit SseDialog(const QString& initialUrl = {}, QWidget* parent = nullptr);
    ~SseDialog() override;

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onReadyRead();
    void onReplyFinished();
    void onReplyError(QNetworkReply::NetworkError error);
    void onEventSelected();
    void onClearClicked();

private:
    void setupUi();
    void updateStatusBadge(const QString& status, const QString& color);

    QNetworkAccessManager* m_nam{nullptr};
    QNetworkReply* m_reply{nullptr};
    core::SseParser m_parser;

    QLineEdit* m_urlEdit{nullptr};
    QPushButton* m_connectBtn{nullptr};
    QLabel* m_statusBadge{nullptr};
    QLabel* m_eventCountBadge{nullptr};

    KeyValueTable* m_headersTable{nullptr};

    QTableWidget* m_eventsTable{nullptr};
    QPlainTextEdit* m_streamAccumulator{nullptr};
    QPlainTextEdit* m_eventDetailViewer{nullptr};
    QLineEdit* m_filterEdit{nullptr};
    QCheckBox* m_autoScrollCheck{nullptr};
    QCheckBox* m_llmModeCheck{nullptr};

    QList<core::SseEvent> m_events;
};

} // namespace poppy::gui
