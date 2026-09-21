#pragma once

#include <QDialog>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <core/RequestModel.h>
#include <core/codegen/CodeGenerator.h>

namespace poppy::gui {

class CodeSnippetDialog : public QDialog {
    Q_OBJECT
public:
    explicit CodeSnippetDialog(const core::RequestModel& req, QWidget* parent = nullptr);

private slots:
    void onLanguageChanged(int index);
    void copyToClipboard();

private:
    void updateSnippet();

    core::RequestModel m_request;
    QComboBox* m_langCombo;
    QPlainTextEdit* m_codeViewer;
    QPushButton* m_copyBtn;
    QPushButton* m_closeBtn;
};

} // namespace poppy::gui
