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

    std::cout << "test_variable_resolver PASSED!" << std::endl;
    return 0;
}
