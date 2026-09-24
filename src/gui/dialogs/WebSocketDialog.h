#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QSplitter>
#include <QCheckBox>
#include <network/WebSocketClient.h>
#include <components/KeyValueTable.h>

namespace poppy::network {
class CurlNetworkEngine;
}

namespace poppy::gui {

class WebSocketDialog : public QDialog {
    Q_OBJECT
public:
    explicit WebSocketDialog(QWidget* parent = nullptr);
    ~WebSocketDialog() override;

    void setNetworkEngine(network::CurlNetworkEngine* engine) { m_engine = engine; }

private slots:
    void onConnectClicked();
    void onSendClicked();
    void onSendPingClicked();
    void onClearLogClicked();
    void onWsConnected();
    void onWsDisconnected();
    void onWsStateChanged(poppy::network::WebSocketClient::State state);
    void onWsTextMessageReceived(const QString& message);
    void onWsBinaryMessageReceived(const QByteArray& data);
    void onWsFrameSent(const QString& type, const QString& summary);
    void onWsFrameReceived(const QString& type, const QString& summary);
    void onErrorOccurred(const QString& error);
    void onLogSelectionChanged();

private:
    void setupUi();
    void appendLog(const QString& direction, const QString& type, const QString& summary, const QString& fullPayload);
    void updateStatusBadge(const QString& status, const QString& color);

    network::WebSocketClient* m_client{nullptr};
    network::CurlNetworkEngine* m_engine{nullptr};

    QLineEdit* m_urlEdit{nullptr};
    QPushButton* m_connectBtn{nullptr};
    QLabel* m_statusBadge{nullptr};

    KeyValueTable* m_headersTable{nullptr};

    QTableWidget* m_logTable{nullptr};
    QPlainTextEdit* m_payloadViewer{nullptr};
    QLineEdit* m_filterEdit{nullptr};
    QCheckBox* m_autoScrollCheck{nullptr};

    QPlainTextEdit* m_sendTextEdit{nullptr};
    QPushButton* m_sendBtn{nullptr};
    QPushButton* m_pingBtn{nullptr};

    struct LogEntry {
        QString timestamp;
        QString direction;
        QString type;
        QString payload;
    };
    QList<LogEntry> m_entries;
};

} // namespace poppy::gui
