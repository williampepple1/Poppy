#include "EnvironmentDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QLineEdit>

namespace poppy::gui {

void EnvironmentDialog::reject() {
    if (m_currentIdx >= 0) {
        saveCurrentEnv();
    }
    QDialog::reject();
}

EnvironmentDialog::EnvironmentDialog(QList<core::EnvironmentModel>& envs, const QString& activeEnvName, const QString& rootPath, QWidget* parent)
    : QDialog(parent), m_envs(envs), m_activeEnvName(activeEnvName), m_rootPath(rootPath) {
    setWindowTitle("Manage Environments");
    resize(700, 450);

    auto* mainLayout = new QHBoxLayout(this);

    // Left pane: Environments list
    auto* leftLayout = new QVBoxLayout();
    m_envListWidget = new QListWidget(this);
    connect(m_envListWidget, &QListWidget::currentRowChanged, this, &EnvironmentDialog::onEnvSelected);
    leftLayout->addWidget(m_envListWidget);

    auto* leftBtnLayout = new QHBoxLayout();
    m_addEnvBtn = new QPushButton("+ Add Env", this);
    connect(m_addEnvBtn, &QPushButton::clicked, this, &EnvironmentDialog::addEnvironment);
    leftBtnLayout->addWidget(m_addEnvBtn);

    m_delEnvBtn = new QPushButton("Delete", this);
    connect(m_delEnvBtn, &QPushButton::clicked, this, &EnvironmentDialog::deleteEnvironment);
    leftBtnLayout->addWidget(m_delEnvBtn);
    leftLayout->addLayout(leftBtnLayout);

    mainLayout->addLayout(leftLayout, 1);

    // Right pane: Variables table
    auto* rightLayout = new QVBoxLayout();
    m_varsTable = new QTableWidget(this);
    m_varsTable->setColumnCount(4);
    m_varsTable->setHorizontalHeaderLabels({"Enabled", "Variable Name", "Value", "Secret"});
    m_varsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_varsTable->setColumnWidth(0, 60);
    m_varsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_varsTable->setColumnWidth(1, 160);
    m_varsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_varsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_varsTable->setColumnWidth(3, 60);
    m_varsTable->verticalHeader()->setVisible(false);
    rightLayout->addWidget(m_varsTable);

    connect(m_varsTable, &QTableWidget::cellChanged, this, [this](int r, int c) {
        if (c == 2) {
            auto* item = m_varsTable->item(r, 2);
            if (item && item->text() != "••••••••") {
                item->setData(Qt::UserRole, item->text());
            }
        } else if (c == 3) {
            auto* sec = m_varsTable->item(r, 3);
            auto* val = m_varsTable->item(r, 2);
            if (sec && val) {
                bool isSecret = (sec->checkState() == Qt::Checked);
                if (isSecret && !m_showSecrets) {
                    val->setText("••••••••");
                } else if (!isSecret) {
                    if (val->data(Qt::UserRole).isValid()) {
                        val->setText(val->data(Qt::UserRole).toString());
                    }
                }
            }
        }
    });

    auto* rightBtnLayout = new QHBoxLayout();
    m_addVarBtn = new QPushButton("+ Add Variable", this);
    connect(m_addVarBtn, &QPushButton::clicked, this, &EnvironmentDialog::addVariable);
    rightBtnLayout->addWidget(m_addVarBtn);

    m_delVarBtn = new QPushButton("Delete Variable", this);
    connect(m_delVarBtn, &QPushButton::clicked, this, &EnvironmentDialog::deleteVariable);
    rightBtnLayout->addWidget(m_delVarBtn);

    m_toggleSecretsBtn = new QPushButton("👁 Show Secrets", this);
    m_toggleSecretsBtn->setToolTip("Toggle visibility of sensitive values");
    connect(m_toggleSecretsBtn, &QPushButton::clicked, this, &EnvironmentDialog::toggleSecrets);
    rightBtnLayout->addWidget(m_toggleSecretsBtn);

    rightBtnLayout->addStretch();

    m_saveBtn = new QPushButton("Save Changes", this);
    m_saveBtn->setObjectName("primaryBtn");
    connect(m_saveBtn, &QPushButton::clicked, this, &EnvironmentDialog::saveCurrentEnv);
    rightBtnLayout->addWidget(m_saveBtn);

    m_closeBtn = new QPushButton("Close", this);
    connect(m_closeBtn, &QPushButton::clicked, this, [this]() {
        if (m_currentIdx >= 0) {
            saveCurrentEnv();
        }
        accept();
    });
    rightBtnLayout->addWidget(m_closeBtn);

    rightLayout->addLayout(rightBtnLayout);

    mainLayout->addLayout(rightLayout, 3);

    populateEnvList();
}

QString EnvironmentDialog::activeEnvironmentName() const {
    if (m_currentIdx >= 0 && m_currentIdx < m_envs.size()) {
        return m_envs[m_currentIdx].name();
    }
    return {};
}

void EnvironmentDialog::populateEnvList() {
    m_envListWidget->clear();
    int selectRow = -1;
    for (int i = 0; i < m_envs.size(); ++i) {
        m_envListWidget->addItem(m_envs[i].name());
        if (m_envs[i].name() == m_activeEnvName) {
            selectRow = i;
        }
    }
    if (selectRow >= 0) {
        m_envListWidget->setCurrentRow(selectRow);
    } else if (m_envs.size() > 0) {
        m_envListWidget->setCurrentRow(0);
    }
}

void EnvironmentDialog::onEnvSelected(int row) {
    if (row < 0 || row >= m_envs.size()) {
        m_varsTable->setRowCount(0);
        m_currentIdx = -1;
        return;
    }
    if (m_currentIdx >= 0 && m_currentIdx < m_envs.size() && m_currentIdx != row) {
        applyTableToEnv(m_currentIdx);
    }
    m_currentIdx = row;
    populateVarsTable(row);
}

void EnvironmentDialog::populateVarsTable(int envIdx) {
    m_varsTable->blockSignals(true);
    m_varsTable->setRowCount(0);
    const auto& vars = m_envs[envIdx].variables();
    for (int i = 0; i < vars.size(); ++i) {
        const auto& v = vars[i];
        int r = m_varsTable->rowCount();
        m_varsTable->insertRow(r);

        // Checkbox: Enabled
        auto* chkItem = new QTableWidgetItem();
        chkItem->setCheckState(v.enabled ? Qt::Checked : Qt::Unchecked);
        chkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_varsTable->setItem(r, 0, chkItem);

        // Name
        m_varsTable->setItem(r, 1, new QTableWidgetItem(v.name));

        // Value
        auto* valItem = new QTableWidgetItem();
        valItem->setData(Qt::UserRole, v.value);
        if (v.isSecret && !m_showSecrets) {
            valItem->setText("••••••••");
        } else {
            valItem->setText(v.value);
        }
        m_varsTable->setItem(r, 2, valItem);

        // Secret
        auto* secItem = new QTableWidgetItem();
        secItem->setCheckState(v.isSecret ? Qt::Checked : Qt::Unchecked);
        secItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_varsTable->setItem(r, 3, secItem);
    }
    m_varsTable->blockSignals(false);
}

void EnvironmentDialog::addEnvironment() {
    bool ok = false;
    QString name = QInputDialog::getText(this, "New Environment", "Environment Name:", QLineEdit::Normal, "", &ok);
    if (ok && !name.trimmed().isEmpty()) {
        name = name.trimmed();
        m_envs.append(core::EnvironmentModel(name));
        populateEnvList();
        m_envListWidget->setCurrentRow(m_envs.size() - 1);
        if (!m_rootPath.isEmpty()) {
            QDir dir(m_rootPath);
            dir.mkpath(QStringLiteral("environments"));
            m_envs.last().saveToEnvFile(dir.filePath("environments/" + name + ".env"));
        }
        emit environmentsModified();
    }
}

void EnvironmentDialog::deleteEnvironment() {
    if (m_currentIdx < 0 || m_currentIdx >= m_envs.size()) return;
    auto ans = QMessageBox::question(this, "Confirm Delete", QString("Delete environment '%1'?").arg(m_envs[m_currentIdx].name()));
    if (ans == QMessageBox::Yes) {
        const QString envName = m_envs[m_currentIdx].name();
        if (!m_rootPath.isEmpty()) {
            QDir dir(m_rootPath);
            QFile::remove(dir.filePath("environments/" + envName + ".env"));
            QFile::remove(dir.filePath("environments/" + envName + ".secret.env"));
        }
        m_envs.removeAt(m_currentIdx);
        m_currentIdx = -1;
        populateEnvList();
        emit environmentsModified();
    }
}

void EnvironmentDialog::addVariable() {
    if (m_currentIdx < 0 || m_currentIdx >= m_envs.size()) return;
    int r = m_varsTable->rowCount();
    m_varsTable->insertRow(r);

    auto* chkItem = new QTableWidgetItem();
    chkItem->setCheckState(Qt::Checked);
    chkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_varsTable->setItem(r, 0, chkItem);

    m_varsTable->setItem(r, 1, new QTableWidgetItem("newVar"));
    m_varsTable->setItem(r, 2, new QTableWidgetItem(""));

    auto* secItem = new QTableWidgetItem();
    secItem->setCheckState(Qt::Unchecked);
    secItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_varsTable->setItem(r, 3, secItem);
}

void EnvironmentDialog::deleteVariable() {
    int r = m_varsTable->currentRow();
    if (r >= 0 && r < m_varsTable->rowCount()) {
        m_varsTable->removeRow(r);
    }
}

void EnvironmentDialog::toggleSecrets() {
    m_showSecrets = !m_showSecrets;
    m_toggleSecretsBtn->setText(m_showSecrets ? "🔒 Hide Secrets" : "👁 Show Secrets");
    m_varsTable->blockSignals(true);
    for (int r = 0; r < m_varsTable->rowCount(); ++r) {
        auto* sec = m_varsTable->item(r, 3);
        auto* val = m_varsTable->item(r, 2);
        if (sec && val && sec->checkState() == Qt::Checked) {
            if (m_showSecrets) {
                QString actual = val->data(Qt::UserRole).isValid() ? val->data(Qt::UserRole).toString() : val->text();
                val->setText(actual);
            } else {
                val->setText("••••••••");
            }
        }
    }
    m_varsTable->blockSignals(false);
}

void EnvironmentDialog::applyTableToEnv(int envIdx) {
    if (envIdx < 0 || envIdx >= m_envs.size()) return;

    auto& env = m_envs[envIdx];
    env.variables().clear();

    for (int i = 0; i < m_varsTable->rowCount(); ++i) {
        auto* chk = m_varsTable->item(i, 0);
        auto* name = m_varsTable->item(i, 1);
        auto* val = m_varsTable->item(i, 2);
        auto* sec = m_varsTable->item(i, 3);

        if (name && !name->text().trimmed().isEmpty()) {
            bool isEnabled = (chk && chk->checkState() == Qt::Checked);
            bool isSecret = (sec && sec->checkState() == Qt::Checked);
            QString realVal;
            if (val) {
                if (val->text() == "••••••••" && val->data(Qt::UserRole).isValid()) {
                    realVal = val->data(Qt::UserRole).toString();
                } else {
                    realVal = val->text();
                }
            }
            env.addOrUpdateVariable(name->text().trimmed(), realVal, isSecret, isEnabled);
        }
    }
}

void EnvironmentDialog::saveCurrentEnv() {
    if (m_currentIdx < 0 || m_currentIdx >= m_envs.size()) return;

    applyTableToEnv(m_currentIdx);
    auto& env = m_envs[m_currentIdx];

    // Save to disk if rootPath has environments/
    if (!m_rootPath.isEmpty()) {
        QDir dir(m_rootPath);
        dir.mkpath("environments");
        QString envPath = dir.filePath("environments/" + env.name() + ".env");
        env.saveToEnvFile(envPath);

        QString secretPath = dir.filePath("environments/" + env.name() + ".secret.env");
        env.saveSecretsToEnvFile(secretPath);
    }

    emit environmentsModified();
}

} // namespace poppy::gui
