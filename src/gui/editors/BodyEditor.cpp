#include "BodyEditor.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QJsonDocument>

namespace poppy::gui {

BodyEditor::BodyEditor(QWidget* parent) : QWidget(parent) {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(8);

    // Top control bar
    auto* topBar = new QHBoxLayout();
    topBar->addWidget(new QLabel("Body Format:", this));

    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem("None", static_cast<int>(core::BodyType::None));
    m_typeCombo->addItem("JSON", static_cast<int>(core::BodyType::Json));
    m_typeCombo->addItem("Text", static_cast<int>(core::BodyType::Text));
    m_typeCombo->addItem("XML", static_cast<int>(core::BodyType::Xml));
    m_typeCombo->addItem("Form URL-Encoded", static_cast<int>(core::BodyType::FormUrlEncoded));
    m_typeCombo->addItem("Multipart Form", static_cast<int>(core::BodyType::MultipartForm));
    m_typeCombo->addItem("GraphQL", static_cast<int>(core::BodyType::GraphQL));
    topBar->addWidget(m_typeCombo);

    m_formatBtn = new QPushButton("Prettify", this);
    m_formatBtn->setToolTip("Format JSON with clean indentation");
    connect(m_formatBtn, &QPushButton::clicked, this, &BodyEditor::formatJson);
    topBar->addWidget(m_formatBtn);

    m_minifyBtn = new QPushButton("Minify", this);
    m_minifyBtn->setToolTip("Compress JSON by removing unnecessary whitespace");
    connect(m_minifyBtn, &QPushButton::clicked, this, &BodyEditor::minifyJson);
    topBar->addWidget(m_minifyBtn);

    m_jsonStatusLabel = new QLabel(this);
    topBar->addWidget(m_jsonStatusLabel);
    topBar->addStretch();

    mainLayout->addLayout(topBar);

    // Stacked widget for format views
    m_stack = new QStackedWidget(this);

    // View 0: None
    m_noneWidget = new QWidget(this);
    auto* noneLayout = new QVBoxLayout(m_noneWidget);
    auto* noneLabel = new QLabel("This request does not have a body.", m_noneWidget);
    noneLabel->setStyleSheet("color: #71717a; font-style: italic;");
    noneLabel->setAlignment(Qt::AlignCenter);
    noneLayout->addWidget(noneLabel);
    m_stack->addWidget(m_noneWidget);

    // View 1: Text Editor (JSON, XML, Text)
    m_codeEditor = new QPlainTextEdit(this);
    m_codeEditor->setPlaceholderText("Enter request body...");
    QFont font("Consolas", 10);
    if (!font.exactMatch()) font = QFont("Courier New", 10);
    m_codeEditor->setFont(font);
    m_jsonHighlighter = new JsonSyntaxHighlighter(m_codeEditor->document());
    connect(m_codeEditor, &QPlainTextEdit::textChanged, this, &BodyEditor::bodyChanged);
    connect(m_codeEditor, &QPlainTextEdit::textChanged, this, &BodyEditor::validateJson);
    m_stack->addWidget(m_codeEditor);

    // View 2: Form Url Encoded Table
    m_formTable = new KeyValueTable(false, this);
    connect(m_formTable, &KeyValueTable::dataChanged, this, &BodyEditor::bodyChanged);
    m_stack->addWidget(m_formTable);

    // View 3: Multipart Form Table (with file attachments)
    m_multipartTable = new KeyValueTable(true, this);
    m_multipartTable->setAllowFiles(true);
    connect(m_multipartTable, &KeyValueTable::dataChanged, this, &BodyEditor::bodyChanged);
    m_stack->addWidget(m_multipartTable);

    // View 4: GraphQL Editor
    m_gqlWidget = new QWidget(this);
    auto* gqlLayout = new QVBoxLayout(m_gqlWidget);
    gqlLayout->setContentsMargins(0, 0, 0, 0);
    gqlLayout->setSpacing(4);

    gqlLayout->addWidget(new QLabel("Query:", m_gqlWidget));
    m_gqlQueryEditor = new QPlainTextEdit(m_gqlWidget);
    m_gqlQueryEditor->setFont(font);
    m_gqlQueryEditor->setPlaceholderText("query {\n  users {\n    id\n    name\n  }\n}");
    connect(m_gqlQueryEditor, &QPlainTextEdit::textChanged, this, &BodyEditor::bodyChanged);
    gqlLayout->addWidget(m_gqlQueryEditor, 2);

    gqlLayout->addWidget(new QLabel("Variables (JSON):", m_gqlWidget));
    m_gqlVarsEditor = new QPlainTextEdit(m_gqlWidget);
    m_gqlVarsEditor->setFont(font);
    m_gqlVarsEditor->setPlaceholderText("{\n  \"limit\": 10\n}");
    new JsonSyntaxHighlighter(m_gqlVarsEditor->document());
    connect(m_gqlVarsEditor, &QPlainTextEdit::textChanged, this, &BodyEditor::bodyChanged);
    gqlLayout->addWidget(m_gqlVarsEditor, 1);

    m_stack->addWidget(m_gqlWidget);

    mainLayout->addWidget(m_stack);

    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BodyEditor::onFormatChanged);
    onFormatChanged(0);
}

