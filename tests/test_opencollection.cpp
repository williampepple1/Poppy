#include <cassert>
#include <iostream>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include "core/YamlNode.h"
#include "core/OpenCollectionParser.h"
#include "core/OpenCollectionWriter.h"
#include "core/CollectionModel.h"
#include "core/VariableResolver.h"

using namespace poppy::core;

void testYamlNodeParsing() {
    QString yaml = R"(
name: Test Node
seq: 42
enabled: true
list:
  - first
  - second
mapping:
  key1: val1
  nested:
    inner: 123
block: |-
  line 1
  line 2
)";

    YamlNode root = YamlNode::parse(yaml);
    assert(root.isMapping());
    assert(root["name"].asString() == "Test Node");
    assert(root["seq"].asInt() == 42);
    assert(root["enabled"].asBool() == true);

    assert(root["list"].isSequence());
    assert(root["list"].sequence.size() == 2);
    assert(root["list"][0].asString() == "first");
    assert(root["list"][1].asString() == "second");

    assert(root["mapping"]["key1"].asString() == "val1");
    assert(root["mapping"]["nested"]["inner"].asInt() == 123);

    QString block = root["block"].asString();
    assert(block.contains("line 1"));
    assert(block.contains("line 2"));

    std::cout << "[PASS] testYamlNodeParsing\n";
}

void testOpenCollectionRequestParsing() {
    QString yml = R"(
info:
  name: Create User
  type: http
  seq: 1

http:
  method: POST
  url: "{{bridgeBaseURL}}/users/v1/users"
  headers:
    - name: client-id
      value: kuidpro
  body:
    type: json
    data: |-
      {
        "name": "Sample User",
        "email": "test@example.com"
      }
  auth:
    type: bearer
    token: my-secret-token

runtime:
  scripts:
    - type: after-response
      code: |-
        bru.setEnvVar("token", res.getBody().token);
)";

    RequestModel req = OpenCollectionParser::parseRequestText(yml);
    assert(req.name == "Create User");
    assert(req.seq == 1);
    assert(req.method == HttpMethod::POST);
    assert(req.url == "{{bridgeBaseURL}}/users/v1/users");
    assert(req.headers.size() == 1);
    assert(req.headers[0].name == "client-id");
    assert(req.headers[0].value == "kuidpro");
    assert(req.bodyType == BodyType::Json);
    assert(req.bodyContent.contains("Sample User"));
    assert(req.auth.type == AuthType::Bearer);
    assert(req.auth.bearerToken == "my-secret-token");
    assert(req.scripts.postResponseScript.contains("bru.setEnvVar"));

    std::cout << "[PASS] testOpenCollectionRequestParsing\n";
}

void testOpenCollectionWriterRoundtrip() {
    RequestModel req;
    req.name = "Get Status";
    req.seq = 2;
    req.method = HttpMethod::GET;
    req.url = "https://api.example.com/status";
    req.queryParams.append(HttpParam{"verbose", "true", true, ""});
    req.headers.append(HttpHeader{"Accept", "application/json", true, ""});
    req.bodyType = BodyType::Text;
    req.bodyContent = "hello world";
    req.auth.type = AuthType::Bearer;
    req.auth.bearerToken = "token123";
    req.scripts.preRequestScript = "console.log('before');";

    QString serialized = OpenCollectionWriter::serializeRequest(req);
    RequestModel parsed = OpenCollectionParser::parseRequestText(serialized);

    assert(parsed.name == req.name);
    assert(parsed.seq == req.seq);
    assert(parsed.method == req.method);
    assert(parsed.url == req.url);
    assert(parsed.queryParams.size() == 1);
    assert(parsed.queryParams[0].key == "verbose");
    assert(parsed.queryParams[0].value == "true");
    assert(parsed.headers.size() == 1);
    assert(parsed.headers[0].name == "Accept");
    assert(parsed.bodyType == BodyType::Text);
    assert(parsed.bodyContent == req.bodyContent);
    assert(parsed.auth.type == AuthType::Bearer);
    assert(parsed.auth.bearerToken == "token123");
    assert(parsed.scripts.preRequestScript == req.scripts.preRequestScript);

    std::cout << "[PASS] testOpenCollectionWriterRoundtrip\n";
}

