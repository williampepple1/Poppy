#include <iostream>
#include <cassert>
#include <QCryptographicHash>
#include <network/WebSocketClient.h>

using namespace poppy::network;

int main() {
    std::cout << "Running test_websocket..." << std::endl;

    WebSocketClient client;
    assert(client.state() == WebSocketClient::State::Disconnected);
    assert(!client.isConnected());

    // Verify RFC 6455 Section 4.2.2 standard test vector:
    // Key: "dGhlIHNhbXBsZSBub25jZQ=="
    // Expected Accept: "s3pPLMBiTxaQ9kYGzzhZRbK+xOo="
    QString testKey = "dGhlIHNhbXBsZSBub25jZQ==";
    QString guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    QString accept = QCryptographicHash::hash((testKey + guid).toUtf8(), QCryptographicHash::Sha1).toBase64();
    assert(accept == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");

    // Masking test: XORing with a 4-byte mask twice restores original payload
    QByteArray payload = "Hello, WebSocket!";
    quint8 mask[4] = { 0x12, 0x34, 0x56, 0x78 };
    QByteArray masked = payload;
    for (int i = 0; i < masked.size(); ++i) {
        masked[i] = masked[i] ^ mask[i % 4];
    }
    assert(masked != payload);
    QByteArray unmasked = masked;
    for (int i = 0; i < unmasked.size(); ++i) {
        unmasked[i] = unmasked[i] ^ mask[i % 4];
    }
    assert(unmasked == payload);

    std::cout << "test_websocket passed successfully!" << std::endl;
    return 0;
}
