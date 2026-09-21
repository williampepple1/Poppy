#include "CollectionRunnerDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <core/VariableResolver.h>
#include <core/assertions/DeclarativeAssertion.h>
#include <Theme.h>

namespace poppy::gui {

CollectionRunnerDialog::CollectionRunnerDialog(
    core::CollectionModel* model,
    network::CurlNetworkEngine* engine,
    core::ScriptRunner* scriptRunner,
    const QString& activeEnvName,
    QWidget* parent
) : QDialog(parent), m_model(model), m_networkEngine(engine), m_scriptRunner(scriptRunner) {
    setWindowTitle("Collection Test Runner");
    resize(850, 600);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // Config Grid
    auto* configGrid = new QGridLayout();
    configGrid->setSpacing(10);

    configGrid->addWidget(new QLabel("Run Target:", this), 0, 0);
    m_folderCombo = new QComboBox(this);
    if (m_model->rootItem()) {
        m_folderCombo->addItem("Entire Collection (" + m_model->rootItem()->name() + ")", "");
        for (auto* child : m_model->rootItem()->children()) {
            if (child->type() == core::CollectionItemType::Folder) {
                m_folderCombo->addItem("📁 " + child->name(), child->path());
            }
        }
    }
    configGrid->addWidget(m_folderCombo, 0, 1);

    configGrid->addWidget(new QLabel("Environment:", this), 0, 2);
    m_envCombo = new QComboBox(this);
    m_envCombo->addItem("No Environment", "");
    for (const auto& env : m_model->environments()) {
        m_envCombo->addItem(env.name(), env.name());
    }
    int envIdx = m_envCombo->findData(activeEnvName);
    if (envIdx >= 0) m_envCombo->setCurrentIndex(envIdx);
    configGrid->addWidget(m_envCombo, 0, 3);

    configGrid->addWidget(new QLabel("Iterations:", this), 1, 0);
    m_iterationsSpin = new QSpinBox(this);
    m_iterationsSpin->setRange(1, 100);
    m_iterationsSpin->setValue(1);
    configGrid->addWidget(m_iterationsSpin, 1, 1);

    configGrid->addWidget(new QLabel("Delay (ms):", this), 1, 2);
    m_delaySpin = new QSpinBox(this);
    m_delaySpin->setRange(0, 10000);
    m_delaySpin->setValue(0);
    m_delaySpin->setSingleStep(100);
    configGrid->addWidget(m_delaySpin, 1, 3);

    m_stopOnFailureChk = new QCheckBox("Stop run on first failure", this);
    configGrid->addWidget(m_stopOnFailureChk, 2, 0, 1, 2);

    mainLayout->addLayout(configGrid);

    // Action Buttons & Progress Bar
    auto* actionLayout = new QHBoxLayout();
    m_startBtn = new QPushButton("Start Run", this);
    m_startBtn->setObjectName("primaryBtn");
    connect(m_startBtn, &QPushButton::clicked, this, &CollectionRunnerDialog::startRun);
    actionLayout->addWidget(m_startBtn);

    m_stopBtn = new QPushButton("Stop", this);
    m_stopBtn->setEnabled(false);
    connect(m_stopBtn, &QPushButton::clicked, this, &CollectionRunnerDialog::stopRun);
    actionLayout->addWidget(m_stopBtn);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    actionLayout->addWidget(m_progressBar, 1);

    mainLayout->addLayout(actionLayout);

    // Results Table
    m_resultsTable = new QTableWidget(this);
    m_resultsTable->setColumnCount(6);
    m_resultsTable->setHorizontalHeaderLabels({"#", "Method", "Request Name", "Status", "Latency", "Tests"});
    m_resultsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_resultsTable->setColumnWidth(0, 40);
    m_resultsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_resultsTable->setColumnWidth(1, 80);
    m_resultsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_resultsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    m_resultsTable->setColumnWidth(3, 130);
    m_resultsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    m_resultsTable->setColumnWidth(4, 80);
    m_resultsTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Interactive);
    m_resultsTable->setColumnWidth(5, 120);
    m_resultsTable->verticalHeader()->setVisible(false);
    m_resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mainLayout->addWidget(m_resultsTable);

    // Summary Banner
    m_summaryLabel = new QLabel("Ready to run.", this);
    m_summaryLabel->setStyleSheet("font-weight: bold; color: #a1a1aa; padding: 4px;");
    mainLayout->addWidget(m_summaryLabel);

    auto* bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();
    m_closeBtn = new QPushButton("Close", this);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    bottomLayout->addWidget(m_closeBtn);
    mainLayout->addLayout(bottomLayout);
}

void CollectionRunnerDialog::collectRequests(core::CollectionItem* item, QList<core::RequestModel>& list) {
    if (!item) return;
    if (item->type() == core::CollectionItemType::Request && item->request()) {
        list.append(*item->request());
    }
    for (auto* child : item->children()) {
        collectRequests(child, list);
    }
}

