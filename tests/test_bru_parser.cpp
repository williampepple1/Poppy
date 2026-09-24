#include <iostream>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <core/BruParser.h>
#include <core/BruWriter.h>
#include <core/CollectionModel.h>
#include <core/EnvironmentModel.h>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QCoreApplication>

using namespace poppy::core;

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
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

    QTemporaryDir collDir;
    assert(collDir.isValid());
    QFile collBru(collDir.filePath("collection.bru"));
    assert(collBru.open(QIODevice::WriteOnly | QIODevice::Text));
    collBru.write("meta {\n  name: Vars Collection\n}\n\nvars {\n  baseUrl: https://api.example.com\n}\n");
    collBru.close();
    CollectionModel coll;
    assert(coll.openDirectory(collDir.path()));
    assert(coll.rootItem());
    assert(coll.rootItem()->variables().value("baseUrl") == "https://api.example.com");

    QTemporaryDir binDir;
    assert(binDir.isValid());
    const QString payloadPath = binDir.filePath("payload.bin");
    QFile payload(payloadPath);
    assert(payload.open(QIODevice::WriteOnly));
    const QByteArray payloadBytes = QByteArray("abc\0def", 7);
    assert(payload.write(payloadBytes) == payloadBytes.size());
    payload.close();
    const QString payloadForBru = QDir::fromNativeSeparators(payloadPath);
    const QString binaryBru = QStringLiteral(
        "meta {\n"
        "  name: Upload\n"
        "}\n\n"
        "post {\n"
        "  url: https://example.com/upload\n"
        "  body: binary\n"
        "  auth: none\n"
        "}\n\n"
        "body:binary {\n"
        "  file: %1\n"
        "}\n").arg(payloadForBru);
    RequestModel binaryReq = BruParser::parse(binaryBru);
    assert(binaryReq.bodyType == BodyType::Binary);
    assert(QDir::fromNativeSeparators(binaryReq.bodyContent) == payloadForBru);
    assert(binaryReq.effectiveBody() == payloadBytes);
    RequestModel binaryRoundTrip = BruParser::parse(BruWriter::serialize(binaryReq));
    assert(binaryRoundTrip.bodyType == BodyType::Binary);
    assert(binaryRoundTrip.bodyContent == payloadForBru);

    QTemporaryDir authDir;
    assert(authDir.isValid());
    QDir(authDir.path()).mkpath("folder");
    QFile authCollection(authDir.filePath("collection.bru"));
    assert(authCollection.open(QIODevice::WriteOnly | QIODevice::Text));
    authCollection.write(
        "meta {\n  name: Auth Collection\n}\n\n"
        "auth {\n  mode: bearer\n}\n\n"
        "auth:bearer {\n  token: collection-token\n}\n");
    authCollection.close();
    QFile folderBru(authDir.filePath("folder/folder.bru"));
    assert(folderBru.open(QIODevice::WriteOnly | QIODevice::Text));
    folderBru.write(
        "meta {\n  name: folder\n  seq: 1\n}\n\n"
        "auth {\n  mode: basic\n}\n\n"
        "auth:basic {\n  username: ada\n  password: secret\n}\n");
    folderBru.close();
    QFile childBru(authDir.filePath("folder/call.bru"));
    assert(childBru.open(QIODevice::WriteOnly | QIODevice::Text));
    childBru.write(
        "meta {\n  name: Call\n  seq: 1\n}\n\n"
        "get {\n  url: https://example.com\n  body: none\n  auth: inherit\n}\n");
    childBru.close();
    QFile rootBru(authDir.filePath("root-call.bru"));
    assert(rootBru.open(QIODevice::WriteOnly | QIODevice::Text));
    rootBru.write(
        "meta {\n  name: Root Call\n  seq: 2\n}\n\n"
        "get {\n  url: https://example.com/root\n  body: none\n  auth: inherit\n}\n");
    rootBru.close();

    CollectionModel authModel;
    assert(authModel.openDirectory(authDir.path()));
    assert(authModel.rootItem()->auth().type == AuthType::Bearer);
    assert(authModel.rootItem()->auth().bearerToken == "collection-token");
    CollectionItem* child = nullptr;
    CollectionItem* rootCall = nullptr;
    for (auto* item : authModel.allRequestItems()) {
        if (item->name() == "Call") child = item;
        if (item->name() == "Root Call") rootCall = item;
    }
    assert(child && child->request() && child->request()->auth.type == AuthType::Inherit);
    RequestModel childExec = child->requestForExecution();
    assert(childExec.auth.type == AuthType::Basic);
    assert(childExec.auth.basicUsername == "ada");
    assert(childExec.auth.basicPassword == "secret");
    assert(rootCall);
    RequestModel rootExec = rootCall->requestForExecution();
    assert(rootExec.auth.type == AuthType::Bearer);
    assert(rootExec.auth.bearerToken == "collection-token");

    QString braceSample = R"(meta {
  name: Brace
  seq: 1
}

