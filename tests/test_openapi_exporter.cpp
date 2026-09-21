#include <iostream>
#include <cassert>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <core/exporters/OpenApiExporter.h>

using namespace poppy::core;

int main() {
    std::cout << "Running test_openapi_exporter..." << std::endl;

    QList<RequestModel> requests;

    RequestModel req1;
    req1.name = "Get User by ID";
    req1.method = HttpMethod::GET;
    req1.url = "https://api.example.com/v1/users/:id";
    req1.pathParams.append(HttpParam{"id", "42", true, "User Identifier"});
    req1.queryParams.append(HttpParam{"details", "true", true, "Include details"});
    requests.append(req1);

    RequestModel req2;
    req2.name = "Create User";
    req2.method = HttpMethod::POST;
    req2.url = "https://api.example.com/v1/users";
    req2.bodyType = BodyType::Json;
    req2.bodyContent = "{\"username\": \"charlie\", \"email\": \"charlie@example.com\"}";
    requests.append(req2);

    QJsonObject openApi = OpenApiExporter::exportToJson(requests, "User Service API", "1.2.0");

    assert(openApi.value("openapi").toString() == "3.0.3");
    QJsonObject info = openApi.value("info").toObject();
    assert(info.value("title").toString() == "User Service API");
    assert(info.value("version").toString() == "1.2.0");

    QJsonObject paths = openApi.value("paths").toObject();
    assert(paths.contains("/v1/users/{id}"));
    assert(paths.contains("/v1/users"));

    // Verify GET /v1/users/{id}
    QJsonObject getOp = paths.value("/v1/users/{id}").toObject().value("get").toObject();
    assert(getOp.value("summary").toString() == "Get User by ID");
    QJsonArray params = getOp.value("parameters").toArray();
    assert(params.size() == 2);

    bool hasPathParam = false;
    bool hasQueryParam = false;
    for (const auto& pVal : params) {
        QJsonObject p = pVal.toObject();
        if (p.value("name").toString() == "id" && p.value("in").toString() == "path") {
            hasPathParam = true;
            assert(p.value("required").toBool() == true);
        } else if (p.value("name").toString() == "details" && p.value("in").toString() == "query") {
            hasQueryParam = true;
        }
    }
    assert(hasPathParam);
    assert(hasQueryParam);

    // Verify POST /v1/users
    QJsonObject postOp = paths.value("/v1/users").toObject().value("post").toObject();
    assert(postOp.value("summary").toString() == "Create User");
    assert(postOp.contains("requestBody"));
    QJsonObject rbContent = postOp.value("requestBody").toObject().value("content").toObject();
    assert(rbContent.contains("application/json"));

    // Verify exportToFile
    QTemporaryDir tempDir;
    assert(tempDir.isValid());
    QString outPath = tempDir.path() + "/openapi_out.json";
    QString error;
    bool ok = OpenApiExporter::exportToFile(outPath, requests, "User Service API", "1.2.0", &error);
    assert(ok);
    assert(QFile::exists(outPath));

    std::cout << "test_openapi_exporter passed successfully!" << std::endl;
    return 0;
}
