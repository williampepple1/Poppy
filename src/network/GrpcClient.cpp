#include "GrpcClient.h"
#include <QRegularExpression>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QThread>
#include <curl/curl.h>

namespace poppy::network {

GrpcClient::GrpcClient(QObject* parent)
    : QObject(parent)
{
}

QString GrpcClient::statusToString(int code) {
    switch (code) {
        case 0: return "OK";
        case 1: return "CANCELLED";
        case 2: return "UNKNOWN";
        case 3: return "INVALID_ARGUMENT";
        case 4: return "DEADLINE_EXCEEDED";
        case 5: return "NOT_FOUND";
        case 6: return "ALREADY_EXISTS";
        case 7: return "PERMISSION_DENIED";
        case 8: return "RESOURCE_EXHAUSTED";
        case 9: return "FAILED_PRECONDITION";
        case 10: return "ABORTED";
        case 11: return "OUT_OF_RANGE";
        case 12: return "UNIMPLEMENTED";
        case 13: return "INTERNAL";
        case 14: return "UNAVAILABLE";
        case 15: return "DATA_LOSS";
        case 16: return "UNAUTHENTICATED";
        default: return QString("STATUS_%1").arg(code);
    }
}

GrpcProtoDefinition GrpcClient::parseProto(const QString& protoContent) {
    GrpcProtoDefinition def;

    // Syntax
    static const QRegularExpression syntaxRegex(R"(syntax\s*=\s*["']([^"']+)["'];)");
    auto syntaxMatch = syntaxRegex.match(protoContent);
    if (syntaxMatch.hasMatch()) {
        def.syntax = syntaxMatch.captured(1);
    }

    // Package
    static const QRegularExpression pkgRegex(R"(package\s+([a-zA-Z0-9_\.]+);)");
    auto pkgMatch = pkgRegex.match(protoContent);
    if (pkgMatch.hasMatch()) {
        def.packageName = pkgMatch.captured(1);
    }

    // Messages
    static const QRegularExpression msgRegex(R"(message\s+([a-zA-Z0-9_]+)\s*\{([^}]+)\})");
    auto msgIt = msgRegex.globalMatch(protoContent);
    while (msgIt.hasNext()) {
        auto m = msgIt.next();
        QString msgName = m.captured(1);
        QString body = m.captured(2);

        QStringList fields;
        QStringList lines = body.split(';', Qt::SkipEmptyParts);
        for (const auto& line : lines) {
            QString trimmed = line.trimmed();
            if (trimmed.isEmpty() || trimmed.startsWith("//")) continue;
            // e.g. "string name = 1" or "int32 id = 2"
            static const QRegularExpression fieldRegex(R"((?:repeated\s+)?([a-zA-Z0-9_\.]+)\s+([a-zA-Z0-9_]+)\s*=\s*\d+)");
            auto fMatch = fieldRegex.match(trimmed);
            if (fMatch.hasMatch()) {
                QString type = fMatch.captured(1);
                QString name = fMatch.captured(2);
                fields.append(QString("%1:%2").arg(name, type));
            }
        }
        def.messageFields[msgName] = fields;
    }

    // Services
    static const QRegularExpression serviceRegex(R"(service\s+([a-zA-Z0-9_]+)\s*\{([^}]+)\})");
    auto srvIt = serviceRegex.globalMatch(protoContent);
    while (srvIt.hasNext()) {
        auto sm = srvIt.next();
        GrpcService srv;
        srv.name = sm.captured(1);
        srv.fullName = def.packageName.isEmpty() ? srv.name : (def.packageName + "." + srv.name);

        QString body = sm.captured(2);
        // rpc MethodName ( [stream] InputType ) returns ( [stream] OutputType );
        static const QRegularExpression rpcRegex(R"(rpc\s+([a-zA-Z0-9_]+)\s*\(\s*(stream\s+)?([a-zA-Z0-9_\.]+)\s*\)\s*returns\s*\(\s*(stream\s+)?([a-zA-Z0-9_\.]+)\s*\))");
        auto rpcIt = rpcRegex.globalMatch(body);
        while (rpcIt.hasNext()) {
            auto rm = rpcIt.next();
            GrpcMethod method;
            method.name = rm.captured(1);
            method.clientStreaming = !rm.captured(2).isEmpty();
            method.inputType = rm.captured(3).trimmed();
            method.serverStreaming = !rm.captured(4).isEmpty();
            method.outputType = rm.captured(5).trimmed();
            srv.methods.append(method);
        }

        def.services.append(srv);
    }

    return def;
}

