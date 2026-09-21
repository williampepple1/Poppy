#pragma once

#include <QString>
#include "RequestModel.h"

namespace poppy::core {

class BruWriter {
public:
    static QString serialize(const RequestModel& req);
    static bool writeToFile(const QString& filePath, const RequestModel& req);
};

} // namespace poppy::core
