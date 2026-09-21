#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QTcpServer>
#include <network/CurlNetworkEngine.h>

namespace poppy::gui {

class OAuth2TokenDialog : public QDialog {
    Q_OBJECT
public:
    explicit OAuth2TokenDialog(network::CurlNetworkEngine* engine, QWidget* parent = nullptr);
    ~OAuth2TokenDialog() override;

    QString acquiredToken() const { return m_accessToken; }

signals:
    void tokenAcquired(const QString& token);

private slots:
    void onGrantTypeChanged(int index);
    void onFetchTokenClicked();
    void onNewTcpConnection();

private:
    void fetchClientCredentialsToken();
    void startAuthCodeFlow();
    void exchangeCodeForToken(const QString& code);

    network::CurlNetworkEngine* m_engine;
    QString m_accessToken;

    QComboBox* m_grantTypeCombo;
    QLineEdit* m_tokenUrlEdit;
    QLineEdit* m_authUrlEdit;
    QLineEdit* m_clientIdEdit;
    QLineEdit* m_clientSecretEdit;
    QLineEdit* m_scopeEdit;
    QLineEdit* m_redirectPortEdit;
    QPushButton* m_fetchBtn;
    QLabel* m_statusLabel;

    QTcpServer* m_server{nullptr};
};

} // namespace poppy::gui
