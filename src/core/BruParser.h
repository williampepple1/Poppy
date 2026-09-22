#pragma once

#include <QString>
#include <QMap>
#include "RequestModel.h"

namespace poppy::core {

class BruParser {
public:
    static RequestModel parse(const QString& content);
    static RequestModel parseFile(const QString& filePath);
    static QMap<QString, QString> parseVars(const QString& content);
    static QMap<QString, QString> parseVarsFile(const QString& filePath);
    static int parseMetaSeq(const QString& content, int fallback = 1);
};

} // namespace poppy::core
