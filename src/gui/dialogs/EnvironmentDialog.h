#pragma once

#include <QDialog>
#include <QListWidget>
#include <QTableWidget>
#include <QPushButton>
#include <core/EnvironmentModel.h>

namespace poppy::gui {

class EnvironmentDialog : public QDialog {
    Q_OBJECT
public:
    explicit EnvironmentDialog(QList<core::EnvironmentModel>& envs, const QString& activeEnvName, const QString& rootPath, QWidget* parent = nullptr);

    QString activeEnvironmentName() const;
    void reject() override;

signals:
    void environmentsModified();

private slots:
    void onEnvSelected(int row);
    void addEnvironment();
    void deleteEnvironment();
    void addVariable();
    void deleteVariable();
    void saveCurrentEnv();
    void toggleSecrets();

private:
    void populateEnvList();
    void populateVarsTable(int envIdx);
    void applyTableToEnv(int envIdx);

    QList<core::EnvironmentModel>& m_envs;
    QString m_activeEnvName;
    QString m_rootPath;
    int m_currentIdx{-1};
    bool m_showSecrets{false};

    QListWidget* m_envListWidget;
    QPushButton* m_addEnvBtn;
    QPushButton* m_delEnvBtn;

    QTableWidget* m_varsTable;
    QPushButton* m_addVarBtn;
    QPushButton* m_delVarBtn;
    QPushButton* m_toggleSecretsBtn;
    QPushButton* m_saveBtn;
    QPushButton* m_closeBtn;
};

} // namespace poppy::gui
