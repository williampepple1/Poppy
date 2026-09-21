#pragma once

#include <QWidget>
#include <components/KeyValueTable.h>
#include <core/RequestModel.h>

namespace poppy::gui {

class HeadersEditor : public QWidget {
    Q_OBJECT
public:
    explicit HeadersEditor(QWidget* parent = nullptr);

    void loadFromRequest(const core::RequestModel& req);
    void saveToRequest(core::RequestModel& req) const;

signals:
    void headersChanged();

private:
    KeyValueTable* m_table;
};

} // namespace poppy::gui
