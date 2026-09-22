#pragma once

#include <QString>
#include <QMap>
#include "RequestModel.h"

namespace poppy::core {

class BruWriter {
public:
    static QString serialize(const RequestModel& req);
    static bool writeToFile(const QString& filePath, const RequestModel& req);
    static QString serializeFolder(const QString& name, const QMap<QString, QString>& vars, int seq = 1);
    static bool writeFolderFile(const QString& dirPath, const QString& name, const QMap<QString, QString>& vars, int seq = 1);

    // Strip characters illegal on Windows (and other reserved names) so
    // imported request files actually land on disk.
    static QString safeFileStem(const QString& name, const QString& fallback = QStringLiteral("item"));
    static QString uniqueFilePath(const QString& directory, const QString& stem, const QString& extension,
                                 const QString& ignorePath = {});
};

} // namespace poppy::core
