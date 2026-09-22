#include "MarkdownExporter.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QJsonDocument>

namespace poppy::core {

static QString slugify(const QString& text) {
    QString slug = text.toLower();
    QString res;
    for (QChar ch : slug) {
        if (ch.isLetterOrNumber()) {
            res.append(ch);
        } else if (ch == ' ' || ch == '-' || ch == '_') {
            if (!res.endsWith('-')) {
                res.append('-');
            }
        }
    }
    while (res.startsWith('-')) res.remove(0, 1);
    while (res.endsWith('-')) res.chop(1);
    return res.isEmpty() ? "endpoint" : res;
}

QString MarkdownExporter::exportToMarkdown(const QList<RequestModel>& requests,
                                          const QString& collectionName) {
    QString md;
    QTextStream out(&md);

    QString title = collectionName.isEmpty() ? "Poppy Collection" : collectionName;
    out << "# " << title << " — API Runbook\n\n";
    out << "> Generated automatically by **Poppy Native API Client**.\n";
    out << "> Total Endpoints: `" << requests.size() << "`\n\n";

    if (requests.isEmpty()) {
        out << "_No requests found in collection._\n";
        return md;
    }

    // Table of Contents
    out << "## 📋 Table of Contents\n\n";
    for (int i = 0; i < requests.size(); ++i) {
        const auto& req = requests[i];
        QString methodStr = methodToString(req.method);
        QString reqName = req.name.isEmpty() ? QString("Request %1").arg(i + 1) : req.name;
        QString anchor = slugify(QString("%1-%2").arg(methodStr, reqName));
        out << QString("- [`%1`] [%2](#%3) — `%4`\n")
                   .arg(methodStr, reqName, anchor, req.url.isEmpty() ? "/" : req.url);
    }
    out << "\n---\n\n";

    // Endpoints Details
    out << "## 🚀 Endpoints\n\n";
    for (int i = 0; i < requests.size(); ++i) {
        const auto& req = requests[i];
        QString methodStr = methodToString(req.method);
        QString reqName = req.name.isEmpty() ? QString("Request %1").arg(i + 1) : req.name;
        
        out << "### " << methodStr << " " << reqName << "\n\n";

        if (!req.description.isEmpty()) {
            out << req.description << "\n\n";
        }

        out << "- **Method:** `" << methodStr << "`\n";
        out << "- **URL:** `" << (req.url.isEmpty() ? "/" : req.url) << "`\n";

        // Auth
        QString authType = authTypeToString(req.auth.type);
        out << "- **Authentication:** `" << authType << "`\n";
        if (req.auth.type == AuthType::Bearer && !req.auth.bearerToken.isEmpty()) {
            out << "  - Token: `••••••••`\n";
        } else if (req.auth.type == AuthType::Basic && !req.auth.basicUsername.isEmpty()) {
            out << "  - Username: `" << req.auth.basicUsername << "`\n";
        } else if (req.auth.type == AuthType::ApiKey) {
            out << "  - Key Name: `" << req.auth.apiKeyName << "` (" << req.auth.apiKeyPlacement << ")\n";
        }

        out << "\n";

        // Query Parameters
        if (!req.queryParams.isEmpty()) {
            out << "#### Query Parameters\n\n";
            out << "| Parameter | Value | Status | Description |\n";
            out << "| :--- | :--- | :--- | :--- |\n";
            for (const auto& p : req.queryParams) {
                out << "| `" << p.key << "` | `" << p.value << "` | "
                    << (p.enabled ? "✅ Active" : "❌ Disabled") << " | "
                    << (p.description.isEmpty() ? "—" : p.description) << " |\n";
            }
            out << "\n";
        }

        // Headers
        if (!req.headers.isEmpty()) {
            out << "#### Request Headers\n\n";
            out << "| Header | Value | Status | Description |\n";
            out << "| :--- | :--- | :--- | :--- |\n";
            for (const auto& h : req.headers) {
                out << "| `" << h.name << "` | `" << h.value << "` | "
                    << (h.enabled ? "✅ Active" : "❌ Disabled") << " | "
                    << (h.description.isEmpty() ? "—" : h.description) << " |\n";
            }
            out << "\n";
        }

        // Request Body
        if (req.bodyType != BodyType::None) {
            out << "#### Request Body (`" << bodyTypeToString(req.bodyType) << "`)\n\n";
            if (req.bodyType == BodyType::Json) {
                out << "```json\n";
                // If it's valid JSON, format it nicely
                QJsonParseError err;
                QJsonDocument doc = QJsonDocument::fromJson(req.bodyContent.toUtf8(), &err);
                if (err.error == QJsonParseError::NoError) {
                    out << doc.toJson(QJsonDocument::Indented);
                } else {
                    out << req.bodyContent << "\n";
                }
                out << "```\n\n";
            } else if (req.bodyType == BodyType::GraphQL) {
                out << "```graphql\n" << req.graphqlQuery << "\n```\n\n";
                if (!req.graphqlVariables.isEmpty()) {
                    out << "**GraphQL Variables:**\n\n```json\n" << req.graphqlVariables << "\n```\n\n";
                }
            } else if (req.bodyType == BodyType::MultipartForm && !req.formDataParams.isEmpty()) {
                out << "| Key | Value | Type | Status |\n";
                out << "| :--- | :--- | :--- | :--- |\n";
                for (const auto& f : req.formDataParams) {
                    out << "| `" << f.key << "` | `" << f.value << "` | "
                        << (f.isFile ? "File" : "Text") << " | "
                        << (f.enabled ? "✅ Active" : "❌ Disabled") << " |\n";
                }
                out << "\n";
            } else if (!req.bodyContent.isEmpty()) {
                out << "```\n" << req.bodyContent << "\n```\n\n";
            }
        }

        // Assertions / Tests
        if (!req.assertions.isEmpty()) {
            out << "#### Declarative Assertions\n\n";
            out << "| Target | Operator | Expected Value | Status |\n";
            out << "| :--- | :--- | :--- | :--- |\n";
            for (const auto& a : req.assertions) {
                out << "| `" << a.target << "` | `"
                    << a.op << "` | `"
                    << a.expected << "` | "
                    << (a.enabled ? "✅ Active" : "❌ Disabled") << " |\n";
            }
            out << "\n";
        }

        // cURL snippet
        out << "#### Example cURL Command\n\n";
        out << "```bash\n";
        out << req.toCurlCommand() << "\n";
        out << "```\n\n";

        out << "---\n\n";
    }

    return md;
}

bool MarkdownExporter::exportToFile(const QString& filePath,
                                   const QList<RequestModel>& requests,
                                   const QString& collectionName,
                                   QString* outError) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (outError) {
            *outError = file.errorString();
        }
        return false;
    }

    QString content = exportToMarkdown(requests, collectionName);
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << content;
    return true;
}

} // namespace poppy::core
