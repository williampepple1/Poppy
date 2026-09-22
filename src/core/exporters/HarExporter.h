#pragma once

#include <QString>
#include <QList>
#include <QJsonObject>
#include <core/RequestModel.h>
#include <core/ResponseModel.h>

namespace poppy::core {

class HarExporter {
public:
    static QJsonObject exportToJson(const QList<RequestModel>& requests);

    static QString exportToJsonString(const QList<RequestModel>& requests, bool indented = true);

    static bool exportToFile(const QString& filePath,
                             const QList<RequestModel>& requests,
                             QString* outError = nullptr);
};

} // namespace poppy::core
