#include "OpenApiImporter.h"
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <core/BruWriter.h>

namespace poppy::core {

bool OpenApiImporter::importSpec(const QString& specFilePath, const QString& destinationDir, QString* outError) {
    QFile file(specFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (outError) *outError = "Could not open file: " + specFilePath;
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
    QString title = info.value("title").toString("OpenAPI Collection");

    // Server base URL
    QString baseUrl = "{{baseUrl}}";
    QJsonArray servers = root.value("servers").toArray();
    if (!servers.isEmpty()) {
        QString sUrl = servers[0].toObject().value("url").toString();
        if (!sUrl.isEmpty()) baseUrl = sUrl;
    }

    QDir dest(destinationDir);
    QString collDir = dest.filePath(title.replace('/', '_').replace('\\', '_'));
    dest.mkpath(collDir);

    // Save poppy.json
    QJsonObject meta;
    meta["name"] = title;
    meta["version"] = "1.0.0";
    QFile poppyMeta(collDir + "/poppy.json");
    if (poppyMeta.open(QIODevice::WriteOnly)) {
        poppyMeta.write(QJsonDocument(meta).toJson(QJsonDocument::Indented));
    }

    // Save default dev.env with baseUrl
    QDir envDir(collDir);
    envDir.mkpath("environments");
    QFile envFile(collDir + "/environments/dev.env");
    if (envFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        envFile.write(QString("baseUrl=%1\n").arg(baseUrl).toUtf8());
    }

    QJsonObject paths = root.value("paths").toObject();
    for (auto it = paths.begin(); it != paths.end(); ++it) {
        QString pathStr = it.key();
        QJsonObject pathObj = it.value().toObject();

        for (auto mIt = pathObj.begin(); mIt != pathObj.end(); ++mIt) {
            QString methodStr = mIt.key().toLower();
            if (methodStr != "get" && methodStr != "post" && methodStr != "put" &&
                methodStr != "delete" && methodStr != "patch" && methodStr != "head" &&
                methodStr != "options") {
                continue;
            }

            QJsonObject opObj = mIt.value().toObject();
            RequestModel req;
            req.method = stringToMethod(methodStr);
            req.url = "{{baseUrl}}" + pathStr;

            QString summary = opObj.value("summary").toString();
            if (summary.isEmpty()) summary = opObj.value("operationId").toString();
            if (summary.isEmpty()) summary = QString("%1 %2").arg(methodStr.toUpper(), pathStr);
            req.name = summary;

            // Parameters
            QJsonArray params = opObj.value("parameters").toArray();
            for (const auto& p : params) {
                QJsonObject pObj = p.toObject();
                QString in = pObj.value("in").toString();
                QString name = pObj.value("name").toString();
                QString desc = pObj.value("description").toString();
                bool required = pObj.value("required").toBool(false);

                if (in == "query") {
                    req.queryParams.append(HttpParam{.key = name, .value = "", .enabled = required, .description = desc});
                } else if (in == "path") {
                    req.pathParams.append(HttpParam{.key = name, .value = "", .enabled = true, .description = desc});
                } else if (in == "header") {
                    req.headers.append(HttpHeader{.name = name, .value = "", .enabled = required, .description = desc});
                }
            }

            // Request body
            if (opObj.contains("requestBody")) {
                QJsonObject rb = opObj.value("requestBody").toObject();
                QJsonObject content = rb.value("content").toObject();
                if (content.contains("application/json")) {
                    req.bodyType = BodyType::Json;
                    req.bodyContent = "{\n  \n}";
                }
            }

            // Tests default
            req.scripts.tests = "test(\"Status is 200\", function() {\n  expect(res.getStatus()).to.equal(200);\n});\n";

            QString fileName = summary.toLower().replace(' ', '-').replace('/', '_').replace('{', "").replace('}', "");
            QString filePath = collDir + "/" + fileName + ".bru";
            BruWriter::writeToFile(filePath, req);
        }
    }

    return true;
}

} // namespace poppy::core
