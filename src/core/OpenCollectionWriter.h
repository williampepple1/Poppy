#pragma once

#include <QString>
#include <QMap>
#include "RequestModel.h"
#include "EnvironmentModel.h"

namespace poppy::core {

class OpenCollectionWriter {
public:
    static QString serializeRequest(const RequestModel& req);
    static bool writeRequestFile(const QString& filePath, const RequestModel& req);

    static QString serializeEnvironment(const EnvironmentModel& env);
    static bool writeEnvironmentFile(const QString& filePath, const EnvironmentModel& env);

    static QString serializeFolder(const QString& name, int seq, const AuthModel& auth, const QMap<QString, QString>& vars);
    static bool writeFolderFile(const QString& folderPath, const QString& name, int seq, const AuthModel& auth, const QMap<QString, QString>& vars);
};

} // namespace poppy::core
