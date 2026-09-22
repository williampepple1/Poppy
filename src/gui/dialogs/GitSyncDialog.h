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
    explicit GitSyncDialog(const QString& repoPath, QWidget* parent = nullptr);
    ~GitSyncDialog() override = default;

private slots:
    void onRefreshStatus();
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
    QProcess* m_process{nullptr};
    QList<QStringList> m_cmdQueue;
    bool m_busy{false};

    QLabel* m_branchLabel{nullptr};
    QLabel* m_statusLabel{nullptr};
    QListWidget* m_changedFilesList{nullptr};
    QLineEdit* m_commitMsgEdit{nullptr};
    QPushButton* m_commitBtn{nullptr};
    QPushButton* m_pullBtn{nullptr};
    QPushButton* m_pushBtn{nullptr};
    QPushButton* m_refreshBtn{nullptr};
    QPlainTextEdit* m_consoleOutput{nullptr};
};

} // namespace poppy::gui
