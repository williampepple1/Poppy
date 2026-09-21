#pragma once

#include <QWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <core/RequestModel.h>

namespace poppy::network { class CurlNetworkEngine; }

namespace poppy::gui {

class AuthEditor : public QWidget {
    Q_OBJECT
public:
    explicit AuthEditor(QWidget* parent = nullptr);

    void setNetworkEngine(network::CurlNetworkEngine* engine) { m_networkEngine = engine; }
    void loadFromRequest(const core::RequestModel& req);
    void saveToRequest(core::RequestModel& req) const;

signals:
    void authChanged();

private slots:
    void onTypeChanged(int index);
    void onGetOAuth2Token();

private:
    network::CurlNetworkEngine* m_networkEngine{nullptr};

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
    QPushButton* m_oauth2GetTokenBtn;

    // AWS SigV4
    QWidget* m_awsWidget;
    QLineEdit* m_awsAccessKeyEdit;
    QLineEdit* m_awsSecretKeyEdit;
    QLineEdit* m_awsSessionTokenEdit;
    QLineEdit* m_awsRegionEdit;
    QLineEdit* m_awsServiceEdit;
};

} // namespace poppy::gui
