#pragma once

#include <QWidget>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <core/RequestModel.h>

namespace poppy::gui {

class ScriptEditor : public QWidget {
    Q_OBJECT
public:
    explicit ScriptEditor(QWidget* parent = nullptr);

    void loadFromRequest(const core::RequestModel& req);
    void saveToRequest(core::RequestModel& req) const;

signals:
    void scriptChanged();

private:
    QTabWidget* m_tabWidget;
    QPlainTextEdit* m_preRequestEdit;
    QPlainTextEdit* m_postResponseEdit;
    QPlainTextEdit* m_testsEdit;
};

} // namespace poppy::gui
