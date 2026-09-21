#pragma once

#include <QString>
#include <QByteArray>
#include <QList>
#include "RequestModel.h"

namespace poppy::core {

class ResponseModel {
public:
    ResponseModel() = default;

    int statusCode{0};
    QString statusText;
    QList<HttpHeader> headers;
    QByteArray rawBody;

    qint64 latencyMs{0};
    qint64 sizeBytes{0};

    // Telemetry breakdown (ms)
    double dnsTimeMs{0.0};
    double connectTimeMs{0.0};
    double sslHandshakeTimeMs{0.0};
    double ttfbMs{0.0};

    QString errorString;

    bool isSuccess() const { return errorString.isEmpty() && statusCode > 0; }
    bool isHttpSuccess() const { return statusCode >= 200 && statusCode < 300; }

    QString bodyAsString() const;
    QString headerValue(const QString& name) const;
    QString contentType() const;
    bool isJson() const;
    QString formattedJson() const;
};

} // namespace poppy::core
