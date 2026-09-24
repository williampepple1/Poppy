#include "InsomniaExporter.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QUuid>

namespace poppy::core {

QJsonObject InsomniaExporter::exportToJson(const QList<RequestModel>& requests,
                                           const QString& collectionName) {
    QJsonObject root;
    root["_type"] = "export";
    root["__export_format"] = 4;
    root["__export_date"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    root["__export_source"] = "poppy:v1.0.0";

    QJsonArray resources;

    // 1. Workspace resource
    QString wrkId = "wrk_" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(12);
    QJsonObject wrkObj;
    wrkObj["_id"] = wrkId;
    wrkObj["_type"] = "workspace";
    wrkObj["parentId"] = QJsonValue::Null;
    wrkObj["name"] = collectionName.isEmpty() ? "Poppy Collection" : collectionName;
    wrkObj["description"] = "Exported from Poppy API Client";
    wrkObj["scope"] = "collection";
    resources.append(wrkObj);

    // 2. Request resources
    for (const auto& req : requests) {
        QString reqId = "req_" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(12);
        QJsonObject rObj;
        rObj["_id"] = reqId;
        rObj["_type"] = "request";
        rObj["parentId"] = wrkId;
        rObj["name"] = req.name.isEmpty() ? "Untitled Request" : req.name;
        rObj["url"] = req.url;
        rObj["method"] = methodToString(req.method);

        // Headers
        QJsonArray hArr;
        for (const auto& h : req.headers) {
            if (h.name.isEmpty()) continue;
            QJsonObject hObj;
            hObj["name"] = h.name;
            hObj["value"] = h.value;
            hObj["disabled"] = !h.enabled;
            hArr.append(hObj);
        }
        rObj["headers"] = hArr;

        // Query parameters
        QJsonArray pArr;
        for (const auto& p : req.queryParams) {
            if (p.key.isEmpty()) continue;
            QJsonObject pObj;
            pObj["name"] = p.key;
            pObj["value"] = p.value;
            pObj["disabled"] = !p.enabled;
            pArr.append(pObj);
        }
        rObj["parameters"] = pArr;

        // Body
        QJsonObject bObj;
        if (req.bodyType == BodyType::Json) {
            bObj["mimeType"] = "application/json";
            bObj["text"] = req.bodyContent;
        } else if (req.bodyType == BodyType::Xml) {
            bObj["mimeType"] = "application/xml";
            bObj["text"] = req.bodyContent;
        } else if (req.bodyType == BodyType::Text) {
            bObj["mimeType"] = "text/plain";
            bObj["text"] = req.bodyContent;
        } else if (req.bodyType == BodyType::FormUrlEncoded) {
            bObj["mimeType"] = "application/x-www-form-urlencoded";
            QJsonArray formParams;
            QStringList pairs = req.bodyContent.split('&', Qt::SkipEmptyParts);
            for (const auto& pair : pairs) {
                int eq = pair.indexOf('=');
                if (eq > 0) {
                    QJsonObject item;
                    item["name"] = pair.left(eq);
                    item["value"] = pair.mid(eq + 1);
                    formParams.append(item);
                }
            }
            bObj["params"] = formParams;
        } else if (req.bodyType == BodyType::MultipartForm) {
            bObj["mimeType"] = "multipart/form-data";
            QJsonArray formParams;
            for (const auto& p : req.formDataParams) {
                QJsonObject item;
                item["name"] = p.key;
                if (p.isFile) {
                    item["fileName"] = p.value;
                    item["type"] = "file";
                } else {
                    item["value"] = p.value;
                    item["type"] = "text";
                }
                item["disabled"] = !p.enabled;
                formParams.append(item);
            }
            bObj["params"] = formParams;
        } else if (req.bodyType == BodyType::GraphQL) {
            bObj["mimeType"] = "application/graphql";
            QJsonObject gqlObj;
            gqlObj["query"] = req.graphqlQuery;
            const QJsonDocument varsDoc = QJsonDocument::fromJson(req.graphqlVariables.toUtf8());
            if (varsDoc.isObject()) gqlObj["variables"] = varsDoc.object();
            else if (varsDoc.isArray()) gqlObj["variables"] = varsDoc.array();
            else if (req.graphqlVariables.trimmed().isEmpty()) gqlObj["variables"] = QJsonObject();
            else gqlObj["variables"] = req.graphqlVariables;
            bObj["text"] = QString::fromUtf8(QJsonDocument(gqlObj).toJson(QJsonDocument::Compact));
        }
        rObj["body"] = bObj;

        // Auth
        QJsonObject authObj;
        if (req.auth.type == AuthType::Bearer) {
            authObj["type"] = "bearer";
            authObj["token"] = req.auth.bearerToken;
        } else if (req.auth.type == AuthType::Basic) {
            authObj["type"] = "basic";
            authObj["username"] = req.auth.basicUsername;
            authObj["password"] = req.auth.basicPassword;
        }
        rObj["authentication"] = authObj;

        resources.append(rObj);
    }

    root["resources"] = resources;
    return root;
}

QString InsomniaExporter::exportToJsonString(const QList<RequestModel>& requests,
                                             const QString& collectionName,
                                             bool indented) {
    QJsonObject obj = exportToJson(requests, collectionName);
    QJsonDocument doc(obj);
    return QString::fromUtf8(doc.toJson(indented ? QJsonDocument::Indented : QJsonDocument::Compact));
}

bool InsomniaExporter::exportToFile(const QString& filePath,
                                    const QList<RequestModel>& requests,
                                    const QString& collectionName,
                                    QString* outError) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (outError) *outError = file.errorString();
        return false;
    }
    QString jsonStr = exportToJsonString(requests, collectionName, true);
    qint64 bytesWritten = file.write(jsonStr.toUtf8());
    if (bytesWritten == -1) {
        if (outError) *outError = "Failed to write to file: " + file.errorString();
        return false;
    }
    return true;
}

} // namespace poppy::core
