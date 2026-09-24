#include "ScriptRunner.h"
#include "RequestModel.h"
#include "ResponseModel.h"
#include "EnvironmentModel.h"
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QThread>
#include <atomic>
#include <thread>

namespace poppy::core {

class ScriptBridge : public QObject {
    Q_OBJECT
public:
    explicit ScriptBridge(EnvironmentModel* env, QJSEngine* engine, QObject* parent = nullptr)
        : QObject(parent), m_env(env), m_engine(engine) {}

    Q_INVOKABLE QString getEnvVar(const QString& name) const {
        return m_env ? m_env->variableValue(name) : QString{};
    }

    Q_INVOKABLE void setEnvVar(const QString& name, const QJSValue& value) {
        if (!m_env) return;
        m_env->setVariableValue(name, jsValueToString(value));
    }

    Q_INVOKABLE QString getVar(const QString& name) const {
        if (m_vars.contains(name)) return m_vars.value(name);
        return m_env ? m_env->variableValue(name) : QString{};
    }

    Q_INVOKABLE void setVar(const QString& name, const QJSValue& value) {
        QString s = jsValueToString(value);
        m_vars.insert(name, s);
        if (m_env) {
            m_env->setVariableValue(name, s);
        }
    }

    Q_INVOKABLE QString getCollectionVar(const QString& name) const {
        return getVar(name);
    }

    Q_INVOKABLE void setCollectionVar(const QString& name, const QJSValue& value) {
        setVar(name, value);
    }

    Q_INVOKABLE void setNextRequest(const QString& requestName) {
        m_nextRequest = requestName;
    }

    Q_INVOKABLE QString getNextRequest() const {
        return m_nextRequest;
    }

    Q_INVOKABLE void sleep(int ms) const {
        if (ms > 0 && ms <= 10000) {
            QThread::msleep(static_cast<unsigned long>(ms));
        }
    }

private:
    QString jsValueToString(const QJSValue& value) const {
        if (value.isString()) return value.toString();
        if (value.isBool()) return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        if (value.isNumber()) return QString::number(value.toNumber());
        if (value.isNull() || value.isUndefined()) return QString();
        if (value.isObject() || value.isArray()) {
            if (m_engine) {
                QJSValue json = m_engine->globalObject().property("JSON");
                QJSValue stringify = json.property("stringify");
                if (stringify.isCallable()) {
                    return stringify.call({value}).toString();
                }
            }
        }
        return value.toString();
    }

    EnvironmentModel* m_env;
    QJSEngine* m_engine{nullptr};
    QMap<QString, QString> m_vars;
    QString m_nextRequest;
};

ScriptRunner::ScriptRunner(QObject* parent) : QObject(parent) {}

namespace {

QJSValue evaluateWithTimeout(QJSEngine& engine, const QString& script, int timeoutMs, QString* outError) {
    std::atomic<bool> finished{false};
    std::thread watchdog([&engine, &finished, timeoutMs]() {
        const int sliceMs = 50;
        int waited = 0;
        while (waited < timeoutMs) {
            if (finished.load()) return;
            QThread::msleep(static_cast<unsigned long>(sliceMs));
            waited += sliceMs;
        }
        if (!finished.load()) {
            engine.setInterrupted(true);
        }
    });

    QJSValue result = engine.evaluate(script);
    finished.store(true);
    watchdog.join();
    const bool timedOut = engine.isInterrupted()
        || (result.isError() && result.toString().contains(QLatin1String("interrupted"), Qt::CaseInsensitive));
    engine.setInterrupted(false);

    if (timedOut) {
        if (outError) {
            *outError = QStringLiteral("Script timed out after %1 ms").arg(timeoutMs);
        }
        return QJSValue();
    }
    return result;
}

QJSValue callWithTimeout(QJSEngine& engine, const QJSValue& fn, int timeoutMs, QString* outError) {
    std::atomic<bool> finished{false};
    std::thread watchdog([&engine, &finished, timeoutMs]() {
        const int sliceMs = 50;
        int waited = 0;
        while (waited < timeoutMs) {
            if (finished.load()) return;
            QThread::msleep(static_cast<unsigned long>(sliceMs));
            waited += sliceMs;
        }
        if (!finished.load()) {
            engine.setInterrupted(true);
        }
    });

    QJSValue result = fn.call();
    finished.store(true);
    watchdog.join();
    const bool timedOut = engine.isInterrupted()
        || (result.isError() && result.toString().contains(QLatin1String("interrupted"), Qt::CaseInsensitive));
    engine.setInterrupted(false);

    if (timedOut) {
        if (outError) {
            *outError = QStringLiteral("Script timed out after %1 ms").arg(timeoutMs);
        }
        return QJSValue();
    }
    return result;
}

constexpr int kScriptTimeoutMs = 8000;

} // namespace

void ScriptRunner::setupSandbox(QJSEngine& engine, EnvironmentModel& env, const RequestModel& req, const ResponseModel* res) {
    // 1. ScriptBridge for poppy and bru objects
    auto* bridge = new ScriptBridge(&env, &engine, &engine);
    QJSValue bridgeVal = engine.newQObject(bridge);
    engine.globalObject().setProperty("poppy", bridgeVal);
    engine.globalObject().setProperty("bru", bridgeVal);

    // 1b. Postman pm compatibility shim
    QString pmShim = R"(
        (function(b) {
            globalThis.pm = {
                environment: {
                    set: function(k, v) { b.setEnvVar(k, v); },
                    get: function(k) { return b.getEnvVar(k); }
                },
                variables: {
                    set: function(k, v) { b.setVar(k, v); },
                    get: function(k) { return b.getVar(k); }
                },
                collectionVariables: {
                    set: function(k, v) { b.setCollectionVar(k, v); },
                    get: function(k) { return b.getCollectionVar(k); }
                }
            };
        })(bru);
    )";
    engine.evaluate(pmShim);

