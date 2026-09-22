#include <iostream>
#include <cassert>
#include <network/MockServer.h>
#include <QCoreApplication>

using namespace poppy::network;

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    std::cout << "Running test_mock_server..." << std::endl;

    MockServer server;
    assert(!server.isRunning());

    MockRoute r1;
    r1.method = "GET";
    r1.path = "/api/v1/test";
    r1.statusCode = 200;
    r1.responseBody = "{\"status\": \"ok\"}";
    server.addRoute(r1);

    assert(server.routes().size() == 1);
    assert(server.routes()[0].path == "/api/v1/test");

    // Start on high port
    quint16 testPort = 18999;
    bool started = server.start(testPort);
    assert(started);
    assert(server.isRunning());
    assert(server.port() == testPort);
    assert(server.serverUrl() == QString("http://127.0.0.1:%1").arg(testPort));

    server.stop();
    assert(!server.isRunning());

    std::cout << "test_mock_server passed successfully!" << std::endl;
    return 0;
}