void CollectionRunnerDialog::startRun() {
    m_queue.clear();
    m_resultsTable->setRowCount(0);

    // Determine target
    QString targetPath = m_folderCombo->currentData().toString();
    core::CollectionItem* targetItem = m_model->rootItem();
    if (!targetPath.isEmpty()) {
        for (auto* child : m_model->rootItem()->children()) {
            if (child->path() == targetPath) {
                targetItem = child;
                break;
            }
        }
    }

    QList<core::RequestModel> baseRequests;
    collectRequests(targetItem, baseRequests);

    if (baseRequests.isEmpty()) {
        QMessageBox::information(this, "Empty Target", "No requests found in the selected target.");
        return;
    }

    // Multiply by iterations
    int iters = m_iterationsSpin->value();
    for (int i = 0; i < iters; ++i) {
        m_queue.append(baseRequests);
    }

    // Active environment
    QString envName = m_envCombo->currentData().toString();
    m_activeEnv = core::EnvironmentModel(envName);
    for (const auto& env : m_model->environments()) {
        if (env.name() == envName) {
            m_activeEnv = env;
            break;
        }
    }

    m_currentIndex = 0;
    m_passedRequests = 0;
    m_totalTests = 0;
    m_passedTests = 0;
    m_totalDurationMs = 0;
    m_isRunning = true;

    m_progressBar->setMaximum(m_queue.size());
    m_progressBar->setValue(0);

    m_startBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_closeBtn->setEnabled(false);
    m_summaryLabel->setText(QString("Running 1 of %1...").arg(m_queue.size()));

    executeNextRequest();
}

void CollectionRunnerDialog::stopRun() {
    m_isRunning = false;
    m_stopBtn->setEnabled(false);
    m_startBtn->setEnabled(true);
    m_closeBtn->setEnabled(true);
    m_summaryLabel->setText("Run cancelled by user.");
}

void CollectionRunnerDialog::executeNextRequest() {
    if (!m_isRunning || m_currentIndex >= m_queue.size()) {
        m_isRunning = false;
        m_startBtn->setEnabled(true);
        m_stopBtn->setEnabled(false);
        m_closeBtn->setEnabled(true);

        QString statusColor = (m_passedRequests == m_queue.size()) ? "#10b981" : "#ef4444";
        m_summaryLabel->setStyleSheet(QString("font-weight: bold; color: %1; padding: 4px;").arg(statusColor));
        m_summaryLabel->setText(QString("Completed: %1 / %2 requests passed, %3 / %4 tests passed (Total Time: %5 ms)")
            .arg(m_passedRequests).arg(m_queue.size()).arg(m_passedTests).arg(m_totalTests).arg(m_totalDurationMs));
        return;
    }

    int row = m_resultsTable->rowCount();
    m_resultsTable->insertRow(row);

    core::RequestModel req = m_queue[m_currentIndex];

    // Variable resolution
    core::VariableResolver resolver;
    resolver.setEnvironment(m_activeEnv);
    core::RequestModel resolvedReq = resolver.resolveRequest(req);

    // Pre-request script
    QString preErr;
    m_scriptRunner->runPreRequestScript(resolvedReq.scripts.preRequestScript, resolvedReq, m_activeEnv, &preErr);

    // Populate initial row info
    m_resultsTable->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
    
    auto* methodItem = new QTableWidgetItem(core::methodToString(resolvedReq.method));
    methodItem->setForeground(Theme::methodColor(resolvedReq.method));
    methodItem->setFont(QFont(methodItem->font().family(), -1, QFont::Bold));
    m_resultsTable->setItem(row, 1, methodItem);

    m_resultsTable->setItem(row, 2, new QTableWidgetItem(resolvedReq.name));
    m_resultsTable->setItem(row, 3, new QTableWidgetItem("Running..."));
    m_resultsTable->scrollToBottom();

    m_networkEngine->sendRequestAsync(resolvedReq, [this, row, resolvedReq](const core::ResponseModel& res) {
        if (!m_isRunning) return;

        m_totalDurationMs += res.latencyMs;

        // Post-response script
        QString postErr;
        m_scriptRunner->runPostResponseScript(resolvedReq.scripts.postResponseScript, resolvedReq, res, m_activeEnv, &postErr);

        // Run JavaScript tests
        core::TestReport report = m_scriptRunner->runTests(resolvedReq.scripts.tests, resolvedReq, res, m_activeEnv);

        // Run Declarative Assertions
        auto declResults = core::DeclarativeAssertionEvaluator::evaluateAll(resolvedReq.assertions, res);
        for (const auto& dr : declResults) {
            report.results.append(dr);
        }

        m_totalTests += report.totalCount();
        m_passedTests += report.passedCount();

        bool success = res.isHttpSuccess() && (report.failedCount() == 0);
        if (success) ++m_passedRequests;

        // Status code cell
        auto* statusItem = new QTableWidgetItem(QString("%1 %2").arg(res.statusCode).arg(res.statusText));
        statusItem->setForeground(Theme::statusColor(res.statusCode));
        statusItem->setFont(QFont(statusItem->font().family(), -1, QFont::Bold));
        m_resultsTable->setItem(row, 3, statusItem);

        // Latency cell
        m_resultsTable->setItem(row, 4, new QTableWidgetItem(QString("%1 ms").arg(res.latencyMs)));

        // Tests cell
        QString testsSummary = QString("%1 / %2").arg(report.passedCount()).arg(report.totalCount());
        auto* testsItem = new QTableWidgetItem(testsSummary);
        testsItem->setForeground((report.failedCount() == 0) ? QColor("#10b981") : QColor("#ef4444"));
        m_resultsTable->setItem(row, 5, testsItem);

        ++m_currentIndex;
        m_progressBar->setValue(m_currentIndex);

        if (!success && m_stopOnFailureChk->isChecked()) {
            stopRun();
            m_summaryLabel->setText(QString("Run stopped on failure at request #%1 (%2)").arg(row + 1).arg(resolvedReq.name));
            return;
        }

        int delay = m_delaySpin->value();
        if (delay > 0) {
            QTimer::singleShot(delay, this, &CollectionRunnerDialog::executeNextRequest);
        } else {
            executeNextRequest();
        }
    });
}

} // namespace poppy::gui
