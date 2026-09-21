#include "ResponseModel.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace poppy::core {

QString ResponseModel::bodyAsString() const {
    return QString::fromUtf8(rawBody);
}

QString ResponseModel::headerValue(const QString& name) const {
    for (const auto& h : headers) {
        if (h.name.compare(name, Qt::CaseInsensitive) == 0) {
            return h.value;
        }
    }
    return {};
}

QString ResponseModel::contentType() const {
    return headerValue("Content-Type");
}

bool ResponseModel::isJson() const {
    const QString type = contentType().toLower();
    if (type.contains("application/json") || type.contains("+json")) {
        return true;
    }
    // Fallback: check if body parses as valid JSON object or array
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(rawBody, &err);
    return (err.error == QJsonParseError::NoError && !doc.isNull());
}

QString ResponseModel::formattedJson() const {
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(rawBody, &err);
    if (err.error == QJsonParseError::NoError && !doc.isNull()) {
        return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
    }
    return bodyAsString();
}

} // namespace poppy::core
