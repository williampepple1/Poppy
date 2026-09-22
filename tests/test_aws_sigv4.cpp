#include <iostream>
#include <cassert>
#include <core/RequestModel.h>
#include <core/auth/AwsSigV4Signer.h>

using namespace poppy::core;

int main() {
    std::cout << "Running test_aws_sigv4..." << std::endl;

    // Test computeSignature directly
    QString secret = "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY";
    QString dateStamp = "20260921";
    QString region = "us-east-1";
    QString service = "s3";
    QString stringToSign = "AWS4-HMAC-SHA256\n20260921T120000Z\n20260921/us-east-1/s3/aws4_request\ncecfd98fb5299277f60b3e0f56e13c6107ad1836452d431ec6e9be49d7fbcfe1";

    QString sig = AwsSigV4Signer::computeSignature(secret, dateStamp, region, service, stringToSign);
    assert(!sig.isEmpty());
    assert(sig.length() == 64); // SHA-256 hex string is 64 characters

    // Test RequestModel header generation
    RequestModel req;
    req.method = HttpMethod::GET;
    req.url = "https://example.execute-api.us-east-1.amazonaws.com/prod/users";
    req.auth.type = AuthType::AwsSigV4;
    req.auth.awsAccessKey = "AKIAIOSFODNN7EXAMPLE";
    req.auth.awsSecretKey = secret;
    req.auth.awsRegion = "us-east-1";
    req.auth.awsService = "execute-api";

    auto authHeaders = AwsSigV4Signer::generateAuthHeaders(req);
    assert(!authHeaders.isEmpty());

    bool hasAuth = false;
    bool hasDate = false;
    bool hasSha = false;

    for (const auto& h : authHeaders) {
        if (h.name.compare("Authorization", Qt::CaseInsensitive) == 0) {
            hasAuth = true;
            assert(h.value.startsWith("AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/"));
            assert(h.value.contains("SignedHeaders="));
            assert(h.value.contains("Signature="));
        } else if (h.name.compare("x-amz-date", Qt::CaseInsensitive) == 0) {
            hasDate = true;
            assert(h.value.endsWith("Z"));
        } else if (h.name.compare("x-amz-content-sha256", Qt::CaseInsensitive) == 0) {
            hasSha = true;
            assert(h.value.length() == 64);
        }
    }

    assert(hasAuth);
    assert(hasDate);
    assert(hasSha);

    // Verify effectiveHeaders() incorporates these headers
    auto effHeaders = req.effectiveHeaders();
    bool effHasAuth = false;
    for (const auto& h : effHeaders) {
        if (h.name.compare("Authorization", Qt::CaseInsensitive) == 0) {
            effHasAuth = true;
            break;
        }
    }
    assert(effHasAuth);

    RequestModel jsonReq = req;
    jsonReq.method = HttpMethod::POST;
    jsonReq.bodyType = BodyType::Json;
    jsonReq.bodyContent = QStringLiteral("{\"ok\":true}");
    auto jsonAuth = AwsSigV4Signer::generateAuthHeaders(jsonReq);
    bool signedContentType = false;
    for (const auto& h : jsonAuth) {
        if (h.name.compare("Authorization", Qt::CaseInsensitive) == 0) {
            assert(h.value.contains("content-type"));
            signedContentType = true;
        }
    }
    assert(signedContentType);
    auto jsonEff = jsonReq.effectiveHeaders();
    bool effHasCt = false;
    for (const auto& h : jsonEff) {
        if (h.enabled && h.name.compare("Content-Type", Qt::CaseInsensitive) == 0) {
            effHasCt = true;
        }
    }
    assert(effHasCt);

    RequestModel pathReq = req;
    pathReq.url = "https://example.execute-api.us-east-1.amazonaws.com/prod/foo bar";
    auto pathHeaders = AwsSigV4Signer::generateAuthHeaders(pathReq);
    assert(!pathHeaders.isEmpty());

    RequestModel gqlReq;
    gqlReq.method = HttpMethod::POST;
    gqlReq.url = req.url;
    gqlReq.auth = req.auth;
    gqlReq.bodyType = BodyType::GraphQL;
    gqlReq.graphqlQuery = "query { user { id } }";
    gqlReq.bodyContent = "this-is-not-the-graphql-envelope";
    QByteArray sent = gqlReq.effectiveBody();
    assert(QString::fromUtf8(sent).contains("query { user { id } }"));
    assert(!QString::fromUtf8(sent).contains("this-is-not-the-graphql-envelope"));

    std::cout << "test_aws_sigv4 passed successfully!" << std::endl;
    return 0;
}