QString GrpcClient::generateSampleJsonForMessage(const QString& messageName, const GrpcProtoDefinition& def) {
    // Strip package prefix if any
    QString key = messageName;
    int dot = key.lastIndexOf('.');
    if (dot != -1) key = key.mid(dot + 1);

    QJsonObject obj;
    if (def.messageFields.contains(key)) {
        const auto& fields = def.messageFields[key];
        for (const auto& f : fields) {
            auto parts = f.split(':');
            if (parts.size() == 2) {
                QString name = parts[0];
                QString type = parts[1];
                if (type == "string") {
                    obj[name] = QString("sample_%1").arg(name);
                } else if (type == "int32" || type == "int64" || type == "uint32" || type == "uint64") {
                    obj[name] = 100;
                } else if (type == "bool") {
                    obj[name] = true;
                } else if (type == "float" || type == "double") {
                    obj[name] = 3.14;
                } else {
                    obj[name] = QJsonObject();
                }
            }
        }
    }

    if (obj.isEmpty()) {
        obj["message"] = "Hello, gRPC!";
    }

    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

static size_t grpcWriteCallback(void* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* buffer = static_cast<QByteArray*>(userdata);
    buffer->append(static_cast<const char*>(ptr), static_cast<int>(size * nmemb));
    return size * nmemb;
}

static size_t grpcHeaderCallback(void* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* response = static_cast<GrpcResponse*>(userdata);
    int total = static_cast<int>(size * nmemb);
    QString line = QString::fromUtf8(static_cast<const char*>(ptr), total).trimmed();
    if (!line.isEmpty()) {
        int colon = line.indexOf(':');
        if (colon != -1) {
            QString k = line.left(colon).trimmed().toLower();
            QString v = line.mid(colon + 1).trimmed();
            response->responseHeaders[k] = v;

            if (k == "grpc-status") {
                response->statusCode = v.toInt();
                response->statusName = GrpcClient::statusToString(response->statusCode);
            } else if (k == "grpc-message") {
                response->statusMessage = v;
            }
        }
    }
    return size * nmemb;
}

void GrpcClient::invokeUnary(const QString& endpoint,
                             const QString& fullMethodPath,
                             const QString& jsonPayload,
                             const QMap<QString, QString>& metadata,
                             bool useTls,
                             int timeoutMs)
{
    emit callStarted();

    // Execute in worker thread to prevent blocking GUI
    QThread::create([this, endpoint, fullMethodPath, jsonPayload, metadata, useTls, timeoutMs]() {
        executeHttp2Call(endpoint, fullMethodPath, jsonPayload, metadata, useTls, timeoutMs);
    })->start();
}

void GrpcClient::executeHttp2Call(const QString& endpoint,
                                 const QString& fullMethodPath,
                                 const QString& payload,
                                 const QMap<QString, QString>& metadata,
                                 bool useTls,
                                 int timeoutMs)
{
    GrpcResponse res;
    QElapsedTimer timer;
    timer.start();

    CURL* curl = curl_easy_init();
    if (!curl) {
        res.success = false;
        res.errorMessage = "Failed to initialize libcurl for gRPC";
        emit callFinished(res);
        return;
    }

    QString scheme = useTls ? "https://" : "http://";
    QString cleanedEndpoint = endpoint;
    if (cleanedEndpoint.startsWith("http://")) cleanedEndpoint.remove(0, 7);
    if (cleanedEndpoint.startsWith("https://")) cleanedEndpoint.remove(0, 8);

    QString methodPath = fullMethodPath;
    if (!methodPath.startsWith("/")) methodPath = "/" + methodPath;

    QString fullUrl = scheme + cleanedEndpoint + methodPath;

    // Build gRPC framed payload: 1 byte compressed flag (0) + 4 bytes big endian length + payload
    QByteArray payloadBytes = payload.toUtf8();
    QByteArray framedBody;
    framedBody.append(static_cast<char>(0)); // Not compressed
    quint32 len = static_cast<quint32>(payloadBytes.size());
    framedBody.append(static_cast<char>((len >> 24) & 0xFF));
    framedBody.append(static_cast<char>((len >> 16) & 0xFF));
    framedBody.append(static_cast<char>((len >> 8) & 0xFF));
    framedBody.append(static_cast<char>(len & 0xFF));
    framedBody.append(payloadBytes);

    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/grpc");
    headers = curl_slist_append(headers, "TE: trailers");

    for (auto it = metadata.cbegin(); it != metadata.cend(); ++it) {
        if (!it.key().isEmpty()) {
            QString h = QString("%1: %2").arg(it.key(), it.value());
            headers = curl_slist_append(headers, h.toUtf8().constData());
        }
    }

    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.toUtf8().constData());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, framedBody.constData());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, framedBody.size());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Force HTTP/2
    if (useTls) {
        curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    } else {
        curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE);
    }

    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(timeoutMs));

    QByteArray responseBuffer;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, grpcWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, grpcHeaderCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &res);

    CURLcode code = curl_easy_perform(curl);
    res.latencyMs = timer.elapsed();

    if (code != CURLE_OK) {
        res.success = false;
        res.errorMessage = QString("cURL error (%1): %2").arg(code).arg(curl_easy_strerror(code));
        if (res.statusCode == 0) {
            res.statusCode = 14; // UNAVAILABLE
            res.statusName = statusToString(14);
        }
    } else {
        res.success = (res.statusCode == 0);
        res.rawResponseBody = responseBuffer;

        // Decode gRPC response framing if present
        if (responseBuffer.size() >= 5) {
            quint8 flag = static_cast<quint8>(responseBuffer[0]);
            Q_UNUSED(flag);
            quint32 respLen = (static_cast<quint8>(responseBuffer[1]) << 24) |
                              (static_cast<quint8>(responseBuffer[2]) << 16) |
                              (static_cast<quint8>(responseBuffer[3]) << 8)  |
                              static_cast<quint8>(responseBuffer[4]);
            if (responseBuffer.size() >= static_cast<int>(5 + respLen)) {
                QByteArray inner = responseBuffer.mid(5, static_cast<int>(respLen));
                res.responseBody = QString::fromUtf8(inner);
            } else {
                res.responseBody = QString::fromUtf8(responseBuffer.mid(5));
            }
        } else {
            res.responseBody = QString::fromUtf8(responseBuffer);
        }

        // If response is valid JSON, format it indented
        QJsonParseError parseErr;
        auto doc = QJsonDocument::fromJson(res.responseBody.toUtf8(), &parseErr);
        if (parseErr.error == QJsonParseError::NoError && (doc.isObject() || doc.isArray())) {
            res.responseBody = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
        }
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    emit callFinished(res);
}

} // namespace poppy::network
