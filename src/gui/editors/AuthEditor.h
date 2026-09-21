#pragma once

#include <QWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QStackedWidget>
#include <core/RequestModel.h>

namespace poppy::gui {

class AuthEditor : public QWidget {
    Q_OBJECT
public:
    explicit AuthEditor(QWidget* parent = nullptr);

    void loadFromRequest(const core::RequestModel& req);
    void saveToRequest(core::RequestModel& req) const;

signals:
    void authChanged();

private slots:
    void onTypeChanged(int index);

private:
    QComboBox* m_typeCombo;
    QStackedWidget* m_stack;

    // None
    QWidget* m_noneWidget;

    // Bearer
    QWidget* m_bearerWidget;
    QLineEdit* m_bearerTokenEdit;

    // Basic
    QWidget* m_basicWidget;
    QLineEdit* m_basicUserEdit;
    QLineEdit* m_basicPassEdit;

    // API Key
    QWidget* m_apiKeyWidget;
    QLineEdit* m_apiKeyNameEdit;
    QLineEdit* m_apiKeyValueEdit;
    QComboBox* m_apiKeyPlacementCombo;

    // OAuth 2.0
    QWidget* m_oauth2Widget;
    QLineEdit* m_oauth2TokenEdit;
};

} // namespace poppy::gui
