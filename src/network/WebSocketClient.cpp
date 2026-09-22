#include "WebSocketClient.h"
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QDateTime>
#include <QDebug>

namespace poppy::network {

WebSocketClient::WebSocketClient(QObject* parent)
    : QObject(parent)
{
}

WebSocketClient::~WebSocketClient() {
    close();
}

void WebSocketClient::setState(State state) {
    if (m_state != state) {
        m_state = state;
        emit stateChanged(m_state);
    }
}

bool WebSocketClient::isConnected() const {
    return m_state == State::Connected && m_handshakeDone;
}

void WebSocketClient::open(const QUrl& url, const QMap<QString, QString>& customHeaders) {
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }

    m_url = url;
    m_headers = customHeaders;
    m_handshakeDone = false;
    m_rxBuffer.clear();
    m_fragmentBuffer.clear();
    m_fragmentOpcode = 0;

    QString scheme = url.scheme().toLower();
    bool isSecure = (scheme == "wss" || scheme == "https");
    int port = url.port(isSecure ? 443 : 80);

    setState(State::Connecting);

    m_socket = new QSslSocket(this);

    connect(m_socket, &QIODevice::readyRead, this, &WebSocketClient::onSocketReadyRead);
    connect(m_socket, &QAbstractSocket::disconnected, this, &WebSocketClient::onSocketDisconnected);
    connect(m_socket, &QAbstractSocket::errorOccurred, this, &WebSocketClient::onSocketError);

    if (isSecure) {
        connect(m_socket, &QSslSocket::encrypted, this, &WebSocketClient::onSocketEncrypted);
        m_socket->connectToHostEncrypted(url.host(), static_cast<quint16>(port));
    } else {
        connect(m_socket, &QAbstractSocket::connected, this, &WebSocketClient::onSocketConnected);
        m_socket->connectToHost(url.host(), static_cast<quint16>(port));
    }
}

void WebSocketClient::close(quint16 code, const QString& reason) {
    if (!m_socket || m_state == State::Disconnected) {
        setState(State::Disconnected);
        return;
    }

    if (m_handshakeDone) {
        setState(State::Closing);
        // Build close frame payload: 2 bytes code + reason
        QByteArray payload;
        payload.append(static_cast<char>((code >> 8) & 0xFF));
        payload.append(static_cast<char>(code & 0xFF));
        payload.append(reason.toUtf8());
        sendFrame(0x8, payload);
        emit frameSent("CLOSE", QString("Code: %1, Reason: %2").arg(code).arg(reason));
    }

    m_socket->disconnectFromHost();
}

void WebSocketClient::onSocketConnected() {
    sendHandshake();
}

void WebSocketClient::onSocketEncrypted() {
    sendHandshake();
}

