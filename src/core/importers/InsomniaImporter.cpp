#include "InsomniaImporter.h"
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <core/BruWriter.h>

namespace poppy::core {

QString InsomniaImporter::sanitizeFileName(const QString& name) {
    QString safe = name;
    safe.replace('/', '_');
    safe.replace('\\', '_');
    safe.replace(':', '_');
    safe.replace('*', '_');
    safe.replace('?', '_');
    safe.replace('"', '_');
    safe.replace('<', '_');
    safe.replace('>', '_');
    safe.replace('|', '_');
    return safe.trimmed().isEmpty() ? "request" : safe.trimmed();
}

RequestModel InsomniaImporter::parseInsomniaRequest(const QJsonObject& reqObj) {
    RequestModel req;
    req.name = reqObj.value("name").toString("Untitled Request");
    req.method = stringToMethod(reqObj.value("method").toString("GET"));
    req.url = reqObj.value("url").toString();

    // Headers
    QJsonArray headersArr = reqObj.value("headers").toArray();
    for (const auto& hVal : headersArr) {
        QJsonObject hObj = hVal.toObject();
        QString hName = hObj.value("name").toString();
        QString hValue = hObj.value("value").toString();
        bool disabled = hObj.value("disabled").toBool(false);
        if (!hName.isEmpty()) {
            req.headers.append(HttpHeader{
                .name = hName,
                .value = hValue,
                .enabled = !disabled
            });
        }
    }

    // Parameters (query params)
    QJsonArray paramsArr = reqObj.value("parameters").toArray();
    for (const auto& pVal : paramsArr) {
        QJsonObject pObj = pVal.toObject();
        QString pName = pObj.value("name").toString();
        QString pValue = pObj.value("value").toString();
        bool disabled = pObj.value("disabled").toBool(false);
        if (!pName.isEmpty()) {
            req.queryParams.append(HttpParam{
                .key = pName,
                .value = pValue,
                .enabled = !disabled
            });
        }
    }

    // Body
    if (reqObj.contains("body")) {
        QJsonObject bodyObj = reqObj.value("body").toObject();
        QString mime = bodyObj.value("mimeType").toString().toLower();
        QString text = bodyObj.value("text").toString();

        if (mime.contains("json")) {
            req.bodyType = BodyType::Json;
            req.bodyContent = text;
        } else if (mime.contains("graphql")) {
            req.bodyType = BodyType::GraphQL;
            QJsonDocument gqlDoc = QJsonDocument::fromJson(text.toUtf8());
            if (gqlDoc.isObject()) {
                req.graphqlQuery = gqlDoc.object().value("query").toString();
                QJsonObject varsObj = gqlDoc.object().value("variables").toObject();
                req.graphqlVariables = QString::fromUtf8(QJsonDocument(varsObj).toJson(QJsonDocument::Indented));
            } else {
                req.graphqlQuery = text;
            }
        } else if (mime.contains("xml")) {
            req.bodyType = BodyType::Xml;
            req.bodyContent = text;
        } else if (mime.contains("x-www-form-urlencoded")) {
            req.bodyType = BodyType::FormUrlEncoded;
            req.bodyContent = text;
        } else if (!text.isEmpty()) {
            req.bodyType = BodyType::Text;
            req.bodyContent = text;
        }
    }

    // Authentication
    if (reqObj.contains("authentication")) {
        QJsonObject authObj = reqObj.value("authentication").toObject();
        QString authType = authObj.value("type").toString().toLower();
        bool disabled = authObj.value("disabled").toBool(false);

        if (!disabled) {
            if (authType == "bearer") {
                req.auth.type = AuthType::Bearer;
                req.auth.bearerToken = authObj.value("token").toString();
            } else if (authType == "basic") {
                req.auth.type = AuthType::Basic;
                req.auth.basicUsername = authObj.value("username").toString();
                req.auth.basicPassword = authObj.value("password").toString();
            } else if (authType == "apikey") {
                req.auth.type = AuthType::ApiKey;
                req.auth.apiKeyName = authObj.value("key").toString();
                req.auth.apiKeyValue = authObj.value("value").toString();
                req.auth.apiKeyPlacement = authObj.value("addTo").toString("header");
            } else if (authType == "oauth2") {
                req.auth.type = AuthType::OAuth2;
                req.auth.oauth2AccessToken = authObj.value("accessToken").toString();
            }
        }
    }

    return req;
}

bool InsomniaImporter::importCollection(const QString& insomniaJsonFile, const QString& destinationDir, QString* outError) {
    QFile file(insomniaJsonFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (outError) *outError = "Could not open Insomnia export file: " + insomniaJsonFile;
        return false;
    }

    QByteArray content = file.readAll();
    file.close();

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(content, &parseErr);
    if (doc.isNull() || !doc.isObject()) {
        if (outError) *outError = "Invalid JSON in Insomnia export: " + parseErr.errorString();
        return false;
    }

    QJsonObject root = doc.object();
    QJsonArray resources = root.value("resources").toArray();

    // Map resource IDs to objects and folder paths
    struct ItemInfo {
        QString id;
        QString type;
        QString name;
        QString parentId;
        QJsonObject raw;
    };

    QMap<QString, ItemInfo> itemsById;
    for (const auto& resVal : resources) {
        QJsonObject resObj = resVal.toObject();
        ItemInfo info;
        info.id = resObj.value("_id").toString();
        info.type = resObj.value("_type").toString();
        info.name = resObj.value("name").toString();
        info.parentId = resObj.value("parentId").toString();
        info.raw = resObj;
        itemsById[info.id] = info;
    }

    // Resolve directory path for any folder / workspace
    std::function<QString(const QString&)> getDirPath = [&](const QString& parentId) -> QString {
        if (parentId.isEmpty() || !itemsById.contains(parentId)) {
            return destinationDir;
        }
        const auto& pInfo = itemsById[parentId];
        if (pInfo.type == "workspace") {
            return destinationDir;
        }
        QString parentFolderDir = getDirPath(pInfo.parentId);
        QString safeName = sanitizeFileName(pInfo.name);
        return parentFolderDir + "/" + safeName;
    };

    // Create destination
    QDir().mkpath(destinationDir);

    // Create poppy.json collection manifest
    QString manifestPath = destinationDir + "/poppy.json";
    if (!QFile::exists(manifestPath)) {
        QJsonObject manifest;
        manifest["version"] = "1";
        manifest["name"] = QFileInfo(insomniaJsonFile).baseName();
        manifest["type"] = "collection";
        QFile mf(manifestPath);
        if (mf.open(QIODevice::WriteOnly | QIODevice::Text)) {
            mf.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
        }
    }

    // Process all requests
    for (const auto& item : itemsById) {
        if (item.type == "request") {
            QString folderPath = getDirPath(item.parentId);
            QDir().mkpath(folderPath);

            RequestModel req = parseInsomniaRequest(item.raw);
            QString safeFileName = sanitizeFileName(req.name) + ".bru";
            QString filePath = folderPath + "/" + safeFileName;

            // Handle name collisions
            int count = 1;
            while (QFile::exists(filePath)) {
                safeFileName = QString("%1 (%2).bru").arg(sanitizeFileName(req.name)).arg(count++);
                filePath = folderPath + "/" + safeFileName;
            }

            QString serialized = BruWriter::serialize(req);
            QFile outBru(filePath);
            if (outBru.open(QIODevice::WriteOnly | QIODevice::Text)) {
                outBru.write(serialized.toUtf8());
            }
        }
    }

    return true;
}

} // namespace poppy::core
