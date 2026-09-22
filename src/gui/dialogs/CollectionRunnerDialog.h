#pragma once

#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QProgressBar>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QTimer>
#include <core/CollectionModel.h>
#include <core/ScriptRunner.h>
#include <network/CurlNetworkEngine.h>

namespace poppy::gui {

class CollectionRunnerDialog : public QDialog {
    Q_OBJECT
public:
    explicit CollectionRunnerDialog(
        core::CollectionModel* model,
        network::CurlNetworkEngine* engine,
        core::ScriptRunner* scriptRunner,
        const QString& activeEnvName,
        QWidget* parent = nullptr
    );

private slots:
    void startRun();
    void stopRun();
    void executeNextRequest();

private:
    void collectRequests(core::CollectionItem* item, QList<core::RequestModel>& list);

    core::CollectionModel* m_model;
    network::CurlNetworkEngine* m_networkEngine;
    core::ScriptRunner* m_scriptRunner;

    // Controls
    QComboBox* m_folderCombo;
    QComboBox* m_envCombo;
    QSpinBox* m_iterationsSpin;
    QSpinBox* m_delaySpin;
    QLineEdit* m_dataFileEdit{nullptr};
    QPushButton* m_browseDataBtn{nullptr};
    QLabel* m_dataStatusLabel{nullptr};
    QCheckBox* m_stopOnFailureChk;
    QPushButton* m_startBtn;
    QPushButton* m_stopBtn;
    QPushButton* m_closeBtn;

    QProgressBar* m_progressBar;
    QTableWidget* m_resultsTable;
    QLabel* m_summaryLabel;

    void loadDataFile(const QString& filePath);

    // State during execution
    QList<QMap<QString, QString>> m_dataRows;
    QList<core::RequestModel> m_queue;
    int m_currentIndex{0};
    int m_passedRequests{0};
    int m_totalTests{0};
    int m_passedTests{0};
    qint64 m_totalDurationMs{0};
    bool m_isRunning{false};
    core::EnvironmentModel m_activeEnv;
};

} // namespace poppy::gui
