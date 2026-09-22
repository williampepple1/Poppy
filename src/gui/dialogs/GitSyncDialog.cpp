#include "GitSyncDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QColor>

namespace poppy::gui {

GitSyncDialog::GitSyncDialog(const QString& repoPath, QWidget* parent)
    : QDialog(parent)
    , m_repoPath(repoPath.isEmpty() ? QDir::currentPath() : repoPath)
    , m_process(new QProcess(this))
{
    setWindowTitle("Git Sync - Poppy");
    resize(720, 520);

    setupUi();

    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &GitSyncDialog::onProcessFinished);

    onRefreshStatus();
}

void GitSyncDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    // Header Info
    auto* infoLayout = new QHBoxLayout();
    infoLayout->addWidget(new QLabel("<b>Repository:</b> " + m_repoPath, this));
    infoLayout->addStretch();

    m_branchLabel = new QLabel("Branch: --", this);
    m_branchLabel->setStyleSheet("background-color: #27272a; color: #a1a1aa; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px;");
    infoLayout->addWidget(m_branchLabel);

    m_refreshBtn = new QPushButton("Refresh", this);
    connect(m_refreshBtn, &QPushButton::clicked, this, &GitSyncDialog::onRefreshStatus);
    infoLayout->addWidget(m_refreshBtn);

    mainLayout->addLayout(infoLayout);

    // Changed Files List
    mainLayout->addWidget(new QLabel("Changed / Untracked Files:", this));
    m_changedFilesList = new QListWidget(this);
    mainLayout->addWidget(m_changedFilesList, 1);

    // Commit row
    auto* commitRow = new QHBoxLayout();
    m_commitMsgEdit = new QLineEdit(this);
    m_commitMsgEdit->setPlaceholderText("Commit message (e.g. update api collection requests)...");
    m_commitMsgEdit->setText("update api collection");
    commitRow->addWidget(m_commitMsgEdit, 1);

    m_commitBtn = new QPushButton("Commit All", this);
    m_commitBtn->setStyleSheet("background-color: #3b82f6; color: white; font-weight: bold; padding: 6px 14px; border-radius: 4px;");
    connect(m_commitBtn, &QPushButton::clicked, this, &GitSyncDialog::onCommit);
    commitRow->addWidget(m_commitBtn);

    m_pullBtn = new QPushButton("Pull (↓)", this);
    connect(m_pullBtn, &QPushButton::clicked, this, &GitSyncDialog::onPull);
    commitRow->addWidget(m_pullBtn);

    m_pushBtn = new QPushButton("Push (↑)", this);
    m_pushBtn->setStyleSheet("background-color: #10b981; color: white; font-weight: bold; padding: 6px 14px; border-radius: 4px;");
    connect(m_pushBtn, &QPushButton::clicked, this, &GitSyncDialog::onPush);
    commitRow->addWidget(m_pushBtn);

    mainLayout->addLayout(commitRow);

    // Console output
    mainLayout->addWidget(new QLabel("Git Output:", this));
    m_consoleOutput = new QPlainTextEdit(this);
    m_consoleOutput->setReadOnly(true);
    m_consoleOutput->setMaximumHeight(140);
    mainLayout->addWidget(m_consoleOutput);
}

void GitSyncDialog::appendLog(const QString& text, bool isError) {
    if (text.trimmed().isEmpty()) return;
    QString prefix = isError ? "[ERR] " : "[GIT] ";
    m_consoleOutput->appendPlainText(prefix + text.trimmed());
}

void GitSyncDialog::runGitCommand(const QStringList& args) {
    m_cmdQueue.append(args);
    pumpCommandQueue();
}

void GitSyncDialog::pumpCommandQueue() {
    if (m_process->state() != QProcess::NotRunning) return;
    if (m_cmdQueue.isEmpty()) {
        setBusy(false);
        return;
    }
    setBusy(true);
    const QStringList args = m_cmdQueue.takeFirst();
    m_process->setWorkingDirectory(m_repoPath);
    appendLog("git " + args.join(' '));
    m_process->start("git", args);
}

