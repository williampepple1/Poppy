#pragma once

#include <QString>
#include <QList>
#include <QDateTime>
#include <QObject>
#include <core/RequestModel.h>
#include <core/ResponseModel.h>

namespace poppy::core {

struct HistoryItem {
    QString id;
    QDateTime timestamp;
    RequestModel request;
    int statusCode = 0;
    QString statusText;
    qint64 responseTimeMs = 0;
    qint64 responseSizeBytes = 0;
    bool success = true;
    QString errorString;
    QByteArray responseRawBody;
    QList<HttpHeader> responseHeaders;
    // Collection item path captured at send time, so a replay can resolve folder vars.
    QString sourcePath;

    ResponseModel toResponseModel() const {
        ResponseModel res;
        res.statusCode = statusCode;
        res.statusText = statusText;
        res.latencyMs = responseTimeMs;
        res.sizeBytes = responseSizeBytes;
        res.rawBody = responseRawBody;
        res.headers = responseHeaders;
        res.errorString = errorString;
        return res;
    }
};

class HistoryManager : public QObject {
    Q_OBJECT
public:
    explicit HistoryManager(QObject* parent = nullptr);

    void setMaxEntries(int max) { m_maxEntries = max; }
    int maxEntries() const { return m_maxEntries; }

    void setAutoSave(bool autoSave) { m_autoSave = autoSave; }
    bool autoSave() const { return m_autoSave; }

    void setHistoryFilePath(const QString& path) { m_historyFilePath = path; }
    QString historyFilePath() const;

    const QList<HistoryItem>& items() const { return m_items; }
    int count() const { return m_items.size(); }

    void addEntry(const RequestModel& req, const ResponseModel& res, const QString& sourcePath = QString());
    bool removeEntry(const QString& id);
    void clear();

    QList<HistoryItem> filter(const QString& query) const;

    bool loadFromFile(const QString& filePath);
    bool saveToFile(const QString& filePath) const;

    static QString defaultHistoryFilePath();

signals:
    void entryAdded(const HistoryItem& item);
    void historyCleared();

private:
    int m_maxEntries = 100;
    bool m_autoSave = true;
    QString m_historyFilePath;
    QList<HistoryItem> m_items; // Index 0 is the most recent
};

} // namespace poppy::core
