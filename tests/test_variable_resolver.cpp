#include <iostream>
#include <cassert>
#include <core/VariableResolver.h>
#include <core/EnvironmentModel.h>

using namespace poppy::core;

int main() {
    std::cout << "Running test_variable_resolver..." << std::endl;

    EnvironmentModel env("dev");
    env.addOrUpdateVariable("baseUrl", "https://api.test.com");
    env.addOrUpdateVariable("token", "secret123");
    env.addOrUpdateVariable("userId", "99");

    VariableResolver resolver;
    resolver.setEnvironment(env);

    // 1. String interpolation
    QString rawUrl = "{{baseUrl}}/users/{{userId}}?auth={{token}}";
    QString resolved = resolver.resolveString(rawUrl);
    assert(resolved == "https://api.test.com/users/99?auth=secret123");

    env.addOrUpdateVariable("a", "{{b}}");
    env.addOrUpdateVariable("b", "hello");
    resolver.setEnvironment(env);
    assert(resolver.resolveString("{{a}}") == "hello");

    // 2. Dynamic variable interpolation
    QString dynamicStr = "UUID: {{$guid}}, Time: {{$timestamp}}";
    QString dynamicRes = resolver.resolveString(dynamicStr);
    assert(!dynamicRes.contains("{{$guid}}"));
    assert(!dynamicRes.contains("{{$timestamp}}"));
    assert(dynamicRes.contains("UUID: "));

    // 3. Request resolution
    RequestModel req;
    req.url = "{{baseUrl}}/v1/test";
    req.headers.append(HttpHeader{.name = "Authorization", .value = "Bearer {{token}}", .enabled = true});
    req.bodyType = BodyType::Json;
    req.bodyContent = "{\"user\": \"{{userId}}\"}";

    RequestModel resolvedReq = resolver.resolveRequest(req);
    assert(resolvedReq.url == "https://api.test.com/v1/test");
    assert(resolvedReq.headers[0].value == "Bearer secret123");
    assert(resolvedReq.bodyContent == "{\"user\": \"99\"}");

    // Empty-but-defined variables must still be substituted
    env.addOrUpdateVariable("emptyHost", "");
    resolver.setEnvironment(env);
    QString emptyResolved = resolver.resolveString("https://api/{{emptyHost}}/x");
    assert(emptyResolved == "https://api//x");

    // GraphQL + form + AWS fields are resolved
    RequestModel gqlReq;
    gqlReq.bodyType = BodyType::GraphQL;
    gqlReq.graphqlQuery = "query { user(id: \"{{userId}}\") { name } }";
    gqlReq.graphqlVariables = "{\"token\":\"{{token}}\"}";
    gqlReq.formDataParams.append(FormDataParam{.key = "k", .value = "{{token}}", .isFile = false, .enabled = true});
    gqlReq.auth.awsAccessKey = "{{token}}";
    RequestModel gqlResolved = resolver.resolveRequest(gqlReq);
    assert(gqlResolved.graphqlQuery.contains("99"));
    assert(gqlResolved.graphqlVariables.contains("secret123"));
    assert(gqlResolved.formDataParams[0].value == "secret123");
    assert(gqlResolved.auth.awsAccessKey == "secret123");

    RequestModel pathReq;
    pathReq.url = "https://api.test.com/users/:id";
    pathReq.pathParams.append(HttpParam{.key = "id", .value = "a b", .enabled = true});
    assert(pathReq.effectiveUrl().contains("a%20b"));

    RequestModel formReq;
    formReq.bodyType = BodyType::FormUrlEncoded;
    formReq.bodyContent = "q=hello world";
    assert(QString::fromUtf8(formReq.effectiveBody()).contains("hello%20world"));

    RequestModel encodedForm;
    encodedForm.bodyType = BodyType::FormUrlEncoded;
    encodedForm.bodyContent = "q=hello%20world&note=a%26b";
    QString encodedBody = QString::fromUtf8(encodedForm.effectiveBody());
    assert(encodedBody.contains("hello%20world"));
    assert(encodedBody.contains("a%26b"));
    assert(!encodedBody.contains("hello%2520"));

    resolver.setEnvironment(env);
    resolver.setCollectionVariables({{"baseUrl", "https://from-collection"}});
    resolver.setFolderVariables({{"baseUrl", "https://from-folder"}});
    assert(resolver.resolveString("{{baseUrl}}") == "https://from-folder");
    QString folderScope;
    assert(resolver.lookupVariableWithScope("baseUrl", &folderScope) == "https://from-folder");
    assert(folderScope == "Folder");
    resolver.setFolderVariables({});
    assert(resolver.resolveString("{{baseUrl}}") == "https://from-collection");
    resolver.setCollectionVariables({});
    assert(resolver.resolveString("{{baseUrl}}") == "https://api.test.com");

    QString tsOk = resolver.resolveString("{{$timestamp}}");
    assert(!tsOk.contains("{{$timestamp}}"));
    assert(resolver.resolveString("{{$timestampMillis}}") == "{{$timestampMillis}}");
    // Test extended dynamic generator variables
    QString emailRes = resolver.resolveString("Email: {{$randomEmail}}");
    assert(emailRes.contains("@example.com"));

    QString uuidRes = resolver.resolveString("UUID: {{$randomUUID}}");
    assert(!uuidRes.contains("{{$randomUUID}}") && uuidRes.length() > 20);

    QString isoRes = resolver.resolveString("ISO: {{$isoTimestamp}}");
    assert(isoRes.contains("T") && isoRes.contains("Z"));

    QString nameRes = resolver.resolveString("Name: {{$randomFullName}}");
    assert(!nameRes.contains("{{$randomFullName}}") && nameRes.contains(" "));

    QString priceRes = resolver.resolveString("Price: {{$randomPrice}}");
    assert(priceRes.contains("."));

    QString colorRes = resolver.resolveString("Color: {{$randomColor}}");
    assert(colorRes.startsWith("Color: #"));

    std::cout << "test_variable_resolver PASSED!" << std::endl;
    return 0;
}
