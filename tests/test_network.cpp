#include <iostream>
#include <cassert>
#include <QCoreApplication>
#include <network/CurlNetworkEngine.h>

using namespace poppy::core;
using namespace poppy::network;

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    std::cout << "Running test_network..." << std::endl;

    CurlNetworkEngine engine;

    // Test 1: GET request to example.com
    RequestModel req;
    req.method = HttpMethod::GET;
    req.url = "http://example.com";

    ResponseModel res = engine.sendRequestSync(req);

    std::cout << "Response status: " << res.statusCode << " " << res.statusText.toStdString() << std::endl;
    std::cout << "Latency: " << res.latencyMs << " ms" << std::endl;
    std::cout << "Body size: " << res.sizeBytes << " bytes" << std::endl;

    assert(res.statusCode == 200 || res.statusCode == 301 || res.statusCode == 302);
    assert(res.latencyMs >= 0);
    assert(!res.rawBody.isEmpty());

    // Test 2: cURL command generation
    RequestModel postReq;
    postReq.method = HttpMethod::POST;
    postReq.url = "https://api.example.com/login";
    postReq.headers.append(HttpHeader{.name = "Content-Type", .value = "application/json", .enabled = true});
    postReq.bodyType = BodyType::Json;
    postReq.bodyContent = "{\"username\": \"admin\"}";

    QString curlCmd = postReq.toCurlCommand();
    assert(curlCmd.contains("curl -X POST"));
    assert(curlCmd.contains("-H \"Content-Type: application/json\""));
    assert(curlCmd.contains("https://api.example.com/login"));

    std::cout << "test_network PASSED!" << std::endl;
    return 0;
}
