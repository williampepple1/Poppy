#include "OpenCollectionWriter.h"
#include <QFile>
#include <QTextStream>
#include <QDir>

namespace poppy::core {

namespace {

QString formatScalar(const QString& val) {
    if (val.isEmpty()) return QStringLiteral("\"\"");
    // Check if needs quoting
    if (val.contains(':') || val.contains('#') || val.contains('\n') ||
        val.contains('\"') || val.contains('\'') || val.startsWith('{') ||
        val.startsWith('[') || val.startsWith(' ') || val.endsWith(' ')) {
        QString escaped = val;
        escaped.replace('\\', "\\\\");
        escaped.replace('\"', "\\\"");
        escaped.replace('\n', "\\n");
        escaped.replace('\r', "\\r");
        escaped.replace('\t', "\\t");
        return QStringLiteral("\"%1\"").arg(escaped);
    }
    return val;
}

QString formatBlockScalar(const QString& val, int indentSpaces) {
    QString indent(indentSpaces, ' ');
    QStringList lines = val.split('\n');
    QString result = QStringLiteral("|-\n");
    for (int i = 0; i < lines.size(); ++i) {
        result += indent + lines[i];
        if (i + 1 < lines.size()) result += '\n';
    }
    return result;
}

} // namespace

QString OpenCollectionWriter::serializeRequest(const RequestModel& req) {
    QString out;
    QTextStream ts(&out);

    ts << "info:\n";
    ts << "  name: " << formatScalar(req.name) << "\n";
    ts << "  type: http\n";
    ts << "  seq: " << req.seq << "\n\n";

    ts << "http:\n";
    ts << "  method: " << methodToString(req.method) << "\n";
    ts << "  url: " << formatScalar(req.url) << "\n";

    // Params
    if (!req.queryParams.isEmpty() || !req.pathParams.isEmpty()) {
        ts << "  params:\n";
        for (const auto& p : req.pathParams) {
            ts << "    - name: " << formatScalar(p.key) << "\n";
            ts << "      value: " << formatScalar(p.value) << "\n";
            ts << "      type: path\n";
            if (!p.enabled) ts << "      disabled: true\n";
        }
        for (const auto& p : req.queryParams) {
            ts << "    - name: " << formatScalar(p.key) << "\n";
            ts << "      value: " << formatScalar(p.value) << "\n";
            ts << "      type: query\n";
            if (!p.enabled) ts << "      disabled: true\n";
        }
    }

    // Headers
    if (!req.headers.isEmpty()) {
        ts << "  headers:\n";
        for (const auto& h : req.headers) {
            ts << "    - name: " << formatScalar(h.name) << "\n";
            ts << "      value: " << formatScalar(h.value) << "\n";
            if (!h.enabled) ts << "      disabled: true\n";
        }
    }

    // Body
    if (req.bodyType != BodyType::None) {
        ts << "  body:\n";
        switch (req.bodyType) {
        case BodyType::Json:
            ts << "    type: json\n";
            ts << "    data: " << formatBlockScalar(req.bodyContent, 6) << "\n";
            break;
        case BodyType::Text:
            ts << "    type: text\n";
            ts << "    data: " << formatBlockScalar(req.bodyContent, 6) << "\n";
            break;
        case BodyType::Xml:
            ts << "    type: xml\n";
            ts << "    data: " << formatBlockScalar(req.bodyContent, 6) << "\n";
            break;
        case BodyType::FormUrlEncoded:
            ts << "    type: form-urlencoded\n";
            if (!req.formDataParams.isEmpty()) {
                ts << "    data:\n";
                for (const auto& fp : req.formDataParams) {
                    ts << "      - name: " << formatScalar(fp.key) << "\n";
                    ts << "        value: " << formatScalar(fp.value) << "\n";
                    if (!fp.enabled) ts << "        disabled: true\n";
                }
            } else if (!req.bodyContent.isEmpty()) {
                ts << "    data: " << formatBlockScalar(req.bodyContent, 6) << "\n";
            }
            break;
        case BodyType::MultipartForm:
            ts << "    type: multipart-form\n";
            if (!req.formDataParams.isEmpty()) {
                ts << "    data:\n";
                for (const auto& fp : req.formDataParams) {
                    ts << "      - name: " << formatScalar(fp.key) << "\n";
                    ts << "        value: " << formatScalar(fp.value) << "\n";
                    ts << "        type: " << (fp.isFile ? "file" : "text") << "\n";
                    if (!fp.enabled) ts << "        disabled: true\n";
                }
            }
            break;
        case BodyType::GraphQL:
            ts << "    type: graphql\n";
            ts << "    data:\n";
            ts << "      query: " << formatBlockScalar(req.graphqlQuery, 8) << "\n";
            if (!req.graphqlVariables.isEmpty()) {
                ts << "      variables: " << formatBlockScalar(req.graphqlVariables, 8) << "\n";
            }
            break;
        default:
            break;
        }
    }

    // Auth
    switch (req.auth.type) {
    case AuthType::Inherit:
        ts << "  auth: inherit\n";
        break;
    case AuthType::Bearer:
        ts << "  auth:\n";
        ts << "    type: bearer\n";
        ts << "    token: " << formatScalar(req.auth.bearerToken) << "\n";
        break;
    case AuthType::Basic:
        ts << "  auth:\n";
        ts << "    type: basic\n";
        ts << "    username: " << formatScalar(req.auth.basicUsername) << "\n";
        ts << "    password: " << formatScalar(req.auth.basicPassword) << "\n";
        break;
    case AuthType::ApiKey:
        ts << "  auth:\n";
        ts << "    type: api-key\n";
        ts << "    name: " << formatScalar(req.auth.apiKeyName) << "\n";
        ts << "    value: " << formatScalar(req.auth.apiKeyValue) << "\n";
        ts << "    placement: " << formatScalar(req.auth.apiKeyPlacement) << "\n";
        break;
    case AuthType::OAuth2:
        ts << "  auth:\n";
        ts << "    type: oauth2\n";
        ts << "    accessToken: " << formatScalar(req.auth.oauth2AccessToken) << "\n";
        break;
    case AuthType::AwsSigV4:
        ts << "  auth:\n";
        ts << "    type: awsv4\n";
        ts << "    accessKey: " << formatScalar(req.auth.awsAccessKey) << "\n";
        ts << "    secretKey: " << formatScalar(req.auth.awsSecretKey) << "\n";
        ts << "    sessionToken: " << formatScalar(req.auth.awsSessionToken) << "\n";
        ts << "    region: " << formatScalar(req.auth.awsRegion) << "\n";
        ts << "    service: " << formatScalar(req.auth.awsService) << "\n";
        break;
    default:
        break;
    }

    // Scripts
    if (!req.scripts.postResponseScript.trimmed().isEmpty() ||
        !req.scripts.preRequestScript.trimmed().isEmpty() ||
        !req.scripts.tests.trimmed().isEmpty()) {
        ts << "\nruntime:\n";
        ts << "  scripts:\n";
        if (!req.scripts.preRequestScript.trimmed().isEmpty()) {
            ts << "    - type: before-request\n";
            ts << "      code: " << formatBlockScalar(req.scripts.preRequestScript, 8) << "\n";
        }
        if (!req.scripts.postResponseScript.trimmed().isEmpty()) {
            ts << "    - type: after-response\n";
            ts << "      code: " << formatBlockScalar(req.scripts.postResponseScript, 8) << "\n";
        }
        if (!req.scripts.tests.trimmed().isEmpty()) {
            ts << "    - type: test\n";
            ts << "      code: " << formatBlockScalar(req.scripts.tests, 8) << "\n";
        }
    }

    return out;
}

bool OpenCollectionWriter::writeRequestFile(const QString& filePath, const RequestModel& req) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream out(&file);
    out << serializeRequest(req);
    return true;
}

