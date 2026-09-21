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

    void setClientCertPath(const QString& path) { m_clientCertPath = path; }
    const QString& clientCertPath() const { return m_clientCertPath; }

    void setClientCertType(const QString& type) { m_clientCertType = type; }
    const QString& clientCertType() const { return m_clientCertType; }

    void setClientKeyPath(const QString& path) { m_clientKeyPath = path; }
    const QString& clientKeyPath() const { return m_clientKeyPath; }

    void setClientKeyPassword(const QString& pass) { m_clientKeyPassword = pass; }
    const QString& clientKeyPassword() const { return m_clientKeyPassword; }

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

    // mTLS
    QString m_clientCertPath;
    QString m_clientCertType{"PEM"};
    QString m_clientKeyPath;
    QString m_clientKeyPassword;
};

} // namespace poppy::network
