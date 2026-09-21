#include "JsonPathEvaluator.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>

namespace poppy::core {

namespace {

enum class TokenType {
    Property,
    Index,
    Wildcard
};

struct PathToken {
    TokenType type;
    QString prop;
    int index{-1};
};

QList<PathToken> parsePath(QString path) {
    QList<PathToken> tokens;
    path = path.trimmed();
    if (path.isEmpty()) return tokens;

    // Strip leading '$' or '$.'
    if (path.startsWith("$.")) {
        path = path.mid(2);
    } else if (path == "$") {
        return tokens;
    } else if (path.startsWith("$[")) {
        path = path.mid(1);
    }

    int i = 0;
    int len = path.length();

    while (i < len) {
        QChar c = path[i];

        if (c == '.') {
            ++i;
            continue;
        }

        if (c == '[') {
            int closeIdx = path.indexOf(']', i + 1);
            if (closeIdx == -1) break;

            QString inner = path.mid(i + 1, closeIdx - i - 1).trimmed();
            if (inner == "*") {
                tokens.append({TokenType::Wildcard, "", -1});
            } else {
                bool ok = false;
                int idx = inner.toInt(&ok);
                if (ok) {
                    tokens.append({TokenType::Index, "", idx});
                } else {
                    // String inside bracket like ['prop'] or ["prop"]
                    if ((inner.startsWith('\'') && inner.endsWith('\'')) ||
                        (inner.startsWith('"') && inner.endsWith('"'))) {
                        inner = inner.mid(1, inner.length() - 2);
                    }
                    tokens.append({TokenType::Property, inner, -1});
                }
            }
            i = closeIdx + 1;
        } else {
            // Read property name until '.' or '['
            int start = i;
            while (i < len && path[i] != '.' && path[i] != '[') {
                ++i;
            }
            QString prop = path.mid(start, i - start).trimmed();
            if (!prop.isEmpty()) {
                tokens.append({TokenType::Property, prop, -1});
            }
        }
    }

    return tokens;
}

} // namespace

QJsonValue JsonPathEvaluator::evaluate(const QJsonDocument& doc, const QString& path) {
    if (doc.isObject()) {
        return evaluate(QJsonValue(doc.object()), path);
    } else if (doc.isArray()) {
        return evaluate(QJsonValue(doc.array()), path);
    }
    return QJsonValue(QJsonValue::Undefined);
}

QJsonValue JsonPathEvaluator::evaluate(const QJsonValue& root, const QString& path) {
    if (root.isUndefined() || root.isNull()) {
        return QJsonValue(QJsonValue::Undefined);
    }

    QList<PathToken> tokens = parsePath(path);
    if (tokens.isEmpty()) {
        return root;
    }

    QJsonValue current = root;

    for (const auto& token : tokens) {
        if (current.isUndefined()) {
            return QJsonValue(QJsonValue::Undefined);
        }

        switch (token.type) {
        case TokenType::Property: {
            if (current.isObject()) {
                QJsonObject obj = current.toObject();
                if (!obj.contains(token.prop)) {
                    return QJsonValue(QJsonValue::Undefined);
                }
                current = obj.value(token.prop);
            } else if (current.isArray()) {
                // Project property across array elements
                QJsonArray resArray;
                for (const auto& el : current.toArray()) {
                    if (el.isObject() && el.toObject().contains(token.prop)) {
                        resArray.append(el.toObject().value(token.prop));
                    }
                }
                current = resArray;
            } else {
                return QJsonValue(QJsonValue::Undefined);
            }
            break;
        }

        case TokenType::Index: {
            if (!current.isArray()) {
                return QJsonValue(QJsonValue::Undefined);
            }
            QJsonArray arr = current.toArray();
            int idx = token.index;
            // Handle negative index
            if (idx < 0) {
                idx = arr.size() + idx;
            }
            if (idx < 0 || idx >= arr.size()) {
                return QJsonValue(QJsonValue::Undefined);
            }
            current = arr.at(idx);
            break;
        }

        case TokenType::Wildcard: {
            if (current.isArray()) {
                // Keep as array
            } else if (current.isObject()) {
                QJsonArray allVals;
                for (const auto& val : current.toObject()) {
                    allVals.append(val);
                }
                current = allVals;
            } else {
                return QJsonValue(QJsonValue::Undefined);
            }
            break;
        }
        }
    }

    return current;
}

QString JsonPathEvaluator::evaluateToString(const QJsonDocument& doc, const QString& path, bool pretty) {
    QJsonValue res = evaluate(doc, path);
    if (res.isUndefined()) {
        return QString();
    }

    if (res.isObject()) {
        return QString::fromUtf8(QJsonDocument(res.toObject()).toJson(pretty ? QJsonDocument::Indented : QJsonDocument::Compact));
    }
    if (res.isArray()) {
        return QString::fromUtf8(QJsonDocument(res.toArray()).toJson(pretty ? QJsonDocument::Indented : QJsonDocument::Compact));
    }
    if (res.isString()) {
        return res.toString();
    }
    if (res.isDouble()) {
        return QString::number(res.toDouble());
    }
    if (res.isBool()) {
        return res.toBool() ? "true" : "false";
    }
    if (res.isNull()) {
        return "null";
    }

    return QString();
}

} // namespace poppy::core
