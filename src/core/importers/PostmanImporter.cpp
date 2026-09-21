#include "PostmanImporter.h"
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <core/BruWriter.h>

namespace poppy::core {

RequestModel PostmanImporter::parsePostmanItem(const QJsonObject& itemObj) {
    RequestModel req;
    req.name = itemObj.value("name").toString("Untitled Request");

    QJsonObject reqObj = itemObj.value("request").toObject();
    req.method = stringToMethod(reqObj.value("method").toString("GET"));

    // URL
    QJsonValue urlVal = reqObj.value("url");
    if (urlVal.isString()) {
        req.url = urlVal.toString();
    } else if (urlVal.isObject()) {
        QJsonObject urlObj = urlVal.toObject();
        req.url = urlObj.value("raw").toString();

        // Query parameters
        QJsonArray queryArr = urlObj.value("query").toArray();
        for (const auto& q : queryArr) {
            QJsonObject qObj = q.toObject();
            req.queryParams.append(HttpParam{
                .key = qObj.value("key").toString(),
                .value = qObj.value("value").toString(),
                .enabled = !qObj.value("disabled").toBool(false),
                .description = qObj.value("description").toString()
            });
        }
    }

    // Headers
    QJsonArray headerArr = reqObj.value("header").toArray();
    for (const auto& h : headerArr) {
        QJsonObject hObj = h.toObject();
        req.headers.append(HttpHeader{
            .name = hObj.value("key").toString(),
            .value = hObj.value("value").toString(),
            .enabled = !hObj.value("disabled").toBool(false),
            .description = hObj.value("description").toString()
        });
    }

    // Body
    QJsonObject bodyObj = reqObj.value("body").toObject();
    QString mode = bodyObj.value("mode").toString();
    if (mode == "raw") {
        req.bodyType = BodyType::Json;
        req.bodyContent = bodyObj.value("raw").toString();
    } else if (mode == "urlencoded") {
        req.bodyType = BodyType::FormUrlEncoded;
        QStringList pairs;
        for (const auto& f : bodyObj.value("urlencoded").toArray()) {
            QJsonObject fObj = f.toObject();
            if (!fObj.value("disabled").toBool(false)) {
                pairs.append(fObj.value("key").toString() + "=" + fObj.value("value").toString());
            }
        }
        req.bodyContent = pairs.join('&');
    }

    // Auth
    QJsonObject authObj = reqObj.value("auth").toObject();
    QString authType = authObj.value("type").toString();
    if (authType == "bearer") {
        req.auth.type = AuthType::Bearer;
        for (const auto& a : authObj.value("bearer").toArray()) {
            QJsonObject aObj = a.toObject();
            if (aObj.value("key").toString() == "token") {
                req.auth.bearerToken = aObj.value("value").toString();
            }
        }
    } else if (authType == "basic") {
        req.auth.type = AuthType::Basic;
        for (const auto& a : authObj.value("basic").toArray()) {
            QJsonObject aObj = a.toObject();
            QString k = aObj.value("key").toString();
            if (k == "username") req.auth.basicUsername = aObj.value("value").toString();
            else if (k == "password") req.auth.basicPassword = aObj.value("value").toString();
        }
    } else if (authType == "apikey") {
        req.auth.type = AuthType::ApiKey;
        for (const auto& a : authObj.value("apikey").toArray()) {
            QJsonObject aObj = a.toObject();
            QString k = aObj.value("key").toString();
            if (k == "key") req.auth.apiKeyName = aObj.value("value").toString();
            else if (k == "value") req.auth.apiKeyValue = aObj.value("value").toString();
            else if (k == "in") req.auth.apiKeyPlacement = aObj.value("value").toString();
        }
    }

    // Scripts (event)
    for (const auto& evVal : itemObj.value("event").toArray()) {
        QJsonObject evObj = evVal.toObject();
        QString listen = evObj.value("listen").toString();
        QJsonObject scriptObj = evObj.value("script").toObject();
        QJsonArray execArr = scriptObj.value("exec").toArray();

        QStringList lines;
        for (const auto& l : execArr) lines.append(l.toString());
        QString scriptContent = lines.join('\n');

        if (listen == "prerequest") {
            req.scripts.preRequestScript = scriptContent;
        } else if (listen == "test") {
            req.scripts.tests = scriptContent;
        }
    }

    return req;
}

bool PostmanImporter::processItems(const QJsonArray& items, const QString& currentDir, QString* outError) {
    QDir dir(currentDir);
    if (!dir.exists() && !dir.mkpath(".")) {
        if (outError) *outError = "Failed to create directory: " + currentDir;
        return false;
    }

    for (const auto& itVal : items) {
        QJsonObject itObj = itVal.toObject();
        QString name = itObj.value("name").toString("item");

        if (itObj.contains("item") && itObj.value("item").isArray()) {
            // Folder
            QString safeFolderName = name;
            safeFolderName.replace('/', '_').replace('\\', '_');
            QString subDirPath = dir.filePath(safeFolderName);
            if (!processItems(itObj.value("item").toArray(), subDirPath, outError)) {
                return false;
            }
        } else if (itObj.contains("request")) {
            // Request
            RequestModel req = parsePostmanItem(itObj);
            QString safeFileName = name.toLower().replace(' ', '-').replace('/', '_');
            QString filePath = dir.filePath(safeFileName + ".bru");
            BruWriter::writeToFile(filePath, req);
        }
    }

    return true;
}

bool PostmanImporter::importCollection(const QString& postmanJsonFile, const QString& destinationDir, QString* outError) {
    QFile file(postmanJsonFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (outError) *outError = "Could not open file: " + postmanJsonFile;
        return false;
    }

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        if (outError) *outError = "Invalid JSON: " + parseErr.errorString();
        return false;
    }

    QJsonObject root = doc.object();
    QJsonObject info = root.value("info").toObject();
    QString collName = info.value("name").toString("Imported Collection");

    QDir dest(destinationDir);
    QString collDir = dest.filePath(collName.replace('/', '_'));
    dest.mkpath(collDir);

    // Write poppy.json metadata
    QJsonObject meta;
    meta["name"] = collName;
    meta["version"] = "1.0.0";
    QFile poppyMeta(collDir + "/poppy.json");
    if (poppyMeta.open(QIODevice::WriteOnly)) {
        poppyMeta.write(QJsonDocument(meta).toJson(QJsonDocument::Indented));
    }

    return processItems(root.value("item").toArray(), collDir, outError);
}

} // namespace poppy::core
