#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QProcess>
#include <QList>
#include <QStringList>

namespace poppy::gui {

class GitSyncDialog : public QDialog {
    Q_OBJECT
public:
    explicit GitSyncDialog(const QString& repoPath, const QString& initialCommitMsg = QString(), QWidget* parent = nullptr);
    ~GitSyncDialog() override = default;

signals:
    void syncCompleted();

private slots:
    void onRefreshStatus();
    void onInitGit();
    void onCommit();
    void onPull();
    void onPush();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void setupUi();
    void runGitCommand(const QStringList& args);
    void appendLog(const QString& text, bool isError = false);
    void pumpCommandQueue();
    void setBusy(bool busy);
    void ensureSecretGitignore();

    QString m_repoPath;
    QString m_initialCommitMsg;
    QString m_currentBranch;
    QProcess* m_process{nullptr};
    QList<QStringList> m_cmdQueue;
    bool m_busy{false};
    int m_uncommittedCount{0};
    int m_unpushedCount{0};

    QLabel* m_branchLabel{nullptr};
    QLabel* m_statusLabel{nullptr};
    QPushButton* m_initGitBtn{nullptr};
    QListWidget* m_changedFilesList{nullptr};
    QLineEdit* m_commitMsgEdit{nullptr};
    QPushButton* m_commitBtn{nullptr};
    QPushButton* m_pullBtn{nullptr};
    QPushButton* m_pushBtn{nullptr};
    QPushButton* m_refreshBtn{nullptr};
    QPlainTextEdit* m_consoleOutput{nullptr};
};

} // namespace poppy::gui
