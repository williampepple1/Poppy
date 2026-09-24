#pragma once

#include <QString>
#include <QMap>
#include <QList>
#include "RequestModel.h"
#include "EnvironmentModel.h"
#include "YamlNode.h"

namespace poppy::core {

struct OpenCollectionFolderInfo {
    QString name;
    int seq{1};
    AuthModel auth;
    QMap<QString, QString> vars;
    QList<HttpHeader> headers;
};

struct OpenCollectionInfo {
    QString name;
    QString version;
    AuthModel auth;
    QMap<QString, QString> vars;
};

class OpenCollectionParser {
public:
    static bool isOpenCollectionFile(const QString& filePath);
    static bool isOpenCollectionRequest(const YamlNode& node);

    static RequestModel parseRequest(const YamlNode& node, const QString& fallbackName = QString());
    static RequestModel parseRequestText(const QString& yamlText, const QString& fallbackName = QString());
    static RequestModel parseRequestFile(const QString& filePath);

    static EnvironmentModel parseEnvironment(const YamlNode& node, const QString& fallbackName = QString());
    static EnvironmentModel parseEnvironmentText(const QString& yamlText, const QString& fallbackName = QString());
    static EnvironmentModel parseEnvironmentFile(const QString& filePath, const QString& fallbackName = QString());

    static OpenCollectionFolderInfo parseFolder(const YamlNode& node, const QString& fallbackName = QString());
    static OpenCollectionFolderInfo parseFolderFile(const QString& filePath, const QString& fallbackName = QString());

    static OpenCollectionInfo parseCollection(const YamlNode& node, const QString& fallbackName = QString());
    static OpenCollectionInfo parseCollectionFile(const QString& filePath, const QString& fallbackName = QString());
};

} // namespace poppy::core
