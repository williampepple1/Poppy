#pragma once

#include <QWidget>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <components/KeyValueTable.h>
#include <components/JsonSyntaxHighlighter.h>
#include <core/RequestModel.h>

namespace poppy::gui {

class BodyEditor : public QWidget {
    Q_OBJECT
public:
    explicit BodyEditor(QWidget* parent = nullptr);

    void loadFromRequest(const core::RequestModel& req);
    void saveToRequest(core::RequestModel& req) const;

signals:
    void bodyChanged();

private slots:
    void onFormatChanged(int index);
    void formatJson();
    void minifyJson();
    void validateJson();

private:
    QComboBox* m_typeCombo;
    QPushButton* m_formatBtn;
    QPushButton* m_minifyBtn;
    QLabel* m_jsonStatusLabel;
    QStackedWidget* m_stack;

    // View 0: None
    QWidget* m_noneWidget;

    // View 1: Text / JSON / XML Editor
    QPlainTextEdit* m_codeEditor;
    JsonSyntaxHighlighter* m_jsonHighlighter;

    // View 2: Form Url Encoded
    KeyValueTable* m_formTable;

    // View 3: Multipart Form (with file uploads)
    KeyValueTable* m_multipartTable;

    // View 4: GraphQL Editor
    QWidget* m_gqlWidget;
    QPlainTextEdit* m_gqlQueryEditor;
    QPlainTextEdit* m_gqlVarsEditor;

    // View 5: Binary file
    QWidget* m_binaryWidget;
    QLineEdit* m_binaryPathEdit;
};

} // namespace poppy::gui
