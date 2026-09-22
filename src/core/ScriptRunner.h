#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QJSEngine>

namespace poppy::core {

class RequestModel;
class ResponseModel;
class EnvironmentModel;

struct TestCaseResult {
    QString name;
    bool passed{true};
    QString errorMessage;
    qint64 durationMs{0};
};

class TestReport {
public:
    QList<TestCaseResult> results;
    qint64 totalDurationMs{0};

    int totalCount() const { return results.size(); }
    int passedCount() const {
        int c = 0;
        for (const auto& r : results) if (r.passed) ++c;
        return c;
    }
    int failedCount() const { return totalCount() - passedCount(); }
};

class ScriptRunner : public QObject {
    Q_OBJECT
public:
    explicit ScriptRunner(QObject* parent = nullptr);

    // Pre-request script execution
    bool runPreRequestScript(const QString& script, RequestModel& req, EnvironmentModel& env, QString* outError = nullptr);

    // Post-response script execution
    bool runPostResponseScript(const QString& script, const RequestModel& req, const ResponseModel& res, EnvironmentModel& env, QString* outError = nullptr);

    // Test runner execution
    TestReport runTests(const QString& testScript, const RequestModel& req, const ResponseModel& res, EnvironmentModel& env);

private:
    void setupSandbox(QJSEngine& engine, EnvironmentModel& env, const RequestModel& req, const ResponseModel* res);
    void applyJsRequestMutations(QJSEngine& engine, RequestModel& req);
};

} // namespace poppy::core
