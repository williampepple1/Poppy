#pragma once

#include <QObject>
#include <QThreadPool>
#include <atomic>
#include "INetworkEngine.h"

namespace poppy::network {

class CurlNetworkEngine : public QObject, public INetworkEngine {
    Q_OBJECT
public:
    explicit CurlNetworkEngine(QObject* parent = nullptr);
    ~CurlNetworkEngine() override;

    void sendRequestAsync(
        const core::RequestModel& req,
        CompletionCallback onComplete,
        ProgressCallback onProgress = nullptr
    ) override;

    // Synchronous execution (e.g. for CLI runner)
    core::ResponseModel sendRequestSync(const core::RequestModel& req);

    void cancelAll() override;

    void setSslVerifyPeer(bool verify) { m_sslVerifyPeer = verify; }
    bool sslVerifyPeer() const { return m_sslVerifyPeer; }

    void setTimeoutMs(long ms) { m_timeoutMs = ms; }
    long timeoutMs() const { return m_timeoutMs; }

    void setProxy(const QString& proxy) { m_proxy = proxy; }
    const QString& proxy() const { return m_proxy; }

signals:
    void requestStarted();
    void requestFinished(const poppy::core::ResponseModel& response);

private:
    core::ResponseModel executeCurl(const core::RequestModel& req, ProgressCallback onProgress);

    QThreadPool m_threadPool;
    std::atomic<bool> m_cancelled{false};
    bool m_sslVerifyPeer{true};
    long m_timeoutMs{30000};
    QString m_proxy;
};

} // namespace poppy::network
