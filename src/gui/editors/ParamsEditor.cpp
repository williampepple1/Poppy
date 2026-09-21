#include "ParamsEditor.h"
#include <QVBoxLayout>

namespace poppy::gui {

ParamsEditor::ParamsEditor(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_tabWidget = new QTabWidget(this);
    m_queryTable = new KeyValueTable(true, this);
    m_pathTable = new KeyValueTable(true, this);

    m_tabWidget->addTab(m_queryTable, "Query Parameters");
    m_tabWidget->addTab(m_pathTable, "Path Parameters");

    connect(m_queryTable, &KeyValueTable::dataChanged, this, &ParamsEditor::paramsChanged);
    connect(m_pathTable, &KeyValueTable::dataChanged, this, &ParamsEditor::paramsChanged);

    layout->addWidget(m_tabWidget);
}

void ParamsEditor::loadFromRequest(const core::RequestModel& req) {
    m_queryTable->setParams(req.queryParams);
    m_pathTable->setParams(req.pathParams);
}

void ParamsEditor::saveToRequest(core::RequestModel& req) const {
    req.queryParams = m_queryTable->params();
    req.pathParams = m_pathTable->params();
}

} // namespace poppy::gui