    // 2. Request object
    QJSValue reqObj = engine.newObject();
    reqObj.setProperty("url", req.url);
    reqObj.setProperty("method", methodToString(req.method));
    reqObj.setProperty("body", req.bodyContent);
    engine.globalObject().setProperty("req", reqObj);

    // 3. Response object (if present)
    if (res) {
        QJSValue resObj = engine.newObject();
        resObj.setProperty("status", res->statusCode);
        resObj.setProperty("statusText", res->statusText);
        resObj.setProperty("responseTime", static_cast<double>(res->latencyMs));
        resObj.setProperty("bodyRaw", res->bodyAsString());

        QJSValue headerObj = engine.newObject();
        for (const auto& h : res->headers) {
            if (!h.enabled || h.name.isEmpty()) continue;
            headerObj.setProperty(h.name, h.value);
        }
        resObj.setProperty("headers", headerObj);

        // Parse JSON body into JS object if JSON
        if (res->isJson()) {
            QJsonDocument doc = QJsonDocument::fromJson(res->rawBody);
            if (doc.isObject()) {
                resObj.setProperty("body", engine.toScriptValue(doc.object().toVariantMap()));
            } else if (doc.isArray()) {
                resObj.setProperty("body", engine.toScriptValue(doc.array().toVariantList()));
            } else {
                resObj.setProperty("body", res->bodyAsString());
            }
        } else {
            resObj.setProperty("body", res->bodyAsString());
        }

        // Methods: getStatus(), getBody(), getHeader(name)
        QString resHelperScript = R"(
            (function(res) {
                res.getStatus = function() { return res.status; };
                res.getBody = function() { return res.body; };
                res.getResponseTime = function() { return res.responseTime; };
                res.getHeader = function(name) {
                    if (!name) return "";
                    var want = String(name).toLowerCase();
                    var headers = res.headers || {};
                    for (var key in headers) {
                        if (String(key).toLowerCase() === want) return headers[key];
                    }
                    return "";
                };
            })
        )";
        QJSValue resHelper = engine.evaluate(resHelperScript);
        if (resHelper.isCallable()) {
            resHelper.call({resObj});
        }

        engine.globalObject().setProperty("res", resObj);
    }

    // 4. Assertions and test() harness
    QString testHarness = R"(
        var __testCases = [];
        function test(name, fn) {
            __testCases.push({ name: name, fn: fn });
        }

        function expect(actual) {
            return {
                to: {
                    equal: function(expected) {
                        if (actual !== expected) {
                            throw new Error("Expected " + JSON.stringify(actual) + " to equal " + JSON.stringify(expected));
                        }
                    },
                    not: {
                        equal: function(expected) {
                            if (actual === expected) {
                                throw new Error("Expected " + JSON.stringify(actual) + " not to equal " + JSON.stringify(expected));
                            }
                        }
                    },
                    be: {
                        a: function(t) {
                            if (typeof actual !== t) {
                                throw new Error("Expected " + typeof actual + " to be " + t);
                            }
                        },
                        true: function() {
                            if (actual !== true) throw new Error("Expected " + actual + " to be true");
                        },
                        false: function() {
                            if (actual !== false) throw new Error("Expected " + actual + " to be false");
                        },
                        null: function() {
                            if (actual !== null) throw new Error("Expected " + actual + " to be null");
                        },
                        above: function(n) {
                            if (!(actual > n)) throw new Error("Expected " + actual + " to be greater than " + n);
                        },
                        below: function(n) {
                            if (!(actual < n)) throw new Error("Expected " + actual + " to be less than " + n);
                        }
                    },
                    have: {
                        property: function(prop) {
                            if (!actual || typeof actual !== 'object' || !(prop in actual)) {
                                throw new Error("Expected object to have property '" + prop + "'");
                            }
                        }
                    }
                }
            };
        }
    )";
    engine.evaluate(testHarness);
}

