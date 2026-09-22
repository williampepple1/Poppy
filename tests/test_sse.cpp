#include <iostream>
#include <cassert>
#include <core/SseParser.h>

using namespace poppy::core;

int main() {
    std::cout << "Running test_sse..." << std::endl;

    SseParser parser;

    // Test 1: Standard event
    QByteArray streamData =
        "event: user_join\n"
        "id: 101\n"
        "data: {\"username\": \"alice\"}\n\n";

    auto events = parser.feed(streamData);
    assert(events.size() == 1);
    assert(events[0].event == "user_join");
    assert(events[0].id == "101");
    assert(events[0].data == "{\"username\": \"alice\"}");

    // Test 2: Multiline data & comments
    QByteArray multiData =
        ": this is a ping comment\n"
        "data: Line 1\n"
        "data: Line 2\n\n";

    auto ev2 = parser.feed(multiData);
    assert(ev2.size() == 1);
    assert(ev2[0].event == "message"); // default
    assert(ev2[0].data == "Line 1\nLine 2");

    // Test 3: LLM delta stream extractor
    QString chunk1 = "{\"id\":\"chatcmpl\",\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}";
    QString delta1 = SseParser::extractLlmStreamDelta(chunk1);
    assert(delta1 == "Hello");

    QString chunk2 = "{\"choices\":[{\"delta\":{\"content\":\" world!\"}}]}";
    QString delta2 = SseParser::extractLlmStreamDelta(chunk2);
    assert(delta2 == " world!");

    QString doneChunk = "[DONE]";
    assert(SseParser::extractLlmStreamDelta(doneChunk) == "");

    std::cout << "test_sse passed successfully!" << std::endl;
    return 0;
}
