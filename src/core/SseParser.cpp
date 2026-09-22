#include "SseParser.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace poppy::core {

void SseParser::reset() {
    m_buffer.clear();
    m_currentEvent = SseEvent{};
    m_dataLines.clear();
}

QList<SseEvent> SseParser::feed(const QByteArray& chunk) {
    return feed(QString::fromUtf8(chunk));
}

QList<SseEvent> SseParser::feed(const QString& chunk) {
    QList<SseEvent> completed;
    m_buffer.append(chunk);

    // Normalize line endings
    m_buffer.replace("\r\n", "\n");
    m_buffer.replace("\r", "\n");

    int newlinePos;
    while ((newlinePos = m_buffer.indexOf('\n')) != -1) {
        QString line = m_buffer.left(newlinePos);
        m_buffer.remove(0, newlinePos + 1);
        processLine(line, completed);
    }

    return completed;
}

void SseParser::processLine(const QString& line, QList<SseEvent>& completed) {
    if (line.isEmpty()) {
        // Empty line dispatches the current event if we have any data or event set
        if (!m_dataLines.isEmpty() || !m_currentEvent.event.isEmpty() || !m_currentEvent.id.isEmpty()) {
            m_currentEvent.data = m_dataLines.join('\n');
            m_currentEvent.timestamp = QDateTime::currentDateTime();
            if (m_currentEvent.event.isEmpty()) {
                m_currentEvent.event = "message";
            }
            completed.append(m_currentEvent);

            // Reset current event
            m_currentEvent = SseEvent{};
            m_dataLines.clear();
        }
        return;
    }

    // Comments start with ':'
    if (line.startsWith(':')) {
        return;
    }

    int colonIdx = line.indexOf(':');
    QString field;
    QString value;

    if (colonIdx != -1) {
        field = line.left(colonIdx).trimmed();
        value = line.mid(colonIdx + 1);
        if (value.startsWith(' ')) {
            value.remove(0, 1);
        }
    } else {
        field = line.trimmed();
        value = "";
    }

    if (field == "event") {
        m_currentEvent.event = value;
    } else if (field == "data") {
        m_dataLines.append(value);
    } else if (field == "id") {
        m_currentEvent.id = value;
    } else if (field == "retry") {
        bool ok = false;
        int r = value.toInt(&ok);
        if (ok) {
            m_currentEvent.retry = r;
        }
    }
}

QString SseParser::extractLlmStreamDelta(const QString& jsonData) {
    if (jsonData.trimmed() == "[DONE]") {
        return "";
    }

    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(jsonData.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return "";
    }

    QJsonObject root = doc.object();
    if (root.contains("choices") && root.value("choices").isArray()) {
        QJsonArray choices = root.value("choices").toArray();
        if (!choices.isEmpty()) {
            QJsonObject firstChoice = choices[0].toObject();
            if (firstChoice.contains("delta") && firstChoice.value("delta").isObject()) {
                QJsonObject delta = firstChoice.value("delta").toObject();
                if (delta.contains("content")) {
                    return delta.value("content").toString();
                }
            } else if (firstChoice.contains("text")) {
                return firstChoice.value("text").toString();
            }
        }
    }

    return "";
}

} // namespace poppy::core