void ScriptRunner::applyJsRequestMutations(QJSEngine& engine, RequestModel& req) {
    QJSValue reqObj = engine.globalObject().property("req");
    if (!reqObj.isObject()) return;

    QJSValue urlVal = reqObj.property("url");
    if (!urlVal.isUndefined() && !urlVal.isNull()) {
        req.url = urlVal.toString();
    }
    QJSValue methodVal = reqObj.property("method");
    if (!methodVal.isUndefined() && !methodVal.isNull() && !methodVal.toString().isEmpty()) {
        req.method = stringToMethod(methodVal.toString());
    }
    QJSValue bodyVal = reqObj.property("body");
    if (!bodyVal.isUndefined() && !bodyVal.isNull()) {
        req.bodyContent = bodyVal.toString();
    }
}

namespace {

struct EnvRollback {
    EnvironmentModel& env;
    QList<EnvironmentVariable> snapshot;
    bool rollback{true};
    ~EnvRollback() {
        if (rollback) env.variables() = snapshot;
    }
    void keep() { rollback = false; }
};

} // namespace

bool ScriptRunner::runPreRequestScript(const QString& script, RequestModel& req, EnvironmentModel& env, QString* outError) {
    if (script.trimmed().isEmpty()) return true;

    EnvRollback guard{env, env.variables()};
    QJSEngine engine;
    setupSandbox(engine, env, req, nullptr);

    QJSValue result = evaluateWithTimeout(engine, script, kScriptTimeoutMs, outError);
    if (outError && !outError->isEmpty()) {
        return false;
    }
    if (result.isError()) {
        if (outError) {
            *outError = QString("Line %1: %2").arg(result.property("lineNumber").toInt()).arg(result.toString());
        }
        return false;
    }
    applyJsRequestMutations(engine, req);
    guard.keep();
    return true;
}

bool ScriptRunner::runPostResponseScript(const QString& script, const RequestModel& req, const ResponseModel& res, EnvironmentModel& env, QString* outError) {
    if (script.trimmed().isEmpty()) return true;

    EnvRollback guard{env, env.variables()};
    QJSEngine engine;
    setupSandbox(engine, env, req, &res);

    QJSValue result = evaluateWithTimeout(engine, script, kScriptTimeoutMs, outError);
    if (outError && !outError->isEmpty()) {
        return false;
    }
    if (result.isError()) {
        if (outError) {
            *outError = QString("Line %1: %2").arg(result.property("lineNumber").toInt()).arg(result.toString());
        }
        return false;
    }
    guard.keep();
    return true;
}

TestReport ScriptRunner::runTests(const QString& testScript, const RequestModel& req, const ResponseModel& res, EnvironmentModel& env) {
    TestReport report;
    if (testScript.trimmed().isEmpty()) return report;

    EnvRollback guard{env, env.variables()};
    QJSEngine engine;
    setupSandbox(engine, env, req, &res);

    QElapsedTimer totalTimer;
    totalTimer.start();

    // 1. Evaluate tests script to register all test() callbacks
    QString timeoutErr;
    QJSValue evalResult = evaluateWithTimeout(engine, testScript, kScriptTimeoutMs, &timeoutErr);
    if (!timeoutErr.isEmpty()) {
        TestCaseResult timeoutResult;
        timeoutResult.name = "Test Script Timeout";
        timeoutResult.passed = false;
        timeoutResult.errorMessage = timeoutErr;
        report.results.append(timeoutResult);
        report.totalDurationMs = totalTimer.elapsed();
        return report;
    }
    if (evalResult.isError()) {
        TestCaseResult syntaxErr;
        syntaxErr.name = "Test Script Compilation";
        syntaxErr.passed = false;
        syntaxErr.errorMessage = QString("Line %1: %2").arg(evalResult.property("lineNumber").toInt()).arg(evalResult.toString());
        report.results.append(syntaxErr);
        report.totalDurationMs = totalTimer.elapsed();
        return report;
    }

    // 2. Extract and run test cases
    QJSValue testCases = engine.globalObject().property("__testCases");
    int length = testCases.property("length").toInt();

    for (int i = 0; i < length; ++i) {
        QJSValue item = testCases.property(i);
        QString testName = item.property("name").toString();
        QJSValue fn = item.property("fn");

        TestCaseResult caseResult;
        caseResult.name = testName;

        QElapsedTimer caseTimer;
        caseTimer.start();

        QString callErr;
        if (fn.isCallable()) {
            QJSValue callRes = callWithTimeout(engine, fn, kScriptTimeoutMs, &callErr);
            if (!callErr.isEmpty()) {
                caseResult.passed = false;
                caseResult.errorMessage = callErr;
            } else if (callRes.isError()) {
                caseResult.passed = false;
                caseResult.errorMessage = callRes.toString();
            } else {
                caseResult.passed = true;
            }
        } else {
            caseResult.passed = false;
            caseResult.errorMessage = "Test argument is not a function";
        }

        caseResult.durationMs = caseTimer.elapsed();
        report.results.append(caseResult);
        if (!callErr.isEmpty() && callErr.contains(QLatin1String("timed out"))) {
            report.totalDurationMs = totalTimer.elapsed();
            return report;
        }
    }
    guard.keep();

    report.totalDurationMs = totalTimer.elapsed();
    return report;
}

} // namespace poppy::core

#include "ScriptRunner.moc"
