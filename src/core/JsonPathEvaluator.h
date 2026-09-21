#pragma once

#include <QJsonDocument>
#include <QJsonValue>
#include <QString>

namespace poppy::core {

class JsonPathEvaluator {
public:
    // Evaluates a JSONPath expression (e.g. "$.store.book[*].author" or "users[0].name") against doc.
    // Returns matching QJsonValue (can be Array, Object, String, Double, Bool, or Undefined if not found).
    static QJsonValue evaluate(const QJsonDocument& doc, const QString& path);
    static QJsonValue evaluate(const QJsonValue& root, const QString& path);

    // Formats evaluation result as a formatted JSON string (or primitive string).
    static QString evaluateToString(const QJsonDocument& doc, const QString& path, bool pretty = true);
};

} // namespace poppy::core
