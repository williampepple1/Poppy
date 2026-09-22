#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QList>
#include <QByteArray>
#include <atomic>

namespace poppy::network {

struct GrpcMethod {
    QString name;
    QString inputType;
    QString outputType;
    bool clientStreaming{false};
    bool serverStreaming{false};
};

struct GrpcService {
    QString name;
    QString fullName;
    QList<GrpcMethod> methods;
};

struct GrpcProtoDefinition {
    QString packageName;
    QString syntax;
    QList<GrpcService> services;
    QMap<QString, QStringList> messageFields; // messageName -> list of field names/types
};

struct GrpcResponse {
    int statusCode{0};
    QString statusName{"OK"};
    QString statusMessage;
    qint64 latencyMs{0};
    QByteArray rawResponseBody;
    QString responseBody;
    QMap<QString, QString> responseHeaders;
    QMap<QString, QString> responseTrailers;
    bool success{true};
    QString errorMessage;
};

class GrpcClient : public QObject {
    Q_OBJECT
public:
    explicit GrpcClient(QObject* parent = nullptr);
    ~GrpcClient() override;

    static GrpcProtoDefinition parseProto(const QString& protoContent);
    static QString generateSampleJsonForMessage(const QString& messageName, const GrpcProtoDefinition& def);
    static QString statusToString(int code);

    void invokeUnary(const QString& endpoint,
                     const QString& fullMethodPath,
                     const QString& jsonPayload,
                     const QMap<QString, QString>& metadata,
                     bool useTls,
                     int timeoutMs = 10000);
    void cancel();

signals:
    void callStarted();
    void callFinished(const poppy::network::GrpcResponse& response);

private:
    void executeHttp2Call(const QString& endpoint,
                         const QString& fullMethodPath,
                         const QString& payload,
                         const QMap<QString, QString>& metadata,
                         bool useTls,
                         int timeoutMs,
                         uint64_t generation);

    std::atomic<uint64_t> m_generation{0};
};

} // namespace poppy::network
