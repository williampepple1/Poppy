#pragma once

#include <QString>
#include "RequestModel.h"

namespace poppy::core {

class BruParser {
public:
    static RequestModel parse(const QString& content);
    static RequestModel parseFile(const QString& filePath);
};

} // namespace poppy::core
