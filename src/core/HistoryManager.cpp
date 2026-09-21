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

        // Headers
        QJsonArray hArr = obj["responseHeaders"].toArray();
        for (const auto& hv : hArr) {
            QJsonObject ho = hv.toObject();
            item.responseHeaders.append(HttpHeader{
                .name = ho["name"].toString(),
                .value = ho["value"].toString(),
                .enabled = ho["enabled"].toBool(true)
            });
        }

        // Request
        QJsonObject ro = obj["request"].toObject();
        item.request.name = ro["name"].toString();
        item.request.method = static_cast<HttpMethod>(ro["method"].toInt());
        item.request.url = ro["url"].toString();
        item.request.bodyType = static_cast<BodyType>(ro["bodyType"].toInt());
        item.request.bodyContent = ro["bodyContent"].toString();

        QJsonArray reqHArr = ro["headers"].toArray();
        for (const auto& hv : reqHArr) {
            QJsonObject ho = hv.toObject();
            item.request.headers.append(HttpHeader{
                .name = ho["name"].toString(),
                .value = ho["value"].toString(),
                .enabled = ho["enabled"].toBool(true)
            });
        }

        QJsonArray reqPArr = ro["params"].toArray();
        for (const auto& pv : reqPArr) {
            QJsonObject po = pv.toObject();
            item.request.queryParams.append(HttpParam{
                .key = po["key"].toString(),
                .value = po["value"].toString(),
                .enabled = po["enabled"].toBool(true)
            });
        }

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

        // Response headers
        QJsonArray hArr;
        for (const auto& h : item.responseHeaders) {
            QJsonObject ho;
            ho["name"] = h.name;
            ho["value"] = h.value;
            ho["enabled"] = h.enabled;
            hArr.append(ho);
        }
        obj["responseHeaders"] = hArr;

        // Request
        QJsonObject ro;
        ro["name"] = item.request.name;
        ro["method"] = static_cast<int>(item.request.method);
        ro["url"] = item.request.url;
        ro["bodyType"] = static_cast<int>(item.request.bodyType);
        ro["bodyContent"] = item.request.bodyContent;

        QJsonArray reqHArr;
        for (const auto& h : item.request.headers) {
            QJsonObject ho;
            ho["name"] = h.name;
            ho["value"] = h.value;
            ho["enabled"] = h.enabled;
            reqHArr.append(ho);
        }
        ro["headers"] = reqHArr;

        QJsonArray reqPArr;
        for (const auto& p : item.request.queryParams) {
            QJsonObject po;
            po["key"] = p.key;
            po["value"] = p.value;
            po["enabled"] = p.enabled;
            reqPArr.append(po);
        }
        ro["params"] = reqPArr;

        obj["request"] = ro;
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
