#include "OAuth2TokenDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QUrlQuery>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>

namespace poppy::gui {

OAuth2TokenDialog::OAuth2TokenDialog(network::CurlNetworkEngine* engine, QWidget* parent)
    : QDialog(parent), m_engine(engine) {
    setWindowTitle("OAuth 2.0 Token Helper");
    resize(520, 380);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    auto* form = new QFormLayout();

    m_grantTypeCombo = new QComboBox(this);
    m_grantTypeCombo->addItem("Client Credentials", "client_credentials");
    m_grantTypeCombo->addItem("Authorization Code (PKCE / Loopback)", "authorization_code");
    connect(m_grantTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OAuth2TokenDialog::onGrantTypeChanged);
    form->addRow("Grant Type:", m_grantTypeCombo);

    m_authUrlEdit = new QLineEdit(this);
    m_authUrlEdit->setPlaceholderText("https://auth.example.com/oauth/authorize");
    form->addRow("Authorization URL:", m_authUrlEdit);

    m_tokenUrlEdit = new QLineEdit(this);
    m_tokenUrlEdit->setPlaceholderText("https://auth.example.com/oauth/token");
    form->addRow("Access Token URL:", m_tokenUrlEdit);

    m_clientIdEdit = new QLineEdit(this);
    form->addRow("Client ID:", m_clientIdEdit);

    m_clientSecretEdit = new QLineEdit(this);
    m_clientSecretEdit->setEchoMode(QLineEdit::Password);
    form->addRow("Client Secret:", m_clientSecretEdit);

    m_scopeEdit = new QLineEdit(this);
    m_scopeEdit->setPlaceholderText("e.g. read write (optional)");
    form->addRow("Scope:", m_scopeEdit);

    m_redirectPortEdit = new QLineEdit("8089", this);
    form->addRow("Loopback Port:", m_redirectPortEdit);

    mainLayout->addLayout(form);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: #a1a1aa; font-style: italic;");
    mainLayout->addWidget(m_statusLabel);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    auto* cancelBtn = new QPushButton("Cancel", this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    m_fetchBtn = new QPushButton("Get Token", this);
    m_fetchBtn->setDefault(true);
    connect(m_fetchBtn, &QPushButton::clicked, this, &OAuth2TokenDialog::onFetchTokenClicked);
    btnLayout->addWidget(m_fetchBtn);

    mainLayout->addLayout(btnLayout);

    onGrantTypeChanged(0);
}

OAuth2TokenDialog::~OAuth2TokenDialog() {
    if (m_server) {
        m_server->close();
        delete m_server;
    }
}

void OAuth2TokenDialog::onGrantTypeChanged(int index) {
    bool isAuthCode = (m_grantTypeCombo->itemData(index).toString() == "authorization_code");
    m_authUrlEdit->setVisible(isAuthCode);
    m_redirectPortEdit->setVisible(isAuthCode);
    m_fetchBtn->setText(isAuthCode ? "Authorize in Browser" : "Get Token");
}

void OAuth2TokenDialog::onFetchTokenClicked() {
    QString grantType = m_grantTypeCombo->currentData().toString();
    if (grantType == "client_credentials") {
        fetchClientCredentialsToken();
    } else {
        startAuthCodeFlow();
    }
}

void OAuth2TokenDialog::fetchClientCredentialsToken() {
    QString tokenUrl = m_tokenUrlEdit->text().trimmed();
    if (tokenUrl.isEmpty()) {
        QMessageBox::warning(this, "Missing URL", "Please provide the Access Token URL.");
        return;
    }

    m_statusLabel->setText("Requesting access token...");

    core::RequestModel req;
    req.method = core::HttpMethod::POST;
    req.url = tokenUrl;
    req.bodyType = core::BodyType::FormUrlEncoded;
    req.headers.append(core::HttpHeader{.name = "Content-Type", .value = "application/x-www-form-urlencoded", .enabled = true});

    QStringList bodyParts;
    bodyParts.append("grant_type=client_credentials");
    bodyParts.append("client_id=" + QUrl::toPercentEncoding(m_clientIdEdit->text().trimmed()));
    bodyParts.append("client_secret=" + QUrl::toPercentEncoding(m_clientSecretEdit->text().trimmed()));
    if (!m_scopeEdit->text().trimmed().isEmpty()) {
        bodyParts.append("scope=" + QUrl::toPercentEncoding(m_scopeEdit->text().trimmed()));
    }
    req.bodyContent = bodyParts.join('&');

    core::ResponseModel res = m_engine ? m_engine->sendRequestSync(req) : core::ResponseModel{};
    if (res.isHttpSuccess()) {
        QJsonDocument doc = QJsonDocument::fromJson(res.rawBody);
        if (doc.isObject() && doc.object().contains("access_token")) {
            m_accessToken = doc.object().value("access_token").toString();
            emit tokenAcquired(m_accessToken);
            m_statusLabel->setText("Token successfully acquired!");
            accept();
            return;
        }
    }

    QString err = res.errorString.isEmpty() ? res.bodyAsString() : res.errorString;
    m_statusLabel->setText("Failed to acquire token: " + err);
    QMessageBox::critical(this, "OAuth Error", "Failed to retrieve access token.\n" + err);
}

void OAuth2TokenDialog::startAuthCodeFlow() {
    QString authUrl = m_authUrlEdit->text().trimmed();
    QString tokenUrl = m_tokenUrlEdit->text().trimmed();
    if (authUrl.isEmpty() || tokenUrl.isEmpty()) {
        QMessageBox::warning(this, "Missing URL", "Please provide both Authorization URL and Access Token URL.");
        return;
    }

    quint16 port = static_cast<quint16>(m_redirectPortEdit->text().toUShort());
    if (port == 0) port = 8089;

    if (!m_server) {
        m_server = new QTcpServer(this);
        connect(m_server, &QTcpServer::newConnection, this, &OAuth2TokenDialog::onNewTcpConnection);
    }

    if (m_server->isListening()) {
        m_server->close();
    }

    if (!m_server->listen(QHostAddress::LocalHost, port)) {
        QMessageBox::critical(this, "Loopback Server Error", "Could not start local callback server on port " + QString::number(port));
        return;
    }

    // Build Authorization URL with query parameters
    QUrl url(authUrl);
    QUrlQuery query(url.query());
    query.addQueryItem("response_type", "code");
    query.addQueryItem("client_id", m_clientIdEdit->text().trimmed());
    query.addQueryItem("redirect_uri", QString("http://127.0.0.1:%1/callback").arg(port));
    if (!m_scopeEdit->text().trimmed().isEmpty()) {
        query.addQueryItem("scope", m_scopeEdit->text().trimmed());
    }
    url.setQuery(query);

    m_statusLabel->setText("Waiting for browser authorization on http://127.0.0.1:" + QString::number(port) + "...");
    QDesktopServices::openUrl(url);
}

void OAuth2TokenDialog::onNewTcpConnection() {
    QTcpSocket* socket = m_server->nextPendingConnection();
    if (!socket) return;

    connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
        m_callbackBuffers.remove(socket);
        socket->deleteLater();
    });

    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
        m_callbackBuffers[socket].append(socket->readAll());
        const QByteArray& requestData = m_callbackBuffers[socket];
        const int headerEnd = requestData.indexOf("\r\n\r\n");
        if (headerEnd < 0) return;

