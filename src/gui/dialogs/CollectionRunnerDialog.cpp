#include "CollectionRunnerDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QLineEdit>
#include <QLabel>
#include <QFileDialog>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QPointer>
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

    configGrid->addWidget(new QLabel("Data File (CSV/JSON):", this), 2, 0);
    auto* dataFileLayout = new QHBoxLayout();
    m_dataFileEdit = new QLineEdit(this);
    m_dataFileEdit->setPlaceholderText("Select CSV or JSON data fixture...");
    connect(m_dataFileEdit, &QLineEdit::textChanged, this, &CollectionRunnerDialog::loadDataFile);
    dataFileLayout->addWidget(m_dataFileEdit, 1);

    m_browseDataBtn = new QPushButton("Browse...", this);
    connect(m_browseDataBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, "Select Data File", QString(), "Data Files (*.csv *.json);;CSV Files (*.csv);;JSON Files (*.json);;All Files (*.*)");
        if (!path.isEmpty()) {
            m_dataFileEdit->setText(path);
        }
    });
    dataFileLayout->addWidget(m_browseDataBtn);
    configGrid->addLayout(dataFileLayout, 2, 1, 1, 3);

    m_stopOnFailureChk = new QCheckBox("Stop run on first failure", this);
    configGrid->addWidget(m_stopOnFailureChk, 3, 0, 1, 2);

    m_dataStatusLabel = new QLabel("No data file loaded", this);
    m_dataStatusLabel->setStyleSheet("color: #71717a; font-size: 11px;");
    configGrid->addWidget(m_dataStatusLabel, 3, 2, 1, 2);

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

