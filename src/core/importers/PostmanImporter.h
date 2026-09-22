#pragma once

#include <QString>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <core/RequestModel.h>

namespace poppy::core {

class PostmanImporter {
public:
    static bool importCollection(const QString& postmanJsonFile, const QString& destinationDir,
                                 QString* outError = nullptr, QString* outCollectionDir = nullptr);
    static RequestModel parsePostmanItem(const QJsonObject& itemObj);

private:
    static bool processItems(const QJsonArray& items, const QString& currentDir, QString* outError);
};

} // namespace poppy::core
