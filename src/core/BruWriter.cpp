#include "BruWriter.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
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
    } else if (req.auth.type == AuthType::AwsSigV4) {
        ts << "auth:awsv4 {\n";
        ts << "  accessKeyId: " << req.auth.awsAccessKey << "\n";
        ts << "  secretAccessKey: " << req.auth.awsSecretKey << "\n";
        if (!req.auth.awsSessionToken.isEmpty()) ts << "  sessionToken: " << req.auth.awsSessionToken << "\n";
        ts << "  region: " << req.auth.awsRegion << "\n";
        ts << "  service: " << req.auth.awsService << "\n";
        ts << "}\n\n";
    } else if (req.auth.type == AuthType::Digest) {
        ts << "auth:digest {\n";
        ts << "  username: " << req.auth.digestUsername << "\n";
        ts << "  password: " << req.auth.digestPassword << "\n";
        ts << "}\n\n";
    } else if (req.auth.type == AuthType::NTLM) {
        ts << "auth:ntlm {\n";
        ts << "  username: " << req.auth.ntlmUsername << "\n";
        ts << "  password: " << req.auth.ntlmPassword << "\n";
        if (!req.auth.ntlmDomain.isEmpty()) ts << "  domain: " << req.auth.ntlmDomain << "\n";
        if (!req.auth.ntlmWorkstation.isEmpty()) ts << "  workstation: " << req.auth.ntlmWorkstation << "\n";
        ts << "}\n\n";
    }

    // body block
    if (req.bodyType == BodyType::GraphQL) {
        if (!req.graphqlQuery.trimmed().isEmpty()) {
            ts << "body:graphql {\n";
            ts << req.graphqlQuery << "\n";
            ts << "}\n\n";
        }
        if (!req.graphqlVariables.trimmed().isEmpty()) {
            ts << "body:graphql:vars {\n";
            ts << req.graphqlVariables << "\n";
            ts << "}\n\n";
        }
    } else if (req.bodyType == BodyType::MultipartForm && !req.formDataParams.isEmpty()) {
        ts << "body:multipart-form {\n";
        for (const auto& p : req.formDataParams) {
            if (p.key.isEmpty()) continue;
            ts << "  " << (p.enabled ? "" : "~") << p.key << ": " << (p.isFile ? "@" : "") << p.value << "\n";
        }
        ts << "}\n\n";
    } else if (req.bodyType != BodyType::None && !req.bodyContent.trimmed().isEmpty()) {
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

QString BruWriter::safeFileStem(const QString& name, const QString& fallback) {
    QString s = name.trimmed();
    static const QString invalid = QStringLiteral("<>:\"/\\|?*");
    for (QChar c : invalid) {
        s.replace(c, QLatin1Char('_'));
    }
    for (int i = 0; i < s.size(); ++i) {
        if (s[i].unicode() < 32) {
            s[i] = QLatin1Char('_');
        }
    }
    while (s.endsWith(QLatin1Char('.')) || s.endsWith(QLatin1Char(' '))) {
        s.chop(1);
    }
    static const QStringList reserved = {
        QStringLiteral("CON"), QStringLiteral("PRN"), QStringLiteral("AUX"),
        QStringLiteral("NUL"), QStringLiteral("COM1"), QStringLiteral("COM2"),
        QStringLiteral("COM3"), QStringLiteral("COM4"), QStringLiteral("COM5"),
        QStringLiteral("COM6"), QStringLiteral("COM7"), QStringLiteral("COM8"),
        QStringLiteral("COM9"), QStringLiteral("LPT1"), QStringLiteral("LPT2"),
        QStringLiteral("LPT3"), QStringLiteral("LPT4"), QStringLiteral("LPT5"),
        QStringLiteral("LPT6"), QStringLiteral("LPT7"), QStringLiteral("LPT8"),
        QStringLiteral("LPT9")
    };
    for (const auto& r : reserved) {
        if (s.compare(r, Qt::CaseInsensitive) == 0) {
            s.append(QLatin1Char('_'));
            break;
        }
    }
    if (s.size() > 80) {
        s = s.left(80);
    }
    if (s.trimmed().isEmpty()) {
        return fallback;
    }
    return s;
}

QString BruWriter::uniqueFilePath(const QString& directory, const QString& stem, const QString& extension) {
    QDir dir(directory);
    QString path = dir.filePath(stem + extension);
    int n = 1;
    while (QFileInfo::exists(path)) {
        path = dir.filePath(QStringLiteral("%1 (%2)%3").arg(stem).arg(n++).arg(extension));
    }
    return path;
}

} // namespace poppy::core