        QString requestStr = QString::fromUtf8(requestData.left(headerEnd));

        QString code;
        int firstLineEnd = requestStr.indexOf("\r\n");
        if (firstLineEnd > 0) {
            QString firstLine = requestStr.left(firstLineEnd);
            int pathStart = firstLine.indexOf(' ');
            int pathEnd = firstLine.lastIndexOf(' ');
            if (pathStart > 0 && pathEnd > pathStart) {
                QString path = firstLine.mid(pathStart + 1, pathEnd - pathStart - 1);
                QUrl url(path);
                QUrlQuery query(url.query());
                code = query.queryItemValue("code");
            }
        }

        QByteArray html = "<!DOCTYPE html><html><body style='font-family: sans-serif; text-align: center; padding: 50px;'>"
                          "<h2 style='color: #22c55e;'>Poppy: Authorization Successful!</h2>"
                          "<p>You can close this window and return to the application.</p>"
                          "</body></html>";
        QByteArray response = "HTTP/1.1 200 OK\r\n"
                              "Content-Type: text/html; charset=utf-8\r\n"
                              "Connection: close\r\n"
                              "Content-Length: " + QByteArray::number(html.size()) + "\r\n\r\n" + html;
        socket->write(response);
        socket->flush();
        socket->disconnectFromHost();
        m_callbackBuffers.remove(socket);

        if (m_server) {
            m_server->close();
        }

        if (!code.isEmpty()) {
            m_statusLabel->setText("Exchanging authorization code for token...");
            exchangeCodeForToken(code);
        } else {
            m_statusLabel->setText("Authorization failed: No code received in callback.");
        }
    });
}

void OAuth2TokenDialog::exchangeCodeForToken(const QString& code) {
    quint16 port = static_cast<quint16>(m_redirectPortEdit->text().toUShort());
    if (port == 0) port = 8089;

    core::RequestModel req;
    req.method = core::HttpMethod::POST;
    req.url = m_tokenUrlEdit->text().trimmed();
    req.bodyType = core::BodyType::FormUrlEncoded;
    req.headers.append(core::HttpHeader{.name = "Content-Type", .value = "application/x-www-form-urlencoded", .enabled = true});

    QStringList bodyParts;
    bodyParts.append("grant_type=authorization_code");
    bodyParts.append("code=" + QUrl::toPercentEncoding(code));
    bodyParts.append("redirect_uri=" + QUrl::toPercentEncoding(QString("http://127.0.0.1:%1/callback").arg(port)));
    bodyParts.append("client_id=" + QUrl::toPercentEncoding(m_clientIdEdit->text().trimmed()));
    bodyParts.append("client_secret=" + QUrl::toPercentEncoding(m_clientSecretEdit->text().trimmed()));
    req.bodyContent = bodyParts.join('&');

    core::ResponseModel res = m_engine ? m_engine->sendRequestSync(req) : core::ResponseModel{};
    if (res.isHttpSuccess()) {
        QJsonDocument doc = QJsonDocument::fromJson(res.rawBody);
        if (doc.isObject() && doc.object().contains("access_token")) {
            m_accessToken = doc.object().value("access_token").toString();
            emit tokenAcquired(m_accessToken);
            m_statusLabel->setText("Token successfully acquired!");
            accept();
            return;
        }
    }

    QString err = res.errorString.isEmpty() ? res.bodyAsString() : res.errorString;
    m_statusLabel->setText("Failed to exchange code: " + err);
    QMessageBox::critical(this, "OAuth Exchange Error", "Failed to exchange authorization code for token.\n" + err);
}

} // namespace poppy::gui
