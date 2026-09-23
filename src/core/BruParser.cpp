#include "BruParser.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

namespace poppy::core {

RequestModel BruParser::parseFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    QTextStream in(&file);
    return parse(in.readAll());
}

RequestModel BruParser::parse(const QString& content) {
    RequestModel req;
    
    // Split into lines
    QStringList lines = content.split('\n');
    
    int i = 0;
    while (i < lines.size()) {
        QString line = lines[i].trimmed();
        if (line.isEmpty() || line.startsWith('#')) {
            ++i;
            continue;
        }

        // Look for block header: <block_name> {
        if (line.endsWith('{')) {
            QString blockName = line.left(line.length() - 1).trimmed();
            ++i;

            // Check if this is a freeform / code block:
            // body:json, body:text, body:xml, script:pre-request, script:post-response, tests
            bool isCodeBlock = (blockName.startsWith("body:") && blockName != "body:multipart-form" && blockName != "body:binary") ||
                               blockName.startsWith("script:") ||
                               blockName == "tests";

            if (isCodeBlock) {
                QString blockContent;
                int braceDepth = 1;
                while (i < lines.size() && braceDepth > 0) {
                    QString curLine = lines[i];
                    QString trimmedCur = curLine.trimmed();

                    // Check brace depth adjustments
                    for (QChar ch : trimmedCur) {
                        if (ch == '{') ++braceDepth;
                        else if (ch == '}') --braceDepth;
                    }

                    if (braceDepth > 0) {
                        if (!blockContent.isEmpty()) blockContent.append('\n');
                        blockContent.append(curLine);
                    }
                    ++i;
                }

                // Assign to model
                if (blockName == "body:json") {
                    req.bodyType = BodyType::Json;
                    req.bodyContent = blockContent.trimmed();
                } else if (blockName == "body:graphql") {
                    req.bodyType = BodyType::GraphQL;
                    req.graphqlQuery = blockContent.trimmed();
                } else if (blockName == "body:graphql:vars") {
                    req.graphqlVariables = blockContent.trimmed();
                } else if (blockName == "body:text") {
                    req.bodyType = BodyType::Text;
                    req.bodyContent = blockContent;
                } else if (blockName == "body:xml") {
                    req.bodyType = BodyType::Xml;
                    req.bodyContent = blockContent;
                } else if (blockName == "body:form-urlencoded") {
                    req.bodyType = BodyType::FormUrlEncoded;
                    req.bodyContent = blockContent;
                } else if (blockName == "script:pre-request") {
                    req.scripts.preRequestScript = blockContent;
                } else if (blockName == "script:post-response") {
                    req.scripts.postResponseScript = blockContent;
                } else if (blockName == "tests") {
                    req.scripts.tests = blockContent;
                }
            } else {
                // Key-value block: meta, get, post, headers, params:query, params:path, auth:bearer, etc.
                while (i < lines.size()) {
                    QString kvLine = lines[i].trimmed();
                    if (kvLine == "}") {
                        ++i;
                        break;
                    }

                    if (!kvLine.isEmpty() && !kvLine.startsWith('#')) {
                        if (blockName == "assertions") {
                            bool enabled = true;
                            if (kvLine.startsWith('~')) {
                                enabled = false;
                                kvLine = kvLine.mid(1).trimmed();
                            }
                            QStringList parts = kvLine.split(' ', Qt::SkipEmptyParts);
                            if (parts.size() >= 3) {
                                QString target = parts[0];
                                QString op = parts[1];
                                QString expected = parts.mid(2).join(' ');
                                req.assertions.append(AssertionRule{
                                    .target = target,
                                    .op = op,
                                    .expected = expected,
                                    .enabled = enabled
                                });
                            }
                            ++i;
                            continue;
                        }

                        int colonIdx = kvLine.indexOf(':');
                        if (blockName == "body:multipart-form" && colonIdx < 0) {
                            colonIdx = kvLine.indexOf('=');
                        }
                        if (colonIdx > 0) {
                            QString key = kvLine.left(colonIdx).trimmed();
                            QString val = kvLine.mid(colonIdx + 1).trimmed();

                            bool enabled = true;
                            if (key.startsWith('~')) {
                                enabled = false;
                                key = key.mid(1).trimmed();
                            }

                            if (blockName == "meta") {
                                if (key == "name") req.name = val;
                                else if (key == "seq") req.seq = val.toInt();
                            } else if (blockName == "get" || blockName == "post" || 
                                       blockName == "put" || blockName == "delete" || 
                                       blockName == "patch" || blockName == "head" || 
                                       blockName == "options") {
                                req.method = stringToMethod(blockName);
                                if (key == "url") req.url = val;
                                else if (key == "body") req.bodyType = stringToBodyType(val);
                                else if (key == "auth") req.auth.type = stringToAuthType(val);
                            } else if (blockName == "headers") {
                                req.headers.append(HttpHeader{
                                    .name = key,
                                    .value = val,
                                    .enabled = enabled
                                });
                            } else if (blockName == "params:query") {
                                req.queryParams.append(HttpParam{
                                    .key = key,
                                    .value = val,
                                    .enabled = enabled
                                });
                            } else if (blockName == "params:path") {
                                req.pathParams.append(HttpParam{
                                    .key = key,
                                    .value = val,
                                    .enabled = enabled
                                });
                            } else if (blockName == "body:binary") {
                                req.bodyType = BodyType::Binary;
                                if (key == "file" || key == "path") {
                                    if ((val.startsWith('"') && val.endsWith('"') && val.size() >= 2)
                                        || (val.startsWith('\'') && val.endsWith('\'') && val.size() >= 2)) {
                                        val = val.mid(1, val.size() - 2);
                                    }
                                    req.bodyContent = val;
                                }
                            } else if (blockName == "body:multipart-form") {
                                req.bodyType = BodyType::MultipartForm;
                                bool isFile = val.startsWith('@');
                                if (isFile) val = val.mid(1);
                                req.formDataParams.append(FormDataParam{
                                    .key = key,
                                    .value = val,
                                    .isFile = isFile,
                                    .enabled = enabled
                                });
                            } else if (blockName == "auth") {
                                if (key == "mode" || key == "type") req.auth.type = stringToAuthType(val);
                            } else if (blockName == "auth:bearer") {
                                if (key == "token") req.auth.bearerToken = val;
                            } else if (blockName == "auth:basic") {
                                if (key == "username") req.auth.basicUsername = val;
                                else if (key == "password") req.auth.basicPassword = val;
                            } else if (blockName == "auth:apikey") {
                                if (key == "key") req.auth.apiKeyName = val;
                                else if (key == "value") req.auth.apiKeyValue = val;
                                else if (key == "placement") req.auth.apiKeyPlacement = val;
                            } else if (blockName == "auth:oauth2") {
                                if (key == "token" || key == "access_token") req.auth.oauth2AccessToken = val;
                            } else if (blockName == "auth:awsv4" || blockName == "auth:aws") {
                                if (key == "accessKeyId" || key == "accessKey") req.auth.awsAccessKey = val;
                                else if (key == "secretAccessKey" || key == "secretKey") req.auth.awsSecretKey = val;
                                else if (key == "sessionToken") req.auth.awsSessionToken = val;
                                else if (key == "region") req.auth.awsRegion = val;
                                else if (key == "service") req.auth.awsService = val;
                            } else if (blockName == "auth:digest") {
                                if (key == "username") req.auth.digestUsername = val;
                                else if (key == "password") req.auth.digestPassword = val;
                            } else if (blockName == "auth:ntlm") {
                                if (key == "username") req.auth.ntlmUsername = val;
                                else if (key == "password") req.auth.ntlmPassword = val;
                                else if (key == "domain") req.auth.ntlmDomain = val;
                                else if (key == "workstation") req.auth.ntlmWorkstation = val;
                            } else if (blockName == "settings") {
                                if (key == "proxy") req.proxy = val;
                            }
                        }
                    }
                    ++i;
                }
            }
        } else {
            ++i;
        }
    }

    return req;
}

