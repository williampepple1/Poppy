#include "BruWriter.h"
#include <QFile>
#include <QTextStream>

namespace poppy::core {

QString BruWriter::serialize(const RequestModel& req) {
    QString out;
    QTextStream ts(&out);

    // meta block
    ts << "meta {\n";
    ts << "  name: " << (req.name.isEmpty() ? "Untitled Request" : req.name) << "\n";
    ts << "  type: http\n";
    ts << "  seq: " << req.seq << "\n";
    ts << "}\n\n";

    // method block
    QString methodStr = methodToString(req.method).toLower();
    ts << methodStr << " {\n";
    ts << "  url: " << req.url << "\n";
    ts << "  body: " << bodyTypeToString(req.bodyType) << "\n";
    ts << "  auth: " << authTypeToString(req.auth.type) << "\n";
    ts << "}\n\n";

    // params:query
    if (!req.queryParams.isEmpty()) {
        ts << "params:query {\n";
        for (const auto& p : req.queryParams) {
            if (!p.key.isEmpty()) {
                ts << "  " << (p.enabled ? "" : "~") << p.key << ": " << p.value << "\n";
            }
        }
        ts << "}\n\n";
    }

    // params:path
    if (!req.pathParams.isEmpty()) {
        ts << "params:path {\n";
        for (const auto& p : req.pathParams) {
            if (!p.key.isEmpty()) {
                ts << "  " << (p.enabled ? "" : "~") << p.key << ": " << p.value << "\n";
            }
        }
        ts << "}\n\n";
    }

    // headers
    if (!req.headers.isEmpty()) {
        ts << "headers {\n";
        for (const auto& h : req.headers) {
            if (!h.name.isEmpty()) {
                ts << "  " << (h.enabled ? "" : "~") << h.name << ": " << h.value << "\n";
            }
        }
        ts << "}\n\n";
    }

    // auth block
    if (req.auth.type == AuthType::Bearer && !req.auth.bearerToken.isEmpty()) {
        ts << "auth:bearer {\n";
        ts << "  token: " << req.auth.bearerToken << "\n";
        ts << "}\n\n";
    } else if (req.auth.type == AuthType::Basic) {
        ts << "auth:basic {\n";
        ts << "  username: " << req.auth.basicUsername << "\n";
        ts << "  password: " << req.auth.basicPassword << "\n";
        ts << "}\n\n";
    } else if (req.auth.type == AuthType::ApiKey) {
        ts << "auth:apikey {\n";
        ts << "  key: " << req.auth.apiKeyName << "\n";
        ts << "  value: " << req.auth.apiKeyValue << "\n";
        ts << "  placement: " << req.auth.apiKeyPlacement << "\n";
        ts << "}\n\n";
    } else if (req.auth.type == AuthType::OAuth2 && !req.auth.oauth2AccessToken.isEmpty()) {
        ts << "auth:oauth2 {\n";
        ts << "  token: " << req.auth.oauth2AccessToken << "\n";
        ts << "}\n\n";
    }

    // body block
    if (req.bodyType != BodyType::None && !req.bodyContent.trimmed().isEmpty()) {
        QString bodyBlock = "body:" + bodyTypeToString(req.bodyType);
        ts << bodyBlock << " {\n";
        ts << req.bodyContent << "\n";
        ts << "}\n\n";
    }

    // script:pre-request
    if (!req.scripts.preRequestScript.trimmed().isEmpty()) {
        ts << "script:pre-request {\n";
        ts << req.scripts.preRequestScript << "\n";
        ts << "}\n\n";
    }

    // script:post-response
    if (!req.scripts.postResponseScript.trimmed().isEmpty()) {
        ts << "script:post-response {\n";
        ts << req.scripts.postResponseScript << "\n";
        ts << "}\n\n";
    }

    // assertions
    if (!req.assertions.isEmpty()) {
        ts << "assertions {\n";
        for (const auto& a : req.assertions) {
            if (!a.target.isEmpty()) {
                ts << "  " << (a.enabled ? "" : "~") << a.target << " " << a.op << " " << a.expected << "\n";
            }
        }
        ts << "}\n\n";
    }

    // tests
    if (!req.scripts.tests.trimmed().isEmpty()) {
        ts << "tests {\n";
        ts << req.scripts.tests << "\n";
        ts << "}\n\n";
    }

    return out.trimmed() + "\n";
}

bool BruWriter::writeToFile(const QString& filePath, const RequestModel& req) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream out(&file);
    out << serialize(req);
    return true;
}

} // namespace poppy::core
