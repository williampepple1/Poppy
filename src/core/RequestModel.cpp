#include "RequestModel.h"
#include "auth/AwsSigV4Signer.h"
#include <QUuid>
#include <QUrlQuery>
#include <QRegularExpression>

namespace poppy::core {

QString methodToString(HttpMethod method) {
    switch (method) {
        case HttpMethod::GET: return "GET";
        case HttpMethod::POST: return "POST";
        case HttpMethod::PUT: return "PUT";
        case HttpMethod::DELETE: return "DELETE";
        case HttpMethod::PATCH: return "PATCH";
        case HttpMethod::HEAD: return "HEAD";
        case HttpMethod::OPTIONS: return "OPTIONS";
    }
    return "GET";
}

HttpMethod stringToMethod(const QString& str) {
    const QString upper = str.trimmed().toUpper();
    if (upper == "POST") return HttpMethod::POST;
    if (upper == "PUT") return HttpMethod::PUT;
    if (upper == "DELETE") return HttpMethod::DELETE;
    if (upper == "PATCH") return HttpMethod::PATCH;
    if (upper == "HEAD") return HttpMethod::HEAD;
    if (upper == "OPTIONS") return HttpMethod::OPTIONS;
    return HttpMethod::GET;
}

QString bodyTypeToString(BodyType type) {
    switch (type) {
        case BodyType::None: return "none";
        case BodyType::Json: return "json";
        case BodyType::Text: return "text";
        case BodyType::Xml: return "xml";
        case BodyType::FormUrlEncoded: return "form-urlencoded";
        case BodyType::MultipartForm: return "multipart-form";
        case BodyType::Binary: return "binary";
        case BodyType::GraphQL: return "graphql";
    }
    return "none";
}

BodyType stringToBodyType(const QString& str) {
    const QString lower = str.trimmed().toLower();
    if (lower == "json") return BodyType::Json;
    if (lower == "text") return BodyType::Text;
    if (lower == "xml") return BodyType::Xml;
    if (lower == "form-urlencoded" || lower == "form_urlencoded") return BodyType::FormUrlEncoded;
    if (lower == "multipart-form" || lower == "multipart_form" || lower == "form-data") return BodyType::MultipartForm;
    if (lower == "binary") return BodyType::Binary;
    if (lower == "graphql") return BodyType::GraphQL;
    return BodyType::None;
}

QString authTypeToString(AuthType type) {
    switch (type) {
        case AuthType::None: return "none";
        case AuthType::Inherit: return "inherit";
        case AuthType::Bearer: return "bearer";
        case AuthType::Basic: return "basic";
        case AuthType::ApiKey: return "apikey";
        case AuthType::OAuth2: return "oauth2";
        case AuthType::AwsSigV4: return "awsv4";
    }
    return "none";
}

AuthType stringToAuthType(const QString& str) {
    const QString lower = str.trimmed().toLower();
    if (lower == "bearer") return AuthType::Bearer;
    if (lower == "basic") return AuthType::Basic;
    if (lower == "apikey") return AuthType::ApiKey;
    if (lower == "oauth2") return AuthType::OAuth2;
    if (lower == "awsv4" || lower == "aws" || lower == "awssigv4") return AuthType::AwsSigV4;
    if (lower == "inherit") return AuthType::Inherit;
    return AuthType::None;
}

RequestModel::RequestModel() 
    : id(QUuid::createUuid().toString(QUuid::WithoutBraces)) {}

QString RequestModel::effectiveUrl() const {
    QString resUrl = url;

    // Substitute path params: :param or {param}
    for (const auto& param : pathParams) {
        if (!param.enabled || param.key.isEmpty()) continue;
        resUrl.replace(":" + param.key, param.value);
        resUrl.replace("{" + param.key + "}", param.value);
    }

    // Append query params
    QStringList queryParts;
    for (const auto& param : queryParams) {
        if (!param.enabled || param.key.isEmpty()) continue;
        queryParts.append(QUrl::toPercentEncoding(param.key) + "=" + QUrl::toPercentEncoding(param.value));
    }

    // Add API key if placement is query
    if (auth.type == AuthType::ApiKey && auth.apiKeyPlacement.toLower() == "query" && !auth.apiKeyName.isEmpty()) {
        queryParts.append(QUrl::toPercentEncoding(auth.apiKeyName) + "=" + QUrl::toPercentEncoding(auth.apiKeyValue));
    }

    if (!queryParts.isEmpty()) {
        const QString joinChar = resUrl.contains('?') ? "&" : "?";
        resUrl += joinChar + queryParts.join('&');
    }

    return resUrl;
}

QList<HttpHeader> RequestModel::effectiveHeaders() const {
    QList<HttpHeader> result = headers;

    // Inject Auth headers
    if (auth.type == AuthType::Bearer && !auth.bearerToken.isEmpty()) {
        result.append(HttpHeader{
            .name = "Authorization",
            .value = "Bearer " + auth.bearerToken,
            .enabled = true
        });
    } else if (auth.type == AuthType::Basic && (!auth.basicUsername.isEmpty() || !auth.basicPassword.isEmpty())) {
        const QString creds = auth.basicUsername + ":" + auth.basicPassword;
        const QString base64 = creds.toUtf8().toBase64();
        result.append(HttpHeader{
            .name = "Authorization",
            .value = "Basic " + base64,
            .enabled = true
        });
    } else if (auth.type == AuthType::ApiKey && auth.apiKeyPlacement.toLower() != "query" && !auth.apiKeyName.isEmpty()) {
        result.append(HttpHeader{
            .name = auth.apiKeyName,
            .value = auth.apiKeyValue,
            .enabled = true
        });
    } else if (auth.type == AuthType::OAuth2 && !auth.oauth2AccessToken.isEmpty()) {
        result.append(HttpHeader{
            .name = "Authorization",
            .value = "Bearer " + auth.oauth2AccessToken,
            .enabled = true
        });
    } else if (auth.type == AuthType::AwsSigV4) {
        auto awsHeaders = AwsSigV4Signer::generateAuthHeaders(*this);
        for (const auto& ah : awsHeaders) {
            result.append(ah);
        }
    }

    // Ensure Content-Type is set if body is JSON or GraphQL and not already present
    if ((bodyType == BodyType::Json && !bodyContent.trimmed().isEmpty()) || bodyType == BodyType::GraphQL) {
        bool hasContentType = false;
        for (const auto& h : result) {
            if (h.enabled && h.name.compare("Content-Type", Qt::CaseInsensitive) == 0) {
                hasContentType = true;
                break;
            }
        }
        if (!hasContentType) {
            result.append(HttpHeader{
                .name = "Content-Type",
                .value = "application/json",
                .enabled = true
            });
        }
    }

    return result;
}

QString RequestModel::toCurlCommand() const {
    QStringList parts;
    parts.append("curl");
    parts.append("-X " + methodToString(method));

    for (const auto& h : effectiveHeaders()) {
        if (!h.enabled || h.name.isEmpty()) continue;
        // Escape quotes
        QString val = h.value;
        val.replace("\"", "\\\"");
        parts.append(QString("-H \"%1: %2\"").arg(h.name, val));
    }

    if (bodyType == BodyType::GraphQL) {
        QString escaped = QString("{\"query\": \"%1\"}").arg(graphqlQuery.trimmed().replace("\"", "\\\"").replace("\n", "\\n"));
        parts.append(QString("-d \"%1\"").arg(escaped));
    } else if (bodyType != BodyType::None && !bodyContent.isEmpty()) {
        QString escaped = bodyContent;
        escaped.replace("\"", "\\\"");
        escaped.replace("\n", "");
        parts.append(QString("-d \"%1\"").arg(escaped));
    }

    parts.append(QString("\"%1\"").arg(effectiveUrl()));
    return parts.join(" \\\n  ");
}

} // namespace poppy::core