QString OpenCollectionWriter::serializeEnvironment(const EnvironmentModel& env) {
    QString out;
    QTextStream ts(&out);

    ts << "name: " << formatScalar(env.name()) << "\n";
    ts << "variables:\n";
    for (const auto& v : env.variables()) {
        ts << "  - name: " << formatScalar(v.name) << "\n";
        ts << "    value: " << formatScalar(v.value) << "\n";
        if (v.isSecret) ts << "    secret: true\n";
        if (!v.enabled) ts << "    disabled: true\n";
    }
    return out;
}

bool OpenCollectionWriter::writeEnvironmentFile(const QString& filePath, const EnvironmentModel& env) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream out(&file);
    out << serializeEnvironment(env);
    return true;
}

QString OpenCollectionWriter::serializeFolder(const QString& name, int seq, const AuthModel& auth, const QMap<QString, QString>& vars) {
    QString out;
    QTextStream ts(&out);

    ts << "info:\n";
    ts << "  name: " << formatScalar(name) << "\n";
    ts << "  type: folder\n";
    ts << "  seq: " << seq << "\n\n";

    ts << "request:\n";
    if (!vars.isEmpty()) {
        ts << "  headers:\n";
        for (auto it = vars.begin(); it != vars.end(); ++it) {
            ts << "    - name: " << formatScalar(it.key()) << "\n";
            ts << "      value: " << formatScalar(it.value()) << "\n";
        }
    }

    if (auth.type == AuthType::Inherit) {
        ts << "  auth: inherit\n";
    } else if (auth.type == AuthType::Bearer) {
        ts << "  auth:\n";
        ts << "    type: bearer\n";
        ts << "    token: " << formatScalar(auth.bearerToken) << "\n";
    }
    return out;
}

bool OpenCollectionWriter::writeFolderFile(const QString& folderPath, const QString& name, int seq, const AuthModel& auth, const QMap<QString, QString>& vars) {
    QDir dir(folderPath);
    QString path = dir.filePath(QStringLiteral("folder.yml"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream out(&file);
    out << serializeFolder(name, seq, auth, vars);
    return true;
}

} // namespace poppy::core
