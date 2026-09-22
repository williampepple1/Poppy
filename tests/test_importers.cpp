#include <iostream>
#include <cassert>
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <core/importers/CurlImporter.h>
#include <core/importers/PostmanImporter.h>
#include <core/importers/OpenApiImporter.h>
#include <core/BruParser.h>
#include <core/BruWriter.h>
#include <core/CollectionModel.h>

using namespace poppy::core;

static int countRequests(const CollectionItem* item) {
    if (!item) return 0;
    int n = (item->type() == CollectionItemType::Request) ? 1 : 0;
    for (const auto* child : item->children()) {
        n += countRequests(child);
    }
    return n;
}

static int countFolders(const CollectionItem* item) {
    if (!item) return 0;
    int n = (item->type() == CollectionItemType::Folder) ? 1 : 0;
    for (const auto* child : item->children()) {
        n += countFolders(child);
    }
    return n;
}

static const CollectionItem* findByName(const CollectionItem* item, const QString& name) {
    if (!item) return nullptr;
    if (item->name() == name) return item;
    for (const auto* child : item->children()) {
        if (const auto* found = findByName(child, name)) return found;
    }
    return nullptr;
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    std::cout << "Running test_importers..." << std::endl;

    assert(BruWriter::safeFileStem("GET /users/:id") == "GET _users__id");
    assert(BruWriter::safeFileStem("Step 1: Login") == "Step 1_ Login");
    assert(BruWriter::safeFileStem("   ") == "item");

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
    QString pmCollDir;
    bool pmSuccess = PostmanImporter::importCollection(postmanFile, tempPostmanDir, &pmErr, &pmCollDir);
    assert(pmSuccess);
    assert(pmErr.isEmpty());
    assert(pmCollDir.endsWith("Postman Sample Collection"));

    QFile pmBru(pmCollDir + "/Health Check.bru");
    assert(pmBru.exists());
    RequestModel pmReq = BruParser::parseFile(pmBru.fileName());
    assert(pmReq.name == "Health Check");
    assert(pmReq.method == HttpMethod::GET);
    assert(pmReq.url == "https://api.sample.com/health");
    assert(pmReq.headers.size() == 1);
    assert(pmReq.headers[0].name == "Accept" && pmReq.headers[0].value == "application/json");

    QDir(tempPostmanDir).removeRecursively();

    // 2b. Folders + requests with Windows-invalid names and empty `item` arrays
    QString nestedPostmanJson = R"({
      "info": { "name": "Nested: API" },
      "item": [
        {
          "name": "Users / Auth",
          "item": [
            {
              "name": "GET /users/:id",
              "item": [],
              "request": {
                "method": "GET",
                "url": "https://api.sample.com/users/1"
              }
            },
            {
              "name": "Create User",
              "request": {
                "method": "POST",
                "url": "https://api.sample.com/users"
              }
            }
          ]
        }
      ]
    })";

    QString tempNestedDir = QDir::tempPath() + "/poppy_test_postman_nested";
    QDir(tempNestedDir).removeRecursively();
    QDir().mkpath(tempNestedDir);

    QString nestedFile = tempNestedDir + "/collection.json";
    QFile nf(nestedFile);
    assert(nf.open(QIODevice::WriteOnly | QIODevice::Text));
    nf.write(nestedPostmanJson.toUtf8());
    nf.close();

    QString nestedErr;
    QString nestedCollDir;
    assert(PostmanImporter::importCollection(nestedFile, tempNestedDir, &nestedErr, &nestedCollDir));
    assert(nestedErr.isEmpty());

    CollectionModel model;
    assert(model.openDirectory(nestedCollDir));
    assert(countFolders(model.rootItem()) == 1);
    assert(countRequests(model.rootItem()) == 2);
    assert(findByName(model.rootItem(), "GET /users/:id") != nullptr);
    assert(findByName(model.rootItem(), "Create User") != nullptr);
    const auto* folder = findByName(model.rootItem(), "Users _ Auth");
    assert(folder != nullptr);
    assert(folder->type() == CollectionItemType::Folder);
    assert(folder->children().size() == 2);

    QDir(tempNestedDir).removeRecursively();

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
    QString oasCollDir;
    bool oasSuccess = OpenApiImporter::importSpec(openApiFile, tempOpenApiDir, &oasErr, &oasCollDir);
    assert(oasSuccess);
    assert(oasErr.isEmpty());

    QFile oasBru(oasCollDir + "/Get Pet By Id.bru");
    assert(oasBru.exists());
    RequestModel oasReq = BruParser::parseFile(oasBru.fileName());
    assert(oasReq.name == "Get Pet By Id");
    assert(oasReq.method == HttpMethod::GET);
    assert(oasReq.url == "{{baseUrl}}/pets/:petId");
    assert(oasReq.pathParams.size() == 1);
    assert(oasReq.pathParams[0].key == "petId");

    CollectionModel oasModel;
    assert(oasModel.openDirectory(oasCollDir));
    assert(countRequests(oasModel.rootItem()) == 1);

    QDir(tempOpenApiDir).removeRecursively();

    std::cout << "test_importers PASSED!" << std::endl;
    return 0;
}
