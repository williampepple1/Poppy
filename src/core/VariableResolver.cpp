#include "VariableResolver.h"
#include <QRegularExpression>
#include <QUuid>
#include <QDateTime>
#include <QRandomGenerator>
#include <optional>

namespace poppy::core {

void VariableResolver::setEnvironment(const EnvironmentModel& env) {
    m_envVars = env.toMap();
}

QString VariableResolver::lookupVariable(const QString& name) const {
    return lookupVariableWithScope(name, nullptr);
}

QString VariableResolver::lookupVariableWithScope(const QString& name, QString* outScope) const {
    // 1. Dynamic generators
    if (name.startsWith("$guid")) {
        if (outScope) *outScope = "Dynamic ($guid)";
        return QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    if (name.startsWith("$timestamp")) {
        if (outScope) *outScope = "Dynamic ($timestamp)";
        return QString::number(QDateTime::currentSecsSinceEpoch());
    }
    if (name.startsWith("$randomInt")) {
        if (outScope) *outScope = "Dynamic ($randomInt)";
        return QString::number(QRandomGenerator::global()->bounded(1000));
    }

    // 2. Script runtime variables
    if (m_runtimeVars.contains(name)) {
        if (outScope) *outScope = "Runtime";
        return m_runtimeVars.value(name);
    }

    // 3. Environment variables
    if (m_envVars.contains(name)) {
        if (outScope) *outScope = "Environment";
        return m_envVars.value(name);
    }

    // 3b. Folder-level variables (scoped to folder hierarchy)
    if (m_folderVars.contains(name)) {
        if (outScope) *outScope = "Folder";
        return m_folderVars.value(name);
    }

    // 4. Collection variables
    if (m_collectionVars.contains(name)) {
        if (outScope) *outScope = "Collection";
        return m_collectionVars.value(name);
    }

    // 5. Global variables
    if (m_globals.contains(name)) {
        if (outScope) *outScope = "Global";
        return m_globals.value(name);
    }

    if (outScope) *outScope = "Unresolved";
    return {};
}

QMap<QString, QPair<QString, QString>> VariableResolver::allAvailableVariables() const {
    QMap<QString, QPair<QString, QString>> result;
    for (auto it = m_globals.cbegin(); it != m_globals.cend(); ++it) {
        result[it.key()] = {it.value(), "Global"};
    }
    for (auto it = m_collectionVars.cbegin(); it != m_collectionVars.cend(); ++it) {
        result[it.key()] = {it.value(), "Collection"};
    }
    for (auto it = m_folderVars.cbegin(); it != m_folderVars.cend(); ++it) {
        result[it.key()] = {it.value(), "Folder"};
    }
    for (auto it = m_envVars.cbegin(); it != m_envVars.cend(); ++it) {
        result[it.key()] = {it.value(), "Environment"};
    }
    for (auto it = m_runtimeVars.cbegin(); it != m_runtimeVars.cend(); ++it) {
        result[it.key()] = {it.value(), "Runtime"};
    }
    return result;
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

        std::optional<QString> resolved;
        QString scoped;
        QString value = lookupVariableWithScope(varName, &scoped);
        if (scoped != "Unresolved") {
            resolved = value;
        }

        if (resolved) {
            int matchStart = match.capturedStart(0) + offset;
            int matchLength = match.capturedLength(0);
            result.replace(matchStart, matchLength, *resolved);
            offset += (resolved->length() - matchLength);
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
    res.graphqlQuery = resolveString(req.graphqlQuery);
    res.graphqlVariables = resolveString(req.graphqlVariables);
    res.proxy = resolveString(req.proxy);

    for (auto& p : res.formDataParams) {
        p.key = resolveString(p.key);
        p.value = resolveString(p.value);
    }

    res.auth.bearerToken = resolveString(req.auth.bearerToken);
    res.auth.basicUsername = resolveString(req.auth.basicUsername);
    res.auth.basicPassword = resolveString(req.auth.basicPassword);
    res.auth.apiKeyName = resolveString(req.auth.apiKeyName);
    res.auth.apiKeyValue = resolveString(req.auth.apiKeyValue);
    res.auth.oauth2AccessToken = resolveString(req.auth.oauth2AccessToken);
    res.auth.awsAccessKey = resolveString(req.auth.awsAccessKey);
    res.auth.awsSecretKey = resolveString(req.auth.awsSecretKey);
    res.auth.awsSessionToken = resolveString(req.auth.awsSessionToken);
    res.auth.awsRegion = resolveString(req.auth.awsRegion);
    res.auth.awsService = resolveString(req.auth.awsService);
    res.auth.digestUsername = resolveString(req.auth.digestUsername);
    res.auth.digestPassword = resolveString(req.auth.digestPassword);
    res.auth.ntlmUsername = resolveString(req.auth.ntlmUsername);
    res.auth.ntlmPassword = resolveString(req.auth.ntlmPassword);
    res.auth.ntlmDomain = resolveString(req.auth.ntlmDomain);
    res.auth.ntlmWorkstation = resolveString(req.auth.ntlmWorkstation);

    return res;
}

} // namespace poppy::core
