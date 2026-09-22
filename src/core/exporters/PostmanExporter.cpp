#include "PostmanExporter.h"
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QUuid>

namespace poppy::core {

QJsonObject PostmanExporter::exportToJson(const QList<RequestModel>& requests,
                                          const QString& collectionName) {
    QJsonObject root;

    // Info block
    QJsonObject info;
    info["_postman_id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    info["name"] = collectionName.isEmpty() ? "Poppy Collection" : collectionName;
    info["schema"] = "https://schema.getpostman.com/json/collection/v2.1.0/collection.json";
    info["description"] = "Exported from Poppy API Client";
    root["info"] = info;

    // Items array
    QJsonArray itemsArr;
    for (const auto& req : requests) {
        QJsonObject itemObj;
        itemObj["name"] = req.name.isEmpty() ? "Untitled Request" : req.name;

        QJsonObject reqObj;
        reqObj["method"] = methodToString(req.method);

        // Headers
        QJsonArray headerArr;
        for (const auto& h : req.headers) {
            if (h.name.isEmpty()) continue;
            QJsonObject hObj;
            hObj["key"] = h.name;
            hObj["value"] = h.value;
            hObj["disabled"] = !h.enabled;
            if (!h.description.isEmpty()) hObj["description"] = h.description;
            headerArr.append(hObj);
        }
        reqObj["header"] = headerArr;

        // URL object
        QJsonObject urlObj;
        urlObj["raw"] = req.url;

        QUrl parsedUrl(req.url);
        if (parsedUrl.isValid()) {
            if (!parsedUrl.scheme().isEmpty()) urlObj["protocol"] = parsedUrl.scheme();
            if (!parsedUrl.host().isEmpty()) {
                QJsonArray hostArr;
                for (const auto& part : parsedUrl.host().split('.')) {
                    if (!part.isEmpty()) hostArr.append(part);
                }
                urlObj["host"] = hostArr;
            }
            if (!parsedUrl.path().isEmpty()) {
                QJsonArray pathArr;
                for (const auto& part : parsedUrl.path().split('/', Qt::SkipEmptyParts)) {
                    pathArr.append(part);
                }
                urlObj["path"] = pathArr;
            }
        }

        // Query parameters
        if (!req.queryParams.isEmpty()) {
            QJsonArray queryArr;
            for (const auto& qp : req.queryParams) {
                if (qp.key.isEmpty()) continue;
                QJsonObject qObj;
                qObj["key"] = qp.key;
                qObj["value"] = qp.value;
                qObj["disabled"] = !qp.enabled;
                if (!qp.description.isEmpty()) qObj["description"] = qp.description;
                queryArr.append(qObj);
            }
            urlObj["query"] = queryArr;
        }
        reqObj["url"] = urlObj;

        // Body
        if (req.bodyType == BodyType::Json) {
            QJsonObject bodyObj;
            bodyObj["mode"] = "raw";
            bodyObj["raw"] = req.bodyContent;
            QJsonObject options;
            QJsonObject rawOptions;
            rawOptions["language"] = "json";
            options["raw"] = rawOptions;
            bodyObj["options"] = options;
            reqObj["body"] = bodyObj;
        } else if (req.bodyType == BodyType::Text || req.bodyType == BodyType::Xml) {
            QJsonObject bodyObj;
            bodyObj["mode"] = "raw";
            bodyObj["raw"] = req.bodyContent;
            if (req.bodyType == BodyType::Xml) {
                QJsonObject options;
                QJsonObject rawOptions;
                rawOptions["language"] = "xml";
                options["raw"] = rawOptions;
                bodyObj["options"] = options;
            }
            reqObj["body"] = bodyObj;
        } else if (req.bodyType == BodyType::FormUrlEncoded) {
            QJsonObject bodyObj;
            bodyObj["mode"] = "urlencoded";
            QJsonArray formArr;
            for (const auto& p : req.queryParams) { // or pairs
                QJsonObject pObj;
                pObj["key"] = p.key;
                pObj["value"] = p.value;
                formArr.append(pObj);
            }
            bodyObj["urlencoded"] = formArr;
            reqObj["body"] = bodyObj;
        } else if (req.bodyType == BodyType::MultipartForm) {
            QJsonObject bodyObj;
            bodyObj["mode"] = "formdata";
            QJsonArray formArr;
            for (const auto& p : req.formDataParams) {
                QJsonObject pObj;
                pObj["key"] = p.key;
                pObj["value"] = p.value;
                pObj["type"] = p.isFile ? "file" : "text";
                pObj["disabled"] = !p.enabled;
                formArr.append(pObj);
            }
            bodyObj["formdata"] = formArr;
            reqObj["body"] = bodyObj;
        } else if (req.bodyType == BodyType::GraphQL) {
            QJsonObject bodyObj;
            bodyObj["mode"] = "graphql";
            QJsonObject gqlObj;
            gqlObj["query"] = req.graphqlQuery;
            gqlObj["variables"] = req.graphqlVariables;
            bodyObj["graphql"] = gqlObj;
            reqObj["body"] = bodyObj;
        }

        // Auth
        if (req.auth.type == AuthType::Bearer) {
            QJsonObject authObj;
            authObj["type"] = "bearer";
            QJsonArray bearerArr;
            QJsonObject bToken;
            bToken["key"] = "token";
            bToken["value"] = req.auth.bearerToken;
            bToken["type"] = "string";
            bearerArr.append(bToken);
            authObj["bearer"] = bearerArr;
            reqObj["auth"] = authObj;
        } else if (req.auth.type == AuthType::Basic) {
            QJsonObject authObj;
            authObj["type"] = "basic";
            QJsonArray basicArr;
            QJsonObject uObj, pObj;
            uObj["key"] = "username"; uObj["value"] = req.auth.basicUsername; uObj["type"] = "string";
            pObj["key"] = "password"; pObj["value"] = req.auth.basicPassword; pObj["type"] = "string";
            basicArr.append(uObj);
            basicArr.append(pObj);
            authObj["basic"] = basicArr;
            reqObj["auth"] = authObj;
        }

        itemObj["request"] = reqObj;
        itemsArr.append(itemObj);
    }
    root["item"] = itemsArr;

    return root;
}

QString PostmanExporter::exportToJsonString(const QList<RequestModel>& requests,
                                            const QString& collectionName,
                                            bool indented) {
    QJsonObject obj = exportToJson(requests, collectionName);
    QJsonDocument doc(obj);
    return QString::fromUtf8(doc.toJson(indented ? QJsonDocument::Indented : QJsonDocument::Compact));
}

bool PostmanExporter::exportToFile(const QString& filePath,
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
