#include "ImportDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <core/importers/CurlImporter.h>
#include <core/importers/PostmanImporter.h>
#include <core/importers/OpenApiImporter.h>

namespace poppy::gui {

ImportDialog::ImportDialog(const QString& defaultOutputDir, QWidget* parent)
    : QDialog(parent), m_defaultOutputDir(defaultOutputDir) {
    setWindowTitle("Import Request / Collection");
    resize(650, 420);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    m_tabWidget = new QTabWidget(this);

    // 1. Tab cURL
    auto* curlTab = new QWidget(this);
    auto* curlLayout = new QVBoxLayout(curlTab);
    curlLayout->addWidget(new QLabel("Paste cURL command below:", curlTab));

    m_curlEdit = new QPlainTextEdit(curlTab);
    m_curlEdit->setPlaceholderText("curl -X POST https://api.example.com/users -H 'Content-Type: application/json' -d '{\"name\":\"Alice\"}'");
    QFont font("Consolas", 10);
    if (!font.exactMatch()) font = QFont("Courier New", 10);
    m_curlEdit->setFont(font);
    curlLayout->addWidget(m_curlEdit);

    auto* curlBtnLayout = new QHBoxLayout();
    curlBtnLayout->addStretch();
    m_importCurlBtn = new QPushButton("Import into Current Request", curlTab);
    m_importCurlBtn->setObjectName("primaryBtn");
    connect(m_importCurlBtn, &QPushButton::clicked, this, &ImportDialog::onImportCurl);
    curlBtnLayout->addWidget(m_importCurlBtn);
    curlLayout->addLayout(curlBtnLayout);

    m_tabWidget->addTab(curlTab, "cURL Command");

    // 2. Tab Postman
    auto* postmanTab = new QWidget(this);
    auto* postmanLayout = new QVBoxLayout(postmanTab);
    postmanLayout->addWidget(new QLabel("Select Postman Collection v2.1 export JSON:", postmanTab));

    auto* pFileLayout = new QHBoxLayout();
    m_postmanFileEdit = new QLineEdit(postmanTab);
    m_postmanFileEdit->setPlaceholderText("Path to Postman JSON collection...");
    pFileLayout->addWidget(m_postmanFileEdit);
    auto* browsePFileBtn = new QPushButton("Browse...", postmanTab);
    connect(browsePFileBtn, &QPushButton::clicked, this, [this]() {
        QString f = QFileDialog::getOpenFileName(this, "Select Postman Collection", QString(), "JSON Files (*.json)");
        if (!f.isEmpty()) m_postmanFileEdit->setText(f);
    });
    pFileLayout->addWidget(browsePFileBtn);
    postmanLayout->addLayout(pFileLayout);

    postmanLayout->addWidget(new QLabel("Destination Directory for Poppy Collection:", postmanTab));
    auto* pDestLayout = new QHBoxLayout();
    m_postmanDestEdit = new QLineEdit(m_defaultOutputDir, postmanTab);
    pDestLayout->addWidget(m_postmanDestEdit);
    auto* browsePDestBtn = new QPushButton("Browse...", postmanTab);
    connect(browsePDestBtn, &QPushButton::clicked, this, [this]() {
        QString d = QFileDialog::getExistingDirectory(this, "Select Destination Directory", m_postmanDestEdit->text());
        if (!d.isEmpty()) m_postmanDestEdit->setText(d);
    });
    pDestLayout->addWidget(browsePDestBtn);
    postmanLayout->addLayout(pDestLayout);

    postmanLayout->addStretch();
    auto* postmanBtnLayout = new QHBoxLayout();
    postmanBtnLayout->addStretch();
    m_importPostmanBtn = new QPushButton("Import Postman Collection", postmanTab);
    m_importPostmanBtn->setObjectName("primaryBtn");
    connect(m_importPostmanBtn, &QPushButton::clicked, this, &ImportDialog::onImportPostman);
    postmanBtnLayout->addWidget(m_importPostmanBtn);
    postmanLayout->addLayout(postmanBtnLayout);

    m_tabWidget->addTab(postmanTab, "Postman Collection");

    // 3. Tab OpenAPI
    auto* openApiTab = new QWidget(this);
    auto* openApiLayout = new QVBoxLayout(openApiTab);
    openApiLayout->addWidget(new QLabel("Select OpenAPI / Swagger 3.0 JSON specification:", openApiTab));

    auto* oFileLayout = new QHBoxLayout();
    m_openApiFileEdit = new QLineEdit(openApiTab);
    m_openApiFileEdit->setPlaceholderText("Path to OpenAPI JSON spec...");
    oFileLayout->addWidget(m_openApiFileEdit);
    auto* browseOFileBtn = new QPushButton("Browse...", openApiTab);
    connect(browseOFileBtn, &QPushButton::clicked, this, [this]() {
        QString f = QFileDialog::getOpenFileName(this, "Select OpenAPI Spec", QString(), "JSON Files (*.json)");
        if (!f.isEmpty()) m_openApiFileEdit->setText(f);
    });
    oFileLayout->addWidget(browseOFileBtn);
    openApiLayout->addLayout(oFileLayout);

    openApiLayout->addWidget(new QLabel("Destination Directory for Poppy Collection:", openApiTab));
    auto* oDestLayout = new QHBoxLayout();
    m_openApiDestEdit = new QLineEdit(m_defaultOutputDir, openApiTab);
    oDestLayout->addWidget(m_openApiDestEdit);
    auto* browseODestBtn = new QPushButton("Browse...", openApiTab);
    connect(browseODestBtn, &QPushButton::clicked, this, [this]() {
        QString d = QFileDialog::getExistingDirectory(this, "Select Destination Directory", m_openApiDestEdit->text());
        if (!d.isEmpty()) m_openApiDestEdit->setText(d);
    });
    oDestLayout->addWidget(browseODestBtn);
    openApiLayout->addLayout(oDestLayout);

    openApiLayout->addStretch();
    auto* openApiBtnLayout = new QHBoxLayout();
    openApiBtnLayout->addStretch();
    m_importOpenApiBtn = new QPushButton("Import OpenAPI Spec", openApiTab);
    m_importOpenApiBtn->setObjectName("primaryBtn");
    connect(m_importOpenApiBtn, &QPushButton::clicked, this, &ImportDialog::onImportOpenApi);
    openApiBtnLayout->addWidget(m_importOpenApiBtn);
    openApiLayout->addLayout(openApiBtnLayout);

    m_tabWidget->addTab(openApiTab, "OpenAPI (v3)");

    mainLayout->addWidget(m_tabWidget);
}

void ImportDialog::onImportCurl() {
    QString cmd = m_curlEdit->toPlainText().trimmed();
    if (cmd.isEmpty()) {
        QMessageBox::warning(this, "Empty Input", "Please paste a cURL command.");
        return;
    }

    core::RequestModel req = core::CurlImporter::importCurl(cmd);
    emit curlImported(req);
    accept();
}

void ImportDialog::onImportPostman() {
    QString file = m_postmanFileEdit->text().trimmed();
    QString dest = m_postmanDestEdit->text().trimmed();

    if (file.isEmpty() || dest.isEmpty()) {
        QMessageBox::warning(this, "Missing Path", "Please provide both the Postman file and destination directory.");
        return;
    }

    QString err;
    if (core::PostmanImporter::importCollection(file, dest, &err)) {
        QMessageBox::information(this, "Import Complete", "Postman collection imported successfully!");
        emit collectionImported(dest);
        accept();
    } else {
        QMessageBox::critical(this, "Import Failed", "Failed to import Postman collection:\n" + err);
    }
}

void ImportDialog::onImportOpenApi() {
    QString file = m_openApiFileEdit->text().trimmed();
    QString dest = m_openApiDestEdit->text().trimmed();

    if (file.isEmpty() || dest.isEmpty()) {
        QMessageBox::warning(this, "Missing Path", "Please provide both the OpenAPI file and destination directory.");
        return;
    }

    QString err;
    if (core::OpenApiImporter::importSpec(file, dest, &err)) {
        QMessageBox::information(this, "Import Complete", "OpenAPI spec imported successfully!");
        emit collectionImported(dest);
        accept();
    } else {
        QMessageBox::critical(this, "Import Failed", "Failed to import OpenAPI spec:\n" + err);
    }
}

} // namespace poppy::gui
