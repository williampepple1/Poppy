#include <iostream>
#include <cassert>
#include <network/MockServer.h>
#include <QCoreApplication>
#include <QTcpSocket>
#include <QHostAddress>

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

    MockRoute wild;
    wild.method = "GET";
    wild.path = "/api/users/*";
    wild.statusCode = 200;
    wild.responseBody = "{\"kind\":\"wild\"}";
    server.addRoute(wild);

    MockRoute param;
    param.method = "GET";
    param.path = "/pets/:id";
    param.statusCode = 201;
    param.responseBody = "{\"kind\":\"pet\"}";
    server.addRoute(param);

    MockRoute fail;
    fail.method = "GET";
    fail.path = "/fail";
    fail.statusCode = 500;
    fail.responseBody = "{\"err\":true}";
    server.addRoute(fail);

    assert(server.routes().size() == 1);
    assert(server.routes()[0].path == "/api/v1/test");

    // Start on high port
    quint16 testPort = 18999;
    bool started = server.start(testPort);
    assert(started);
    assert(server.isRunning());
    assert(server.port() == testPort);
    assert(server.serverUrl() == QString("http://127.0.0.1:%1").arg(testPort));

    auto httpGet = [&](const QString& path) {
        QTcpSocket sock;
        sock.connectToHost(QHostAddress::LocalHost, testPort);
        assert(sock.waitForConnected(2000));
        sock.write(QString("GET %1 HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n").arg(path).toUtf8());
        assert(sock.waitForBytesWritten(2000));
        assert(sock.waitForReadyRead(2000));
        QByteArray resp = sock.readAll();
        while (sock.waitForReadyRead(200)) resp += sock.readAll();
        return QString::fromUtf8(resp);
    };

    const QString wildResp = httpGet("/api/users/42");
    assert(wildResp.startsWith("HTTP/1.1 200 OK"));
    assert(wildResp.contains("\"kind\":\"wild\""));

    const QString petResp = httpGet("/pets/9");
    assert(petResp.startsWith("HTTP/1.1 201 Created"));
    assert(petResp.contains("\"kind\":\"pet\""));

    const QString failResp = httpGet("/fail");
    assert(failResp.startsWith("HTTP/1.1 500 Internal Server Error"));

    server.stop();
    assert(!server.isRunning());

    std::cout << "test_mock_server passed successfully!" << std::endl;
    return 0;
}
