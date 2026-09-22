#include "AwsSigV4Signer.h"
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QDateTime>
#include <QUrl>
#include <QUrlQuery>
#include <QMap>

namespace poppy::core {

QString AwsSigV4Signer::computeSignature(const QString& secretKey, const QString& dateStamp, 
                                        const QString& region, const QString& service, 
                                        const QString& stringToSign) {
    QByteArray kSecret = QByteArray("AWS4") + secretKey.toUtf8();
    QByteArray kDate = QMessageAuthenticationCode::hash(dateStamp.toUtf8(), kSecret, QCryptographicHash::Sha256);
    QByteArray kRegion = QMessageAuthenticationCode::hash(region.toUtf8(), kDate, QCryptographicHash::Sha256);
    QByteArray kService = QMessageAuthenticationCode::hash(service.toUtf8(), kRegion, QCryptographicHash::Sha256);
    QByteArray kSigning = QMessageAuthenticationCode::hash(QByteArray("aws4_request"), kService, QCryptographicHash::Sha256);

    QByteArray signature = QMessageAuthenticationCode::hash(stringToSign.toUtf8(), kSigning, QCryptographicHash::Sha256).toHex();
    return QString::fromLatin1(signature);
}

QList<HttpHeader> AwsSigV4Signer::generateAuthHeaders(const RequestModel& req) {
    QList<HttpHeader> authHeaders;
    if (req.auth.type != AuthType::AwsSigV4 || req.auth.awsAccessKey.isEmpty() || req.auth.awsSecretKey.isEmpty()) {
        return authHeaders;
    }

    QUrl url(req.effectiveUrl());
    QString host = url.host();
    if (url.port() > 0 && url.port() != 80 && url.port() != 443) {
        host += ":" + QString::number(url.port());
    }

    QString amzDate = QDateTime::currentDateTimeUtc().toString("yyyyMMddTHHmmssZ");
    QString dateStamp = amzDate.left(8);
    QString region = req.auth.awsRegion.isEmpty() ? "us-east-1" : req.auth.awsRegion;
    QString service = req.auth.awsService.isEmpty() ? "execute-api" : req.auth.awsService;

    // 1. Hash of payload actually sent. Multipart is built by libcurl after signing,
    // so use the AWS unsigned-payload convention.
    QString payloadHash;
    if (req.bodyType == BodyType::MultipartForm) {
        payloadHash = QStringLiteral("UNSIGNED-PAYLOAD");
    } else {
        QByteArray bodyBytes = req.effectiveBody();
        payloadHash = QString::fromLatin1(QCryptographicHash::hash(bodyBytes, QCryptographicHash::Sha256).toHex());
    }

    // 2. Canonical headers — include user headers and the Content-Type
    // effectiveHeaders() will send, so the signature matches the wire request.
    QMap<QString, QString> headersToSign;
    headersToSign["host"] = host;
    headersToSign["x-amz-date"] = amzDate;
    headersToSign["x-amz-content-sha256"] = payloadHash;
    if (!req.auth.awsSessionToken.isEmpty()) {
        headersToSign["x-amz-security-token"] = req.auth.awsSessionToken;
    }
    for (const auto& h : req.headers) {
        if (!h.enabled || h.name.trimmed().isEmpty()) continue;
        headersToSign[h.name.trimmed().toLower()] = h.value.trimmed();
    }
    QString autoContentType;
    if ((req.bodyType == BodyType::Json && !req.bodyContent.trimmed().isEmpty()) ||
        req.bodyType == BodyType::GraphQL) {
        autoContentType = QStringLiteral("application/json");
    } else if (req.bodyType == BodyType::FormUrlEncoded) {
        autoContentType = QStringLiteral("application/x-www-form-urlencoded");
    }
    if (!autoContentType.isEmpty() && !headersToSign.contains(QStringLiteral("content-type"))) {
        headersToSign[QStringLiteral("content-type")] = autoContentType;
    }

    QString canonicalHeaders;
    QStringList signedHeadersList;
    for (auto it = headersToSign.constBegin(); it != headersToSign.constEnd(); ++it) {
        canonicalHeaders += it.key() + ":" + it.value().trimmed() + "\n";
        signedHeadersList.append(it.key());
    }
    QString signedHeaders = signedHeadersList.join(';');

    // 3. Canonical URI & Query string
    QString canonicalUri = url.path(QUrl::FullyEncoded);
    if (canonicalUri.isEmpty()) canonicalUri = QStringLiteral("/");
    if (!canonicalUri.startsWith('/')) canonicalUri.prepend('/');
    
    // Sort query parameters
    QUrlQuery query(url.query());
    auto queryItems = query.queryItems(QUrl::FullyEncoded);
    std::sort(queryItems.begin(), queryItems.end(), [](const QPair<QString, QString>& a, const QPair<QString, QString>& b) {
        return a.first < b.first;
    });
    QStringList canonicalQueryParts;
    for (const auto& item : queryItems) {
        canonicalQueryParts.append(item.first + "=" + item.second);
    }
    QString canonicalQuery = canonicalQueryParts.join('&');

    // 4. Canonical Request
    QString canonicalRequest = QString("%1\n%2\n%3\n%4\n%5\n%6")
        .arg(methodToString(req.method))
        .arg(canonicalUri)
        .arg(canonicalQuery)
        .arg(canonicalHeaders)
        .arg(signedHeaders)
        .arg(payloadHash);

    QByteArray canonicalRequestHash = QCryptographicHash::hash(canonicalRequest.toUtf8(), QCryptographicHash::Sha256).toHex();

    // 5. String to Sign
    QString credentialScope = QString("%1/%2/%3/aws4_request").arg(dateStamp, region, service);
    QString stringToSign = QString("AWS4-HMAC-SHA256\n%1\n%2\n%3")
        .arg(amzDate)
        .arg(credentialScope)
        .arg(QString::fromLatin1(canonicalRequestHash));

    // 6. Signature
    QString signature = computeSignature(req.auth.awsSecretKey, dateStamp, region, service, stringToSign);

    // 7. Authorization Header
    QString authHeaderValue = QString("AWS4-HMAC-SHA256 Credential=%1/%2, SignedHeaders=%3, Signature=%4")
        .arg(req.auth.awsAccessKey, credentialScope, signedHeaders, signature);

    authHeaders.append(HttpHeader{.name = "Authorization", .value = authHeaderValue, .enabled = true});
    authHeaders.append(HttpHeader{.name = "X-Amz-Date", .value = amzDate, .enabled = true});
    authHeaders.append(HttpHeader{.name = "X-Amz-Content-Sha256", .value = payloadHash, .enabled = true});
    if (!req.auth.awsSessionToken.isEmpty()) {
        authHeaders.append(HttpHeader{.name = "X-Amz-Security-Token", .value = req.auth.awsSessionToken, .enabled = true});
    }
    if (!autoContentType.isEmpty()) {
        bool hasCt = false;
        for (const auto& h : req.headers) {
            if (h.enabled && h.name.compare("Content-Type", Qt::CaseInsensitive) == 0) {
                hasCt = true;
                break;
            }
        }
        if (!hasCt) {
            authHeaders.append(HttpHeader{.name = "Content-Type", .value = autoContentType, .enabled = true});
        }
    }

    return authHeaders;
}

} // namespace poppy::core
