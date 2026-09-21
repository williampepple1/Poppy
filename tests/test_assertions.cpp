#include <iostream>
#include <cassert>
#include <core/assertions/DeclarativeAssertion.h>
#include <core/ResponseModel.h>
#include <core/BruParser.h>
#include <core/BruWriter.h>

using namespace poppy::core;

int main() {
    std::cout << "Running test_assertions..." << std::endl;

    ResponseModel res;
    res.statusCode = 200;
    res.statusText = "OK";
    res.latencyMs = 150;
    res.headers.append(HttpHeader{.name = "Content-Type", .value = "application/json; charset=utf-8", .enabled = true});
    res.headers.append(HttpHeader{.name = "X-Custom-Header", .value = "PoppyClient", .enabled = true});
    res.rawBody = R"({
        "user": {
            "name": "Alice",
            "age": 30,
            "roles": ["admin", "editor"]
        },
        "items": [
            {"id": "item-1", "price": 19.99},
            {"id": "item-2", "price": 49.99}
        ]
    })";

    // 1. Status assertions
    AssertionRule rule1{"res.status", "eq", "200", true};
    auto r1 = DeclarativeAssertionEvaluator::evaluate(rule1, res);
    assert(r1.passed == true);

    AssertionRule rule2{"res.status", "neq", "404", true};
    auto r2 = DeclarativeAssertionEvaluator::evaluate(rule2, res);
    assert(r2.passed == true);

    // 2. Latency / Response Time
    AssertionRule rule3{"res.responseTime", "lt", "500", true};
    auto r3 = DeclarativeAssertionEvaluator::evaluate(rule3, res);
    assert(r3.passed == true);

    AssertionRule rule4{"res.responseTime", "gt", "10", true};
    auto r4 = DeclarativeAssertionEvaluator::evaluate(rule4, res);
    assert(r4.passed == true);

    // 3. Headers
    AssertionRule rule5{"res.header(Content-Type)", "contains", "application/json", true};
    auto r5 = DeclarativeAssertionEvaluator::evaluate(rule5, res);
    assert(r5.passed == true);

    AssertionRule rule6{"res.header(X-Custom-Header)", "eq", "PoppyClient", true};
    auto r6 = DeclarativeAssertionEvaluator::evaluate(rule6, res);
    assert(r6.passed == true);

    // 4. JSON body dot-paths
    AssertionRule rule7{"res.body.user.name", "eq", "Alice", true};
    auto r7 = DeclarativeAssertionEvaluator::evaluate(rule7, res);
    assert(r7.passed == true);

    AssertionRule rule8{"res.body.user.age", "gte", "18", true};
    auto r8 = DeclarativeAssertionEvaluator::evaluate(rule8, res);
    assert(r8.passed == true);

    AssertionRule rule9{"res.body.items[0].id", "eq", "item-1", true};
    auto r9 = DeclarativeAssertionEvaluator::evaluate(rule9, res);
    assert(r9.passed == true);

    // 5. Negative / Failing assertion
    AssertionRule ruleFail{"res.status", "eq", "500", true};
    auto rFail = DeclarativeAssertionEvaluator::evaluate(ruleFail, res);
    assert(rFail.passed == false);
    assert(rFail.errorMessage.contains("Expected '500' but got '200'"));

    // 6. Test evaluateAll
    QList<AssertionRule> rules = {rule1, rule2, rule3, rule7, rule9};
    auto allResults = DeclarativeAssertionEvaluator::evaluateAll(rules, res);
    assert(allResults.size() == 5);
    for (const auto& r : allResults) {
        assert(r.passed == true);
    }

    // 7. BruParser and BruWriter roundtrip for assertions
    QString sampleBru = R"(meta {
  name: Test Assertions
  type: http
  seq: 1
}

get {
  url: https://api.example.com
  body: none
  auth: none
}

assertions {
  res.status eq 200
  res.responseTime lt 300
  res.body.user.name eq Alice
}
)";

    RequestModel req = BruParser::parse(sampleBru);
    assert(req.assertions.size() == 3);
    assert(req.assertions[0].target == "res.status" && req.assertions[0].op == "eq" && req.assertions[0].expected == "200");
    assert(req.assertions[1].target == "res.responseTime" && req.assertions[1].op == "lt" && req.assertions[1].expected == "300");
    assert(req.assertions[2].target == "res.body.user.name" && req.assertions[2].op == "eq" && req.assertions[2].expected == "Alice");

    QString serialized = BruWriter::serialize(req);
    assert(serialized.contains("assertions {"));
    assert(serialized.contains("res.status eq 200"));
    assert(serialized.contains("res.responseTime lt 300"));
    assert(serialized.contains("res.body.user.name eq Alice"));

    RequestModel roundTrip = BruParser::parse(serialized);
    assert(roundTrip.assertions.size() == 3);
    assert(roundTrip.assertions[0] == req.assertions[0]);
    assert(roundTrip.assertions[1] == req.assertions[1]);
    assert(roundTrip.assertions[2] == req.assertions[2]);

    std::cout << "test_assertions PASSED!" << std::endl;
    return 0;
}