void testOpenCollectionEnvironmentParsing() {
    QString yml = R"(
name: Staging
variables:
  - name: baseURL
    value: https://staging-api.example.com
  - name: secretKey
    value: secret-val
    secret: true
)";

    EnvironmentModel env = OpenCollectionParser::parseEnvironmentText(yml);
    assert(env.name() == "Staging");
    assert(env.variables().size() == 2);
    assert(env.variableValue("baseURL") == "https://staging-api.example.com");
    assert(env.variableValue("secretKey") == "secret-val");
    assert(env.isSecretVariable("secretKey"));

    std::cout << "[PASS] testOpenCollectionEnvironmentParsing\n";
}

void testRuntimeVariablesAndPreservedSections() {
    QString yml = R"(
info:
  name: Fetch Countries
  type: http
  seq: 3

http:
  method: GET
  url: "{{ddt-base-url}}/public/v1/countries"
  auth: inherit

runtime:
  variables:
    - name: ddt-base-url
      value: https://staging-api.kuidpro.io
    - name: hidden
      value: no
      disabled: true

settings:
  encodeUrl: true
  timeout: 0

examples:
  - name: Fetch Countries
    request:
      url: "{{ddt-base-url}}/public/v1/countries"
      method: GET
)";

    RequestModel req = OpenCollectionParser::parseRequestText(yml);
    assert(req.runtimeVariables.size() == 2);
    assert(req.runtimeVariables[0].name == "ddt-base-url");
    assert(req.runtimeVariables[0].enabled);
    assert(!req.runtimeVariables[1].enabled);
    assert(req.preservedOpenCollectionYaml.contains("encodeUrl: true"));
    assert(req.preservedOpenCollectionYaml.contains("examples:"));

    VariableResolver resolver;
    RequestModel resolved = resolver.resolveRequest(req);
    assert(resolved.url == "https://staging-api.kuidpro.io/public/v1/countries");

    QString saved = OpenCollectionWriter::serializeRequest(req);
    assert(saved.contains("name: ddt-base-url"));
    assert(saved.contains("disabled: true"));
    assert(saved.contains("encodeUrl: true"));
    assert(saved.contains("name: Fetch Countries"));
    RequestModel again = OpenCollectionParser::parseRequestText(saved);
    assert(again.runtimeVariables.size() == 2);
    assert(again.preservedOpenCollectionYaml.contains("timeout: 0"));

    std::cout << "[PASS] testRuntimeVariablesAndPreservedSections\n";
}

