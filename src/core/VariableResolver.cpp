#include "VariableResolver.h"
#include <QRegularExpression>
#include <QUuid>
#include <QDateTime>
#include <QRandomGenerator>

namespace poppy::core {

void VariableResolver::setEnvironment(const EnvironmentModel& env) {
    m_envVars = env.toMap();
}

QString VariableResolver::lookupVariable(const QString& name) const {
    // 1. Dynamic generators
    if (name == "$guid" || name == "$uuid") {
        return QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    if (name == "$timestamp") {
        return QString::number(QDateTime::currentSecsSinceEpoch());
    }
    if (name == "$isoTimestamp") {
        return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    }
    if (name == "$randomInt") {
        return QString::number(QRandomGenerator::global()->bounded(1, 1000));
    }

    // 2. Runtime variables
    if (m_runtimeVars.contains(name)) {
        return m_runtimeVars.value(name);
    }

    // 3. Environment variables
    if (m_envVars.contains(name)) {
        return m_envVars.value(name);
    }

    // 4. Collection variables
    if (m_collectionVars.contains(name)) {
        return m_collectionVars.value(name);
    }

    // 5. Global variables
    if (m_globals.contains(name)) {
        return m_globals.value(name);
    }

    // If unresolved, leave placeholder intact or return empty? Bruno leaves {{name}} or empties it.
    return {};
}

QString VariableResolver::resolveString(const QString& input) const {
    if (input.isEmpty()) return input;

    static const QRegularExpression regex(R"(\{\{([^{}]+)\}\})");
    QString result = input;
    int offset = 0;

    auto matchIterator = regex.globalMatch(input);
    while (matchIterator.hasNext()) {
        auto match = matchIterator.next();
        QString varName = match.captured(1).trimmed();
        
        // Lookup
        QString resolved = lookupVariable(varName);
        if (!resolved.isEmpty()) {
            // Replace in result
            int matchStart = match.capturedStart(0) + offset;
            int matchLength = match.capturedLength(0);
            result.replace(matchStart, matchLength, resolved);
            offset += (resolved.length() - matchLength);
        }
    }

    return result;
}

RequestModel VariableResolver::resolveRequest(const RequestModel& req) const {
    RequestModel res = req;

    res.url = resolveString(req.url);

    for (auto& param : res.queryParams) {
        param.key = resolveString(param.key);
        param.value = resolveString(param.value);
    }

    for (auto& param : res.pathParams) {
        param.key = resolveString(param.key);
        param.value = resolveString(param.value);
    }

    for (auto& h : res.headers) {
        h.name = resolveString(h.name);
        h.value = resolveString(h.value);
    }

    res.bodyContent = resolveString(req.bodyContent);

    res.auth.bearerToken = resolveString(req.auth.bearerToken);
    res.auth.basicUsername = resolveString(req.auth.basicUsername);
    res.auth.basicPassword = resolveString(req.auth.basicPassword);
    res.auth.apiKeyName = resolveString(req.auth.apiKeyName);
    res.auth.apiKeyValue = resolveString(req.auth.apiKeyValue);
    res.auth.oauth2AccessToken = resolveString(req.auth.oauth2AccessToken);

    return res;
}

} // namespace poppy::core
