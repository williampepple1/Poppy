#include "CurlNetworkEngine.h"
#include <curl/curl.h>
#include <QRunnable>
#include <QPointer>
#include <QDebug>
#include <mutex>

namespace poppy::network {

namespace {
    std::once_flag curlInitOnce;
    void ensureCurlInitialized() {
        std::call_once(curlInitOnce, []() {
            curl_global_init(CURL_GLOBAL_DEFAULT);
        });
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
}

class RequestTask : public QRunnable {
public:
    RequestTask(CurlNetworkEngine* engine,
                const core::RequestModel& req,
                CompletionCallback onComplete,
                ProgressCallback onProgress)
        : m_engine(engine), m_req(req), m_onComplete(std::move(onComplete)), m_onProgress(std::move(onProgress)) {
        setAutoDelete(true);
    }

    void run() override {
        if (!m_engine) return;
        core::ResponseModel res = m_engine->sendRequestSync(m_req);

        // Deliver result back on the engine's Qt thread
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
    CompletionCallback m_onComplete;
    ProgressCallback m_onProgress;
};

CurlNetworkEngine::CurlNetworkEngine(QObject* parent) : QObject(parent) {
    ensureCurlInitialized();
    m_threadPool.setMaxThreadCount(8);
}

CurlNetworkEngine::~CurlNetworkEngine() {
    cancelAll();
    m_threadPool.waitForDone();
}

void CurlNetworkEngine::cancelAll() {
    m_cancelled = true;
    m_threadPool.clear();
}

void CurlNetworkEngine::sendRequestAsync(
    const core::RequestModel& req,
    CompletionCallback onComplete,
    ProgressCallback onProgress
) {
    m_cancelled = false;
    emit requestStarted();
    auto* task = new RequestTask(this, req, std::move(onComplete), std::move(onProgress));
    m_threadPool.start(task);
}

core::ResponseModel CurlNetworkEngine::sendRequestSync(const core::RequestModel& req) {
    return executeCurl(req, nullptr);
}

core::ResponseModel CurlNetworkEngine::executeCurl(const core::RequestModel& req, ProgressCallback /*onProgress*/) {
    core::ResponseModel response;

    CURL* curl = curl_easy_init();
    if (!curl) {
        response.errorString = "Failed to initialize libcurl";
        return response;
    }

    // 1. URL
    QString effectiveUrl = req.effectiveUrl();
    if (effectiveUrl.isEmpty()) {
        response.errorString = "URL is empty";
        curl_easy_cleanup(curl);
        return response;
    }

    // Default to http:// if scheme missing
    if (!effectiveUrl.startsWith("http://", Qt::CaseInsensitive) && 
        !effectiveUrl.startsWith("https://", Qt::CaseInsensitive)) {
        effectiveUrl = "http://" + effectiveUrl;
    }

    QByteArray urlBytes = effectiveUrl.toUtf8();
    curl_easy_setopt(curl, CURLOPT_URL, urlBytes.constData());

    // 2. HTTP Method
    QString methodStr = core::methodToString(req.method);
    if (req.method == core::HttpMethod::GET) {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    } else if (req.method == core::HttpMethod::POST) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
    } else if (req.method == core::HttpMethod::HEAD) {
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    } else {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, methodStr.toUtf8().constData());
    }

    // 3. Headers
    struct curl_slist* headerList = nullptr;
    for (const auto& h : req.effectiveHeaders()) {
        if (!h.enabled || h.name.isEmpty()) continue;
        QByteArray headerLine = (h.name + ": " + h.value).toUtf8();
        headerList = curl_slist_append(headerList, headerLine.constData());
    }
    if (headerList) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    }

    // 4. Request Body
    QByteArray bodyBytes;
    if (req.bodyType != core::BodyType::None && !req.bodyContent.isEmpty()) {
        bodyBytes = req.bodyContent.toUtf8();
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, bodyBytes.constData());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(bodyBytes.size()));
    } else if (req.method == core::HttpMethod::POST || req.method == core::HttpMethod::PUT || req.method == core::HttpMethod::PATCH) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
    }

    // 5. Callbacks for response
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.rawBody);

    // 6. Redirects & SSL
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);

    if (m_sslVerifyPeer) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    } else {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    // 7. Timeouts & Proxy
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, m_timeoutMs);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 10000L);

    if (!m_proxy.isEmpty()) {
        curl_easy_setopt(curl, CURLOPT_PROXY, m_proxy.toUtf8().constData());
    }

    // 8. Execute request
    CURLcode resCode = curl_easy_perform(curl);

    // 9. Collect Telemetry
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

    // Cleanup
    if (headerList) {
        curl_slist_free_all(headerList);
    }
    curl_easy_cleanup(curl);

    return response;
}

} // namespace poppy::network

#include "CurlNetworkEngine.moc"