void testFolderHeadersAreNotVariables() {
    QString yml = R"(
info:
  name: 6thbridge
  type: folder
  seq: 13

request:
  headers:
    - name: client-id
      value: kuidpro
  variables:
    - name: region
      value: eu
  auth: inherit
)";
    OpenCollectionFolderInfo info = OpenCollectionParser::parseFolder(YamlNode::parse(yml), "fallback");
    assert(info.headers.size() == 1);
    assert(info.headers[0].name == "client-id");
    assert(info.headers[0].value == "kuidpro");
    assert(info.vars.value("region") == "eu");
    assert(!info.vars.contains("client-id"));

    QString written = OpenCollectionWriter::serializeFolder(info.name, info.seq, info.auth, info.vars, info.headers);
    assert(written.contains("headers:"));
    assert(written.contains("variables:"));
    OpenCollectionFolderInfo again = OpenCollectionParser::parseFolder(YamlNode::parse(written));
    assert(again.headers.size() == 1);
    assert(again.headers[0].name == "client-id");
    assert(again.vars.value("region") == "eu");
    assert(!again.vars.contains("client-id"));

    QTemporaryDir dir;
    if (!dir.isValid()) {
        std::cerr << "temp dir failed\n";
        std::abort();
    }
    QDir root(dir.path());
    if (!root.mkpath("bridge")) {
        std::cerr << "mkdir failed\n";
        std::abort();
    }
    auto writeFile = [](const QString& path, const QByteArray& data) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            std::cerr << "open failed: " << file.errorString().toStdString() << "\n";
            std::abort();
        }
        if (file.write(data) != data.size()) {
            std::cerr << "write failed: " << file.errorString().toStdString() << "\n";
            std::abort();
        }
        file.close();
    };
    writeFile(root.filePath("bridge/folder.yml"), yml.toUtf8());
    writeFile(root.filePath("bridge/Ping.yml"), R"(info:
  name: Ping
  type: http
  seq: 1
http:
  method: GET
  url: https://example.com
  headers:
    - name: client-id
      value: override
  auth: inherit
)");
    writeFile(root.filePath("bridge/Get.yml"), R"(info:
  name: Get
  type: http
  seq: 2
http:
  method: GET
  url: https://example.com/get
  auth: inherit
)");

    CollectionModel model;
    if (!model.openDirectory(dir.path()) || !model.rootItem()) {
        std::cerr << "failed to open temp collection\n";
        std::abort();
    }
    CollectionItem* folderItem = nullptr;
    for (auto* child : model.rootItem()->children()) {
        if (child->type() == CollectionItemType::Folder) folderItem = child;
    }
    assert(folderItem != nullptr);
    assert(folderItem->headers().size() == 1);
    assert(folderItem->variables().value("region") == "eu");
    assert(!folderItem->children().isEmpty());
    CollectionItem* pingItem = nullptr;
    for (auto* child : folderItem->children()) {
        if (child->name() == "Ping") pingItem = child;
    }
    assert(pingItem != nullptr);
    RequestModel exec = pingItem->requestForExecution();
    int clientHeaders = 0;
    QString clientValue;
    for (const auto& header : exec.headers) {
        if (header.name == "client-id") {
            ++clientHeaders;
            clientValue = header.value;
        }
    }
    assert(clientHeaders == 1);
    assert(clientValue == "override");

    CollectionItem* getItem = nullptr;
    for (auto* child : folderItem->children()) {
        if (child->name() == "Get") getItem = child;
    }
    assert(getItem != nullptr && getItem->request() != nullptr);
    RequestModel inherited = getItem->requestForExecution();
    bool inheritedHeader = false;
    for (const auto& header : inherited.headers) {
        if (header.name == "client-id" && header.value == "kuidpro") inheritedHeader = true;
    }
    assert(inheritedHeader);

    std::cout << "[PASS] testFolderHeadersAreNotVariables\n";
}

void testCollectionHeadersAndVariablesInherit() {
    QTemporaryDir dir;
    if (!dir.isValid()) {
        std::cerr << "temp dir failed\n";
        std::abort();
    }

    QDir root(dir.path());
    auto writeFile = [](const QString& path, const QByteArray& data) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            std::cerr << "open failed: " << file.errorString().toStdString() << "\n";
            std::abort();
        }
        if (file.write(data) != data.size()) {
            std::cerr << "write failed: " << file.errorString().toStdString() << "\n";
            std::abort();
        }
        file.close();
    };

    writeFile(root.filePath("opencollection.yml"), R"(opencollection: 1.0.0
info:
  name: Rooted APIs
request:
  headers:
    - name: client-id
      value: root-client
  variables:
    - name: baseURL
      value: https://api.example.com
  auth:
    type: bearer
    token: root-token
)");
    writeFile(root.filePath("Get.yml"), R"(info:
  name: Get
  type: http
  seq: 1
http:
  method: GET
  url: "{{baseURL}}/health"
  auth: inherit
)");

    CollectionModel model;
    if (!model.openDirectory(dir.path()) || !model.rootItem()) {
        std::cerr << "failed to open temp collection\n";
        std::abort();
    }

    assert(model.rootItem()->headers().size() == 1);
    assert(model.rootItem()->headers()[0].name == "client-id");
    assert(model.rootItem()->variables().value("baseURL") == "https://api.example.com");

    auto requests = model.allRequestItems();
    assert(requests.size() == 1);
    RequestModel exec = requests[0]->requestForExecution();
    assert(exec.auth.type == AuthType::Bearer);
    assert(exec.auth.bearerToken == "root-token");

    bool inheritedHeader = false;
    for (const auto& header : exec.headers) {
        if (header.name == "client-id" && header.value == "root-client") inheritedHeader = true;
    }
    assert(inheritedHeader);

    VariableResolver resolver;
    resolver.setCollectionVariables(model.rootItem()->variables());
    RequestModel resolved = resolver.resolveRequest(exec);
    assert(resolved.url == "https://api.example.com/health");

    std::cout << "[PASS] testCollectionHeadersAndVariablesInherit\n";
}

