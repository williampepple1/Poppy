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
    // 1. Dynamic generators (faker and runtime utilities)
    if (name == QLatin1String("$guid") || name == QLatin1String("$randomUUID")) {
        if (outScope) *outScope = "Dynamic ($guid)";
        return QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    if (name == QLatin1String("$timestamp")) {
        if (outScope) *outScope = "Dynamic ($timestamp)";
        return QString::number(QDateTime::currentSecsSinceEpoch());
    }
    if (name == QLatin1String("$timestampMs")) {
        if (outScope) *outScope = "Dynamic ($timestampMs)";
        return QString::number(QDateTime::currentMSecsSinceEpoch());
    }
    if (name == QLatin1String("$isoTimestamp")) {
        if (outScope) *outScope = "Dynamic ($isoTimestamp)";
        return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    }
    if (name == QLatin1String("$randomInt")) {
        if (outScope) *outScope = "Dynamic ($randomInt)";
        return QString::number(QRandomGenerator::global()->bounded(1, 1000));
    }
    if (name == QLatin1String("$randomEmail")) {
        if (outScope) *outScope = "Dynamic ($randomEmail)";
        quint32 n = QRandomGenerator::global()->bounded(100, 9999);
        return QString("user%1@example.com").arg(n);
    }
    if (name == QLatin1String("$randomFirstName")) {
        if (outScope) *outScope = "Dynamic ($randomFirstName)";
        static const QStringList names = {"Alex", "Jordan", "Taylor", "Morgan", "Sam", "Chris", "Pat", "Riley", "Casey", "Avery"};
        return names.at(QRandomGenerator::global()->bounded(names.size()));
    }
    if (name == QLatin1String("$randomLastName")) {
        if (outScope) *outScope = "Dynamic ($randomLastName)";
        static const QStringList names = {"Smith", "Johnson", "Williams", "Brown", "Jones", "Garcia", "Miller", "Davis", "Taylor"};
        return names.at(QRandomGenerator::global()->bounded(names.size()));
    }
    if (name == QLatin1String("$randomFullName")) {
        if (outScope) *outScope = "Dynamic ($randomFullName)";
        static const QStringList firsts = {"Alex", "Jordan", "Taylor", "Morgan", "Sam", "Chris"};
        static const QStringList lasts = {"Smith", "Johnson", "Williams", "Brown", "Davis"};
        return QString("%1 %2").arg(firsts.at(QRandomGenerator::global()->bounded(firsts.size())),
                                    lasts.at(QRandomGenerator::global()->bounded(lasts.size())));
    }
    if (name == QLatin1String("$randomBoolean")) {
        if (outScope) *outScope = "Dynamic ($randomBoolean)";
        return (QRandomGenerator::global()->bounded(2) == 1) ? QStringLiteral("true") : QStringLiteral("false");
    }
    if (name == QLatin1String("$randomPrice")) {
        if (outScope) *outScope = "Dynamic ($randomPrice)";
        double price = QRandomGenerator::global()->bounded(100, 9999) / 100.0;
        return QString::number(price, 'f', 2);
    }
    if (name == QLatin1String("$randomColor")) {
        if (outScope) *outScope = "Dynamic ($randomColor)";
        static const QStringList colors = {"#10b981", "#f59e0b", "#3b82f6", "#ef4444", "#8b5cf6", "#06b6d4"};
        return colors.at(QRandomGenerator::global()->bounded(colors.size()));
    }

    // Nearest scope wins: runtime, then folder, collection, environment, global.
    if (m_runtimeVars.contains(name)) {
        if (outScope) *outScope = "Runtime";
        return m_runtimeVars.value(name);
    }

    if (m_folderVars.contains(name)) {
        if (outScope) *outScope = "Folder";
        return m_folderVars.value(name);
    }

    if (m_collectionVars.contains(name)) {
        if (outScope) *outScope = "Collection";
        return m_collectionVars.value(name);
    }

    if (m_envVars.contains(name)) {
        if (outScope) *outScope = "Environment";
        return m_envVars.value(name);
    }

    if (m_globals.contains(name)) {
        if (outScope) *outScope = "Global";
        return m_globals.value(name);
    }

    if (outScope) *outScope = "Unresolved";
    return {};
}

QMap<QString, QPair<QString, QString>> VariableResolver::allAvailableVariables() const {
    QMap<QString, QPair<QString, QString>> result;
    // Dynamic utility tokens
    result["$guid"] = {"Random UUID v4", "Dynamic"};
    result["$randomUUID"] = {"Random UUID v4", "Dynamic"};
    result["$timestamp"] = {"Current Unix epoch (s)", "Dynamic"};
    result["$timestampMs"] = {"Current Unix epoch (ms)", "Dynamic"};
    result["$isoTimestamp"] = {"Current UTC ISO-8601 string", "Dynamic"};
    result["$randomInt"] = {"Random integer (1-1000)", "Dynamic"};
    result["$randomEmail"] = {"Random email address", "Dynamic"};
    result["$randomFirstName"] = {"Random first name", "Dynamic"};
    result["$randomLastName"] = {"Random last name", "Dynamic"};
    result["$randomFullName"] = {"Random full name", "Dynamic"};
    result["$randomBoolean"] = {"Random boolean (true/false)", "Dynamic"};
    result["$randomPrice"] = {"Random price (0.00-99.99)", "Dynamic"};
    result["$randomColor"] = {"Random hex color", "Dynamic"};

    for (auto it = m_globals.cbegin(); it != m_globals.cend(); ++it) {
        result[it.key()] = {it.value(), "Global"};
    }
    for (auto it = m_envVars.cbegin(); it != m_envVars.cend(); ++it) {
        result[it.key()] = {it.value(), "Environment"};
    }
    for (auto it = m_collectionVars.cbegin(); it != m_collectionVars.cend(); ++it) {
        result[it.key()] = {it.value(), "Collection"};
    }
    for (auto it = m_folderVars.cbegin(); it != m_folderVars.cend(); ++it) {
        result[it.key()] = {it.value(), "Folder"};
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
    for (int pass = 0; pass < 8; ++pass) {
        if (!result.contains(QLatin1String("{{"))) break;
        QString next = result;
        int offset = 0;
        bool changed = false;
        auto matchIterator = regex.globalMatch(result);
        while (matchIterator.hasNext()) {
            auto match = matchIterator.next();
            QString varName = match.captured(1).trimmed();

            QString scoped;
            QString value = lookupVariableWithScope(varName, &scoped);
            if (scoped == "Unresolved") continue;

            int matchStart = match.capturedStart(0) + offset;
            int matchLength = match.capturedLength(0);
            next.replace(matchStart, matchLength, value);
            offset += (value.length() - matchLength);
            changed = true;
        }
        if (!changed || next == result) break;
        result = next;
    }
    return result;
}

RequestModel VariableResolver::resolveRequest(const RequestModel& req) const {
    if (!req.runtimeVariables.isEmpty()) {
        VariableResolver copy = *this;
        for (const auto& var : req.runtimeVariables) {
            if (!var.enabled || var.name.isEmpty() || copy.m_runtimeVars.contains(var.name)) continue;
            copy.m_runtimeVars.insert(var.name, var.value);
        }
        RequestModel stripped = req;
        stripped.runtimeVariables.clear();
        return copy.resolveRequest(stripped);
    }

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
