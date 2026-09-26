#include "GitSyncDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QInputDialog>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QColor>

namespace poppy::gui {

GitSyncDialog::GitSyncDialog(const QString& repoPath, const QString& initialCommitMsg, QWidget* parent)
    : QDialog(parent)
    , m_repoPath(repoPath.isEmpty() ? QDir::currentPath() : repoPath)
    , m_initialCommitMsg(initialCommitMsg)
    , m_process(new QProcess(this))
{
    setWindowTitle("Git Sync - Poppy");
    resize(740, 540);

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

    m_initGitBtn = new QPushButton("Initialize Git (git init)", this);
    m_initGitBtn->setStyleSheet("background-color: #3b82f6; color: white; font-weight: bold; border-radius: 4px; padding: 4px 8px; font-size: 11px;");
    m_initGitBtn->setVisible(false);
    connect(m_initGitBtn, &QPushButton::clicked, this, &GitSyncDialog::onInitGit);
    infoLayout->addWidget(m_initGitBtn);

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

    // Commit & Push row
    auto* commitRow = new QHBoxLayout();
    m_commitMsgEdit = new QLineEdit(this);
    m_commitMsgEdit->setPlaceholderText("Commit message (e.g. update api collection requests)...");
    m_commitMsgEdit->setText(m_initialCommitMsg.isEmpty() ? QStringLiteral("update api collection") : m_initialCommitMsg);
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
    if (m_commitBtn) m_commitBtn->setEnabled(!busy && m_uncommittedCount > 0);
    if (m_pullBtn) m_pullBtn->setEnabled(!busy);
    if (m_pushBtn) m_pushBtn->setEnabled(!busy && (m_uncommittedCount > 0 || m_unpushedCount > 0));
    if (m_refreshBtn) m_refreshBtn->setEnabled(!busy);
    if (m_initGitBtn) m_initGitBtn->setEnabled(!busy);
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

void GitSyncDialog::onInitGit() {
    if (m_busy) return;
    ensureSecretGitignore();
    runGitCommand({"init"});
    runGitCommand({"add", "--", ".", ":(exclude)*.secret.env", ":(exclude)**/*.secret.env"});
    runGitCommand({"commit", "-m", "Initial commit from Poppy"});
}

void GitSyncDialog::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitStatus);
    QString out = QString::fromUtf8(m_process->readAllStandardOutput());
    QString err = QString::fromUtf8(m_process->readAllStandardError());

    if (!out.isEmpty()) appendLog(out);
    if (!err.isEmpty()) appendLog(err, exitCode != 0);

    auto args = m_process->arguments();
    const bool isPush = !args.isEmpty() && args.first() == "push";
    const bool shouldRefresh = !args.isEmpty()
        && (args.first() == "commit" || args.first() == "push" || args.first() == "pull" || args.first() == "init");

    // Handle push error cases
    if (isPush && exitCode != 0) {
        if (err.contains("no upstream branch") || err.contains("--set-upstream")) {
            appendLog("Upstream tracking branch not set. Setting upstream to origin/" + m_currentBranch + "...");
            m_cmdQueue.prepend({"push", "--set-upstream", "origin", m_currentBranch.isEmpty() ? QStringLiteral("main") : m_currentBranch});
        } else if (err.contains("No such remote 'origin'") || err.contains("does not appear to be a git repository")) {
            bool ok = false;
            QString url = QInputDialog::getText(this, "Configure Git Remote",
                "No remote 'origin' repository is configured for this collection.\nEnter remote Git repository URL (e.g. https://github.com/user/collection.git):",
                QLineEdit::Normal, QString(), &ok);
            if (ok && !url.trimmed().isEmpty()) {
                m_cmdQueue.clear();
                runGitCommand({"remote", "add", "origin", url.trimmed()});
                runGitCommand({"push", "--set-upstream", "origin", m_currentBranch.isEmpty() ? QStringLiteral("main") : m_currentBranch});
            }
        } else if (err.contains("Updates were rejected") || err.contains("fetch first")) {
            auto ans = QMessageBox::question(this, "Remote Changes Detected",
                "The remote repository has changes that you do not have locally.\nWould you like to pull with rebase and then push?",
                QMessageBox::Yes | QMessageBox::No);
            if (ans == QMessageBox::Yes) {
                m_cmdQueue.clear();
                runGitCommand({"pull", "--rebase"});
                runGitCommand({"push"});
            }
        }
    }

    if (!m_cmdQueue.isEmpty()) {
        pumpCommandQueue();
        return;
    }
    setBusy(false);
    if (shouldRefresh) {
        emit syncCompleted();
        onRefreshStatus();
    }
}

