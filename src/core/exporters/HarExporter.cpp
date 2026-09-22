#include "HarExporter.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>

namespace poppy::core {

QJsonObject HarExporter::exportToJson(const QList<RequestModel>& requests) {
    QJsonObject root;
    QJsonObject logObj;
    logObj["version"] = "1.2";

    QJsonObject creator;
    creator["name"] = "Poppy";
    creator["version"] = "1.0.0";
    logObj["creator"] = creator;

    QJsonArray entriesArr;
    for (const auto& req : requests) {
        QJsonObject entry;
        entry["startedDateTime"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        entry["time"] = 0;

        // Request
        QJsonObject reqObj;
        reqObj["method"] = methodToString(req.method);
        reqObj["url"] = req.effectiveUrl();
        reqObj["httpVersion"] = "HTTP/1.1";

        // Headers
        QJsonArray hArr;
        for (const auto& h : req.effectiveHeaders()) {
            if (h.name.isEmpty()) continue;
            QJsonObject hObj;
            hObj["name"] = h.name;
            hObj["value"] = h.value;
            hArr.append(hObj);
        }
        reqObj["headers"] = hArr;

        // Query string
        QJsonArray qsArr;
        for (const auto& qp : req.queryParams) {
            if (qp.key.isEmpty()) continue;
            QJsonObject qObj;
            qObj["name"] = qp.key;
            qObj["value"] = qp.value;
            qsArr.append(qObj);
        }
        reqObj["queryString"] = qsArr;
        reqObj["cookies"] = QJsonArray{};
        reqObj["headersSize"] = -1;
        reqObj["bodySize"] = req.bodyContent.size();

        // Post data
        if (req.bodyType != BodyType::None && !req.bodyContent.isEmpty()) {
            QJsonObject postData;
            if (req.bodyType == BodyType::Json) postData["mimeType"] = "application/json";
            else if (req.bodyType == BodyType::FormUrlEncoded) postData["mimeType"] = "application/x-www-form-urlencoded";
            else if (req.bodyType == BodyType::MultipartForm) postData["mimeType"] = "multipart/form-data";
            else postData["mimeType"] = "text/plain";

            postData["text"] = req.bodyContent;
            reqObj["postData"] = postData;
        }

        entry["request"] = reqObj;

        // Response placeholder
        QJsonObject resObj;
        resObj["status"] = 0;
        resObj["statusText"] = "";
        resObj["httpVersion"] = "HTTP/1.1";
        resObj["headers"] = QJsonArray{};
        resObj["cookies"] = QJsonArray{};
        QJsonObject content;
        content["size"] = 0;
        content["mimeType"] = "";
        resObj["content"] = content;
        resObj["redirectURL"] = "";
        resObj["headersSize"] = -1;
        resObj["bodySize"] = 0;
        entry["response"] = resObj;

        entry["cache"] = QJsonObject{};
        QJsonObject timings;
        timings["send"] = 0;
        timings["wait"] = 0;
        timings["receive"] = 0;
        entry["timings"] = timings;

        entriesArr.append(entry);
    }
    logObj["entries"] = entriesArr;
    root["log"] = logObj;

    return root;
}

QString HarExporter::exportToJsonString(const QList<RequestModel>& requests, bool indented) {
    QJsonObject obj = exportToJson(requests);
    QJsonDocument doc(obj);
    return QString::fromUtf8(doc.toJson(indented ? QJsonDocument::Indented : QJsonDocument::Compact));
}

bool HarExporter::exportToFile(const QString& filePath,
                              const QList<RequestModel>& requests,
                              QString* outError) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (outError) *outError = file.errorString();
        return false;
    }
    QString jsonStr = exportToJsonString(requests, true);
    qint64 bytesWritten = file.write(jsonStr.toUtf8());
    if (bytesWritten == -1) {
        if (outError) *outError = "Failed to write to file: " + file.errorString();
        return false;
    }
    return true;
}

} // namespace poppy::core
