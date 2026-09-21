#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <iostream>
#include <core/CollectionModel.h>
#include <core/BruParser.h>
#include <core/VariableResolver.h>
#include <core/ScriptRunner.h>
#include <network/CurlNetworkEngine.h>

using namespace poppy;

void collectRequests(core::CollectionItem* item, QList<core::RequestModel>& list) {
    if (!item) return;
    if (item->type() == core::CollectionItemType::Request && item->request()) {
        list.append(*item->request());
    }
    for (auto* child : item->children()) {
        collectRequests(child, list);
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("poppy-cli");
    app.setApplicationVersion("1.0.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("Poppy Headless Collection Runner & Testing CLI");
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addPositionalArgument("command", "Command to execute (e.g. 'run')");
    parser.addPositionalArgument("collection", "Path to the collection directory or .bru file");

    QCommandLineOption envOption(QStringList() << "e" << "env", "Environment name to execute against", "env");
    parser.addOption(envOption);

    QCommandLineOption reporterOption(QStringList() << "r" << "reporter", "Output format: cli, json, or junit", "reporter", "cli");
    parser.addOption(reporterOption);

    QCommandLineOption outputOption(QStringList() << "o" << "output", "Save output report to file", "file");
    parser.addOption(outputOption);

    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if (args.isEmpty() || (args.size() > 0 && args.at(0) != "run")) {
        std::cout << "Usage: poppy run <path-to-collection> [--env <env-name>] [--reporter cli|json|junit]" << std::endl;
        return 1;
    }

    if (args.size() < 2) {
        std::cerr << "Error: Missing path to collection directory or .bru file." << std::endl;
        return 1;
    }

    QString collectionPath = args.at(1);
    QString envName = parser.value(envOption);
    QString reporter = parser.value(reporterOption).toLower();
    QString outputFile = parser.value(outputOption);

    core::CollectionModel collection;
    QList<core::RequestModel> requestsToRun;

    QFileInfo fi(collectionPath);
    if (!fi.exists()) {
        std::cerr << "Error: Path does not exist: " << collectionPath.toStdString() << std::endl;
        return 1;
    }

    if (fi.isDir()) {
        if (!collection.openDirectory(collectionPath)) {
            std::cerr << "Error: Could not open collection directory." << std::endl;
            return 1;
        }
        collectRequests(collection.rootItem(), requestsToRun);
    } else if (fi.isFile() && fi.suffix().toLower() == "bru") {
        core::RequestModel single = core::BruParser::parseFile(collectionPath);
        requestsToRun.append(single);
    }

    if (requestsToRun.isEmpty()) {
        std::cout << "No requests found in target path." << std::endl;
        return 0;
    }

    // Resolve environment
    core::EnvironmentModel activeEnv(envName);
    for (const auto& env : collection.environments()) {
        if (env.name().compare(envName, Qt::CaseInsensitive) == 0) {
            activeEnv = env;
            break;
        }
    }

    network::CurlNetworkEngine engine;
    core::ScriptRunner scriptRunner;
    core::VariableResolver resolver;
    resolver.setEnvironment(activeEnv);

    if (reporter == "cli") {
        std::cout << "\n=======================================================\n";
        std::cout << " Poppy Collection Runner\n";
        std::cout << " Target:      " << collectionPath.toStdString() << "\n";
        std::cout << " Environment: " << (envName.isEmpty() ? "None" : envName.toStdString()) << "\n";
        std::cout << " Requests:    " << requestsToRun.size() << "\n";
        std::cout << "=======================================================\n\n";
    }

    int totalRequests = requestsToRun.size();
    int passedRequests = 0;
    int totalTests = 0;
    int passedTests = 0;
    qint64 totalLatencyMs = 0;

    QJsonArray jsonResults;

    for (int i = 0; i < requestsToRun.size(); ++i) {
        core::RequestModel req = requestsToRun[i];
        core::RequestModel resolvedReq = resolver.resolveRequest(req);

        // Pre-request script
        QString preErr;
        scriptRunner.runPreRequestScript(resolvedReq.scripts.preRequestScript, resolvedReq, activeEnv, &preErr);

        if (reporter == "cli") {
            std::cout << "[" << (i + 1) << "/" << totalRequests << "] "
                      << core::methodToString(resolvedReq.method).toStdString() << " "
                      << resolvedReq.effectiveUrl().toStdString() << "\n";
        }

        // Execute request synchronously
        core::ResponseModel res = engine.sendRequestSync(resolvedReq);
        totalLatencyMs += res.latencyMs;

        // Post-response script
        QString postErr;
        scriptRunner.runPostResponseScript(resolvedReq.scripts.postResponseScript, resolvedReq, res, activeEnv, &postErr);

        // Tests
        core::TestReport report = scriptRunner.runTests(resolvedReq.scripts.tests, resolvedReq, res, activeEnv);
        totalTests += report.totalCount();
        passedTests += report.passedCount();

        bool requestPassed = res.isHttpSuccess() && (report.failedCount() == 0);
        if (requestPassed) ++passedRequests;

        if (reporter == "cli") {
            std::cout << "      Status: " << res.statusCode << " " << res.statusText.toStdString()
                      << " (" << res.latencyMs << " ms, " << res.sizeBytes << " B)\n";

            if (!res.errorString.isEmpty()) {
                std::cout << "      ERROR: " << res.errorString.toStdString() << "\n";
            }

            for (const auto& t : report.results) {
                if (t.passed) {
                    std::cout << "      [PASS] " << t.name.toStdString() << " (" << t.durationMs << " ms)\n";
                } else {
                    std::cout << "      [FAIL] " << t.name.toStdString() << " -> " << t.errorMessage.toStdString() << "\n";
                }
            }
            std::cout << "\n";
        }

        if (reporter == "json") {
            QJsonObject reqObj;
            reqObj["name"] = resolvedReq.name;
            reqObj["url"] = resolvedReq.effectiveUrl();
            reqObj["method"] = core::methodToString(resolvedReq.method);
            reqObj["statusCode"] = res.statusCode;
            reqObj["latencyMs"] = res.latencyMs;
            reqObj["sizeBytes"] = res.sizeBytes;
            reqObj["passed"] = requestPassed;

            QJsonArray testsArr;
            for (const auto& t : report.results) {
                QJsonObject tObj;
                tObj["name"] = t.name;
                tObj["passed"] = t.passed;
                tObj["errorMessage"] = t.errorMessage;
                tObj["durationMs"] = t.durationMs;
                testsArr.append(tObj);
            }
            reqObj["tests"] = testsArr;
            jsonResults.append(reqObj);
        }
    }

    if (reporter == "cli") {
        std::cout << "-------------------------------------------------------\n";
        std::cout << "Summary:\n";
        std::cout << "  Requests: " << passedRequests << " / " << totalRequests << " passed\n";
        std::cout << "  Tests:    " << passedTests << " / " << totalTests << " passed\n";
        std::cout << "  Latency:  " << totalLatencyMs << " ms total\n";
        std::cout << "-------------------------------------------------------\n\n";
    } else if (reporter == "json") {
        QJsonObject summary;
        summary["totalRequests"] = totalRequests;
        summary["passedRequests"] = passedRequests;
        summary["totalTests"] = totalTests;
        summary["passedTests"] = passedTests;
        summary["totalLatencyMs"] = totalLatencyMs;
        summary["results"] = jsonResults;

        QJsonDocument doc(summary);
        QString jsonStr = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
        if (!outputFile.isEmpty()) {
            QFile out(outputFile);
            if (out.open(QIODevice::WriteOnly | QIODevice::Text)) {
                out.write(jsonStr.toUtf8());
            }
        } else {
            std::cout << jsonStr.toStdString() << std::endl;
        }
    }

    bool allSuccess = (passedRequests == totalRequests) && (passedTests == totalTests);
    return allSuccess ? 0 : 1;
}
