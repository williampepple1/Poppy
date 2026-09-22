#pragma once

#include <QObject>
#include <QThreadPool>
#include <QMutex>
#include <atomic>
#include "INetworkEngine.h"

namespace poppy::network {

class RequestTask;

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

    core::ResponseModel sendRequestSync(const core::RequestModel& req);

    void cancelAll() override;
    bool isGenerationCurrent(uint64_t generation) const;

    void setSslVerifyPeer(bool verify);
    bool sslVerifyPeer() const;

    void setTimeoutMs(long ms);
    long timeoutMs() const;

    void setProxy(const QString& proxy);
    QString proxy() const;

    void setCookieJarEnabled(bool enabled);
    bool cookieJarEnabled() const;

    void setCookieJarPath(const QString& path);
    QString cookieJarPath() const;

    void clearCookies();

    void setClientCertPath(const QString& path);
    QString clientCertPath() const;

    void setClientCertType(const QString& type);
    QString clientCertType() const;

    void setClientKeyPath(const QString& path);
    QString clientKeyPath() const;

    void setClientKeyPassword(const QString& pass);
    QString clientKeyPassword() const;

signals:
    void requestStarted();
    void requestFinished(const poppy::core::ResponseModel& response);

private:
    struct SettingsSnapshot {
        bool sslVerifyPeer{true};
        long timeoutMs{30000};
        QString proxy;
        bool cookieJarEnabled{true};
        QString cookieJarPath;
        QString clientCertPath;
        QString clientCertType{"PEM"};
        QString clientKeyPath;
        QString clientKeyPassword;
    };

    SettingsSnapshot snapshotSettings() const;
    core::ResponseModel executeCurl(const core::RequestModel& req, uint64_t generation, ProgressCallback onProgress);

    QThreadPool m_threadPool;
    mutable QMutex m_settingsMutex;
    std::atomic<uint64_t> m_cancelGeneration{0};
    void* m_cookieShare{nullptr};

    friend class RequestTask;

    bool m_sslVerifyPeer{true};
    long m_timeoutMs{30000};
    QString m_proxy;
    bool m_cookieJarEnabled{true};
    QString m_cookieJarPath;
    QString m_clientCertPath;
    QString m_clientCertType{"PEM"};
    QString m_clientKeyPath;
    QString m_clientKeyPassword;
};

} // namespace poppy::network
