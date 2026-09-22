#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QString>
#include <QMap>
#include <QHash>
#include <QList>
#include <QDateTime>
#include <core/RequestModel.h>

namespace poppy::network {

struct MockRoute {
    QString id;
    QString method{"GET"}; // GET, POST, PUT, DELETE, PATCH, or *
    QString path{"/"};     // e.g. /api/users
    int statusCode{200};
    QString contentType{"application/json"};
    QMap<QString, QString> headers;
    QString responseBody{"{\n  \"status\": \"success\"\n}"};
    int delayMs{0};
    bool enabled{true};
};

struct MockRequestLog {
    QString id;
    QDateTime timestamp;
    QString method;
    QString path;
    QString queryString;
    QMap<QString, QString> headers;
    QString body;
    int responseCode{200};
    QString matchedRouteId;
};

class MockServer : public QObject {
    Q_OBJECT
public:
    explicit MockServer(QObject* parent = nullptr);
    ~MockServer() override;

    bool start(quint16 port = 8080);
    void stop();
    bool isRunning() const;
    quint16 port() const { return m_port; }
    QString serverUrl() const;

    void addRoute(const MockRoute& route);
    void updateRoute(int index, const MockRoute& route);
    void removeRoute(int index);
    void clearRoutes();
    const QList<MockRoute>& routes() const { return m_routes; }

    void importFromRequests(const QList<core::RequestModel>& requests);

    const QList<MockRequestLog>& logs() const { return m_logs; }
    void clearLogs();

signals:
    void serverStarted(quint16 port);
    void serverStopped();
    void requestReceived(const poppy::network::MockRequestLog& log);
    void errorOccurred(const QString& errorString);

private slots:
    void onNewConnection();

private:
    void handleClientSocket(QTcpSocket* socket);
    const MockRoute* matchRoute(const QString& method, const QString& path) const;

    QTcpServer* m_server{nullptr};
    quint16 m_port{8080};
    QList<MockRoute> m_routes;
    QList<MockRequestLog> m_logs;
    QHash<QTcpSocket*, QByteArray> m_recvBuffers;
};

} // namespace poppy::network
