#include <iostream>
#include <cassert>
#include <core/BruParser.h>
#include <core/BruWriter.h>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

using namespace poppy::core;

int main() {
    std::cout << "Running test_bru_parser..." << std::endl;

    QString bruSample = R"(meta {
  name: Get User Profile
  type: http
  seq: 1
}

get {
  url: {{baseUrl}}/api/users/:id
  body: json
  auth: bearer
}

params:query {
  active: true
  ~debug: 1
}

params:path {
  id: 123
}

headers {
  Accept: application/json
  ~X-Test: disabled
}

auth:bearer {
  token: {{authToken}}
}

body:json {
  {
    "username": "johndoe",
    "role": "admin"
  }
}

tests {
  test("Status is 200", function() {
    expect(res.getStatus()).to.equal(200);
  });
}
)";

    RequestModel req = BruParser::parse(bruSample);

    // Assertions
    assert(req.name == "Get User Profile");
    assert(req.seq == 1);
    assert(req.method == HttpMethod::GET);
    assert(req.url == "{{baseUrl}}/api/users/:id");
    assert(req.auth.type == AuthType::Bearer);
    assert(req.auth.bearerToken == "{{authToken}}");
    assert(req.bodyType == BodyType::Json);
    assert(req.bodyContent.contains("\"username\": \"johndoe\""));

    assert(req.queryParams.size() == 2);
    assert(req.queryParams[0].key == "active" && req.queryParams[0].value == "true" && req.queryParams[0].enabled == true);
    assert(req.queryParams[1].key == "debug" && req.queryParams[1].value == "1" && req.queryParams[1].enabled == false);

    assert(req.pathParams.size() == 1);
    assert(req.pathParams[0].key == "id" && req.pathParams[0].value == "123");

    assert(req.headers.size() == 2);
    assert(req.headers[0].name == "Accept" && req.headers[0].value == "application/json");
    assert(req.headers[1].name == "X-Test" && req.headers[1].enabled == false);

    assert(req.scripts.tests.contains("expect(res.getStatus()).to.equal(200);"));

    // Serialize back and parse again
    QString serialized = BruWriter::serialize(req);
    RequestModel roundTrip = BruParser::parse(serialized);

    assert(roundTrip.name == req.name);
    assert(roundTrip.method == req.method);
    assert(roundTrip.url == req.url);
    assert(roundTrip.auth.type == req.auth.type);
    assert(roundTrip.auth.bearerToken == req.auth.bearerToken);
    assert(roundTrip.bodyType == req.bodyType);
    assert(roundTrip.queryParams.size() == req.queryParams.size());
    assert(roundTrip.headers.size() == req.headers.size());

    RequestModel multipart;
    multipart.name = "Upload";
    multipart.method = HttpMethod::POST;
    multipart.url = "https://example.com/upload";
    multipart.bodyType = BodyType::MultipartForm;
    multipart.formDataParams.append(FormDataParam{.key = "title", .value = "hello", .isFile = false, .enabled = true});
    multipart.formDataParams.append(FormDataParam{.key = "file", .value = "/tmp/a.txt", .isFile = true, .enabled = true});
    QString mpSerialized = BruWriter::serialize(multipart);
    RequestModel mpRoundTrip = BruParser::parse(mpSerialized);
    assert(mpRoundTrip.bodyType == BodyType::MultipartForm);
    assert(mpRoundTrip.formDataParams.size() == 2);
    assert(mpRoundTrip.formDataParams[0].key == "title" && mpRoundTrip.formDataParams[0].value == "hello");
    assert(mpRoundTrip.formDataParams[1].isFile && mpRoundTrip.formDataParams[1].value == "/tmp/a.txt");

    RequestModel proxied;
    proxied.name = "Proxied";
    proxied.method = HttpMethod::GET;
    proxied.url = "https://example.com";
    proxied.proxy = "http://127.0.0.1:8080";
    RequestModel proxyRoundTrip = BruParser::parse(BruWriter::serialize(proxied));
    assert(proxyRoundTrip.proxy == "http://127.0.0.1:8080");

    QTemporaryDir tmp;
    assert(tmp.isValid());
    QFile existing(tmp.filePath("Get_User.bru"));
    assert(existing.open(QIODevice::WriteOnly));
    existing.write("x");
    existing.close();
    QString reused = BruWriter::uniqueFilePath(tmp.path(), "Get_User", ".bru", existing.fileName());
    assert(QDir::cleanPath(reused) == QDir::cleanPath(existing.fileName()));
    QString other = BruWriter::uniqueFilePath(tmp.path(), "Get_User", ".bru");
    assert(QDir::cleanPath(other) != QDir::cleanPath(existing.fileName()));

    std::cout << "test_bru_parser PASSED!" << std::endl;
    return 0;
}
