#pragma once

#include <QString>
#include <QList>
#include <QJsonObject>
#include <core/RequestModel.h>

namespace poppy::core {

class OpenApiExporter {
public:
    static QJsonObject exportToJson(const QList<RequestModel>& requests, 
                                    const QString& collectionName = "Poppy Collection",
                                    const QString& version = "1.0.0");

    static QString exportToJsonString(const QList<RequestModel>& requests,
                                      const QString& collectionName = "Poppy Collection",
                                      const QString& version = "1.0.0",
                                      bool indented = true);

    static bool exportToFile(const QString& filePath,
                             const QList<RequestModel>& requests,
                             const QString& collectionName = "Poppy Collection",
                             const QString& version = "1.0.0",
                             QString* outError = nullptr);
};

} // namespace poppy::core