void testKuidproApisCollection() {
    QString kuidproPath = "C:/Users/PC/Documents/GitHub/kuidpro-apis";
    if (!QDir(kuidproPath).exists()) {
        std::cout << "[SKIP] testKuidproApisCollection: path does not exist on this machine\n";
        return;
    }

    CollectionModel model;
    bool ok = model.openDirectory(kuidproPath);
    assert(ok);

    // 1. Root collection name
    assert(model.rootItem() != nullptr);
    assert(model.rootItem()->name() == "KuidPro APIs");

    // 2. Environments loaded
    assert(model.environments().size() >= 2);
    bool hasLocal = false;
    bool hasStaging = false;
    for (const auto& env : model.environments()) {
        if (env.name() == "Local") hasLocal = true;
        if (env.name() == "Staging") hasStaging = true;
    }
    assert(hasLocal);
    assert(hasStaging);

    // 3. Requests loaded: all 241 requests!
    auto allReqs = model.allRequests();
    std::cout << "Loaded " << allReqs.size() << " requests from KuidPro APIs OpenCollection\n";
    assert(allReqs.size() == 241);

    // 4. Verify specific requests in folders
    CollectionItem* auditFolder = nullptr;
    for (auto* child : model.rootItem()->children()) {
        if (child->name() == "Admin") {
            for (auto* sub : child->children()) {
                if (sub->name() == "Audit") {
                    auditFolder = sub;
                    break;
                }
            }
        }
    }
    assert(auditFolder != nullptr);
    assert(!auditFolder->children().isEmpty());
    assert(auditFolder->children()[0]->name() == "List Audit Events");
    assert(auditFolder->children()[0]->request() != nullptr);
    assert(auditFolder->children()[0]->request()->method == HttpMethod::GET);
    assert(auditFolder->children()[0]->request()->url == "{{baseURL}}/v1/audit/events");

    std::cout << "[PASS] testKuidproApisCollection (All 241 requests loaded & verified!)\n";
}

void testCloseCollection() {
    QTemporaryDir dir;
    assert(dir.isValid());
    QFile f(dir.filePath("opencollection.yml"));
    assert(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("opencollection: 1.0.0\ninfo:\n  name: TempColl\n");
    f.close();

    CollectionModel model;
    assert(model.openDirectory(dir.path()));
    assert(model.rootItem() != nullptr);
    assert(model.name() == "TempColl");

    model.closeCollection();
    assert(model.rootItem() == nullptr);
    assert(model.rootPath().isEmpty());
    assert(model.name().isEmpty());
    assert(model.environments().isEmpty());
    std::cout << "[PASS] testCloseCollection\n";
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    testYamlNodeParsing();
    testOpenCollectionRequestParsing();
    testOpenCollectionWriterRoundtrip();
    testOpenCollectionEnvironmentParsing();
    testRuntimeVariablesAndPreservedSections();
    testFolderHeadersAreNotVariables();
    testCollectionHeadersAndVariablesInherit();
    testCloseCollection();
    testKuidproApisCollection();
    std::cout << "All OpenCollection tests passed!\n";
    return 0;
}