QMap<QString, QString> BruParser::parseVars(const QString& content) {
    QMap<QString, QString> vars;
    const QStringList lines = content.split('\n');
    bool inVars = false;
    for (const QString& raw : lines) {
        const QString line = raw.trimmed();
        if (line.startsWith("vars") && line.endsWith('{')) {
            inVars = true;
            continue;
        }
        if (!inVars) continue;
        if (line == "}") {
            inVars = false;
            continue;
        }
        if (line.isEmpty() || line.startsWith('#')) continue;
        const int colon = line.indexOf(':');
        if (colon > 0) {
            vars.insert(line.left(colon).trimmed(), line.mid(colon + 1).trimmed());
        }
    }
    return vars;
}

QMap<QString, QString> BruParser::parseVarsFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    QTextStream in(&file);
    return parseVars(in.readAll());
}

int BruParser::parseMetaSeq(const QString& content, int fallback) {
    bool inMeta = false;
    const QStringList lines = content.split('\n');
    for (const QString& raw : lines) {
        const QString line = raw.trimmed();
        if (line.startsWith("meta") && line.endsWith('{')) {
            inMeta = true;
            continue;
        }
        if (!inMeta) continue;
        if (line == "}") break;
        if (line.startsWith("seq:")) {
            bool ok = false;
            const int seq = line.mid(4).trimmed().toInt(&ok);
            if (ok && seq > 0) return seq;
        }
    }
    return fallback;
}

} // namespace poppy::core