void BodyEditor::onFormatChanged(int index) {
    auto type = static_cast<core::BodyType>(m_typeCombo->itemData(index).toInt());
    bool isJson = (type == core::BodyType::Json);
    m_formatBtn->setVisible(isJson);
    m_minifyBtn->setVisible(isJson);
    m_jsonStatusLabel->setVisible(isJson);

    if (type == core::BodyType::None) {
        m_stack->setCurrentWidget(m_noneWidget);
    } else if (type == core::BodyType::FormUrlEncoded) {
        m_stack->setCurrentWidget(m_formTable);
    } else if (type == core::BodyType::MultipartForm) {
        m_stack->setCurrentWidget(m_multipartTable);
    } else if (type == core::BodyType::GraphQL) {
        m_stack->setCurrentWidget(m_gqlWidget);
    } else {
        m_stack->setCurrentWidget(m_codeEditor);
        if (isJson) validateJson();
        else m_jsonStatusLabel->clear();
    }
    emit bodyChanged();
}

void BodyEditor::formatJson() {
    QString raw = m_codeEditor->toPlainText().trimmed();
    if (raw.isEmpty()) return;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8(), &err);
    if (err.error == QJsonParseError::NoError && !doc.isNull()) {
        m_codeEditor->setPlainText(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
        validateJson();
    }
}

void BodyEditor::minifyJson() {
    QString raw = m_codeEditor->toPlainText().trimmed();
    if (raw.isEmpty()) return;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8(), &err);
    if (err.error == QJsonParseError::NoError && !doc.isNull()) {
        m_codeEditor->setPlainText(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
        validateJson();
    }
}

void BodyEditor::validateJson() {
    auto type = static_cast<core::BodyType>(m_typeCombo->currentData().toInt());
    if (type != core::BodyType::Json) {
        m_jsonStatusLabel->clear();
        return;
    }
    QString raw = m_codeEditor->toPlainText().trimmed();
    if (raw.isEmpty()) {
        m_jsonStatusLabel->clear();
        return;
    }
    QJsonParseError err;
    QJsonDocument::fromJson(raw.toUtf8(), &err);
    if (err.error == QJsonParseError::NoError) {
        m_jsonStatusLabel->setText("✓ Valid JSON");
        m_jsonStatusLabel->setStyleSheet("color: #10b981; font-size: 11px; font-weight: bold; margin-left: 8px;");
    } else {
        m_jsonStatusLabel->setText(QString("⚠ %1 (offset %2)").arg(err.errorString()).arg(err.offset));
        m_jsonStatusLabel->setStyleSheet("color: #ef4444; font-size: 11px; margin-left: 8px;");
    }
}

void BodyEditor::loadFromRequest(const core::RequestModel& req) {
    int idx = m_typeCombo->findData(static_cast<int>(req.bodyType));
    if (idx >= 0) {
        m_typeCombo->setCurrentIndex(idx);
    }

    if (req.bodyType == core::BodyType::GraphQL) {
        m_gqlQueryEditor->setPlainText(req.graphqlQuery);
        m_gqlVarsEditor->setPlainText(req.graphqlVariables);
    } else if (req.bodyType == core::BodyType::MultipartForm) {
        m_multipartTable->setFormData(req.formDataParams);
    } else if (req.bodyType == core::BodyType::FormUrlEncoded) {
        // Parse key=val&... lines into table
        QList<core::HttpParam> params;
        QStringList pairs = req.bodyContent.split('&', Qt::SkipEmptyParts);
        for (const auto& pair : pairs) {
            int eq = pair.indexOf('=');
            if (eq > 0) {
                params.append({.key = pair.left(eq), .value = pair.mid(eq + 1), .enabled = true});
            }
        }
        m_formTable->setParams(params);
    } else {
        m_codeEditor->setPlainText(req.bodyContent);
    }
}

void BodyEditor::saveToRequest(core::RequestModel& req) const {
    req.bodyType = static_cast<core::BodyType>(m_typeCombo->currentData().toInt());

    if (req.bodyType == core::BodyType::None) {
        req.bodyContent.clear();
    } else if (req.bodyType == core::BodyType::GraphQL) {
        req.graphqlQuery = m_gqlQueryEditor->toPlainText();
        req.graphqlVariables = m_gqlVarsEditor->toPlainText();
        req.bodyContent = req.graphqlQuery;
    } else if (req.bodyType == core::BodyType::MultipartForm) {
        req.formDataParams = m_multipartTable->formData();
        QStringList summaries;
        for (const auto& p : req.formDataParams) {
            if (p.enabled) summaries.append(p.key + (p.isFile ? "=@" : "=") + p.value);
        }
        req.bodyContent = summaries.join("; ");
    } else if (req.bodyType == core::BodyType::FormUrlEncoded) {
        QStringList parts;
        for (const auto& p : m_formTable->params()) {
            if (p.enabled && !p.key.isEmpty()) {
                parts.append(p.key + "=" + p.value);
            }
        }
        req.bodyContent = parts.join('&');
    } else {
        req.bodyContent = m_codeEditor->toPlainText();
    }
}

} // namespace poppy::gui
