#include "OpenCollectionParser.h"
#include <QFileInfo>
#include <QDir>

namespace poppy::core {

namespace {

void parseAuthNode(const YamlNode& authNode, AuthModel& auth) {
    if (authNode.isScalar()) {
        QString str = authNode.asString().trimmed().toLower();
        if (str == "inherit") {
            auth.type = AuthType::Inherit;
        } else if (str == "none") {
            auth.type = AuthType::None;
        }
        return;
    }

    if (!authNode.isMapping()) return;

    QString aType = authNode["type"].asString().trimmed().toLower();
    if (aType == "bearer") {
        auth.type = AuthType::Bearer;
        auth.bearerToken = authNode["token"].asString();
    } else if (aType == "basic") {
        auth.type = AuthType::Basic;
        auth.basicUsername = authNode["username"].asString();
        auth.basicPassword = authNode["password"].asString();
    } else if (aType == "api-key" || aType == "apikey") {
        auth.type = AuthType::ApiKey;
        auth.apiKeyName = authNode["name"].asString();
        auth.apiKeyValue = authNode["value"].asString();
        auth.apiKeyPlacement = authNode["placement"].asString("header");
    } else if (aType == "oauth2") {
        auth.type = AuthType::OAuth2;
        auth.oauth2AccessToken = authNode["accessToken"].asString();
    } else if (aType == "awsv4" || aType == "aws") {
        auth.type = AuthType::AwsSigV4;
        auth.awsAccessKey = authNode["accessKey"].asString();
        auth.awsSecretKey = authNode["secretKey"].asString();
        auth.awsSessionToken = authNode["sessionToken"].asString();
        auth.awsRegion = authNode["region"].asString();
        auth.awsService = authNode["service"].asString();
    } else if (aType == "inherit") {
        auth.type = AuthType::Inherit;
    } else if (aType == "none") {
        auth.type = AuthType::None;
    }
}

} // namespace

bool OpenCollectionParser::isOpenCollectionFile(const QString& filePath) {
    QString ext = QFileInfo(filePath).suffix().toLower();
    return ext == "yml" || ext == "yaml";
}

bool OpenCollectionParser::isOpenCollectionRequest(const YamlNode& node) {
    if (node.hasKey("http")) return true;
    if (node.hasKey("info") && node["info"]["type"].asString().trimmed().toLower() == "http") return true;
    return false;
}

RequestModel OpenCollectionParser::parseRequest(const YamlNode& node, const QString& fallbackName) {
    RequestModel req;

    // Info
    const auto& info = node["info"];
    req.name = info["name"].asString(fallbackName);
    if (req.name.trimmed().isEmpty()) {
        req.name = fallbackName.isEmpty() ? QStringLiteral("Untitled Request") : fallbackName;
    }
    req.seq = info["seq"].asInt(1);

    // HTTP
    const auto& http = node["http"];
    if (http.isMapping()) {
        QString methodStr = http["method"].asString("GET");
        req.method = stringToMethod(methodStr);
        req.url = http["url"].asString();

        // Params
        if (http.hasKey("params") && http["params"].isSequence()) {
            for (const auto& item : http["params"].sequence) {
                QString pName = item["name"].asString();
                QString pVal = item["value"].asString();
                QString pType = item["type"].asString("query").trimmed().toLower();
                bool disabled = item["disabled"].asBool(false);
                if (pType == "path") {
                    req.pathParams.append(HttpParam{pName, pVal, !disabled, QString()});
                } else {
                    req.queryParams.append(HttpParam{pName, pVal, !disabled, QString()});
                }
            }
        }

        // Headers
        if (http.hasKey("headers") && http["headers"].isSequence()) {
            for (const auto& item : http["headers"].sequence) {
                QString hName = item["name"].asString();
                QString hVal = item["value"].asString();
                bool disabled = item["disabled"].asBool(false);
                req.headers.append(HttpHeader{hName, hVal, !disabled, QString()});
            }
        }

        // Body
        if (http.hasKey("body") && http["body"].isMapping()) {
            const auto& b = http["body"];
            QString btype = b["type"].asString().trimmed().toLower();
            if (btype == "json") {
                req.bodyType = BodyType::Json;
                req.bodyContent = b["data"].asString();
            } else if (btype == "text") {
                req.bodyType = BodyType::Text;
                req.bodyContent = b["data"].asString();
            } else if (btype == "xml") {
                req.bodyType = BodyType::Xml;
                req.bodyContent = b["data"].asString();
            } else if (btype == "form-urlencoded") {
                req.bodyType = BodyType::FormUrlEncoded;
                if (b["data"].isSequence()) {
                    for (const auto& item : b["data"].sequence) {
                        QString name = item["name"].asString();
                        QString val = item["value"].asString();
                        bool disabled = item["disabled"].asBool(false);
                        req.formDataParams.append(FormDataParam{name, val, false, !disabled, QString()});
                    }
                } else {
                    req.bodyContent = b["data"].asString();
                }
            } else if (btype == "multipart-form") {
                req.bodyType = BodyType::MultipartForm;
                if (b["data"].isSequence()) {
                    for (const auto& item : b["data"].sequence) {
                        QString name = item["name"].asString();
                        QString val;
                        if (item["value"].isSequence() && !item["value"].sequence.isEmpty()) {
                            val = item["value"].sequence.first().asString();
                        } else {
                            val = item["value"].asString();
                        }
                        bool isFile = (item["type"].asString().trimmed().toLower() == "file");
                        bool disabled = item["disabled"].asBool(false);
                        req.formDataParams.append(FormDataParam{name, val, isFile, !disabled, item["description"].asString()});
                    }
                }
            } else if (btype == "graphql") {
                req.bodyType = BodyType::GraphQL;
                if (b["data"].isMapping()) {
                    req.graphqlQuery = b["data"]["query"].asString();
                    req.graphqlVariables = b["data"]["variables"].asString();
                } else {
                    req.graphqlQuery = b["data"].asString();
                }
            } else if (btype == "none") {
                req.bodyType = BodyType::None;
            }
        }

        // Auth inside http
        if (http.hasKey("auth")) {
            parseAuthNode(http["auth"], req.auth);
        }
    }

    // Top-level or request-level auth override
    if (node.hasKey("auth")) {
        parseAuthNode(node["auth"], req.auth);
    } else if (node.hasKey("request") && node["request"].hasKey("auth")) {
        parseAuthNode(node["request"]["auth"], req.auth);
    }

    // Runtime scripts
    if (node.hasKey("runtime") && node["runtime"].hasKey("scripts") && node["runtime"]["scripts"].isSequence()) {
        for (const auto& script : node["runtime"]["scripts"].sequence) {
            QString type = script["type"].asString().trimmed().toLower();
            QString code = script["code"].asString();
            if (type == "after-response" || type == "post-response") {
                req.scripts.postResponseScript = code;
            } else if (type == "before-request" || type == "pre-request") {
                req.scripts.preRequestScript = code;
            } else if (type == "test" || type == "tests") {
                req.scripts.tests = code;
            }
        }
    }

    return req;
}

RequestModel OpenCollectionParser::parseRequestText(const QString& yamlText, const QString& fallbackName) {
    YamlNode node = YamlNode::parse(yamlText);
    return parseRequest(node, fallbackName);
}

RequestModel OpenCollectionParser::parseRequestFile(const QString& filePath) {
    YamlNode node = YamlNode::parseFile(filePath);
    QString fallbackName = QFileInfo(filePath).completeBaseName();
    return parseRequest(node, fallbackName);
}

EnvironmentModel OpenCollectionParser::parseEnvironment(const YamlNode& node, const QString& fallbackName) {
    QString name = node["name"].asString(fallbackName);
    if (name.trimmed().isEmpty()) {
        name = fallbackName.isEmpty() ? QStringLiteral("Environment") : fallbackName;
    }

    EnvironmentModel env(name);
    if (node.hasKey("variables") && node["variables"].isSequence()) {
        for (const auto& item : node["variables"].sequence) {
            QString vName = item["name"].asString();
            QString vVal = item["value"].asString();
            bool isSecret = item["secret"].asBool(false);
            bool enabled = !item["disabled"].asBool(false);
            env.addOrUpdateVariable(vName, vVal, isSecret, enabled);
        }
    }
    return env;
}

EnvironmentModel OpenCollectionParser::parseEnvironmentText(const QString& yamlText, const QString& fallbackName) {
    YamlNode node = YamlNode::parse(yamlText);
    return parseEnvironment(node, fallbackName);
}

EnvironmentModel OpenCollectionParser::parseEnvironmentFile(const QString& filePath, const QString& fallbackName) {
    YamlNode node = YamlNode::parseFile(filePath);
    QString name = fallbackName.isEmpty() ? QFileInfo(filePath).completeBaseName() : fallbackName;
    return parseEnvironment(node, name);
}

OpenCollectionFolderInfo OpenCollectionParser::parseFolder(const YamlNode& node, const QString& fallbackName) {
    OpenCollectionFolderInfo info;
    info.name = node["info"]["name"].asString(fallbackName);
    if (info.name.trimmed().isEmpty()) info.name = fallbackName;
    info.seq = node["info"]["seq"].asInt(1);

    if (node.hasKey("request")) {
        const auto& req = node["request"];
        if (req.hasKey("auth")) {
            parseAuthNode(req["auth"], info.auth);
        }
        if (req.hasKey("headers") && req["headers"].isSequence()) {
            for (const auto& h : req["headers"].sequence) {
                QString hn = h["name"].asString();
                QString hv = h["value"].asString();
                bool disabled = h["disabled"].asBool(false);
                info.headers.append(HttpHeader{hn, hv, !disabled, QString()});
                info.vars.insert(hn, hv);
            }
        }
    }
    if (node.hasKey("auth")) {
        parseAuthNode(node["auth"], info.auth);
    }
    return info;
}

OpenCollectionFolderInfo OpenCollectionParser::parseFolderFile(const QString& filePath, const QString& fallbackName) {
    YamlNode node = YamlNode::parseFile(filePath);
    QString name = fallbackName.isEmpty() ? QFileInfo(filePath).dir().dirName() : fallbackName;
    return parseFolder(node, name);
}

OpenCollectionInfo OpenCollectionParser::parseCollection(const YamlNode& node, const QString& fallbackName) {
    OpenCollectionInfo info;
    info.version = node["opencollection"].asString("1.0.0");
    info.name = node["info"]["name"].asString(fallbackName);
    if (info.name.trimmed().isEmpty()) info.name = fallbackName;

    if (node.hasKey("auth")) {
        parseAuthNode(node["auth"], info.auth);
    }
    if (node.hasKey("request") && node["request"].hasKey("auth")) {
        parseAuthNode(node["request"]["auth"], info.auth);
    }
    return info;
}

OpenCollectionInfo OpenCollectionParser::parseCollectionFile(const QString& filePath, const QString& fallbackName) {
    YamlNode node = YamlNode::parseFile(filePath);
    QString name = fallbackName.isEmpty() ? QFileInfo(filePath).dir().dirName() : fallbackName;
    return parseCollection(node, name);
}

} // namespace poppy::core
