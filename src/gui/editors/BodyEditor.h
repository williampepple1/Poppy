#pragma once

#include <QWidget>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QStackedWidget>
#include <QPushButton>
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

private:
    QComboBox* m_typeCombo;
    QPushButton* m_formatBtn;
    QStackedWidget* m_stack;

    // View 0: None
    QWidget* m_noneWidget;

    // View 1: Text / JSON / XML Editor
    QPlainTextEdit* m_codeEditor;
    JsonSyntaxHighlighter* m_jsonHighlighter;

    // View 2: Form Url Encoded / Multipart Form
    KeyValueTable* m_formTable;
};

} // namespace poppy::gui
