#pragma once

#include <QWidget>
#include <QTabWidget>
#include <components/KeyValueTable.h>
#include <core/RequestModel.h>

namespace poppy::gui {

class ParamsEditor : public QWidget {
    Q_OBJECT
public:
    explicit ParamsEditor(QWidget* parent = nullptr);

    void loadFromRequest(const core::RequestModel& req);
    void saveToRequest(core::RequestModel& req) const;

signals:
    void paramsChanged();

private:
    QTabWidget* m_tabWidget;
    KeyValueTable* m_queryTable;
    KeyValueTable* m_pathTable;
};

} // namespace poppy::gui
