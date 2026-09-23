#include "CurlNetworkEngine.h"
#include <curl/curl.h>
#include <QRunnable>
#include <QPointer>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <mutex>

namespace poppy::network {

namespace {
    std::once_flag curlInitOnce;
    std::mutex g_curlShareMutex;

    void ensureCurlInitialized() {
        std::call_once(curlInitOnce, []() {
            curl_global_init(CURL_GLOBAL_DEFAULT);
        });
    }

    void shareLock(CURL*, curl_lock_data, curl_lock_access, void*) {
        g_curlShareMutex.lock();
    }

    void shareUnlock(CURL*, curl_lock_data, void*) {
        g_curlShareMutex.unlock();
    }

    size_t headerCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
        size_t totalBytes = size * nitems;
        auto* headers = static_cast<QList<core::HttpHeader>*>(userdata);
        if (!headers) return totalBytes;

        QString line = QString::fromUtf8(buffer, static_cast<int>(totalBytes)).trimmed();
        if (!line.isEmpty()) {
            int colonIdx = line.indexOf(':');
            if (colonIdx > 0) {
                QString name = line.left(colonIdx).trimmed();
                QString val = line.mid(colonIdx + 1).trimmed();
                headers->append(core::HttpHeader{.name = name, .value = val, .enabled = true});
            }
        }
        return totalBytes;
    }

    size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
        size_t totalBytes = size * nmemb;
        auto* rawBody = static_cast<QByteArray*>(userdata);
        if (rawBody) {
            rawBody->append(ptr, static_cast<qsizetype>(totalBytes));
        }
        return totalBytes;
    }

    struct CancelContext {
        std::atomic<uint64_t>* generation{nullptr};
        uint64_t expected{0};
    };

    int xferInfoCallback(void* clientp, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
        auto* ctx = static_cast<CancelContext*>(clientp);
        if (ctx && ctx->generation && ctx->generation->load() != ctx->expected) {
            return 1;
        }
        return 0;
    }
}

class RequestTask : public QRunnable {
public:
    RequestTask(CurlNetworkEngine* engine,
                const core::RequestModel& req,
                uint64_t generation,
                CompletionCallback onComplete)
        : m_engine(engine), m_req(req), m_generation(generation), m_onComplete(std::move(onComplete)) {
        setAutoDelete(true);
    }

    void run() override {
        if (!m_engine) return;
        core::ResponseModel res = m_engine->executeCurl(m_req, m_generation, nullptr);
        if (!m_engine->isGenerationCurrent(m_generation) && res.errorString.isEmpty()) {
            res.errorString = QStringLiteral("Request cancelled");
        }

        QPointer<CurlNetworkEngine> safeEngine = m_engine;
        auto callback = m_onComplete;
        QMetaObject::invokeMethod(m_engine, [safeEngine, res, callback]() {
            if (callback) {
                callback(res);
            }
            if (safeEngine) {
                emit safeEngine->requestFinished(res);
            }
        }, Qt::QueuedConnection);
    }

private:
    CurlNetworkEngine* m_engine;
    core::RequestModel m_req;
    uint64_t m_generation;
    CompletionCallback m_onComplete;
};

