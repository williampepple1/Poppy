#include "MockServer.h"
#include <QUrl>
#include <QUrlQuery>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <QPointer>

namespace poppy::network {

MockServer::MockServer(QObject* parent)
    : QObject(parent)
{
}

MockServer::~MockServer() {
    stop();
}

bool MockServer::start(quint16 port) {
    stop();
    m_port = port;
    m_server = new QTcpServer(this);

    connect(m_server, &QTcpServer::newConnection, this, &MockServer::onNewConnection);

    if (!m_server->listen(QHostAddress::Any, m_port)) {
        emit errorOccurred(m_server->errorString());
        delete m_server;
        m_server = nullptr;
        return false;
    }

    emit serverStarted(m_port);
    return true;
}

void MockServer::stop() {
    if (m_server) {
        m_recvBuffers.clear();
        m_server->close();
        delete m_server;
        m_server = nullptr;
        emit serverStopped();
    }
}

bool MockServer::isRunning() const {
    return m_server != nullptr && m_server->isListening();
}

QString MockServer::serverUrl() const {
    if (!isRunning()) return QString();
    return QString("http://127.0.0.1:%1").arg(m_port);
}

void MockServer::addRoute(const MockRoute& route) {
    MockRoute r = route;
    if (r.id.isEmpty()) {
        r.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    m_routes.append(r);
}

void MockServer::updateRoute(int index, const MockRoute& route) {
    if (index >= 0 && index < m_routes.size()) {
        m_routes[index] = route;
    }
}

void MockServer::removeRoute(int index) {
    if (index >= 0 && index < m_routes.size()) {
        m_routes.removeAt(index);
    }
}

void MockServer::clearRoutes() {
    m_routes.clear();
}

void MockServer::clearLogs() {
    m_logs.clear();
}

void MockServer::importFromRequests(const QList<core::RequestModel>& requests) {
    for (const auto& req : requests) {
        QUrl url(req.url);
        QString path = url.path();
        if (path.isEmpty()) path = "/";

        MockRoute route;
        route.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        route.method = core::methodToString(req.method);
        route.path = path;
        route.statusCode = (req.method == core::HttpMethod::POST) ? 201 : 200;
        route.contentType = "application/json";

        if (!req.bodyContent.isEmpty() && req.bodyType == core::BodyType::Json) {
            route.responseBody = req.bodyContent;
        } else {
            route.responseBody = QString("{\n  \"message\": \"Mock response for %1 %2\"\n}")
                                     .arg(route.method, route.path);
        }
        addRoute(route);
    }
}

const MockRoute* MockServer::matchRoute(const QString& method, const QString& path) const {
    for (const auto& r : m_routes) {
        if (!r.enabled) continue;

        bool methodMatch = (r.method == "*" || r.method.compare(method, Qt::CaseInsensitive) == 0);
        if (!methodMatch) continue;

        // Path match: exact or wildcard prefix or param matching
        if (r.path == path || r.path == "*") {
            return &r;
        }

        // Compare ignoring trailing slash
        QString rPath = r.path.endsWith('/') && r.path.size() > 1 ? r.path.chopped(1) : r.path;
        QString reqPath = path.endsWith('/') && path.size() > 1 ? path.chopped(1) : path;
        if (rPath == reqPath) {
            return &r;
        }
    }
    return nullptr;
}

void MockServer::onNewConnection() {
    while (m_server && m_server->hasPendingConnections()) {
        QTcpSocket* socket = m_server->nextPendingConnection();
        socket->setParent(m_server);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            handleClientSocket(socket);
        });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
            m_recvBuffers.remove(socket);
            socket->deleteLater();
        });
    }
}

static bool decodeChunkedBody(const QByteArray& in, QByteArray& bodyOut, int& consumed) {
    int pos = 0;
    QByteArray body;
    while (pos < in.size()) {
        const int lineEnd = in.indexOf("\r\n", pos);
        if (lineEnd < 0) return false;
        bool ok = false;
        const int chunkSize = in.mid(pos, lineEnd - pos).trimmed().toInt(&ok, 16);
        if (!ok || chunkSize < 0) return false;
        pos = lineEnd + 2;
        if (chunkSize == 0) {
            const int trailEnd = in.indexOf("\r\n", pos);
            if (trailEnd < 0) return false;
            consumed = trailEnd + 2;
            bodyOut = body;
            return true;
        }
        if (pos + chunkSize + 2 > in.size()) return false;
        body.append(in.mid(pos, chunkSize));
        pos += chunkSize;
        if (in.mid(pos, 2) != "\r\n") return false;
        pos += 2;
    }
    return false;
}