post {
  url: https://example.com
  body: json
  auth: none
}

body:json {
{
  "note": "}"
}
}

tests {
  test("msg", function() {
    const s = "}";
    expect(s).to.equal("}");
  });
}
)";
    RequestModel braceReq = BruParser::parse(braceSample);
    assert(braceReq.bodyContent.contains("\"note\""));
    assert(braceReq.bodyContent.contains("\"}\""));
    assert(braceReq.scripts.tests.contains("to.equal(\"}\")"));
    assert(braceReq.scripts.tests.contains("function()"));

    RequestModel described;
    described.name = "Described";
    described.method = HttpMethod::GET;
    described.url = "https://example.com/items";
    described.queryParams.append(HttpParam{.key = "q", .value = "pet", .enabled = true, .description = "Search term"});
    described.headers.append(HttpHeader{.name = "X-Note", .value = "a", .enabled = true, .description = "A note"});
    QString describedBru = BruWriter::serialize(described);
    RequestModel describedRound = BruParser::parse(describedBru);
    assert(describedRound.queryParams.size() == 1);
    assert(describedRound.queryParams[0].description == "Search term");
    assert(describedRound.headers.size() == 1);
    assert(describedRound.headers[0].description == "A note");

    EnvironmentModel disabledEnv("dev");
    disabledEnv.addOrUpdateVariable("visible", "yes", false, true);
    disabledEnv.addOrUpdateVariable("hidden", "no", false, false);
    QTemporaryDir envDir;
    assert(envDir.isValid());
    QString envPath = envDir.filePath("dev.env");
    assert(disabledEnv.saveToEnvFile(envPath));
    EnvironmentModel loadedEnv = EnvironmentModel::loadFromEnvFile(envPath, "dev");
    assert(loadedEnv.hasVariable("visible"));
    assert(loadedEnv.hasVariable("hidden"));
    bool hiddenEnabled = true;
    for (const auto& v : loadedEnv.variables()) {
        if (v.name == "hidden") hiddenEnabled = v.enabled;
    }
    assert(hiddenEnabled == false);

    QFile legacyEnv(envDir.filePath("legacy.env"));
    assert(legacyEnv.open(QIODevice::WriteOnly | QIODevice::Text));
    legacyEnv.write("# Poppy Environment: legacy\n# API_KEY=xyz\nHOST=example\n");
    legacyEnv.close();
    EnvironmentModel legacy = EnvironmentModel::loadFromEnvFile(legacyEnv.fileName(), "legacy");
    assert(legacy.variableValue("HOST") == "example");
    assert(legacy.hasVariable("API_KEY"));
    bool legacyEnabled = true;
    for (const auto& v : legacy.variables()) {
        if (v.name == "API_KEY") legacyEnabled = v.enabled;
    }
    assert(legacyEnabled == false);

    std::cout << "test_bru_parser PASSED!" << std::endl;
    return 0;
}
