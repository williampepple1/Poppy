#include <iostream>
#include <cassert>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <core/HistoryManager.h>

using namespace poppy::core;

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    std::cout << "Running test_history_manager..." << std::endl;

    HistoryManager mgr;
    mgr.setAutoSave(false);
    mgr.setMaxEntries(3);

    RequestModel req1;
    req1.name = "Get Users";
    req1.method = HttpMethod::GET;
    req1.url = "https://api.example.com/users";

    ResponseModel res1;
    res1.statusCode = 200;
    res1.statusText = "OK";
    res1.latencyMs = 45;
    res1.rawBody = "{\"users\": [\"alice\", \"bob\"]}";

    mgr.addEntry(req1, res1);
    assert(mgr.count() == 1);
    assert(mgr.items()[0].statusCode == 200);
    assert(mgr.items()[0].request.url == "https://api.example.com/users");
    assert(mgr.items()[0].responseRawBody.contains("alice"));

    RequestModel req2;
    req2.name = "Create User";
    req2.method = HttpMethod::POST;
    req2.url = "https://api.example.com/users";
    req2.bodyType = BodyType::Json;
    req2.bodyContent = "{\"name\": \"charlie\"}";

    ResponseModel res2;
    res2.statusCode = 201;
    res2.statusText = "Created";
    res2.latencyMs = 120;
    res2.rawBody = "{\"id\": 3}";

    mgr.addEntry(req2, res2);
    assert(mgr.count() == 2);
    assert(mgr.items()[0].request.method == HttpMethod::POST); // Most recent is at index 0

    // Test filter
    auto filteredPost = mgr.filter("POST");
    assert(filteredPost.size() == 1);
    assert(filteredPost[0].statusCode == 201);

    auto filteredUsers = mgr.filter("users");
    assert(filteredUsers.size() == 2);

    auto filtered200 = mgr.filter("200");
    assert(filtered200.size() == 1);
    assert(filtered200[0].statusCode == 200);

    // Test maxEntries pruning
    RequestModel req3;
    req3.name = "Delete User";
    req3.method = HttpMethod::DELETE;
    req3.url = "https://api.example.com/users/3";
    ResponseModel res3;
    res3.statusCode = 204;
    mgr.addEntry(req3, res3);
    assert(mgr.count() == 3);

    RequestModel req4;
    req4.name = "Not Found";
    req4.method = HttpMethod::GET;
    req4.url = "https://api.example.com/unknown";
    ResponseModel res4;
    res4.statusCode = 404;
    mgr.addEntry(req4, res4);
    assert(mgr.count() == 3); // Capped at 3, req1 dropped
    assert(mgr.items()[0].statusCode == 404);

    // Test persistence roundtrip
    QString tmpFile = QDir::tempPath() + "/poppy_test_history.json";
    QFile::remove(tmpFile);

    bool saveOk = mgr.saveToFile(tmpFile);
    if (!saveOk) {
        std::cerr << "FAILED: mgr.saveToFile failed!" << std::endl;
        return 1;
    }

    HistoryManager loadedMgr;
    loadedMgr.setAutoSave(false);
    bool loadOk = loadedMgr.loadFromFile(tmpFile);
    if (!loadOk) {
        std::cerr << "FAILED: loadedMgr.loadFromFile failed!" << std::endl;
        return 1;
    }
    if (loadedMgr.count() != 3) {
        std::cerr << "FAILED: loaded count expected 3, got " << loadedMgr.count() << std::endl;
        return 1;
    }
    if (loadedMgr.items()[0].statusCode != 404) {
        std::cerr << "FAILED: expected status 404, got " << loadedMgr.items()[0].statusCode << std::endl;
        return 1;
    }

    // Test toResponseModel conversion
    ResponseModel convertedRes = loadedMgr.items()[0].toResponseModel();
    if (convertedRes.statusCode != 404) {
        std::cerr << "FAILED: convertedRes expected 404, got " << convertedRes.statusCode << std::endl;
        return 1;
    }

    QFile::remove(tmpFile);

    RequestModel secretReq;
    secretReq.name = "Login";
    secretReq.url = "https://api.example.com/login";
    secretReq.auth.type = AuthType::Bearer;
    secretReq.auth.bearerToken = "super-secret-token";
    secretReq.auth.basicPassword = "hunter2";
    secretReq.auth.awsSecretKey = "aws-secret";
    ResponseModel secretRes;
    secretRes.statusCode = 200;
    HistoryManager secretMgr;
    secretMgr.setAutoSave(false);
    secretMgr.addEntry(secretReq, secretRes);
    QString secretFile = QDir::tempPath() + "/poppy_test_history_secrets.json";
    QFile::remove(secretFile);
    assert(secretMgr.saveToFile(secretFile));
    QFile disk(secretFile);
    assert(disk.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString saved = QString::fromUtf8(disk.readAll());
    disk.close();
    assert(!saved.contains("super-secret-token"));
    assert(!saved.contains("hunter2"));
    assert(!saved.contains("aws-secret"));
    assert(saved.contains("***"));
    HistoryManager reloaded;
    reloaded.setAutoSave(false);
    assert(reloaded.loadFromFile(secretFile));
    assert(reloaded.items()[0].request.auth.bearerToken.isEmpty());
    assert(reloaded.items()[0].request.auth.basicPassword.isEmpty());
    assert(reloaded.items()[0].request.auth.awsSecretKey.isEmpty());
    QFile::remove(secretFile);

    std::cout << "test_history_manager PASSED!" << std::endl;
    return 0;
}
