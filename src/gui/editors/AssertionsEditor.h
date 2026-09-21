#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <core/RequestModel.h>
#include <core/assertions/DeclarativeAssertion.h>

namespace poppy::gui {

class AssertionsEditor : public QWidget {
    Q_OBJECT
public:
    explicit AssertionsEditor(QWidget* parent = nullptr);

    void loadFromRequest(const core::RequestModel& req);
    void saveToRequest(core::RequestModel& req) const;

signals:
    void assertionsChanged();

private slots:
    void addRow(bool enabled = true, const QString& target = "res.status", const QString& op = "eq", const QString& expected = "200");
    void removeCurrentRow();
    void onCellChanged(int row, int column);

private:
    QTableWidget* m_table;
    QPushButton* m_addBtn;
    QPushButton* m_removeBtn;
    bool m_updating{false};
};

} // namespace poppy::gui