void CollectionRunnerDialog::collectRequests(core::CollectionItem* item, QList<QueuedRequest>& list) {
    if (!item) return;
    if (item->type() == core::CollectionItemType::Request && item->request()) {
        list.append(QueuedRequest{*item->request(), item->effectiveVariables()});
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

    QList<QueuedRequest> baseRequests;
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
    ++m_runGeneration;

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
    ++m_runGeneration;
    if (m_networkEngine) {
        m_networkEngine->cancelAll();
    }
    m_stopBtn->setEnabled(false);
    m_startBtn->setEnabled(true);
    m_closeBtn->setEnabled(true);
    m_summaryLabel->setText("Run cancelled by user.");
}

void CollectionRunnerDialog::loadDataFile(const QString& filePath) {
    m_dataRows.clear();
    QString path = filePath.trimmed();
    if (path.isEmpty()) {
        m_dataStatusLabel->setText("No data file loaded");
        m_dataStatusLabel->setStyleSheet("color: #71717a; font-size: 11px;");
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_dataStatusLabel->setText("Failed to open file: " + file.errorString());
        m_dataStatusLabel->setStyleSheet("color: #ef4444; font-size: 11px;");
        return;
    }

    QByteArray content = file.readAll();
    file.close();

    // Try parsing as JSON array
    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(content, &parseErr);
    if (parseErr.error == QJsonParseError::NoError && doc.isArray()) {
        QJsonArray arr = doc.array();
        for (const auto& val : arr) {
            if (val.isObject()) {
                QJsonObject obj = val.toObject();
                QMap<QString, QString> row;
                for (auto it = obj.begin(); it != obj.end(); ++it) {
                    if (it.value().isArray()) {
                        row[it.key()] = QString::fromUtf8(QJsonDocument(it.value().toArray()).toJson(QJsonDocument::Compact));
                    } else if (it.value().isObject()) {
                        row[it.key()] = QString::fromUtf8(QJsonDocument(it.value().toObject()).toJson(QJsonDocument::Compact));
                    } else if (it.value().isBool()) {
                        row[it.key()] = it.value().toBool() ? QStringLiteral("true") : QStringLiteral("false");
                    } else {
                        row[it.key()] = it.value().toVariant().toString();
                    }
                }
                if (!row.isEmpty()) {
                    m_dataRows.append(row);
                }
            }
        }
    } else {
        // Fallback: Parse CSV
        QString text = QString::fromUtf8(content);
        QStringList lines = text.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
        if (!lines.isEmpty()) {
            QStringList headers = lines.first().split(',');
            for (auto& h : headers) h = h.trimmed().remove('\"');

            for (int i = 1; i < lines.size(); ++i) {
                QString line = lines[i].trimmed();
                if (line.isEmpty()) continue;
                QStringList cols = line.split(',');
                QMap<QString, QString> row;
                for (int c = 0; c < headers.size() && c < cols.size(); ++c) {
                    QString val = cols[c].trimmed().remove('\"');
                    row[headers[c]] = val;
                }
                if (!row.isEmpty()) {
                    m_dataRows.append(row);
                }
            }
        }
    }

    if (!m_dataRows.isEmpty()) {
        m_iterationsSpin->setValue(m_dataRows.size());
        int keyCount = m_dataRows.first().keys().size();
        m_dataStatusLabel->setText(QString("✓ Loaded %1 data rows (%2 fields per row)").arg(m_dataRows.size()).arg(keyCount));
        m_dataStatusLabel->setStyleSheet("color: #10b981; font-weight: bold; font-size: 11px;");
    } else {
        m_dataStatusLabel->setText("No valid records found in file.");
        m_dataStatusLabel->setStyleSheet("color: #ef4444; font-size: 11px;");
    }
}

void CollectionRunnerDialog::executeNextRequest() {
    if (!m_isRunning) {
        return;
    }
    if (m_currentIndex >= m_queue.size()) {
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

    const QueuedRequest& queued = m_queue[m_currentIndex];
    core::RequestModel req = queued.request;

    // Data-driven iteration variable injection
    int iters = m_iterationsSpin->value();
    int requestsPerIter = (iters > 0) ? (m_queue.size() / iters) : m_queue.size();
    int currentIter = (requestsPerIter > 0) ? (m_currentIndex / requestsPerIter) : 0;
    if (!m_dataRows.isEmpty() && currentIter < m_dataRows.size()) {
        const auto& rowData = m_dataRows[currentIter];
        for (auto it = rowData.begin(); it != rowData.end(); ++it) {
            m_activeEnv.setVariableValue(it.key(), it.value());
        }
    }

    // Variable resolution
    core::VariableResolver resolver;
    resolver.setEnvironment(m_activeEnv);
    if (m_model && m_model->rootItem()) {
        resolver.setCollectionVariables(m_model->rootItem()->variables());
    }
    resolver.setFolderVariables(queued.folderVars);
    core::RequestModel resolvedReq = resolver.resolveRequest(req);

    // Pre-request script
    QString preErr;
    if (!m_scriptRunner->runPreRequestScript(resolvedReq.scripts.preRequestScript, resolvedReq, m_activeEnv, &preErr)) {
        m_resultsTable->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
        auto* methodItem = new QTableWidgetItem(core::methodToString(resolvedReq.method));
        methodItem->setForeground(Theme::methodColor(resolvedReq.method));
        m_resultsTable->setItem(row, 1, methodItem);
        QString displayName = (iters > 1) ? QString("[#%1] %2").arg(currentIter + 1).arg(resolvedReq.name) : resolvedReq.name;
        m_resultsTable->setItem(row, 2, new QTableWidgetItem(displayName));
        auto* statusItem = new QTableWidgetItem("Script Error");
        statusItem->setForeground(QColor("#ef4444"));
        m_resultsTable->setItem(row, 3, statusItem);
        m_resultsTable->setItem(row, 5, new QTableWidgetItem(preErr));

        ++m_currentIndex;
        m_progressBar->setValue(m_currentIndex);
        if (m_stopOnFailureChk->isChecked()) {
            stopRun();
            m_summaryLabel->setText(QString("Run stopped on pre-request script error at request #%1").arg(row + 1));
            return;
        }
        QTimer::singleShot(0, this, &CollectionRunnerDialog::executeNextRequest);
        return;
    }

    // Populate initial row info
    m_resultsTable->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
    
    auto* methodItem = new QTableWidgetItem(core::methodToString(resolvedReq.method));
    methodItem->setForeground(Theme::methodColor(resolvedReq.method));
    methodItem->setFont(QFont(methodItem->font().family(), -1, QFont::Bold));
    m_resultsTable->setItem(row, 1, methodItem);

    QString displayName = (iters > 1) ? QString("[#%1] %2").arg(currentIter + 1).arg(resolvedReq.name) : resolvedReq.name;
    m_resultsTable->setItem(row, 2, new QTableWidgetItem(displayName));
    m_resultsTable->setItem(row, 3, new QTableWidgetItem("Running..."));
    m_resultsTable->scrollToBottom();

    const quint64 runGen = m_runGeneration;
    m_networkEngine->sendRequestAsync(resolvedReq, [this, row, resolvedReq, runGen](const core::ResponseModel& res) {
        QPointer<CollectionRunnerDialog> self(this);
        if (!self || !m_isRunning || runGen != m_runGeneration) return;

        m_totalDurationMs += res.latencyMs;

        // Post-response script
        QString postErr;
        if (!m_scriptRunner->runPostResponseScript(resolvedReq.scripts.postResponseScript, resolvedReq, res, m_activeEnv, &postErr)) {
            if (postErr.isEmpty()) postErr = QStringLiteral("Post-response script failed");
        }

        // Run JavaScript tests
        core::TestReport report = m_scriptRunner->runTests(resolvedReq.scripts.tests, resolvedReq, res, m_activeEnv);

        // Run Declarative Assertions
        auto declResults = core::DeclarativeAssertionEvaluator::evaluateAll(resolvedReq.assertions, res);
        for (const auto& dr : declResults) {
            report.results.append(dr);
        }

        m_totalTests += report.totalCount();
        m_passedTests += report.passedCount();

        bool success = res.isHttpSuccess() && (report.failedCount() == 0) && postErr.isEmpty();
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

        if (!postErr.isEmpty()) {
            auto* errItem = m_resultsTable->item(row, 5);
            if (errItem) {
                errItem->setToolTip(postErr);
            }
        }

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
            QTimer::singleShot(0, this, &CollectionRunnerDialog::executeNextRequest);
        }
    });
}

} // namespace poppy::gui
