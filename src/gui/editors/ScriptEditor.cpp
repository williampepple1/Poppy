#include "ScriptEditor.h"
#include <QVBoxLayout>

namespace poppy::gui {

ScriptEditor::ScriptEditor(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_tabWidget = new QTabWidget(this);

    QFont codeFont("Consolas", 10);
    if (!codeFont.exactMatch()) codeFont = QFont("Courier New", 10);

    m_preRequestEdit = new QPlainTextEdit(this);
    m_preRequestEdit->setFont(codeFont);
    m_preRequestEdit->setPlaceholderText("// Pre-request script runs before sending HTTP request\n// poppy.setEnvVar('timestamp', Date.now());");

    m_postResponseEdit = new QPlainTextEdit(this);
    m_postResponseEdit->setFont(codeFont);
    m_postResponseEdit->setPlaceholderText("// Post-response script runs after receiving response\n// const data = res.getBody();\n// poppy.setEnvVar('token', data.token);");

    m_testsEdit = new QPlainTextEdit(this);
    m_testsEdit->setFont(codeFont);
    m_testsEdit->setPlaceholderText("// Test assertions\ntest(\"Status is 200\", function() {\n  expect(res.getStatus()).to.equal(200);\n});");

    connect(m_preRequestEdit, &QPlainTextEdit::textChanged, this, &ScriptEditor::scriptChanged);
    connect(m_postResponseEdit, &QPlainTextEdit::textChanged, this, &ScriptEditor::scriptChanged);
    connect(m_testsEdit, &QPlainTextEdit::textChanged, this, &ScriptEditor::scriptChanged);

    m_tabWidget->addTab(m_preRequestEdit, "Pre-request Script");
    m_tabWidget->addTab(m_postResponseEdit, "Post-response Script");
    m_tabWidget->addTab(m_testsEdit, "Tests");

    layout->addWidget(m_tabWidget);
}

void ScriptEditor::loadFromRequest(const core::RequestModel& req) {
    m_preRequestEdit->setPlainText(req.scripts.preRequestScript);
    m_postResponseEdit->setPlainText(req.scripts.postResponseScript);
    m_testsEdit->setPlainText(req.scripts.tests);
}

void ScriptEditor::saveToRequest(core::RequestModel& req) const {
    req.scripts.preRequestScript = m_preRequestEdit->toPlainText();
    req.scripts.postResponseScript = m_postResponseEdit->toPlainText();
    req.scripts.tests = m_testsEdit->toPlainText();
}

} // namespace poppy::gui
