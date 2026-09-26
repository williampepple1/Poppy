#include "SettingsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QFileDialog>
#include <Theme.h>

namespace poppy::gui {

SettingsDialog::SettingsDialog(network::CurlNetworkEngine* engine, QWidget* parent)
    : QDialog(parent), m_engine(engine) {
    setWindowTitle("Poppy Settings");
    resize(520, 520);

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

    // 3. Client Certificates (mTLS) Group
    auto* mtlsGroup = new QGroupBox("Client Certificates (mTLS)", this);
    auto* mtlsForm = new QFormLayout(mtlsGroup);

    auto* certRow = new QHBoxLayout();
    m_clientCertEdit = new QLineEdit(mtlsGroup);
    m_clientCertEdit->setPlaceholderText("Path to client certificate (.pem, .p12, .crt)");
    if (m_engine) m_clientCertEdit->setText(m_engine->clientCertPath());
    m_browseCertBtn = new QPushButton("Browse...", mtlsGroup);
    connect(m_browseCertBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseCert);
    certRow->addWidget(m_clientCertEdit, 1);
    certRow->addWidget(m_browseCertBtn);
    mtlsForm->addRow("Certificate File:", certRow);

    m_clientCertTypeCombo = new QComboBox(mtlsGroup);
    m_clientCertTypeCombo->addItems({"PEM", "DER", "P12"});
    if (m_engine) {
        int idx = m_clientCertTypeCombo->findText(m_engine->clientCertType());
        if (idx >= 0) m_clientCertTypeCombo->setCurrentIndex(idx);
    }
    mtlsForm->addRow("Format:", m_clientCertTypeCombo);

    auto* keyRow = new QHBoxLayout();
    m_clientKeyEdit = new QLineEdit(mtlsGroup);
    m_clientKeyEdit->setPlaceholderText("Path to private key (.key, optional if in cert)");
    if (m_engine) m_clientKeyEdit->setText(m_engine->clientKeyPath());
    m_browseKeyBtn = new QPushButton("Browse...", mtlsGroup);
    connect(m_browseKeyBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseKey);
    keyRow->addWidget(m_clientKeyEdit, 1);
    keyRow->addWidget(m_browseKeyBtn);
    mtlsForm->addRow("Private Key:", keyRow);

    m_clientPassEdit = new QLineEdit(mtlsGroup);
    m_clientPassEdit->setPlaceholderText("Passphrase (if key/cert is encrypted)");
    m_clientPassEdit->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    if (m_engine) m_clientPassEdit->setText(m_engine->clientKeyPassword());
    mtlsForm->addRow("Passphrase:", m_clientPassEdit);

    mainLayout->addWidget(mtlsGroup);

    // 4. Cookie Jar Group
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

    // 5. Appearance & Theme Group
    auto* themeGroup = new QGroupBox("Appearance & Theme", this);
    auto* themeForm = new QFormLayout(themeGroup);

    m_themeCombo = new QComboBox(themeGroup);
    for (const auto& th : Theme::availableThemes()) {
        m_themeCombo->addItem(QString("%1 (%2)").arg(th.name, th.category), th.id);
        if (th.id == Theme::currentThemeId()) {
            m_themeCombo->setCurrentIndex(m_themeCombo->count() - 1);
        }
    }
    themeForm->addRow("Color Theme:", m_themeCombo);
    mainLayout->addWidget(themeGroup);

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

void SettingsDialog::onBrowseCert() {
    QString file = QFileDialog::getOpenFileName(this, "Select Client Certificate", "", "Certificates (*.pem *.crt *.p12 *.pfx);;All Files (*.*)");
    if (!file.isEmpty()) {
        m_clientCertEdit->setText(file);
    }
}

void SettingsDialog::onBrowseKey() {
    QString file = QFileDialog::getOpenFileName(this, "Select Private Key", "", "Key Files (*.key *.pem);;All Files (*.*)");
    if (!file.isEmpty()) {
        m_clientKeyEdit->setText(file);
    }
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
        m_engine->setClientCertPath(m_clientCertEdit->text().trimmed());
        m_engine->setClientCertType(m_clientCertTypeCombo->currentText());
        m_engine->setClientKeyPath(m_clientKeyEdit->text().trimmed());
        m_engine->setClientKeyPassword(m_clientPassEdit->text());
    }
    QString selectedTheme = m_themeCombo->currentData().toString();
    if (!selectedTheme.isEmpty()) {
        emit themeChanged(selectedTheme);
    }
    accept();
}

} // namespace poppy::gui