CurlNetworkEngine::CurlNetworkEngine(QObject* parent) : QObject(parent) {
    ensureCurlInitialized();
    m_threadPool.setMaxThreadCount(8);

    auto* share = curl_share_init();
    if (share) {
        curl_share_setopt(share, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
        curl_share_setopt(share, CURLSHOPT_LOCKFUNC, shareLock);
        curl_share_setopt(share, CURLSHOPT_UNLOCKFUNC, shareUnlock);
        m_cookieShare = share;
    }
}

CurlNetworkEngine::~CurlNetworkEngine() {
    cancelAll();
    m_threadPool.waitForDone();
    if (m_cookieShare) {
        curl_share_cleanup(static_cast<CURLSH*>(m_cookieShare));
        m_cookieShare = nullptr;
    }
}

void CurlNetworkEngine::cancelAll() {
    m_cancelGeneration.fetch_add(1);
    m_threadPool.clear();
}

bool CurlNetworkEngine::isGenerationCurrent(uint64_t generation) const {
    return m_cancelGeneration.load() == generation;
}

void CurlNetworkEngine::setSslVerifyPeer(bool verify) {
    QMutexLocker lock(&m_settingsMutex);
    m_sslVerifyPeer = verify;
}
bool CurlNetworkEngine::sslVerifyPeer() const {
    QMutexLocker lock(&m_settingsMutex);
    return m_sslVerifyPeer;
}

void CurlNetworkEngine::setTimeoutMs(long ms) {
    QMutexLocker lock(&m_settingsMutex);
    m_timeoutMs = ms;
}
long CurlNetworkEngine::timeoutMs() const {
    QMutexLocker lock(&m_settingsMutex);
    return m_timeoutMs;
}

void CurlNetworkEngine::setProxy(const QString& proxy) {
    QMutexLocker lock(&m_settingsMutex);
    m_proxy = proxy;
}
QString CurlNetworkEngine::proxy() const {
    QMutexLocker lock(&m_settingsMutex);
    return m_proxy;
}

void CurlNetworkEngine::setCookieJarEnabled(bool enabled) {
    QMutexLocker lock(&m_settingsMutex);
    m_cookieJarEnabled = enabled;
}
bool CurlNetworkEngine::cookieJarEnabled() const {
    QMutexLocker lock(&m_settingsMutex);
    return m_cookieJarEnabled;
}

void CurlNetworkEngine::setCookieJarPath(const QString& path) {
    QMutexLocker lock(&m_settingsMutex);
    m_cookieJarPath = path;
}
QString CurlNetworkEngine::cookieJarPath() const {
    QMutexLocker lock(&m_settingsMutex);
    return m_cookieJarPath;
}

void CurlNetworkEngine::setClientCertPath(const QString& path) {
    QMutexLocker lock(&m_settingsMutex);
    m_clientCertPath = path;
}
QString CurlNetworkEngine::clientCertPath() const {
    QMutexLocker lock(&m_settingsMutex);
    return m_clientCertPath;
}

void CurlNetworkEngine::setClientCertType(const QString& type) {
    QMutexLocker lock(&m_settingsMutex);
    m_clientCertType = type;
}
QString CurlNetworkEngine::clientCertType() const {
    QMutexLocker lock(&m_settingsMutex);
    return m_clientCertType;
}

void CurlNetworkEngine::setClientKeyPath(const QString& path) {
    QMutexLocker lock(&m_settingsMutex);
    m_clientKeyPath = path;
}
QString CurlNetworkEngine::clientKeyPath() const {
    QMutexLocker lock(&m_settingsMutex);
    return m_clientKeyPath;
}

void CurlNetworkEngine::setClientKeyPassword(const QString& pass) {
    QMutexLocker lock(&m_settingsMutex);
    m_clientKeyPassword = pass;
}
QString CurlNetworkEngine::clientKeyPassword() const {
    QMutexLocker lock(&m_settingsMutex);
    return m_clientKeyPassword;
}

CurlNetworkEngine::SettingsSnapshot CurlNetworkEngine::snapshotSettings() const {
    QMutexLocker lock(&m_settingsMutex);
    SettingsSnapshot snap;
    snap.sslVerifyPeer = m_sslVerifyPeer;
    snap.timeoutMs = m_timeoutMs;
    snap.proxy = m_proxy;
    snap.cookieJarEnabled = m_cookieJarEnabled;
    snap.cookieJarPath = m_cookieJarPath;
    snap.clientCertPath = m_clientCertPath;
    snap.clientCertType = m_clientCertType;
    snap.clientKeyPath = m_clientKeyPath;
    snap.clientKeyPassword = m_clientKeyPassword;
    return snap;
}

void CurlNetworkEngine::sendRequestAsync(
    const core::RequestModel& req,
    CompletionCallback onComplete,
    ProgressCallback /*onProgress*/
) {
    emit requestStarted();
    const uint64_t generation = m_cancelGeneration.load();
    auto* task = new RequestTask(this, req, generation, std::move(onComplete));
    m_threadPool.start(task);
}

core::ResponseModel CurlNetworkEngine::sendRequestSync(const core::RequestModel& req) {
    return executeCurl(req, m_cancelGeneration.load(), nullptr);
}

core::ResponseModel CurlNetworkEngine::executeCurl(const core::RequestModel& req, uint64_t generation, ProgressCallback /*onProgress*/) {
    core::ResponseModel response;
    const SettingsSnapshot settings = snapshotSettings();

    CURL* curl = curl_easy_init();
    if (!curl) {
        response.errorString = "Failed to initialize libcurl";
        return response;
    }

    QString effectiveUrl = req.effectiveUrl();
    if (effectiveUrl.isEmpty()) {
        response.errorString = "URL is empty";
        curl_easy_cleanup(curl);
        return response;
    }

    if (!effectiveUrl.startsWith("http://", Qt::CaseInsensitive) &&
        !effectiveUrl.startsWith("https://", Qt::CaseInsensitive)) {
        effectiveUrl = "http://" + effectiveUrl;
    }

    QByteArray urlBytes = effectiveUrl.toUtf8();
    curl_easy_setopt(curl, CURLOPT_URL, urlBytes.constData());

    QByteArray methodBytes = core::methodToString(req.method).toUtf8();
    if (req.method == core::HttpMethod::GET) {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    } else if (req.method == core::HttpMethod::POST) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
    } else if (req.method == core::HttpMethod::HEAD) {
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    } else {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, methodBytes.constData());
    }

    struct curl_slist* headerList = nullptr;
    QList<QByteArray> headerStorage;
    for (const auto& h : req.effectiveHeaders()) {
        if (!h.enabled || h.name.isEmpty()) continue;
        headerStorage.append((h.name + ": " + h.value).toUtf8());
        headerList = curl_slist_append(headerList, headerStorage.last().constData());
    }
    if (headerList) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    }

    curl_mime* mime = nullptr;
    QByteArray bodyBytes;
    QList<QByteArray> mimeStorage;
    const bool methodHasBody = req.method != core::HttpMethod::GET && req.method != core::HttpMethod::HEAD;
    if (methodHasBody && req.bodyType == core::BodyType::MultipartForm && !req.formDataParams.isEmpty()) {
        mime = curl_mime_init(curl);
        for (const auto& p : req.formDataParams) {
            if (!p.enabled || p.key.isEmpty()) continue;
            curl_mimepart* part = curl_mime_addpart(mime);
            mimeStorage.append(p.key.toUtf8());
            curl_mime_name(part, mimeStorage.last().constData());
            if (p.isFile) {
                mimeStorage.append(p.value.toUtf8());
                curl_mime_filedata(part, mimeStorage.last().constData());
            } else {
                mimeStorage.append(p.value.toUtf8());
                curl_mime_data(part, mimeStorage.last().constData(), CURL_ZERO_TERMINATED);
            }
        }
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    } else if (methodHasBody && req.bodyType == core::BodyType::Binary) {
        const QString path = req.bodyContent.trimmed();
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            response.errorString = QString("Binary body file could not be read: %1")
                .arg(path.isEmpty() ? QStringLiteral("(empty path)") : path);
            curl_easy_cleanup(curl);
            return response;
        }
        bodyBytes = file.readAll();
        if (!bodyBytes.isEmpty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, bodyBytes.constData());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(bodyBytes.size()));
        } else if (req.method == core::HttpMethod::POST || req.method == core::HttpMethod::PUT || req.method == core::HttpMethod::PATCH) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
        }
    } else if (methodHasBody) {
        bodyBytes = req.effectiveBody();
        if (!bodyBytes.isEmpty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, bodyBytes.constData());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(bodyBytes.size()));
        } else if (req.method == core::HttpMethod::POST || req.method == core::HttpMethod::PUT || req.method == core::HttpMethod::PATCH) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
        }
    }

    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.rawBody);

    CancelContext cancelCtx{&m_cancelGeneration, generation};
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferInfoCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &cancelCtx);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_CERTINFO, 1L);

    if (settings.sslVerifyPeer) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    } else {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, settings.timeoutMs);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 10000L);

    QString effectiveProxy = !req.proxy.trimmed().isEmpty() ? req.proxy.trimmed() : settings.proxy;
    QByteArray proxyBytes = effectiveProxy.toUtf8();
    if (!effectiveProxy.isEmpty()) {
        curl_easy_setopt(curl, CURLOPT_PROXY, proxyBytes.constData());
    }

    QByteArray cookiePathBytes;
    if (settings.cookieJarEnabled) {
        QString cpath = settings.cookieJarPath.isEmpty() ? (QDir::tempPath() + "/poppy_cookies.txt") : settings.cookieJarPath;
        cookiePathBytes = cpath.toUtf8();
        curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookiePathBytes.constData());
        curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookiePathBytes.constData());
        if (m_cookieShare) {
            curl_easy_setopt(curl, CURLOPT_SHARE, static_cast<CURLSH*>(m_cookieShare));
        }
    }

    QByteArray userPwdBytes;
    if (req.auth.type == core::AuthType::Digest) {
        curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_DIGEST);
        userPwdBytes = (req.auth.digestUsername + ":" + req.auth.digestPassword).toUtf8();
        curl_easy_setopt(curl, CURLOPT_USERPWD, userPwdBytes.constData());
    } else if (req.auth.type == core::AuthType::NTLM) {
        curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_NTLM);
        QString creds = req.auth.ntlmUsername;
        if (!req.auth.ntlmDomain.isEmpty()) {
            creds = req.auth.ntlmDomain + "\\" + creds;
        }
        creds += ":" + req.auth.ntlmPassword;
        userPwdBytes = creds.toUtf8();
        curl_easy_setopt(curl, CURLOPT_USERPWD, userPwdBytes.constData());
    }

    QByteArray certBytes = settings.clientCertPath.toUtf8();
    QByteArray certTypeBytes = settings.clientCertType.toUtf8();
    QByteArray keyBytes = settings.clientKeyPath.toUtf8();
    QByteArray passBytes = settings.clientKeyPassword.toUtf8();
    if (!settings.clientCertPath.isEmpty()) {
        curl_easy_setopt(curl, CURLOPT_SSLCERT, certBytes.constData());
        if (!settings.clientCertType.isEmpty()) {
            curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, certTypeBytes.constData());
        }
        if (!settings.clientKeyPath.isEmpty()) {
            curl_easy_setopt(curl, CURLOPT_SSLKEY, keyBytes.constData());
        }
        if (!settings.clientKeyPassword.isEmpty()) {
            curl_easy_setopt(curl, CURLOPT_KEYPASSWD, passBytes.constData());
        }
    }

    CURLcode resCode = curl_easy_perform(curl);

    if (m_cancelGeneration.load() != generation) {
        response.errorString = "Request cancelled";
        if (mime) curl_mime_free(mime);
        if (headerList) curl_slist_free_all(headerList);
        curl_easy_cleanup(curl);
        return response;
    }

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    response.statusCode = static_cast<int>(httpCode);

    double totalTime = 0;
    curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &totalTime);
    response.latencyMs = static_cast<qint64>(totalTime * 1000.0);

    double dnsTime = 0;
    curl_easy_getinfo(curl, CURLINFO_NAMELOOKUP_TIME, &dnsTime);
    response.dnsTimeMs = dnsTime * 1000.0;

    double connectTime = 0;
    curl_easy_getinfo(curl, CURLINFO_CONNECT_TIME, &connectTime);
    response.connectTimeMs = connectTime * 1000.0;

    double sslTime = 0;
    curl_easy_getinfo(curl, CURLINFO_APPCONNECT_TIME, &sslTime);
    response.sslHandshakeTimeMs = sslTime * 1000.0;

    double ttfbTime = 0;
    curl_easy_getinfo(curl, CURLINFO_STARTTRANSFER_TIME, &ttfbTime);
    response.ttfbMs = ttfbTime * 1000.0;

    curl_off_t downloadSize = 0;
    curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD_T, &downloadSize);
    response.sizeBytes = static_cast<qint64>(downloadSize);
    if (response.sizeBytes == 0) {
        response.sizeBytes = response.rawBody.size();
    }

    if (resCode != CURLE_OK) {
        response.errorString = QString::fromUtf8(curl_easy_strerror(resCode));
    } else {
        if (response.statusCode >= 200 && response.statusCode < 300) response.statusText = "OK";
        else if (response.statusCode == 301) response.statusText = "Moved Permanently";
        else if (response.statusCode == 302) response.statusText = "Found";
        else if (response.statusCode == 400) response.statusText = "Bad Request";
        else if (response.statusCode == 401) response.statusText = "Unauthorized";
        else if (response.statusCode == 403) response.statusText = "Forbidden";
        else if (response.statusCode == 404) response.statusText = "Not Found";
        else if (response.statusCode == 500) response.statusText = "Internal Server Error";
        else if (response.statusCode == 502) response.statusText = "Bad Gateway";
        else if (response.statusCode == 503) response.statusText = "Service Unavailable";
        else response.statusText = QString::number(response.statusCode);
    }

    long httpVer = 0;
    if (curl_easy_getinfo(curl, CURLINFO_HTTP_VERSION, &httpVer) == CURLE_OK) {
        if (httpVer == CURL_HTTP_VERSION_1_0) response.protocol = "HTTP/1.0";
        else if (httpVer == CURL_HTTP_VERSION_1_1) response.protocol = "HTTP/1.1";
        else if (httpVer == CURL_HTTP_VERSION_2_0) response.protocol = "HTTP/2";
        else if (httpVer == CURL_HTTP_VERSION_3) response.protocol = "HTTP/3";
        else response.protocol = "HTTP";
    }

    struct curl_certinfo* ci = nullptr;
    if (curl_easy_getinfo(curl, CURLINFO_CERTINFO, &ci) == CURLE_OK && ci) {
        for (int i = 0; i < ci->num_of_certs; ++i) {
            struct curl_slist* slist = ci->certinfo[i];
            QString certStr;
            for (struct curl_slist* curr = slist; curr; curr = curr->next) {
                if (curr->data) certStr += QString::fromUtf8(curr->data) + "\n";
            }
            if (!certStr.isEmpty()) {
                response.certDetails.append(certStr.trimmed());
            }
        }
    }

    if (mime) {
        curl_mime_free(mime);
    }
    if (headerList) {
        curl_slist_free_all(headerList);
    }
    curl_easy_cleanup(curl);

    return response;
}

void CurlNetworkEngine::clearCookies() {
    QString cpath = cookieJarPath();
    if (cpath.isEmpty()) {
        cpath = QDir::tempPath() + "/poppy_cookies.txt";
    }
    QFile::remove(cpath);
}

} // namespace poppy::network
