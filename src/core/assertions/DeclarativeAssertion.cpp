#include "DeclarativeAssertion.h"
#include <core/ResponseModel.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>

namespace poppy::core {

QString DeclarativeAssertionEvaluator::resolveTargetValue(const QString& target, const ResponseModel& res) {
    QString t = target.trimmed();
    if (t == "res.status") {
        return QString::number(res.statusCode);
    }
    if (t == "res.responseTime" || t == "res.latency") {
        return QString::number(res.latencyMs);
    }
    if (t == "res.size") {
        return QString::number(res.sizeBytes);
    }
    if (t == "res.body") {
        return res.bodyAsString();
    }

    // res.header(Name) or res.header('Name')
    static const QRegularExpression headerRegex(R"(res\.header\s*\(\s*['"]?([^'"]+)['"]?\s*\))");
    auto match = headerRegex.match(t);
    if (match.hasMatch()) {
        QString headerName = match.captured(1);
        return res.headerValue(headerName);
    }

    // res.body.property.subproperty
    if (t.startsWith("res.body.")) {
        QString path = t.mid(9);
        QStringList parts = path.split('.', Qt::SkipEmptyParts);

        QJsonDocument doc = QJsonDocument::fromJson(res.rawBody);
        if (!doc.isObject()) return {};

        QJsonValue current = doc.object();
        for (const QString& part : parts) {
            if (current.isObject()) {
                current = current.toObject().value(part);
            } else {
                return {};
            }
        }

        if (current.isDouble()) {
            return QString::number(current.toDouble());
        } else if (current.isBool()) {
            return current.toBool() ? "true" : "false";
        } else if (current.isString()) {
            return current.toString();
        } else if (current.isNull() || current.isUndefined()) {
            return "null";
        } else {
            return QString::fromUtf8(QJsonDocument(current.toObject()).toJson(QJsonDocument::Compact));
        }
    }

    return {};
}

bool DeclarativeAssertionEvaluator::compare(const QString& actual, const QString& op, const QString& expected) {
    QString o = op.trimmed().toLower();

    // Numeric comparison if both strings parse as numbers
    bool actualIsNum = false;
    bool expIsNum = false;
    double actualNum = actual.toDouble(&actualIsNum);
    double expNum = expected.toDouble(&expIsNum);

    if (o == "eq") {
        if (actualIsNum && expIsNum) return actualNum == expNum;
        return actual.compare(expected, Qt::CaseInsensitive) == 0;
    }
    if (o == "neq") {
        if (actualIsNum && expIsNum) return actualNum != expNum;
        return actual.compare(expected, Qt::CaseInsensitive) != 0;
    }
    if (o == "gt") {
        return actualIsNum && expIsNum && (actualNum > expNum);
    }
    if (o == "gte") {
        return actualIsNum && expIsNum && (actualNum >= expNum);
    }
    if (o == "lt") {
        return actualIsNum && expIsNum && (actualNum < expNum);
    }
    if (o == "lte") {
        return actualIsNum && expIsNum && (actualNum <= expNum);
    }
    if (o == "contains") {
        return actual.contains(expected, Qt::CaseInsensitive);
    }
    if (o == "not_contains") {
        return !actual.contains(expected, Qt::CaseInsensitive);
    }

    return false;
}

TestCaseResult DeclarativeAssertionEvaluator::evaluate(const AssertionRule& rule, const ResponseModel& res) {
    TestCaseResult result;
    result.name = QString("%1 %2 %3").arg(rule.target, rule.op, rule.expected);

    if (!rule.enabled) {
        result.passed = true;
        result.errorMessage = "Disabled";
        return result;
    }

    QString actual = resolveTargetValue(rule.target, res);
    bool pass = compare(actual, rule.op, rule.expected);

    result.passed = pass;
    if (!pass) {
        result.errorMessage = QString("Expected '%1' %2 '%3', but got '%4'")
            .arg(rule.target, rule.op, rule.expected, actual);
    }

    return result;
}

QList<TestCaseResult> DeclarativeAssertionEvaluator::evaluateAll(const QList<AssertionRule>& rules, const ResponseModel& res) {
    QList<TestCaseResult> list;
    for (const auto& r : rules) {
        if (r.enabled && !r.target.isEmpty()) {
            list.append(evaluate(r, res));
        }
    }
    return list;
}

} // namespace poppy::core
