#include "SettingsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>

namespace poppy::gui {

SettingsDialog::SettingsDialog(network::CurlNetworkEngine* engine, QWidget* parent)
    : QDialog(parent), m_engine(engine) {
    setWindowTitle("Poppy Settings");
    resize(480, 360);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // 1. Network Group
    auto* netGroup = new QGroupBox("Network & Proxy", this);
    auto* netForm = new QFormLayout(netGroup);

    m_proxyEdit = new QLineEdit(netGroup);
    m_proxyEdit->setPlaceholderText("e.g. http://127.0.0.1:8080 or socks5://127.0.0.1:1080");
    if (m_engine) m_proxyEdit->setText(m_engine->proxy());
    netForm->addRow("Proxy URL:", m_proxyEdit);

    m_timeoutSpin = new QSpinBox(netGroup);
    m_timeoutSpin->setRange(1000, 300000);
    m_timeoutSpin->setSingleStep(1000);
    m_timeoutSpin->setSuffix(" ms");
    if (m_engine) m_timeoutSpin->setValue(static_cast<int>(m_engine->timeoutMs()));
    else m_timeoutSpin->setValue(30000);
    netForm->addRow("Request Timeout:", m_timeoutSpin);

    mainLayout->addWidget(netGroup);

    // 2. SSL / Security Group
    auto* secGroup = new QGroupBox("Security & SSL", this);
    auto* secLayout = new QVBoxLayout(secGroup);

    m_sslVerifyChk = new QCheckBox("Reject unauthorized SSL certificates (Verify Peer)", secGroup);
    if (m_engine) m_sslVerifyChk->setChecked(m_engine->sslVerifyPeer());
    else m_sslVerifyChk->setChecked(true);
    secLayout->addWidget(m_sslVerifyChk);

    mainLayout->addWidget(secGroup);

    // 3. Cookie Jar Group
    auto* cookieGroup = new QGroupBox("Cookie Management", this);
    auto* cookieLayout = new QHBoxLayout(cookieGroup);

    m_cookieJarChk = new QCheckBox("Maintain session cookies across requests (Cookie Jar)", cookieGroup);
    if (m_engine) m_cookieJarChk->setChecked(m_engine->cookieJarEnabled());
    else m_cookieJarChk->setChecked(true);
    cookieLayout->addWidget(m_cookieJarChk, 1);

    m_clearCookiesBtn = new QPushButton("Clear Cookies", cookieGroup);
    connect(m_clearCookiesBtn, &QPushButton::clicked, this, &SettingsDialog::onClearCookies);
    cookieLayout->addWidget(m_clearCookiesBtn);

    mainLayout->addWidget(cookieGroup);

    mainLayout->addStretch();

    // Dialog buttons
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    auto* cancelBtn = new QPushButton("Cancel", this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    auto* saveBtn = new QPushButton("Save Settings", this);
    saveBtn->setDefault(true);
    connect(saveBtn, &QPushButton::clicked, this, &SettingsDialog::onApply);
    btnLayout->addWidget(saveBtn);

    mainLayout->addLayout(btnLayout);
}

void SettingsDialog::onClearCookies() {
    if (m_engine) {
        m_engine->clearCookies();
        QMessageBox::information(this, "Cookies Cleared", "The persistent cookie jar has been cleared.");
    }
}

void SettingsDialog::onApply() {
    if (m_engine) {
        m_engine->setProxy(m_proxyEdit->text().trimmed());
        m_engine->setTimeoutMs(m_timeoutSpin->value());
        m_engine->setSslVerifyPeer(m_sslVerifyChk->isChecked());
        m_engine->setCookieJarEnabled(m_cookieJarChk->isChecked());
    }
    accept();
}

} // namespace poppy::gui
