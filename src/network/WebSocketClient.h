#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QUrl>
#include <QSslSocket>
#include <QMap>

namespace poppy::network {

class WebSocketClient : public QObject {
    Q_OBJECT
public:
    enum class State {
        Disconnected,
        Connecting,
        Connected,
        Closing
    };
    Q_ENUM(State)

    explicit WebSocketClient(QObject* parent = nullptr);
    ~WebSocketClient() override;

    void open(const QUrl& url, const QMap<QString, QString>& customHeaders = {});
    void setVerifyPeer(bool verify) { m_verifyPeer = verify; }
    void close(quint16 code = 1000, const QString& reason = {});
    bool isConnected() const;
    State state() const { return m_state; }

    void sendTextMessage(const QString& message);
    void sendBinaryMessage(const QByteArray& data);
    void sendPing(const QByteArray& payload = {});

signals:
    void connected();
    void disconnected();
    void stateChanged(poppy::network::WebSocketClient::State state);
    void textMessageReceived(const QString& message);
    void binaryMessageReceived(const QByteArray& data);
    void frameSent(const QString& type, const QString& summary);
    void frameReceived(const QString& type, const QString& summary);
    void errorOccurred(const QString& errorString);

private slots:
    void onSocketConnected();
    void onSocketEncrypted();
    void onSocketReadyRead();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);

private:
    void setState(State state);
    void sendHandshake();
    void processHandshakeResponse();
    void processFrames();
    void sendFrame(quint8 opcode, const QByteArray& payload);

    QSslSocket* m_socket{nullptr};
    bool m_verifyPeer{true};
    QUrl m_url;
    QMap<QString, QString> m_headers;
    State m_state{State::Disconnected};
    QString m_handshakeKey;
    bool m_handshakeDone{false};
    QByteArray m_rxBuffer;
    QByteArray m_fragmentBuffer;
    quint8 m_fragmentOpcode{0};
};

} // namespace poppy::network
