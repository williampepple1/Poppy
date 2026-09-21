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

    void setCookieJarEnabled(bool enabled) { m_cookieJarEnabled = enabled; }
    bool cookieJarEnabled() const { return m_cookieJarEnabled; }

    void setCookieJarPath(const QString& path) { m_cookieJarPath = path; }
    const QString& cookieJarPath() const { return m_cookieJarPath; }

    void clearCookies();

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
    bool m_cookieJarEnabled{true};
    QString m_cookieJarPath;
};

} // namespace poppy::network
