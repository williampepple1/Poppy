#include <iostream>
#include <cassert>
#include <core/BruParser.h>
#include <core/BruWriter.h>
#include <core/RequestModel.h>

using namespace poppy::core;

int main() {
    std::cout << "Running test_auth_digest_ntlm..." << std::endl;

    // 1. Enum conversions
    assert(authTypeToString(AuthType::Digest) == "digest");
    assert(authTypeToString(AuthType::NTLM) == "ntlm");
    assert(stringToAuthType("digest") == AuthType::Digest);
    assert(stringToAuthType("ntlm") == AuthType::NTLM);

    // 2. Digest Auth round-trip
    QString bruDigest = R"(meta {
  name: Digest Test Request
  type: http
  seq: 1
}

get {
  url: https://httpbin.org/digest-auth/auth/user/passwd
  body: none
  auth: digest
}

auth:digest {
  username: testuser
  password: secretpassword
}
)";

    RequestModel reqDigest = BruParser::parse(bruDigest);
    assert(reqDigest.name == "Digest Test Request");
    assert(reqDigest.auth.type == AuthType::Digest);
    assert(reqDigest.auth.digestUsername == "testuser");
    assert(reqDigest.auth.digestPassword == "secretpassword");

    QString serializedDigest = BruWriter::serialize(reqDigest);
    assert(serializedDigest.contains("auth: digest"));
    assert(serializedDigest.contains("auth:digest {"));
    assert(serializedDigest.contains("username: testuser"));
    assert(serializedDigest.contains("password: secretpassword"));

    RequestModel roundtripDigest = BruParser::parse(serializedDigest);
    assert(roundtripDigest.auth.type == AuthType::Digest);
    assert(roundtripDigest.auth.digestUsername == "testuser");
    assert(roundtripDigest.auth.digestPassword == "secretpassword");

    // 3. NTLM Auth round-trip
    QString bruNtlm = R"(meta {
  name: NTLM Windows Service
  type: http
  seq: 2
}

post {
  url: https://internal.corp/api/v1/resource
  body: none
  auth: ntlm
}

auth:ntlm {
  username: jdoe
  password: corporatePass!
  domain: CORP_DOMAIN
  workstation: WORKSTATION_01
}
)";

    RequestModel reqNtlm = BruParser::parse(bruNtlm);
    assert(reqNtlm.name == "NTLM Windows Service");
    assert(reqNtlm.auth.type == AuthType::NTLM);
    assert(reqNtlm.auth.ntlmUsername == "jdoe");
    assert(reqNtlm.auth.ntlmPassword == "corporatePass!");
    assert(reqNtlm.auth.ntlmDomain == "CORP_DOMAIN");
    assert(reqNtlm.auth.ntlmWorkstation == "WORKSTATION_01");

    QString serializedNtlm = BruWriter::serialize(reqNtlm);
    assert(serializedNtlm.contains("auth: ntlm"));
    assert(serializedNtlm.contains("auth:ntlm {"));
    assert(serializedNtlm.contains("username: jdoe"));
    assert(serializedNtlm.contains("password: corporatePass!"));
    assert(serializedNtlm.contains("domain: CORP_DOMAIN"));
    assert(serializedNtlm.contains("workstation: WORKSTATION_01"));

    RequestModel roundtripNtlm = BruParser::parse(serializedNtlm);
    assert(roundtripNtlm.auth.type == AuthType::NTLM);
    assert(roundtripNtlm.auth.ntlmUsername == "jdoe");
    assert(roundtripNtlm.auth.ntlmPassword == "corporatePass!");
    assert(roundtripNtlm.auth.ntlmDomain == "CORP_DOMAIN");
    assert(roundtripNtlm.auth.ntlmWorkstation == "WORKSTATION_01");

    std::cout << "test_auth_digest_ntlm passed successfully!" << std::endl;
    return 0;
}
