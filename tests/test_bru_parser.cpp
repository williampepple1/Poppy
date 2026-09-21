#include <iostream>
#include <cassert>
#include <core/BruParser.h>
#include <core/BruWriter.h>

using namespace poppy::core;

int main() {
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

    std::cout << "test_bru_parser PASSED!" << std::endl;
    return 0;
}