void WebSocketClient::sendHandshake() {
    QByteArray nonce(16, 0);
    for (int i = 0; i < 16; ++i) {
        nonce[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    m_handshakeKey = QString::fromLatin1(nonce.toBase64());

    QString path = m_url.path();
    if (path.isEmpty()) path = "/";
    if (m_url.hasQuery()) path += "?" + m_url.query();

    QString hostHeader = m_url.host();
    int port = m_url.port();
    if (port > 0 && port != 80 && port != 443) {
        hostHeader += QString(":%1").arg(port);
    }

    QByteArray request;
    request.append(QString("GET %1 HTTP/1.1\r\n").arg(path).toUtf8());
    request.append(QString("Host: %1\r\n").arg(hostHeader).toUtf8());
    request.append("Upgrade: websocket\r\n");
    request.append("Connection: Upgrade\r\n");
    request.append(QString("Sec-WebSocket-Key: %1\r\n").arg(m_handshakeKey).toUtf8());
    request.append("Sec-WebSocket-Version: 13\r\n");

    for (auto it = m_headers.cbegin(); it != m_headers.cend(); ++it) {
        if (!it.key().trimmed().isEmpty()) {
            request.append(QString("%1: %2\r\n").arg(it.key(), it.value()).toUtf8());
        }
    }
    request.append("\r\n");

    m_socket->write(request);
    m_socket->flush();
}

void WebSocketClient::onSocketReadyRead() {
    m_rxBuffer.append(m_socket->readAll());

    if (!m_handshakeDone) {
        processHandshakeResponse();
        if (!m_handshakeDone) return;
    }

    processFrames();
}

void WebSocketClient::processHandshakeResponse() {
    int headerEnd = m_rxBuffer.indexOf("\r\n\r\n");
    if (headerEnd == -1) return;

    QByteArray headerBytes = m_rxBuffer.left(headerEnd);
    m_rxBuffer.remove(0, headerEnd + 4);

    QString headerStr = QString::fromUtf8(headerBytes);
    QStringList lines = headerStr.split("\r\n", Qt::SkipEmptyParts);
    if (lines.isEmpty()) {
        emit errorOccurred("Empty response from server");
        close();
        return;
    }

    QString statusLine = lines.first();
    if (!statusLine.contains("101")) {
        emit errorOccurred(QString("Handshake failed: %1").arg(statusLine));
        close();
        return;
    }

    // Verify Sec-WebSocket-Accept
    QString expectedAccept = QCryptographicHash::hash(
        (m_handshakeKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").toUtf8(),
        QCryptographicHash::Sha1
    ).toBase64();

    bool acceptFound = false;
    for (int i = 1; i < lines.size(); ++i) {
        const auto& line = lines[i];
        int colonIdx = line.indexOf(':');
        if (colonIdx != -1) {
            QString name = line.left(colonIdx).trimmed();
            QString val = line.mid(colonIdx + 1).trimmed();
            if (name.compare("Sec-WebSocket-Accept", Qt::CaseInsensitive) == 0) {
                acceptFound = true;
                if (val != expectedAccept) {
                    emit errorOccurred("Invalid Sec-WebSocket-Accept received from server");
                    close();
                    return;
                }
            }
        }
    }

    if (!acceptFound) {
        emit errorOccurred("Missing Sec-WebSocket-Accept in server response");
        close();
        return;
    }

    m_handshakeDone = true;
    setState(State::Connected);
    emit connected();
}

void WebSocketClient::processFrames() {
    while (m_rxBuffer.size() >= 2) {
        quint8 byte0 = static_cast<quint8>(m_rxBuffer[0]);
        quint8 byte1 = static_cast<quint8>(m_rxBuffer[1]);

        quint8 opcode = (byte0 & 0x0F);
        bool fin = (byte0 & 0x80) != 0;
        bool hasMask = (byte1 & 0x80) != 0;
        quint64 payloadLen = (byte1 & 0x7F);

        int offset = 2;
        if (payloadLen == 126) {
            if (m_rxBuffer.size() < offset + 2) return;
            quint16 len16 = (static_cast<quint8>(m_rxBuffer[2]) << 8) | static_cast<quint8>(m_rxBuffer[3]);
            payloadLen = len16;
            offset += 2;
        } else if (payloadLen == 127) {
            if (m_rxBuffer.size() < offset + 8) return;
            payloadLen = 0;
            for (int i = 0; i < 8; ++i) {
                payloadLen = (payloadLen << 8) | static_cast<quint8>(m_rxBuffer[offset + i]);
            }
            offset += 8;
        }

        QByteArray maskKey;
        if (hasMask) {
            if (m_rxBuffer.size() < offset + 4) return;
            maskKey = m_rxBuffer.mid(offset, 4);
            offset += 4;
        }

        if (static_cast<quint64>(m_rxBuffer.size()) < offset + payloadLen) {
            return; // Not all payload data has arrived yet
        }

        QByteArray payload = m_rxBuffer.mid(offset, static_cast<int>(payloadLen));
        m_rxBuffer.remove(0, offset + static_cast<int>(payloadLen));

        if (hasMask && maskKey.size() == 4) {
            for (int i = 0; i < payload.size(); ++i) {
                payload[i] = payload[i] ^ maskKey[i % 4];
            }
        }

        auto emitComplete = [&](quint8 completeOpcode, const QByteArray& data) {
            if (completeOpcode == 0x1) {
                emit frameReceived("TEXT", QString("%1 bytes").arg(data.size()));
                emit textMessageReceived(QString::fromUtf8(data));
            } else if (completeOpcode == 0x2) {
                emit frameReceived("BINARY", QString("%1 bytes").arg(data.size()));
                emit binaryMessageReceived(data);
            }
        };

        switch (opcode) {
            case 0x0: { // Continuation
                if (m_fragmentOpcode == 0) {
                    break;
                }
                m_fragmentBuffer.append(payload);
                if (fin) {
                    emitComplete(m_fragmentOpcode, m_fragmentBuffer);
                    m_fragmentBuffer.clear();
                    m_fragmentOpcode = 0;
                }
                break;
            }
            case 0x1: // Text
            case 0x2: { // Binary
                if (!fin) {
                    m_fragmentOpcode = opcode;
                    m_fragmentBuffer = payload;
                } else {
                    emitComplete(opcode, payload);
                }
                break;
            }
            case 0x8: { // Close
                quint16 closeCode = 1000;
                QString reason;
                if (payload.size() >= 2) {
                    closeCode = (static_cast<quint8>(payload[0]) << 8) | static_cast<quint8>(payload[1]);
                    if (payload.size() > 2) {
                        reason = QString::fromUtf8(payload.mid(2));
                    }
                }
                emit frameReceived("CLOSE", QString("Code: %1, Reason: %2").arg(closeCode).arg(reason));
                m_socket->disconnectFromHost();
                break;
            }
            case 0x9: { // Ping
                emit frameReceived("PING", QString("%1 bytes").arg(payload.size()));
                // Send Pong response with matching payload
                sendFrame(0xA, payload);
                emit frameSent("PONG", QString("%1 bytes").arg(payload.size()));
                break;
            }
            case 0xA: { // Pong
                emit frameReceived("PONG", QString("%1 bytes").arg(payload.size()));
                break;
            }
            default:
                break;
        }
    }
}

void WebSocketClient::sendFrame(quint8 opcode, const QByteArray& payload) {
    if (!m_socket || !m_socket->isOpen()) return;

    QByteArray frame;
    quint8 byte0 = 0x80 | (opcode & 0x0F); // FIN=1
    frame.append(static_cast<char>(byte0));

    quint64 len = static_cast<quint64>(payload.size());
    // Client-to-server frames MUST be masked (0x80)
    if (len <= 125) {
        frame.append(static_cast<char>(0x80 | static_cast<quint8>(len)));
    } else if (len <= 65535) {
        frame.append(static_cast<char>(0x80 | 126));
        frame.append(static_cast<char>((len >> 8) & 0xFF));
        frame.append(static_cast<char>(len & 0xFF));
    } else {
        frame.append(static_cast<char>(0x80 | 127));
        for (int i = 7; i >= 0; --i) {
            frame.append(static_cast<char>((len >> (i * 8)) & 0xFF));
        }
    }

    // Generate 4-byte mask
    quint8 mask[4];
    for (int i = 0; i < 4; ++i) {
        mask[i] = static_cast<quint8>(QRandomGenerator::global()->bounded(256));
        frame.append(static_cast<char>(mask[i]));
    }

    // Mask payload
    QByteArray maskedPayload = payload;
    for (int i = 0; i < maskedPayload.size(); ++i) {
        maskedPayload[i] = maskedPayload[i] ^ mask[i % 4];
    }
    frame.append(maskedPayload);

    m_socket->write(frame);
    m_socket->flush();
}

void WebSocketClient::sendTextMessage(const QString& message) {
    if (!isConnected()) return;
    QByteArray utf8 = message.toUtf8();
    sendFrame(0x1, utf8);
    emit frameSent("TEXT", QString("%1 bytes").arg(utf8.size()));
}

void WebSocketClient::sendBinaryMessage(const QByteArray& data) {
    if (!isConnected()) return;
    sendFrame(0x2, data);
    emit frameSent("BINARY", QString("%1 bytes").arg(data.size()));
}

void WebSocketClient::sendPing(const QByteArray& payload) {
    if (!isConnected()) return;
    sendFrame(0x9, payload);
    emit frameSent("PING", QString("%1 bytes").arg(payload.size()));
}

void WebSocketClient::onSocketDisconnected() {
    m_handshakeDone = false;
    setState(State::Disconnected);
    emit disconnected();
}

void WebSocketClient::onSocketError(QAbstractSocket::SocketError error) {
    Q_UNUSED(error);
    if (m_socket) {
        emit errorOccurred(m_socket->errorString());
    }
    m_handshakeDone = false;
    setState(State::Disconnected);
    emit disconnected();
}

} // namespace poppy::network
