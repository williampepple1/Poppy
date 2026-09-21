#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <network/CurlNetworkEngine.h>

namespace poppy::gui {

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(network::CurlNetworkEngine* engine, QWidget* parent = nullptr);

private slots:
    void onApply();
    void onClearCookies();
    void onBrowseCert();
    void onBrowseKey();

private:
    network::CurlNetworkEngine* m_engine;

    QLineEdit* m_proxyEdit;
    QSpinBox* m_timeoutSpin;
    QCheckBox* m_sslVerifyChk;
    QCheckBox* m_cookieJarChk;
    QPushButton* m_clearCookiesBtn;

    // mTLS
    QLineEdit* m_clientCertEdit;
    QLineEdit* m_clientKeyEdit;
    QLineEdit* m_clientPassEdit;
    QComboBox* m_clientCertTypeCombo;
    QPushButton* m_browseCertBtn;
    QPushButton* m_browseKeyBtn;
};

} // namespace poppy::gui
