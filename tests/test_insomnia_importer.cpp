#include <iostream>
#include <cassert>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <core/BruParser.h>
#include <core/importers/InsomniaImporter.h>

using namespace poppy::core;

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    std::cout << "Running test_insomnia_importer..." << std::endl;

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        std::cerr << "Failed to create temp dir" << std::endl;
        return 1;
    }

    QString insomniaJsonPath = tempDir.path() + "/insomnia_sample.json";
    QString destDir = tempDir.path() + "/imported_collection";

    QString sampleContent = R"({
  "_type": "export",
  "__export_format": 4,
  "resources": [
    {
      "_id": "wrk_1",
      "_type": "workspace",
      "name": "Acme API"
    },
    {
      "_id": "fld_1",
      "_type": "request_group",
      "parentId": "wrk_1",
      "name": "Users"
    },
    {
      "_id": "req_1",
      "_type": "request",
      "parentId": "fld_1",
      "name": "List Users",
      "method": "GET",
      "url": "https://api.acme.com/v1/users",
      "headers": [
        {
          "name": "Accept",
          "value": "application/json"
        }
      ],
      "parameters": [
        {
          "name": "limit",
          "value": "10"
        }
      ],
      "authentication": {
        "type": "bearer",
        "token": "secret_token_123"
      }
    },
    {
      "_id": "req_2",
      "_type": "request",
      "parentId": "fld_1",
      "name": "Create User",
      "method": "POST",
      "url": "https://api.acme.com/v1/users",
      "body": {
        "mimeType": "application/json",
        "text": "{\"name\": \"Alice\"}"
      }
    }
  ]
})";

    QFile file(insomniaJsonPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        std::cerr << "Failed to write sample export: " << file.errorString().toStdString()
                  << " path=" << insomniaJsonPath.toStdString() << std::endl;
        return 1;
    }
    file.write(sampleContent.toUtf8());
    file.close();

    QString error;
    bool success = InsomniaImporter::importCollection(insomniaJsonPath, destDir, &error);
    if (!success) {
        std::cerr << "Import failed: " << error.toStdString() << std::endl;
        return 1;
    }

    // Verify manifest
    assert(QFile::exists(destDir + "/poppy.json"));

    // Verify imported .bru files in subfolder
    QString listUsersBru = destDir + "/Users/List Users.bru";
    QString createUserBru = destDir + "/Users/Create User.bru";

    assert(QFile::exists(listUsersBru));
    assert(QFile::exists(createUserBru));

    // Parse List Users .bru
    QFile fList(listUsersBru);
    assert(fList.open(QIODevice::ReadOnly | QIODevice::Text));
    QString listContent = QString::fromUtf8(fList.readAll());
    fList.close();

    RequestModel reqList = BruParser::parse(listContent);
    assert(reqList.name == "List Users");
    assert(reqList.method == HttpMethod::GET);
    assert(reqList.url == "https://api.acme.com/v1/users");
    assert(reqList.auth.type == AuthType::Bearer);
    assert(reqList.auth.bearerToken == "secret_token_123");

    // Parse Create User .bru
    QFile fCreate(createUserBru);
    assert(fCreate.open(QIODevice::ReadOnly | QIODevice::Text));
    QString createContent = QString::fromUtf8(fCreate.readAll());
    fCreate.close();

    RequestModel reqCreate = BruParser::parse(createContent);
    assert(reqCreate.name == "Create User");
    assert(reqCreate.method == HttpMethod::POST);
    assert(reqCreate.bodyType == BodyType::Json);
    assert(reqCreate.bodyContent.contains("\"name\": \"Alice\""));

    std::cout << "test_insomnia_importer passed successfully!" << std::endl;
    return 0;
}
