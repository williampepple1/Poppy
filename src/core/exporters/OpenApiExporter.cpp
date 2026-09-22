#include "OpenApiExporter.h"
#include <QUrl>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QRegularExpression>

namespace poppy::core {

static QString extractPath(const QString& rawUrl) {
    QUrl url(rawUrl);
    QString path = url.path();
    if (path.isEmpty()) {
        if (!rawUrl.startsWith("http://") && !rawUrl.startsWith("https://")) {
            int slashIdx = rawUrl.indexOf('/');
            if (slashIdx >= 0) {
                path = rawUrl.mid(slashIdx);
                int qIdx = path.indexOf('?');
                if (qIdx >= 0) path = path.left(qIdx);
            }
        }
    }
    if (path.isEmpty()) path = "/";

    // Replace :param with {param} for OpenAPI
    static QRegularExpression colonParam(":([a-zA-Z0-9_]+)");
    path.replace(colonParam, "{\\1}");
    return path;
}

QJsonObject OpenApiExporter::exportToJson(const QList<RequestModel>& requests,
                                          const QString& collectionName,
                                          const QString& version) {
    QJsonObject root;
    root["openapi"] = "3.0.3";

    QJsonObject info;
    info["title"] = collectionName.isEmpty() ? "Poppy Collection" : collectionName;
    info["version"] = version.isEmpty() ? "1.0.0" : version;
    info["description"] = "Exported from Poppy - Native API Client";
    root["info"] = info;

    QJsonObject pathsObj;

    for (const auto& req : requests) {
        QString path = extractPath(req.url);
        QString methodStr = methodToString(req.method).toLower();
        QJsonObject pathItem = pathsObj.value(path).toObject();
        if (pathItem.contains(methodStr)) {
            int n = 2;
            QString unique = path;
            while (pathsObj.value(unique).toObject().contains(methodStr)) {
                unique = path + "-" + QString::number(n++);
            }
            path = unique;
            pathItem = pathsObj.value(path).toObject();
        }
        QJsonObject opObj;
        opObj["summary"] = req.name.isEmpty() ? (methodToString(req.method) + " " + path) : req.name;

        // Parameters
        QJsonArray paramsArr;

        // Path params
        for (const auto& p : req.pathParams) {
            if (!p.enabled || p.key.isEmpty()) continue;
            QJsonObject pObj;
            pObj["name"] = p.key;
            pObj["in"] = "path";
            pObj["required"] = true;
            QJsonObject schema;
            schema["type"] = "string";
            if (!p.value.isEmpty()) schema["example"] = p.value;
            pObj["schema"] = schema;
            paramsArr.append(pObj);
        }

        // Query params
        for (const auto& q : req.queryParams) {
            if (!q.enabled || q.key.isEmpty()) continue;
            QJsonObject qObj;
            qObj["name"] = q.key;
            qObj["in"] = "query";
            QJsonObject schema;
            schema["type"] = "string";
            if (!q.value.isEmpty()) schema["example"] = q.value;
            qObj["schema"] = schema;
            paramsArr.append(qObj);
        }

        // Headers
        for (const auto& h : req.headers) {
            if (!h.enabled || h.name.isEmpty()) continue;
            if (h.name.compare("Authorization", Qt::CaseInsensitive) == 0 ||
                h.name.compare("Content-Type", Qt::CaseInsensitive) == 0) {
                continue; // Typically defined in security or requestBody
            }
            QJsonObject hObj;
            hObj["name"] = h.name;
            hObj["in"] = "header";
            QJsonObject schema;
            schema["type"] = "string";
            if (!h.value.isEmpty()) schema["example"] = h.value;
            hObj["schema"] = schema;
            paramsArr.append(hObj);
        }

        if (!paramsArr.isEmpty()) {
            opObj["parameters"] = paramsArr;
        }

        // Request Body
        if (req.bodyType == BodyType::Json && !req.bodyContent.trimmed().isEmpty()) {
            QJsonObject rb;
            QJsonObject content;
            QJsonObject jsonMedia;
            QJsonDocument bDoc = QJsonDocument::fromJson(req.bodyContent.toUtf8());
            if (!bDoc.isNull()) {
                if (bDoc.isObject()) jsonMedia["example"] = bDoc.object();
                else if (bDoc.isArray()) jsonMedia["example"] = bDoc.array();
            }
            QJsonObject schema;
            schema["type"] = "object";
            jsonMedia["schema"] = schema;
            content["application/json"] = jsonMedia;
            rb["content"] = content;
            opObj["requestBody"] = rb;
        } else if (req.bodyType == BodyType::GraphQL) {
            QJsonObject rb;
            QJsonObject content;
            QJsonObject gqlMedia;
            QJsonObject schema;
            schema["type"] = "object";
            gqlMedia["schema"] = schema;
            content["application/json"] = gqlMedia;
            rb["content"] = content;
            opObj["requestBody"] = rb;
        }

        // Responses
        QJsonObject responses;
        QJsonObject r200;
        r200["description"] = "Successful response";
        responses["200"] = r200;
        opObj["responses"] = responses;

        pathItem[methodStr] = opObj;
        pathsObj[path] = pathItem;
    }

    root["paths"] = pathsObj;
    return root;
}

QString OpenApiExporter::exportToJsonString(const QList<RequestModel>& requests,
                                            const QString& collectionName,
                                            const QString& version,
                                            bool indented) {
    QJsonObject root = exportToJson(requests, collectionName, version);
    QJsonDocument doc(root);
    return QString::fromUtf8(doc.toJson(indented ? QJsonDocument::Indented : QJsonDocument::Compact));
}

bool OpenApiExporter::exportToFile(const QString& filePath,
                                   const QList<RequestModel>& requests,
                                   const QString& collectionName,
                                   const QString& version,
                                   QString* outError) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (outError) *outError = "Could not open file for writing: " + file.errorString();
        return false;
    }

    QString json = exportToJsonString(requests, collectionName, version, true);
    file.write(json.toUtf8());
    file.close();
    return true;
}

} // namespace poppy::core
