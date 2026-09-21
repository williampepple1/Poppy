#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
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

private:
    network::CurlNetworkEngine* m_engine;

    QLineEdit* m_proxyEdit;
    QSpinBox* m_timeoutSpin;
    QCheckBox* m_sslVerifyChk;
    QCheckBox* m_cookieJarChk;
    QPushButton* m_clearCookiesBtn;
};

} // namespace poppy::gui
