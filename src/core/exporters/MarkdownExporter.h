#pragma once

#include <QString>
#include <QList>
#include <core/RequestModel.h>

namespace poppy::core {

class MarkdownExporter {
public:
    static QString exportToMarkdown(const QList<RequestModel>& requests,
                                   const QString& collectionName = "Poppy Collection");

    static bool exportToFile(const QString& filePath,
                             const QList<RequestModel>& requests,
                             const QString& collectionName = "Poppy Collection",
                             QString* outError = nullptr);
};

} // namespace poppy::core
