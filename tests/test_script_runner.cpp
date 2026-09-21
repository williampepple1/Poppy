#include <iostream>
#include <cassert>
#include <QCoreApplication>
#include <core/ScriptRunner.h>
#include <core/RequestModel.h>
#include <core/ResponseModel.h>
#include <core/EnvironmentModel.h>

using namespace poppy::core;

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    std::cout << "Running test_script_runner..." << std::endl;

    ScriptRunner runner;
    EnvironmentModel env("test-env");
    RequestModel req;
    req.url = "http://example.com/api";

    // 1. Pre-request script
    QString preScript = R"(
        poppy.setEnvVar("authToken", "token-xyz");
        poppy.setEnvVar("calc", 10 + 20);
    )";
    bool preOk = runner.runPreRequestScript(preScript, req, env);
    assert(preOk);
    assert(env.variableValue("authToken") == "token-xyz");
    assert(env.variableValue("calc") == "30");

    // 2. Setup mock response
    ResponseModel res;
    res.statusCode = 200;
    res.statusText = "OK";
    res.latencyMs = 45;
    res.rawBody = R"({"user": "admin", "id": 1234})";
    res.headers.append(HttpHeader{.name = "Content-Type", .value = "application/json", .enabled = true});

    // 3. Post-response script
    QString postScript = R"(
        var body = res.getBody();
        poppy.setEnvVar("savedUserId", body.id);
    )";
    bool postOk = runner.runPostResponseScript(postScript, req, res, env);
    assert(postOk);
    assert(env.variableValue("savedUserId") == "1234");

    // 4. Test assertions runner
    QString testScript = R"(
        test("Status is 200", function() {
            expect(res.getStatus()).to.equal(200);
        });

        test("User is admin", function() {
            var body = res.getBody();
            expect(body.user).to.equal("admin");
        });

        test("Response time is positive", function() {
            expect(res.getResponseTime()).to.be.above(0);
        });

        test("Failing test should fail", function() {
            expect(res.getStatus()).to.equal(404);
        });
    )";

    TestReport report = runner.runTests(testScript, req, res, env);
    assert(report.totalCount() == 4);
    assert(report.passedCount() == 3);
    assert(report.failedCount() == 1);
    assert(report.results[0].passed == true);
    assert(report.results[1].passed == true);
    assert(report.results[2].passed == true);
    assert(report.results[3].passed == false);
    assert(!report.results[3].errorMessage.isEmpty());

    std::cout << "test_script_runner PASSED!" << std::endl;
    return 0;
}
