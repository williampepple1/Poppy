#include "AuthEditor.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>

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
    m_oauth2TokenEdit = new QLineEdit(m_oauth2Widget);
    m_oauth2TokenEdit->setPlaceholderText("Access Token or {{accessToken}}");
    m_oauth2TokenEdit->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    connect(m_oauth2TokenEdit, &QLineEdit::textChanged, this, &AuthEditor::authChanged);
    oauthLayout->addRow("Access Token:", m_oauth2TokenEdit);
    m_stack->addWidget(m_oauth2Widget);

    mainLayout->addWidget(m_stack);

    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AuthEditor::onTypeChanged);
    onTypeChanged(0);
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
}

} // namespace poppy::gui
