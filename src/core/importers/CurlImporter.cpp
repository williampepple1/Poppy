#include "CurlImporter.h"
#include <QRegularExpression>
#include <QJsonDocument>

namespace poppy::core {

namespace {
    QStringList tokenizeCommandLine(const QString& cmd) {
        QStringList tokens;
        QString current;
        bool inSingleQuote = false;
        bool inDoubleQuote = false;
        bool escaped = false;

        for (int i = 0; i < cmd.length(); ++i) {
            QChar c = cmd[i];

            if (escaped) {
                current.append(c);
                escaped = false;
                continue;
            }

            if (c == '\\' && !inSingleQuote) {
                escaped = true;
                continue;
            }

            if (c == '\'' && !inDoubleQuote) {
                inSingleQuote = !inSingleQuote;
                continue;
            }

            if (c == '"' && !inSingleQuote) {
                inDoubleQuote = !inDoubleQuote;
                continue;
            }

            if (c.isSpace() && !inSingleQuote && !inDoubleQuote) {
                if (!current.isEmpty()) {
                    tokens.append(current);
                    current.clear();
                }
            } else {
                current.append(c);
            }
        }

        if (!current.isEmpty()) {
            tokens.append(current);
        }

        return tokens;
    }
}

RequestModel CurlImporter::importCurl(const QString& curlCommand) {
    RequestModel req;
    req.name = "Imported cURL Request";
    req.method = HttpMethod::GET;

    QString cleaned = curlCommand.trimmed();
    // remove backslash line continuations
    cleaned.replace("\\\n", " ");
    cleaned.replace("\\\r\n", " ");

    QStringList tokens = tokenizeCommandLine(cleaned);
    if (tokens.isEmpty()) return req;

    // Skip "curl" token if present
    int start = 0;
    if (tokens[0].toLower() == "curl") {
        start = 1;
    }

    bool explicitMethod = false;

    for (int i = start; i < tokens.size(); ++i) {
        QString tok = tokens[i];

        if ((tok == "-X" || tok == "--request") && i + 1 < tokens.size()) {
            req.method = stringToMethod(tokens[++i]);
            explicitMethod = true;
        } else if ((tok == "-H" || tok == "--header") && i + 1 < tokens.size()) {
            QString hStr = tokens[++i];
            int colon = hStr.indexOf(':');
            if (colon > 0) {
                QString name = hStr.left(colon).trimmed();
                QString val = hStr.mid(colon + 1).trimmed();
                req.headers.append(HttpHeader{.name = name, .value = val, .enabled = true});
            }
        } else if ((tok == "-d" || tok == "--data" || tok == "--data-raw" || tok == "--data-ascii") && i + 1 < tokens.size()) {
            req.bodyContent = tokens[++i];
            if (!explicitMethod) {
                req.method = HttpMethod::POST;
            }
            // Detect JSON or Text
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(req.bodyContent.toUtf8(), &err);
            if (err.error == QJsonParseError::NoError && !doc.isNull()) {
                req.bodyType = BodyType::Json;
            } else {
                req.bodyType = BodyType::Text;
            }
        } else if (tok == "--data-urlencode" && i + 1 < tokens.size()) {
            req.bodyType = BodyType::FormUrlEncoded;
            if (!req.bodyContent.isEmpty()) req.bodyContent.append("&");
            req.bodyContent.append(tokens[++i]);
            if (!explicitMethod) {
                req.method = HttpMethod::POST;
            }
        } else if ((tok == "-u" || tok == "--user") && i + 1 < tokens.size()) {
            QString userPass = tokens[++i];
            int colon = userPass.indexOf(':');
            req.auth.type = AuthType::Basic;
            if (colon > 0) {
                req.auth.basicUsername = userPass.left(colon);
                req.auth.basicPassword = userPass.mid(colon + 1);
            } else {
                req.auth.basicUsername = userPass;
            }
        } else if (!tok.startsWith('-')) {
            // URL candidate
            if (req.url.isEmpty()) {
                req.url = tok;
                // If quotes remained around URL, strip
                if ((req.url.startsWith('"') && req.url.endsWith('"')) ||
                    (req.url.startsWith('\'') && req.url.endsWith('\''))) {
                    req.url = req.url.mid(1, req.url.length() - 2);
                }
            }
        }
    }

    // Check if Authorization header has Bearer token and map to AuthModel
    for (int idx = 0; idx < req.headers.size(); ++idx) {
        if (req.headers[idx].name.compare("Authorization", Qt::CaseInsensitive) == 0) {
            QString val = req.headers[idx].value;
            if (val.startsWith("Bearer ", Qt::CaseInsensitive)) {
                req.auth.type = AuthType::Bearer;
                req.auth.bearerToken = val.mid(7).trimmed();
                req.headers.removeAt(idx);
                break;
            }
        }
    }

    return req;
}

} // namespace poppy::core
