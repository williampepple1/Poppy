#include <cassert>
#include <iostream>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include "core/YamlNode.h"
#include "core/OpenCollectionParser.h"
#include "core/OpenCollectionWriter.h"
#include "core/CollectionModel.h"

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

int main(int argc, char* argv[]) {
    testYamlNodeParsing();
    testOpenCollectionRequestParsing();
    testOpenCollectionWriterRoundtrip();
    testOpenCollectionEnvironmentParsing();
    testKuidproApisCollection();
    std::cout << "All OpenCollection tests passed!\n";
    return 0;
}
