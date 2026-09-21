#include "AssertionsEditor.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QComboBox>

namespace poppy::gui {

AssertionsEditor::AssertionsEditor(QWidget* parent) : QWidget(parent) {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(8);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({"Enabled", "Target Expression", "Operator", "Expected Value"});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_table->setColumnWidth(0, 60);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_table->setColumnWidth(1, 240);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_table->setColumnWidth(2, 130);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);

    connect(m_table, &QTableWidget::cellChanged, this, &AssertionsEditor::onCellChanged);
    mainLayout->addWidget(m_table);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_addBtn = new QPushButton("+ Add Assertion", this);
    connect(m_addBtn, &QPushButton::clicked, this, [this]() {
        addRow(true, "res.status", "eq", "200");
        emit assertionsChanged();
    });
    btnLayout->addWidget(m_addBtn);

    m_removeBtn = new QPushButton("Remove", this);
    connect(m_removeBtn, &QPushButton::clicked, this, &AssertionsEditor::removeCurrentRow);
    btnLayout->addWidget(m_removeBtn);

    mainLayout->addLayout(btnLayout);
}

void AssertionsEditor::addRow(bool enabled, const QString& target, const QString& op, const QString& expected) {
    m_updating = true;
    int row = m_table->rowCount();
    m_table->insertRow(row);

    // 0: Checkbox
    auto* chkItem = new QTableWidgetItem();
    chkItem->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
    chkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_table->setItem(row, 0, chkItem);

    // 1: Target
    auto* targetItem = new QTableWidgetItem(target);
    m_table->setItem(row, 1, targetItem);

    // 2: Operator combo
    auto* opCombo = new QComboBox(m_table);
    opCombo->addItem("eq (equal)", "eq");
    opCombo->addItem("neq (not equal)", "neq");
    opCombo->addItem("gt (greater than)", "gt");
    opCombo->addItem("gte (greater or equal)", "gte");
    opCombo->addItem("lt (less than)", "lt");
    opCombo->addItem("lte (less or equal)", "lte");
    opCombo->addItem("contains", "contains");
    opCombo->addItem("not contains", "not_contains");

    int opIdx = opCombo->findData(op.toLower());
    if (opIdx >= 0) opCombo->setCurrentIndex(opIdx);
    connect(opCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        emit assertionsChanged();
    });
    m_table->setCellWidget(row, 2, opCombo);

    // 3: Expected
    auto* expItem = new QTableWidgetItem(expected);
    m_table->setItem(row, 3, expItem);

    m_updating = false;
}

void AssertionsEditor::removeCurrentRow() {
    int row = m_table->currentRow();
    if (row >= 0 && row < m_table->rowCount()) {
        m_table->removeRow(row);
        emit assertionsChanged();
    }
}

void AssertionsEditor::onCellChanged(int /*row*/, int /*column*/) {
    if (!m_updating) {
        emit assertionsChanged();
    }
}

void AssertionsEditor::loadFromRequest(const core::RequestModel& req) {
    m_updating = true;
    m_table->setRowCount(0);
    for (const auto& a : req.assertions) {
        addRow(a.enabled, a.target, a.op, a.expected);
    }
    m_updating = false;
}

void AssertionsEditor::saveToRequest(core::RequestModel& req) const {
    req.assertions.clear();
    for (int r = 0; r < m_table->rowCount(); ++r) {
        auto* chk = m_table->item(r, 0);
        auto* targetItem = m_table->item(r, 1);
        auto* opCombo = qobject_cast<QComboBox*>(m_table->cellWidget(r, 2));
        auto* expItem = m_table->item(r, 3);

        if (targetItem && !targetItem->text().trimmed().isEmpty()) {
            req.assertions.append(core::AssertionRule{
                .target = targetItem->text().trimmed(),
                .op = opCombo ? opCombo->currentData().toString() : "eq",
                .expected = expItem ? expItem->text() : "",
                .enabled = (chk && chk->checkState() == Qt::Checked)
            });
        }
    }
}

} // namespace poppy::gui
