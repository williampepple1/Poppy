#include "AuthEditor.h"
#include "dialogs/OAuth2TokenDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>

namespace poppy::gui {

AuthEditor::AuthEditor(QWidget* parent) : QWidget(parent) {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(8);

    auto* topBar = new QHBoxLayout();
    topBar->addWidget(new QLabel("Auth Type:", this));

    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem("No Auth", static_cast<int>(core::AuthType::None));
    m_typeCombo->addItem("Inherit from Parent", static_cast<int>(core::AuthType::Inherit));
    m_typeCombo->addItem("Bearer Token", static_cast<int>(core::AuthType::Bearer));
    m_typeCombo->addItem("Basic Auth", static_cast<int>(core::AuthType::Basic));
    m_typeCombo->addItem("API Key", static_cast<int>(core::AuthType::ApiKey));
    m_typeCombo->addItem("OAuth 2.0", static_cast<int>(core::AuthType::OAuth2));
    m_typeCombo->addItem("AWS SigV4", static_cast<int>(core::AuthType::AwsSigV4));
    topBar->addWidget(m_typeCombo);
    topBar->addStretch();

    mainLayout->addLayout(topBar);

    m_stack = new QStackedWidget(this);

    // 0: None / Inherit
    m_noneWidget = new QWidget(this);
    auto* noneLayout = new QVBoxLayout(m_noneWidget);
    auto* noneLbl = new QLabel("This request does not use authentication.", m_noneWidget);
    noneLbl->setStyleSheet("color: #71717a; font-style: italic;");
    noneLbl->setAlignment(Qt::AlignCenter);
    noneLayout->addWidget(noneLbl);
    m_stack->addWidget(m_noneWidget);

    // 1: Bearer
    m_bearerWidget = new QWidget(this);
    auto* bearerLayout = new QFormLayout(m_bearerWidget);
    m_bearerTokenEdit = new QLineEdit(m_bearerWidget);
    m_bearerTokenEdit->setPlaceholderText("Enter token or {{variable}}");
    m_bearerTokenEdit->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    connect(m_bearerTokenEdit, &QLineEdit::textChanged, this, &AuthEditor::authChanged);
    bearerLayout->addRow("Token:", m_bearerTokenEdit);
    m_stack->addWidget(m_bearerWidget);

    // 2: Basic
    m_basicWidget = new QWidget(this);
    auto* basicLayout = new QFormLayout(m_basicWidget);
    m_basicUserEdit = new QLineEdit(m_basicWidget);
    m_basicUserEdit->setPlaceholderText("Username or {{username}}");
    m_basicPassEdit = new QLineEdit(m_basicWidget);
    m_basicPassEdit->setPlaceholderText("Password or {{password}}");
    m_basicPassEdit->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    connect(m_basicUserEdit, &QLineEdit::textChanged, this, &AuthEditor::authChanged);
    connect(m_basicPassEdit, &QLineEdit::textChanged, this, &AuthEditor::authChanged);
    basicLayout->addRow("Username:", m_basicUserEdit);
    basicLayout->addRow("Password:", m_basicPassEdit);
    m_stack->addWidget(m_basicWidget);

    // 3: API Key
    m_apiKeyWidget = new QWidget(this);
    auto* apiKeyLayout = new QFormLayout(m_apiKeyWidget);
    m_apiKeyNameEdit = new QLineEdit(m_apiKeyWidget);
    m_apiKeyNameEdit->setPlaceholderText("e.g. X-API-Key");
    m_apiKeyValueEdit = new QLineEdit(m_apiKeyWidget);
    m_apiKeyValueEdit->setPlaceholderText("e.g. 12345 or {{apiKey}}");
    m_apiKeyValueEdit->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    m_apiKeyPlacementCombo = new QComboBox(m_apiKeyWidget);
    m_apiKeyPlacementCombo->addItem("Header", "header");
    m_apiKeyPlacementCombo->addItem("Query Parameter", "query");
    connect(m_apiKeyNameEdit, &QLineEdit::textChanged, this, &AuthEditor::authChanged);
    connect(m_apiKeyValueEdit, &QLineEdit::textChanged, this, &AuthEditor::authChanged);
    connect(m_apiKeyPlacementCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AuthEditor::authChanged);
    apiKeyLayout->addRow("Key Name:", m_apiKeyNameEdit);
    apiKeyLayout->addRow("Value:", m_apiKeyValueEdit);
    apiKeyLayout->addRow("Add to:", m_apiKeyPlacementCombo);
    m_stack->addWidget(m_apiKeyWidget);

    // 4: OAuth 2.0
    m_oauth2Widget = new QWidget(this);
    auto* oauthLayout = new QFormLayout(m_oauth2Widget);
    auto* tokenRow = new QHBoxLayout();
    m_oauth2TokenEdit = new QLineEdit(m_oauth2Widget);
    m_oauth2TokenEdit->setPlaceholderText("Access Token or {{accessToken}}");
    m_oauth2TokenEdit->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    connect(m_oauth2TokenEdit, &QLineEdit::textChanged, this, &AuthEditor::authChanged);
    tokenRow->addWidget(m_oauth2TokenEdit, 1);

    m_oauth2GetTokenBtn = new QPushButton("Get Token...", m_oauth2Widget);
    connect(m_oauth2GetTokenBtn, &QPushButton::clicked, this, &AuthEditor::onGetOAuth2Token);
    tokenRow->addWidget(m_oauth2GetTokenBtn);

    oauthLayout->addRow("Access Token:", tokenRow);
    m_stack->addWidget(m_oauth2Widget);

    // 5: AWS SigV4
    m_awsWidget = new QWidget(this);
    auto* awsLayout = new QFormLayout(m_awsWidget);
    m_awsAccessKeyEdit = new QLineEdit(m_awsWidget);
    m_awsAccessKeyEdit->setPlaceholderText("e.g. AKIAIOSFODNN7EXAMPLE or {{awsAccessKey}}");
    m_awsSecretKeyEdit = new QLineEdit(m_awsWidget);
    m_awsSecretKeyEdit->setPlaceholderText("e.g. wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY or {{awsSecretKey}}");
    m_awsSecretKeyEdit->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    m_awsSessionTokenEdit = new QLineEdit(m_awsWidget);
    m_awsSessionTokenEdit->setPlaceholderText("Session Token (optional for IAM roles)");
    m_awsRegionEdit = new QLineEdit(m_awsWidget);
    m_awsRegionEdit->setPlaceholderText("e.g. us-east-1");
    m_awsServiceEdit = new QLineEdit(m_awsWidget);
    m_awsServiceEdit->setPlaceholderText("e.g. execute-api or s3");

    connect(m_awsAccessKeyEdit, &QLineEdit::textChanged, this, &AuthEditor::authChanged);
    connect(m_awsSecretKeyEdit, &QLineEdit::textChanged, this, &AuthEditor::authChanged);
    connect(m_awsSessionTokenEdit, &QLineEdit::textChanged, this, &AuthEditor::authChanged);
    connect(m_awsRegionEdit, &QLineEdit::textChanged, this, &AuthEditor::authChanged);
    connect(m_awsServiceEdit, &QLineEdit::textChanged, this, &AuthEditor::authChanged);

    awsLayout->addRow("Access Key ID:", m_awsAccessKeyEdit);
    awsLayout->addRow("Secret Access Key:", m_awsSecretKeyEdit);
    awsLayout->addRow("Session Token:", m_awsSessionTokenEdit);
    awsLayout->addRow("AWS Region:", m_awsRegionEdit);
    awsLayout->addRow("Service Name:", m_awsServiceEdit);
    m_stack->addWidget(m_awsWidget);

    mainLayout->addWidget(m_stack);

    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AuthEditor::onTypeChanged);
    onTypeChanged(0);
}

