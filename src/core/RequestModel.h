#pragma once

#include <QString>
#include <QList>
#include <QMap>
#include <QUrl>
#include <QUrlQuery>
#include "assertions/AssertionRule.h"

namespace poppy::core {

enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE,
    PATCH,
    HEAD,
    OPTIONS
};

QString methodToString(HttpMethod method);
HttpMethod stringToMethod(const QString& str);

enum class BodyType {
    None,
    Json,
    Text,
    Xml,
    FormUrlEncoded,
    MultipartForm,
    Binary,
    GraphQL
};

QString bodyTypeToString(BodyType type);
BodyType stringToBodyType(const QString& str);

struct HttpHeader {
    QString name;
    QString value;
    bool enabled{true};
    QString description;

    bool operator==(const HttpHeader& other) const = default;
};

struct HttpParam {
    QString key;
    QString value;
    bool enabled{true};
    QString description;

    bool operator==(const HttpParam& other) const = default;
};

struct FormDataParam {
    QString key;
    QString value;
    bool isFile{false};
    bool enabled{true};
    QString description;

    bool operator==(const FormDataParam& other) const = default;
};

enum class AuthType {
    None,
    Inherit,
    Bearer,
    Basic,
    ApiKey,
    OAuth2,
    AwsSigV4,
    Digest,
    NTLM
};

QString authTypeToString(AuthType type);
AuthType stringToAuthType(const QString& str);

struct AuthModel {
    AuthType type{AuthType::None};
    
    // Bearer
    QString bearerToken;

    // Basic
    QString basicUsername;
    QString basicPassword;

    // API Key
    QString apiKeyName;
    QString apiKeyValue;
    QString apiKeyPlacement{"header"}; // "header" or "query"

    // OAuth2
    QString oauth2AccessToken;

    // AWS SigV4
    QString awsAccessKey;
    QString awsSecretKey;
    QString awsSessionToken;
    QString awsRegion;
    QString awsService;

    // Digest
    QString digestUsername;
    QString digestPassword;

    // NTLM
    QString ntlmUsername;
    QString ntlmPassword;
    QString ntlmDomain;
    QString ntlmWorkstation;

    bool operator==(const AuthModel& other) const = default;
};

struct ScriptModel {
    QString preRequestScript;
    QString postResponseScript;
    QString tests;

    bool operator==(const ScriptModel& other) const = default;
};

class RequestModel {
public:
    RequestModel();

    QString id;
    QString name{"New Request"};
    QString description;
    HttpMethod method{HttpMethod::GET};
    QString url;
    int seq{1};

    QList<HttpParam> queryParams;
    QList<HttpParam> pathParams;
    QList<HttpHeader> headers;
    QList<FormDataParam> formDataParams;
    QString proxy;
    
    BodyType bodyType{BodyType::None};
    QString bodyContent;
    QString graphqlQuery;
    QString graphqlVariables;

    AuthModel auth;
    ScriptModel scripts;
    QList<AssertionRule> assertions;

    // Computed effective URL after query and path parameter resolution
    QString effectiveUrl() const;

    // Body bytes actually sent (GraphQL JSON envelope, encoded form, etc.)
    QByteArray effectiveBody() const;

    // Compute effective headers with Auth headers injected
    QList<HttpHeader> effectiveHeaders() const;

    // Export as cURL command
    QString toCurlCommand() const;

    bool operator==(const RequestModel& other) const = default;
};

} // namespace poppy::core
