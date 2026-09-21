#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <core/RequestModel.h>

namespace poppy::core {

class InsomniaImporter {
public:
    static bool importCollection(const QString& insomniaJsonFile, const QString& destinationDir, QString* outError = nullptr);
    static RequestModel parseInsomniaRequest(const QJsonObject& reqObj);

private:
    static QString sanitizeFileName(const QString& name);
};

} // namespace poppy::core
