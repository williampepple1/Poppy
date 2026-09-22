#pragma once

#include <QString>
#include <QList>
#include <QDateTime>
#include <QByteArray>

namespace poppy::core {

struct SseEvent {
    QString id;
    QString event{"message"};
    QString data;
    int retry{-1};
    QDateTime timestamp;
};

class SseParser {
public:
    SseParser() = default;

    // Feeds incoming raw bytes and returns any completed SSE events
    QList<SseEvent> feed(const QByteArray& chunk);
    QList<SseEvent> feed(const QString& chunk);

    // Resets parser state
    void reset();

    // Reconstructs aggregated text from OpenAI/LLM style streaming chunks
    // where each data chunk is JSON: {"choices":[{"delta":{"content":"..."}}]}
    static QString extractLlmStreamDelta(const QString& jsonData);

private:
    void processLine(const QString& line, QList<SseEvent>& completed);

    QString m_buffer;
    SseEvent m_currentEvent;
    QStringList m_dataLines;
};

} // namespace poppy::core