void GitSyncDialog::setBusy(bool busy) {
    m_busy = busy;
    if (m_commitBtn) m_commitBtn->setEnabled(!busy);
    if (m_pullBtn) m_pullBtn->setEnabled(!busy);
    if (m_pushBtn) m_pushBtn->setEnabled(!busy);
    if (m_refreshBtn) m_refreshBtn->setEnabled(!busy);
}

void GitSyncDialog::ensureSecretGitignore() {
    const QString giPath = QDir(m_repoPath).filePath(QStringLiteral(".gitignore"));
    QFile gi(giPath);
    QString content;
    if (gi.exists() && gi.open(QIODevice::ReadOnly | QIODevice::Text)) {
        content = QString::fromUtf8(gi.readAll());
        gi.close();
    }
    if (content.contains("*.secret.env")) return;
    if (!gi.open(QIODevice::Append | QIODevice::Text)) return;
    QTextStream out(&gi);
    if (!content.isEmpty() && !content.endsWith('\n')) out << '\n';
    out << "*.secret.env\n";
}

void GitSyncDialog::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitStatus);
    QString out = QString::fromUtf8(m_process->readAllStandardOutput());
    QString err = QString::fromUtf8(m_process->readAllStandardError());

    if (!out.isEmpty()) appendLog(out);
    if (!err.isEmpty()) appendLog(err, exitCode != 0);

    auto args = m_process->arguments();
    const bool shouldRefresh = !args.isEmpty()
        && (args.first() == "commit" || args.first() == "push" || args.first() == "pull");

    if (!m_cmdQueue.isEmpty()) {
        pumpCommandQueue();
        return;
    }
    setBusy(false);
    if (shouldRefresh) {
        onRefreshStatus();
    }
}

void GitSyncDialog::onRefreshStatus() {
    if (m_process->state() != QProcess::NotRunning) return;
    m_changedFilesList->clear();

    // Query branch
    QProcess branchProc;
    branchProc.setWorkingDirectory(m_repoPath);
    branchProc.start("git", {"rev-parse", "--abbrev-ref", "HEAD"});
    if (branchProc.waitForFinished(3000)) {
        QString branch = QString::fromUtf8(branchProc.readAllStandardOutput()).trimmed();
        if (!branch.isEmpty()) {
            m_branchLabel->setText("Branch: " + branch);
            m_branchLabel->setStyleSheet("background-color: #10b981; color: white; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px;");
        } else {
            m_branchLabel->setText("Not a Git Repo");
            m_branchLabel->setStyleSheet("background-color: #ef4444; color: white; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px;");
        }
    }

    // Query status
    QProcess statusProc;
    statusProc.setWorkingDirectory(m_repoPath);
    statusProc.start("git", {"status", "--porcelain"});
    if (statusProc.waitForFinished(3000)) {
        QString statusOut = QString::fromUtf8(statusProc.readAllStandardOutput());
        QStringList lines = statusOut.split('\n', Qt::SkipEmptyParts);
        for (const auto& line : lines) {
            m_changedFilesList->addItem(line.trimmed());
        }
    }

    if (m_changedFilesList->count() == 0) {
        auto* item = new QListWidgetItem("Working tree clean (no uncommitted changes).");
        item->setForeground(QColor("#10b981"));
        m_changedFilesList->addItem(item);
    }
}

void GitSyncDialog::onCommit() {
    QString msg = m_commitMsgEdit->text().trimmed();
    if (msg.isEmpty()) {
        QMessageBox::warning(this, "Empty Message", "Please enter a commit message.");
        return;
    }
    if (m_busy || m_process->state() != QProcess::NotRunning) {
        appendLog("A git command is already running.", true);
        return;
    }

    ensureSecretGitignore();
    runGitCommand({"add", "--", ".", ":(exclude)*.secret.env", ":(exclude)**/*.secret.env"});
    runGitCommand({"commit", "-m", msg});
}

void GitSyncDialog::onPull() {
    if (m_busy || m_process->state() != QProcess::NotRunning) {
        appendLog("A git command is already running.", true);
        return;
    }
    runGitCommand({"pull", "--rebase"});
}

void GitSyncDialog::onPush() {
    if (m_busy || m_process->state() != QProcess::NotRunning) {
        appendLog("A git command is already running.", true);
        return;
    }
    runGitCommand({"push"});
}

} // namespace poppy::gui
