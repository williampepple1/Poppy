#pragma once

#include <QString>
#include <core/RequestModel.h>

namespace poppy::core {

class CurlImporter {
public:
    static RequestModel importCurl(const QString& curlCommand);
};

} // namespace poppy::core
