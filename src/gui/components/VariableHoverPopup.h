#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QTimer>
#include <core/VariableResolver.h>
#include <core/CollectionModel.h>

namespace poppy::gui {

class VariableHoverPopup : public QWidget {
    Q_OBJECT
public:
    explicit VariableHoverPopup(QWidget* parent = nullptr);

    void showForVariable(const QString& varName,
                         const QPoint& globalPos,
                         core::VariableResolver* resolver,
                         core::CollectionModel* model,
                         const QString& activeEnvName);

    void cancelHideTimer();
    void scheduleHide(int delayMs = 350);
    bool isUserInteracting() const;
    QString currentVariableName() const { return m_varName; }

signals:
    void variableSaved(const QString& name, const QString& value, const QString& scope, bool isSecret);
    void manageEnvironmentsRequested();

protected:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onSaveClicked();

private:
    void applyThemeStyles();

    QString m_varName;
    QString m_activeEnvName;
    core::VariableResolver* m_resolver{nullptr};
    core::CollectionModel* m_model{nullptr};

    QLabel* m_tokenBadge;
    QLabel* m_scopeBadge;
    QPushButton* m_closeBtn;

    QLineEdit* m_valueEdit;
    QComboBox* m_targetScopeCombo;
    QCheckBox* m_isSecretCheck;

    QLabel* m_feedbackLabel;
    QPushButton* m_saveBtn;
    QPushButton* m_manageBtn;

    QTimer* m_hideTimer;
};

} // namespace poppy::gui
