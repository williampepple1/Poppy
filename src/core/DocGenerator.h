#pragma once

#include <QString>
#include <QList>
#include <core/RequestModel.h>
#include <core/VariableResolver.h>

namespace poppy::core {

class DocGenerator {
public:
    static QString generateHtml(const QList<RequestModel>& requests,
                                const QString& collectionName = "API Documentation",
                                const QString& description = {},
                                const VariableResolver* resolver = nullptr);

    static QString generateHtml(const QList<RequestModel>& requests,
                                const QString& collectionName,
                                const QString& description,
                                const VariableResolver& resolver)
    {
        return generateHtml(requests, collectionName, description, &resolver);
    }

    static bool exportToFile(const QString& filePath,
                             const QList<RequestModel>& requests,
                             const QString& collectionName = "API Documentation",
                             const QString& description = {},
                             const VariableResolver* resolver = nullptr,
                             QString* errorMessage = nullptr);

    static bool generateHtmlFile(const QString& filePath,
                                 const QList<RequestModel>& requests,
                                 const QString& collectionName = "API Documentation",
                                 const QString& description = {},
                                 const VariableResolver& resolver = {},
                                 QString* errorMessage = nullptr)
    {
        return exportToFile(filePath, requests, collectionName, description, &resolver, errorMessage);
    }
};

} // namespace poppy::core