void GitSyncDialog::onRefreshStatus() {
    if (m_process->state() != QProcess::NotRunning) return;
    m_changedFilesList->clear();
    m_uncommittedCount = 0;
    m_unpushedCount = 0;

    // Check if .git directory exists
    bool isRepo = false;
    QDir d(m_repoPath);
    while (true) {
        if (QFileInfo::exists(d.filePath(".git"))) {
            isRepo = true;
            break;
        }
        if (!d.cdUp()) break;
    }

    if (!isRepo) {
        m_branchLabel->setText("Not a Git Repo");
        m_branchLabel->setStyleSheet("background-color: #ef4444; color: white; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px;");
        if (m_initGitBtn) m_initGitBtn->setVisible(true);
        if (m_commitBtn) m_commitBtn->setEnabled(false);
        if (m_pullBtn) m_pullBtn->setEnabled(false);
        if (m_pushBtn) m_pushBtn->setEnabled(false);
        auto* item = new QListWidgetItem("This folder is not a Git repository yet. Click 'Initialize Git' to track it.");
        item->setForeground(QColor("#f59e0b"));
        m_changedFilesList->addItem(item);
        return;
    }

    if (m_initGitBtn) m_initGitBtn->setVisible(false);

    // Query branch
    QProcess branchProc;
    branchProc.setWorkingDirectory(m_repoPath);
    branchProc.start("git", {"rev-parse", "--abbrev-ref", "HEAD"});
    if (branchProc.waitForFinished(3000)) {
        m_currentBranch = QString::fromUtf8(branchProc.readAllStandardOutput()).trimmed();
        if (!m_currentBranch.isEmpty()) {
            m_branchLabel->setText("Branch: " + m_currentBranch);
            m_branchLabel->setStyleSheet("background-color: #10b981; color: white; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px;");
        } else {
            m_currentBranch = "main";
            m_branchLabel->setText("Branch: main");
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
            QString trimmed = line.trimmed();
            if (!trimmed.isEmpty()) {
                m_changedFilesList->addItem(trimmed);
                m_uncommittedCount++;
            }
        }
    }

    // Query unpushed commits ahead of upstream
    QProcess revProc;
    revProc.setWorkingDirectory(m_repoPath);
    revProc.start("git", {"rev-list", "--count", "@{u}..HEAD"});
    if (revProc.waitForFinished(3000) && revProc.exitCode() == 0) {
        bool ok = false;
        int count = QString::fromUtf8(revProc.readAllStandardOutput()).trimmed().toInt(&ok);
        if (ok) m_unpushedCount = count;
    }

    if (m_uncommittedCount == 0 && m_unpushedCount == 0) {
        auto* item = new QListWidgetItem("Working tree clean (no uncommitted or unpushed changes).");
        item->setForeground(QColor("#10b981"));
        m_changedFilesList->addItem(item);
    } else if (m_uncommittedCount == 0 && m_unpushedCount > 0) {
        auto* item = new QListWidgetItem(QString("%1 commit(s) ahead of remote (ready to push).").arg(m_unpushedCount));
        item->setForeground(QColor("#3b82f6"));
        m_changedFilesList->addItem(item);
    }

    // Configure push & commit button states following the Git way
    if (m_uncommittedCount > 0) {
        m_commitBtn->setEnabled(true);
        m_pushBtn->setEnabled(true);
        m_pushBtn->setText("🚀 Commit & Push");
        m_pushBtn->setToolTip(QString("Stage %1 changed file(s), commit them, and push to remote").arg(m_uncommittedCount));
    } else if (m_unpushedCount > 0) {
        m_commitBtn->setEnabled(false);
        m_pushBtn->setEnabled(true);
        m_pushBtn->setText("Push (↑)");
        m_pushBtn->setToolTip(QString("Push %1 commit(s) to remote Git repository").arg(m_unpushedCount));
    } else {
        m_commitBtn->setEnabled(false);
        m_pushBtn->setEnabled(false);
        m_pushBtn->setText("Push (↑)");
        m_pushBtn->setToolTip("Working tree is clean and up to date with remote.");
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

    // Follow the Git way through and through:
    // If there are uncommitted changes, stage and commit first, then push!
    if (m_uncommittedCount > 0) {
        QString msg = m_commitMsgEdit->text().trimmed();
        if (msg.isEmpty()) {
            msg = QStringLiteral("update api collection");
        }
        ensureSecretGitignore();
        runGitCommand({"add", "--", ".", ":(exclude)*.secret.env", ":(exclude)**/*.secret.env"});
        runGitCommand({"commit", "-m", msg});
    }

    runGitCommand({"push"});
}

} // namespace poppy::gui
