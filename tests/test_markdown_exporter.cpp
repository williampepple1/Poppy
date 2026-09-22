#include <iostream>
#include <cassert>
#include <QTemporaryDir>
#include <QFile>
#include <core/exporters/MarkdownExporter.h>

using namespace poppy::core;

int main() {
    std::cout << "Running test_markdown_exporter..." << std::endl;

    QList<RequestModel> requests;

    RequestModel req1;
    req1.name = "List Users";
    req1.method = HttpMethod::GET;
    req1.url = "https://api.example.com/v1/users";
    req1.queryParams.append(HttpParam{"limit", "20", true, "Max results"});
    req1.headers.append(HttpHeader{"Accept", "application/json", true, "JSON only"});
    req1.auth.type = AuthType::Bearer;
    req1.auth.bearerToken = "secret-token-12345";
    requests.append(req1);

    RequestModel req2;
    req2.name = "Create Product";
    req2.method = HttpMethod::POST;
    req2.url = "https://api.example.com/v1/products";
    req2.bodyType = BodyType::Json;
    req2.bodyContent = "{\"title\": \"Coffee Mug\", \"price\": 12.99}";
    req2.assertions.append(AssertionRule{"res.status", "eq", "201", true});
    requests.append(req2);

    // 1. Test in-memory Markdown generation
    QString md = MarkdownExporter::exportToMarkdown(requests, "Acme Store API");

    assert(!md.isEmpty());
    assert(md.contains("# Acme Store API — API Runbook"));
    assert(md.contains("Total Endpoints: `2`"));
    assert(md.contains("## 📋 Table of Contents"));
    assert(md.contains("List Users"));
    assert(md.contains("Create Product"));
    assert(md.contains("https://api.example.com/v1/users"));
    assert(md.contains("https://api.example.com/v1/products"));
    assert(md.contains("`limit`"));
    assert(md.contains("`Accept`"));
    assert(md.contains("Bearer"));
    assert(md.contains("••••••••")); // masked secret
    assert(md.contains("```json"));
    assert(md.contains("Coffee Mug"));
    assert(md.contains("Example cURL Command"));
    assert(md.contains("Declarative Assertions"));
    assert(md.contains("`eq`"));

    // 2. Test file export
    QTemporaryDir tempDir;
    assert(tempDir.isValid());
    QString filePath = tempDir.filePath("API_RUNBOOK.md");

    QString err;
    bool success = MarkdownExporter::exportToFile(filePath, requests, "Acme Store API", &err);
    assert(success);
    assert(err.isEmpty());

    QFile file(filePath);
    assert(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QString fileContent = QString::fromUtf8(file.readAll());
    assert(fileContent == md);
    file.close();

    std::cout << "test_markdown_exporter passed successfully!" << std::endl;
    return 0;
}
