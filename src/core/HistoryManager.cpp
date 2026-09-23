#include "HistoryManager.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QStandardPaths>
#include <QCoreApplication>

namespace poppy::core {

namespace {

QJsonArray headersToJson(const QList<HttpHeader>& headers) {
    QJsonArray arr;
    for (const auto& h : headers) {
        QJsonObject ho;
        ho["name"] = h.name;
        ho["value"] = h.value;
        ho["enabled"] = h.enabled;
        ho["description"] = h.description;
        arr.append(ho);
    }
    return arr;
}

QList<HttpHeader> headersFromJson(const QJsonArray& arr) {
    QList<HttpHeader> list;
    for (const auto& hv : arr) {
        QJsonObject ho = hv.toObject();
        list.append(HttpHeader{
            .name = ho["name"].toString(),
            .value = ho["value"].toString(),
            .enabled = ho["enabled"].toBool(true),
            .description = ho["description"].toString()
        });
    }
    return list;
}

QJsonArray paramsToJson(const QList<HttpParam>& params) {
    QJsonArray arr;
    for (const auto& p : params) {
        QJsonObject po;
        po["key"] = p.key;
        po["value"] = p.value;
        po["enabled"] = p.enabled;
        po["description"] = p.description;
        arr.append(po);
    }
    return arr;
}

QList<HttpParam> paramsFromJson(const QJsonArray& arr) {
    QList<HttpParam> list;
    for (const auto& pv : arr) {
        QJsonObject po = pv.toObject();
        list.append(HttpParam{
            .key = po["key"].toString(),
            .value = po["value"].toString(),
            .enabled = po["enabled"].toBool(true),
            .description = po["description"].toString()
        });
    }
    return list;
}

QJsonObject requestToJson(const RequestModel& req) {
    QJsonObject ro;
    ro["name"] = req.name;
    ro["method"] = static_cast<int>(req.method);
    ro["url"] = req.url;
    ro["bodyType"] = static_cast<int>(req.bodyType);
    ro["bodyContent"] = req.bodyContent;
    ro["graphqlQuery"] = req.graphqlQuery;
    ro["graphqlVariables"] = req.graphqlVariables;
    ro["proxy"] = req.proxy;
    ro["seq"] = req.seq;
    ro["description"] = req.description;
    ro["headers"] = headersToJson(req.headers);
    ro["params"] = paramsToJson(req.queryParams);
    ro["pathParams"] = paramsToJson(req.pathParams);

    QJsonArray formArr;
    for (const auto& p : req.formDataParams) {
        QJsonObject fo;
        fo["key"] = p.key;
        fo["value"] = p.value;
        fo["isFile"] = p.isFile;
        fo["enabled"] = p.enabled;
        formArr.append(fo);
    }
    ro["formDataParams"] = formArr;

    auto redact = [](const QString& value) {
        return value.isEmpty() ? QString() : QStringLiteral("***");
    };

    QJsonObject auth;
    auth["type"] = static_cast<int>(req.auth.type);
    auth["bearerToken"] = redact(req.auth.bearerToken);
    auth["basicUsername"] = req.auth.basicUsername;
    auth["basicPassword"] = redact(req.auth.basicPassword);
    auth["apiKeyName"] = req.auth.apiKeyName;
    auth["apiKeyValue"] = redact(req.auth.apiKeyValue);
    auth["apiKeyPlacement"] = req.auth.apiKeyPlacement;
    auth["oauth2AccessToken"] = redact(req.auth.oauth2AccessToken);
    auth["awsAccessKey"] = req.auth.awsAccessKey;
    auth["awsSecretKey"] = redact(req.auth.awsSecretKey);
    auth["awsSessionToken"] = redact(req.auth.awsSessionToken);
    auth["awsRegion"] = req.auth.awsRegion;
    auth["awsService"] = req.auth.awsService;
    auth["digestUsername"] = req.auth.digestUsername;
    auth["digestPassword"] = redact(req.auth.digestPassword);
    auth["ntlmUsername"] = req.auth.ntlmUsername;
    auth["ntlmPassword"] = redact(req.auth.ntlmPassword);
    auth["ntlmDomain"] = req.auth.ntlmDomain;
    auth["ntlmWorkstation"] = req.auth.ntlmWorkstation;
    ro["auth"] = auth;

    QJsonObject scripts;
    scripts["preRequest"] = req.scripts.preRequestScript;
    scripts["postResponse"] = req.scripts.postResponseScript;
    scripts["tests"] = req.scripts.tests;
    ro["scripts"] = scripts;

    QJsonArray asArr;
    for (const auto& a : req.assertions) {
        QJsonObject ao;
        ao["target"] = a.target;
        ao["op"] = a.op;
        ao["expected"] = a.expected;
        ao["enabled"] = a.enabled;
        asArr.append(ao);
    }
    ro["assertions"] = asArr;
    return ro;
}

RequestModel requestFromJson(const QJsonObject& ro) {
    RequestModel req;
    req.name = ro["name"].toString();
    req.method = static_cast<HttpMethod>(ro["method"].toInt());
    req.url = ro["url"].toString();
    req.bodyType = static_cast<BodyType>(ro["bodyType"].toInt());
    req.bodyContent = ro["bodyContent"].toString();
    req.graphqlQuery = ro["graphqlQuery"].toString();
    req.graphqlVariables = ro["graphqlVariables"].toString();
    req.proxy = ro["proxy"].toString();
    req.seq = ro["seq"].toInt(1);
    req.description = ro["description"].toString();
    req.headers = headersFromJson(ro["headers"].toArray());
    req.queryParams = paramsFromJson(ro["params"].toArray());
    req.pathParams = paramsFromJson(ro["pathParams"].toArray());

    for (const auto& fv : ro["formDataParams"].toArray()) {
        QJsonObject fo = fv.toObject();
        req.formDataParams.append(FormDataParam{
            .key = fo["key"].toString(),
            .value = fo["value"].toString(),
            .isFile = fo["isFile"].toBool(false),
            .enabled = fo["enabled"].toBool(true)
        });
    }

    auto storedSecret = [](const QString& value) {
        return value == QLatin1String("***") ? QString() : value;
    };

    QJsonObject auth = ro["auth"].toObject();
    req.auth.type = static_cast<AuthType>(auth["type"].toInt());
    req.auth.bearerToken = storedSecret(auth["bearerToken"].toString());
    req.auth.basicUsername = auth["basicUsername"].toString();
    req.auth.basicPassword = storedSecret(auth["basicPassword"].toString());
    req.auth.apiKeyName = auth["apiKeyName"].toString();
    req.auth.apiKeyValue = storedSecret(auth["apiKeyValue"].toString());
    req.auth.apiKeyPlacement = auth["apiKeyPlacement"].toString("header");
    req.auth.oauth2AccessToken = storedSecret(auth["oauth2AccessToken"].toString());
    req.auth.awsAccessKey = auth["awsAccessKey"].toString();
    req.auth.awsSecretKey = storedSecret(auth["awsSecretKey"].toString());
    req.auth.awsSessionToken = storedSecret(auth["awsSessionToken"].toString());
    req.auth.awsRegion = auth["awsRegion"].toString();
    req.auth.awsService = auth["awsService"].toString();
    req.auth.digestUsername = auth["digestUsername"].toString();
    req.auth.digestPassword = storedSecret(auth["digestPassword"].toString());
    req.auth.ntlmUsername = auth["ntlmUsername"].toString();
    req.auth.ntlmPassword = storedSecret(auth["ntlmPassword"].toString());
    req.auth.ntlmDomain = auth["ntlmDomain"].toString();
    req.auth.ntlmWorkstation = auth["ntlmWorkstation"].toString();

    QJsonObject scripts = ro["scripts"].toObject();
    req.scripts.preRequestScript = scripts["preRequest"].toString();
    req.scripts.postResponseScript = scripts["postResponse"].toString();
    req.scripts.tests = scripts["tests"].toString();

    for (const auto& av : ro["assertions"].toArray()) {
        QJsonObject ao = av.toObject();
        req.assertions.append(AssertionRule{
            .target = ao["target"].toString(),
            .op = ao["op"].toString(),
            .expected = ao["expected"].toString(),
            .enabled = ao["enabled"].toBool(true)
        });
    }
    return req;
}

} // namespace

HistoryManager::HistoryManager(QObject* parent) : QObject(parent) {
}

QString HistoryManager::historyFilePath() const {
    if (!m_historyFilePath.isEmpty()) return m_historyFilePath;
    return defaultHistoryFilePath();
}

QString HistoryManager::defaultHistoryFilePath() {
    QString path;
    if (QCoreApplication::instance()) {
        path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    if (path.isEmpty()) {
        path = QDir::tempPath();
    }
    return path + "/poppy_history.json";
}

void HistoryManager::addEntry(const RequestModel& req, const ResponseModel& res) {
    HistoryItem item;
    item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    item.timestamp = QDateTime::currentDateTime();
    item.request = req;
    item.statusCode = res.statusCode;
    item.statusText = res.statusText;
    item.responseTimeMs = res.latencyMs;
    item.responseSizeBytes = res.sizeBytes;
    item.success = res.isSuccess();
    item.errorString = res.errorString;
    // Store first 512KB of raw body to prevent unbounded memory in history
    item.responseRawBody = res.rawBody.left(512 * 1024);
    item.responseHeaders = res.headers;

    m_items.prepend(item);
    while (m_items.size() > m_maxEntries) {
        m_items.removeLast();
    }

    emit entryAdded(item);
    if (m_autoSave) {
        saveToFile(historyFilePath());
    }
}

bool HistoryManager::removeEntry(const QString& id) {
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].id == id) {
            m_items.removeAt(i);
            if (m_autoSave) {
                saveToFile(historyFilePath());
            }
            return true;
        }
    }
    return false;
}

