#include <iostream>
#include <cassert>
#include <QFile>
#include <QDir>
#include <core/importers/CurlImporter.h>
#include <core/importers/PostmanImporter.h>
#include <core/importers/OpenApiImporter.h>
#include <core/BruParser.h>

using namespace poppy::core;

int main() {
    std::cout << "Running test_importers..." << std::endl;

    // 1. CurlImporter
    QString curlCommand = "curl -X POST https://api.example.com/items "
                          "-H \"Authorization: Bearer secret-tok\" "
                          "-H \"Content-Type: application/json\" "
                          "-d '{\"name\": \"keyboard\", \"price\": 49.99}'";

    RequestModel req = CurlImporter::importCurl(curlCommand);
    assert(req.method == HttpMethod::POST);
    assert(req.url == "https://api.example.com/items");
    assert(req.auth.type == AuthType::Bearer);
    assert(req.auth.bearerToken == "secret-tok");
    assert(req.bodyType == BodyType::Json);
    assert(req.bodyContent.contains("\"name\": \"keyboard\""));

    // 2. PostmanImporter
    QString postmanJson = R"({
      "info": {
        "name": "Postman Sample Collection",
        "schema": "https://schema.getpostman.com/json/collection/v2.1.0/collection.json"
      },
      "item": [
        {
          "name": "Health Check",
          "request": {
            "method": "GET",
            "header": [
              {
                "key": "Accept",
                "value": "application/json"
              }
            ],
            "url": {
              "raw": "https://api.sample.com/health"
            }
          }
        }
      ]
    })";

    QString tempPostmanDir = QDir::tempPath() + "/poppy_test_postman_import";
    QDir(tempPostmanDir).removeRecursively();
    QDir().mkpath(tempPostmanDir);

    QString postmanFile = tempPostmanDir + "/collection.json";
    QFile pf(postmanFile);
    assert(pf.open(QIODevice::WriteOnly | QIODevice::Text));
    pf.write(postmanJson.toUtf8());
    pf.close();

    QString pmErr;
    bool pmSuccess = PostmanImporter::importCollection(postmanFile, tempPostmanDir, &pmErr);
    assert(pmSuccess);
    assert(pmErr.isEmpty());

    QFile pmBru(tempPostmanDir + "/Health Check.bru");
    assert(pmBru.exists());
    RequestModel pmReq = BruParser::parseFile(pmBru.fileName());
    assert(pmReq.name == "Health Check");
    assert(pmReq.method == HttpMethod::GET);
    assert(pmReq.url == "https://api.sample.com/health");
    assert(pmReq.headers.size() == 1);
    assert(pmReq.headers[0].name == "Accept" && pmReq.headers[0].value == "application/json");

    // Clean up
    QDir(tempPostmanDir).removeRecursively();

    // 3. OpenApiImporter
    QString openApiJson = R"({
      "openapi": "3.0.0",
      "info": {
        "title": "Swagger Petstore Sample",
        "version": "1.0.0"
      },
      "servers": [
        {
          "url": "https://petstore.example.com/v2"
        }
      ],
      "paths": {
        "/pets/{petId}": {
          "get": {
            "summary": "Get Pet By Id",
            "responses": {
              "200": {
                "description": "Pet found"
              }
            }
          }
        }
      }
    })";

    QString tempOpenApiDir = QDir::tempPath() + "/poppy_test_openapi_import";
    QDir(tempOpenApiDir).removeRecursively();
    QDir().mkpath(tempOpenApiDir);

    QString openApiFile = tempOpenApiDir + "/openapi.json";
    QFile oaf(openApiFile);
    assert(oaf.open(QIODevice::WriteOnly | QIODevice::Text));
    oaf.write(openApiJson.toUtf8());
    oaf.close();

    QString oasErr;
    bool oasSuccess = OpenApiImporter::importSpec(openApiFile, tempOpenApiDir, &oasErr);
    assert(oasSuccess);
    assert(oasErr.isEmpty());

    QFile oasBru(tempOpenApiDir + "/Get Pet By Id.bru");
    assert(oasBru.exists());
    RequestModel oasReq = BruParser::parseFile(oasBru.fileName());
    assert(oasReq.name == "Get Pet By Id");
    assert(oasReq.method == HttpMethod::GET);
    assert(oasReq.url == "https://petstore.example.com/v2/pets/:petId");
    assert(oasReq.pathParams.size() == 1);
    assert(oasReq.pathParams[0].key == "petId");

    // Clean up
    QDir(tempOpenApiDir).removeRecursively();

    std::cout << "test_importers PASSED!" << std::endl;
    return 0;
}