void AuthEditor::onGetOAuth2Token() {
    OAuth2TokenDialog dlg(m_networkEngine, this);
    if (dlg.exec() == QDialog::Accepted) {
        if (!dlg.acquiredToken().isEmpty()) {
            m_oauth2TokenEdit->setText(dlg.acquiredToken());
        }
    }
}

void AuthEditor::onTypeChanged(int index) {
    auto type = static_cast<core::AuthType>(m_typeCombo->itemData(index).toInt());
    switch (type) {
        case core::AuthType::None:
        case core::AuthType::Inherit:
            m_stack->setCurrentWidget(m_noneWidget);
            break;
        case core::AuthType::Bearer:
            m_stack->setCurrentWidget(m_bearerWidget);
            break;
        case core::AuthType::Basic:
            m_stack->setCurrentWidget(m_basicWidget);
            break;
        case core::AuthType::ApiKey:
            m_stack->setCurrentWidget(m_apiKeyWidget);
            break;
        case core::AuthType::OAuth2:
            m_stack->setCurrentWidget(m_oauth2Widget);
            break;
        case core::AuthType::AwsSigV4:
            m_stack->setCurrentWidget(m_awsWidget);
            break;
    }
    emit authChanged();
}

void AuthEditor::loadFromRequest(const core::RequestModel& req) {
    int idx = m_typeCombo->findData(static_cast<int>(req.auth.type));
    if (idx >= 0) {
        m_typeCombo->setCurrentIndex(idx);
    }

    m_bearerTokenEdit->setText(req.auth.bearerToken);
    m_basicUserEdit->setText(req.auth.basicUsername);
    m_basicPassEdit->setText(req.auth.basicPassword);
    m_apiKeyNameEdit->setText(req.auth.apiKeyName);
    m_apiKeyValueEdit->setText(req.auth.apiKeyValue);
    
    int placeIdx = m_apiKeyPlacementCombo->findData(req.auth.apiKeyPlacement.toLower());
    if (placeIdx >= 0) m_apiKeyPlacementCombo->setCurrentIndex(placeIdx);

    m_oauth2TokenEdit->setText(req.auth.oauth2AccessToken);

    m_awsAccessKeyEdit->setText(req.auth.awsAccessKey);
    m_awsSecretKeyEdit->setText(req.auth.awsSecretKey);
    m_awsSessionTokenEdit->setText(req.auth.awsSessionToken);
    m_awsRegionEdit->setText(req.auth.awsRegion.isEmpty() ? "us-east-1" : req.auth.awsRegion);
    m_awsServiceEdit->setText(req.auth.awsService.isEmpty() ? "execute-api" : req.auth.awsService);
}

void AuthEditor::saveToRequest(core::RequestModel& req) const {
    req.auth.type = static_cast<core::AuthType>(m_typeCombo->currentData().toInt());
    req.auth.bearerToken = m_bearerTokenEdit->text();
    req.auth.basicUsername = m_basicUserEdit->text();
    req.auth.basicPassword = m_basicPassEdit->text();
    req.auth.apiKeyName = m_apiKeyNameEdit->text();
    req.auth.apiKeyValue = m_apiKeyValueEdit->text();
    req.auth.apiKeyPlacement = m_apiKeyPlacementCombo->currentData().toString();
    req.auth.oauth2AccessToken = m_oauth2TokenEdit->text();

    req.auth.awsAccessKey = m_awsAccessKeyEdit->text();
    req.auth.awsSecretKey = m_awsSecretKeyEdit->text();
    req.auth.awsSessionToken = m_awsSessionTokenEdit->text();
    req.auth.awsRegion = m_awsRegionEdit->text();
    req.auth.awsService = m_awsServiceEdit->text();
}

} // namespace poppy::gui
