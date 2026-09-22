#include <iostream>
#include <cassert>
#include <QFile>
#include <QTemporaryFile>
#include <core/RequestModel.h>
#include <core/VariableResolver.h>
#include <core/DocGenerator.h>

using namespace poppy::core;

int main() {
    std::cout << "Running test_doc_generator..." << std::endl;

    QList<RequestModel> requests;

    RequestModel req1;
    req1.name = "List Users";
    req1.description = "Retrieves a paginated list of system users.";
    req1.method = HttpMethod::GET;
    req1.url = "{{baseUrl}}/users?page=1&limit=20";
    req1.headers.append(HttpHeader{"Accept", "application/json", true});
    req1.headers.append(HttpHeader{"X-Request-ID", "req-123", true});
    requests.append(req1);

    RequestModel req2;
    req2.name = "Create User";
    req2.description = "Creates a new user in the organization.";
    req2.method = HttpMethod::POST;
    req2.url = "{{baseUrl}}/users";
    req2.headers.append(HttpHeader{"Content-Type", "application/json", true});
    req2.bodyType = BodyType::Json;
    req2.bodyContent = "{\"name\": \"Alice\", \"email\": \"alice@example.com\"}";
    requests.append(req2);

    VariableResolver resolver;
    EnvironmentModel env("Production");
    env.addOrUpdateVariable("baseUrl", "https://api.poppy.dev/v1");
    resolver.setEnvironment(env);

    // 1. Generate HTML string
    QString html = DocGenerator::generateHtml(requests, "User Management API", "Official User Service API Docs", resolver);

    assert(!html.isEmpty());
    assert(html.contains("<!DOCTYPE html>"));
    assert(html.contains("User Management API"));
    assert(html.contains("Official User Service API Docs"));
    assert(html.contains("List Users"));
    assert(html.contains("Create User"));
    assert(html.contains("https://api.poppy.dev/v1/users?page=1&limit=20"));
    assert(html.contains("https://api.poppy.dev/v1/users"));
    assert(html.contains("curl -X GET"));
    assert(html.contains("curl -X POST"));
    assert(html.contains("fetch("));
    assert(html.contains("requests.get") || html.contains("requests.post"));
    assert(html.contains("X-Request-ID"));
    assert(html.contains("req-123"));

    // 2. Generate to file
    QTemporaryFile tempFile;
    assert(tempFile.open());
    QString tempPath = tempFile.fileName();
    tempFile.close();

    QString error;
    bool success = DocGenerator::generateHtmlFile(tempPath, requests, "User Management API", "Official User Service API Docs", resolver, &error);
    assert(success);
    assert(error.isEmpty());

    QFile file(tempPath);
    assert(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QString fileContent = QString::fromUtf8(file.readAll());
    file.close();
    assert(fileContent.size() == html.size());
    assert(fileContent.contains("User Management API"));

    std::cout << "test_doc_generator passed successfully!" << std::endl;
    return 0;
}
