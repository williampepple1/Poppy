#pragma once

#include <QString>
#include <QList>
#include <QJsonObject>
#include <core/RequestModel.h>

namespace poppy::core {

class PostmanExporter {
public:
    static QJsonObject exportToJson(const QList<RequestModel>& requests,
                                    const QString& collectionName = "Poppy Collection");

    static QString exportToJsonString(const QList<RequestModel>& requests,
                                      const QString& collectionName = "Poppy Collection",
                                      bool indented = true);

    static bool exportToFile(const QString& filePath,
                             const QList<RequestModel>& requests,
                             const QString& collectionName = "Poppy Collection",
                             QString* outError = nullptr);
};

} // namespace poppy::core
