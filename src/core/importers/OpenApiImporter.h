#pragma once

#include <QString>
#include <core/RequestModel.h>

namespace poppy::core {

class OpenApiImporter {
public:
    static bool importSpec(const QString& specFilePath, const QString& destinationDir, QString* outError = nullptr);
};

} // namespace poppy::core