void MockServer::handleClientSocket(QTcpSocket* socket) {
    if (!socket) return;
    m_recvBuffers[socket].append(socket->readAll());
    QByteArray& rawData = m_recvBuffers[socket];
    int headerEnd = rawData.indexOf("\r\n\r\n");
    if (headerEnd == -1) return;

    QByteArray headerBytes = rawData.left(headerEnd);
    QString headersPart = QString::fromUtf8(headerBytes);
    QByteArray bodyBytesSoFar = rawData.mid(headerEnd + 4);

    QStringList lines = headersPart.split("\r\n");
    if (lines.isEmpty()) return;

    bool hasContentLength = false;
    int contentLength = 0;
    bool isChunked = false;
    for (int i = 1; i < lines.size(); ++i) {
        int colon = lines[i].indexOf(':');
        if (colon == -1) continue;
        const QString hName = lines[i].left(colon).trimmed();
        const QString hVal = lines[i].mid(colon + 1).trimmed();
        if (hName.compare("Content-Length", Qt::CaseInsensitive) == 0) {
            hasContentLength = true;
            contentLength = hVal.toInt();
        } else if (hName.compare("Transfer-Encoding", Qt::CaseInsensitive) == 0
                   && hVal.contains("chunked", Qt::CaseInsensitive)) {
            isChunked = true;
        }
    }

    QByteArray bodyBytes;
    if (hasContentLength) {
        if (contentLength < 0 || bodyBytesSoFar.size() < contentLength) return;
        bodyBytes = bodyBytesSoFar.left(contentLength);
        rawData.remove(0, headerEnd + 4 + contentLength);
    } else if (isChunked) {
        int consumed = 0;
        if (!decodeChunkedBody(bodyBytesSoFar, bodyBytes, consumed)) return;
        rawData.remove(0, headerEnd + 4 + consumed);
    } else {
        bodyBytes = bodyBytesSoFar;
        rawData.remove(0, headerEnd + 4 + bodyBytesSoFar.size());
    }
    QString bodyPart = QString::fromUtf8(bodyBytes);

    QString requestLine = lines.first();
    auto reqParts = requestLine.split(' ');
    if (reqParts.size() < 2) return;

    QString method = reqParts[0].trimmed();
    QString fullUri = reqParts[1].trimmed();

    QUrl parsed("http://localhost" + fullUri);
    QString path = parsed.path();
    QString query = parsed.query();

    QMap<QString, QString> reqHeaders;
    for (int i = 1; i < lines.size(); ++i) {
        int colon = lines[i].indexOf(':');
        if (colon != -1) {
            reqHeaders[lines[i].left(colon).trimmed().toLower()] = lines[i].mid(colon + 1).trimmed();
        }
    }

    // Handle OPTIONS CORS preflight
    if (method.compare("OPTIONS", Qt::CaseInsensitive) == 0) {
        QByteArray resp;
        resp.append("HTTP/1.1 204 No Content\r\n");
        resp.append("Access-Control-Allow-Origin: *\r\n");
        resp.append("Access-Control-Allow-Methods: GET, POST, PUT, DELETE, PATCH, OPTIONS\r\n");
        resp.append("Access-Control-Allow-Headers: *\r\n");
        resp.append("Content-Length: 0\r\n\r\n");
        socket->write(resp);
        socket->flush();
        socket->disconnectFromHost();
        return;
    }

    const MockRoute* route = matchRoute(method, path);

    MockRequestLog log;
    log.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    log.timestamp = QDateTime::currentDateTime();
    log.method = method;
    log.path = path;
    log.queryString = query;
    log.headers = reqHeaders;
    log.body = bodyPart;

    int status = route ? route->statusCode : 404;
    QString respBody = route ? route->responseBody : QString("{\"error\": \"No mock route found for %1 %2\"}").arg(method, path);
    QString contentType = route ? route->contentType : "application/json";
    int delay = route ? route->delayMs : 0;

    log.responseCode = status;
    log.matchedRouteId = route ? route->id : QString();
    m_logs.append(log);
    emit requestReceived(log);

    MockRoute routeCopy;
    bool hasRoute = false;
    if (route) {
        routeCopy = *route;
        hasRoute = true;
    }

    auto sendResponse = [socket, status, contentType, respBody, hasRoute, routeCopy]() {
        if (!socket || !socket->isOpen()) return;

        QByteArray bodyBytes = respBody.toUtf8();
        QByteArray resp;
        resp.append(QString("HTTP/1.1 %1 %2\r\n").arg(status).arg(status == 200 ? "OK" : (status == 201 ? "Created" : "Not Found")).toUtf8());
        resp.append(QString("Content-Type: %1\r\n").arg(contentType).toUtf8());
        resp.append("Access-Control-Allow-Origin: *\r\n");
        resp.append("Connection: close\r\n");

        if (hasRoute) {
            for (auto it = routeCopy.headers.cbegin(); it != routeCopy.headers.cend(); ++it) {
                if (!it.key().isEmpty()) {
                    resp.append(QString("%1: %2\r\n").arg(it.key(), it.value()).toUtf8());
                }
            }
        }

        resp.append(QString("Content-Length: %1\r\n\r\n").arg(bodyBytes.size()).toUtf8());
        resp.append(bodyBytes);

        socket->write(resp);
        socket->flush();
        socket->disconnectFromHost();
    };

    if (delay > 0) {
        QPointer<QTcpSocket> safeSocket(socket);
        QTimer::singleShot(delay, this, [sendResponse, safeSocket]() {
            if (safeSocket) sendResponse();
        });
    } else {
        sendResponse();
    }
}

} // namespace poppy::network
