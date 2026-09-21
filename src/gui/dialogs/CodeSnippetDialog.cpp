#include "CodeSnippetDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QClipboard>
#include <QGuiApplication>

namespace poppy::gui {

CodeSnippetDialog::CodeSnippetDialog(const core::RequestModel& req, QWidget* parent)
    : QDialog(parent), m_request(req) {
    setWindowTitle("Generate Code Snippet");
    resize(720, 500);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    auto* topLayout = new QHBoxLayout();
    topLayout->addWidget(new QLabel("Select Language:", this));

    m_langCombo = new QComboBox(this);
    m_langCombo->addItem("Python (requests)", static_cast<int>(core::TargetLanguage::PythonRequests));
    m_langCombo->addItem("JavaScript (fetch)", static_cast<int>(core::TargetLanguage::JavaScriptFetch));
    m_langCombo->addItem("JavaScript (axios)", static_cast<int>(core::TargetLanguage::JavaScriptAxios));
    m_langCombo->addItem("Go (net/http)", static_cast<int>(core::TargetLanguage::GoHttp));
    m_langCombo->addItem("Rust (reqwest)", static_cast<int>(core::TargetLanguage::RustReqwest));
    m_langCombo->addItem("C# (HttpClient)", static_cast<int>(core::TargetLanguage::CSharpHttpClient));
    m_langCombo->addItem("Java (java.net.http)", static_cast<int>(core::TargetLanguage::JavaHttpClient));
    m_langCombo->addItem("C++ (libcurl)", static_cast<int>(core::TargetLanguage::CppCurl));
    m_langCombo->addItem("cURL Command", static_cast<int>(core::TargetLanguage::Curl));
    topLayout->addWidget(m_langCombo, 1);
    topLayout->addStretch();

    mainLayout->addLayout(topLayout);

    m_codeViewer = new QPlainTextEdit(this);
    m_codeViewer->setReadOnly(true);
    QFont font("Consolas", 10);
    if (!font.exactMatch()) font = QFont("Courier New", 10);
    m_codeViewer->setFont(font);
    mainLayout->addWidget(m_codeViewer);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_copyBtn = new QPushButton("Copy to Clipboard", this);
    m_copyBtn->setObjectName("primaryBtn");
    connect(m_copyBtn, &QPushButton::clicked, this, &CodeSnippetDialog::copyToClipboard);
    btnLayout->addWidget(m_copyBtn);

    m_closeBtn = new QPushButton("Close", this);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(m_closeBtn);

    mainLayout->addLayout(btnLayout);

    connect(m_langCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CodeSnippetDialog::onLanguageChanged);

    updateSnippet();
}

void CodeSnippetDialog::onLanguageChanged(int /*index*/) {
    updateSnippet();
}

void CodeSnippetDialog::updateSnippet() {
    auto lang = static_cast<core::TargetLanguage>(m_langCombo->currentData().toInt());
    QString code = core::CodeGenerator::generate(lang, m_request);
    m_codeViewer->setPlainText(code);
    m_copyBtn->setText("Copy to Clipboard");
}

void CodeSnippetDialog::copyToClipboard() {
    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setText(m_codeViewer->toPlainText());
    m_copyBtn->setText("Copied!");
}

} // namespace poppy::gui