void HistoryManager::clear() {
    m_items.clear();
    if (m_autoSave) {
        saveToFile(historyFilePath());
    }
    emit historyCleared();
}

QList<HistoryItem> HistoryManager::filter(const QString& query) const {
    if (query.trimmed().isEmpty()) {
        return m_items;
    }
    QString lower = query.trimmed().toLower();
    QList<HistoryItem> result;
    for (const auto& it : m_items) {
        QString methodStr = methodToString(it.request.method).toLower();
        QString urlStr = it.request.url.toLower();
        QString codeStr = QString::number(it.statusCode);
        QString nameStr = it.request.name.toLower();
        if (methodStr.contains(lower) || urlStr.contains(lower) || codeStr.contains(lower) || nameStr.contains(lower)) {
            result.append(it);
        }
    }
    return result;
}

bool HistoryManager::loadFromFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        return false;
    }

    m_items.clear();
    QJsonArray arr = doc.array();
    for (const auto& val : arr) {
        if (!val.isObject()) continue;
        QJsonObject obj = val.toObject();

        HistoryItem item;
        item.id = obj["id"].toString();
        item.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODate);
        item.statusCode = obj["statusCode"].toInt();
        item.statusText = obj["statusText"].toString();
        item.responseTimeMs = obj["responseTimeMs"].toVariant().toLongLong();
        item.responseSizeBytes = obj["responseSizeBytes"].toVariant().toLongLong();
        item.success = obj["success"].toBool();
        item.errorString = obj["errorString"].toString();
        item.responseRawBody = QByteArray::fromBase64(obj["responseRawBody"].toString().toUtf8());

        item.responseHeaders = headersFromJson(obj["responseHeaders"].toArray());
        item.request = requestFromJson(obj["request"].toObject());

        m_items.append(item);
        if (m_items.size() >= m_maxEntries) break;
    }
    return true;
}

bool HistoryManager::saveToFile(const QString& filePath) const {
    QFileInfo info(filePath);
    QDir dir = info.dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QJsonArray arr;
    for (const auto& item : m_items) {
        QJsonObject obj;
        obj["id"] = item.id;
        obj["timestamp"] = item.timestamp.toString(Qt::ISODate);
        obj["statusCode"] = item.statusCode;
        obj["statusText"] = item.statusText;
        obj["responseTimeMs"] = static_cast<qint64>(item.responseTimeMs);
        obj["responseSizeBytes"] = static_cast<qint64>(item.responseSizeBytes);
        obj["success"] = item.success;
        obj["errorString"] = item.errorString;
        obj["responseRawBody"] = QString::fromUtf8(item.responseRawBody.toBase64());
        obj["responseHeaders"] = headersToJson(item.responseHeaders);
        obj["request"] = requestToJson(item.request);
        arr.append(obj);
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    return true;
}

} // namespace poppy::core
