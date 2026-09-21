#include "KeyValueTable.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QCheckBox>
#include <QLineEdit>
#include <QCompleter>

namespace poppy::gui {

KeyValueTable::KeyValueTable(bool showDescription, QWidget* parent)
    : QWidget(parent), m_showDescription(showDescription) {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(8);

    m_table = new QTableWidget(this);
    QStringList headers;
    headers << "" << "Key" << "Value";
    if (m_showDescription) headers << "Description";

    m_table->setColumnCount(headers.size());
    m_table->setHorizontalHeaderLabels(headers);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_table->setColumnWidth(0, 32);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_table->setColumnWidth(1, 180);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    if (m_showDescription) {
        m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
        m_table->setColumnWidth(3, 160);
    }
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);

    connect(m_table, &QTableWidget::cellChanged, this, &KeyValueTable::onCellChanged);

    mainLayout->addWidget(m_table);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_addRowBtn = new QPushButton("+ Add Item", this);
    connect(m_addRowBtn, &QPushButton::clicked, this, [this]() {
        addRow(true, "", "", "");
    });
    btnLayout->addWidget(m_addRowBtn);

    m_removeRowBtn = new QPushButton("Remove", this);
    connect(m_removeRowBtn, &QPushButton::clicked, this, &KeyValueTable::removeCurrentRow);
    btnLayout->addWidget(m_removeRowBtn);

    mainLayout->addLayout(btnLayout);

    ensureTrailingEmptyRow();
}

void KeyValueTable::setKeyCompleterWords(const QStringList& words) {
    m_completerWords = words;
}

void KeyValueTable::addRow(bool enabled, const QString& key, const QString& value, const QString& desc) {
    m_updating = true;
    int row = m_table->rowCount();
    m_table->insertRow(row);

    auto* chkItem = new QTableWidgetItem();
    chkItem->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
    chkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_table->setItem(row, 0, chkItem);

    auto* keyItem = new QTableWidgetItem(key);
    m_table->setItem(row, 1, keyItem);

    auto* valItem = new QTableWidgetItem(value);
    m_table->setItem(row, 2, valItem);

    if (m_showDescription) {
        auto* descItem = new QTableWidgetItem(desc);
        m_table->setItem(row, 3, descItem);
    }

    m_updating = false;
}

void KeyValueTable::removeCurrentRow() {
    int row = m_table->currentRow();
    if (row >= 0 && row < m_table->rowCount()) {
        m_table->removeRow(row);
        ensureTrailingEmptyRow();
        emit dataChanged();
    }
}

void KeyValueTable::onCellChanged(int row, int /*column*/) {
    if (m_updating) return;

    // If edited the last row and key is not empty, auto add a new trailing row
    if (row == m_table->rowCount() - 1) {
        auto* keyItem = m_table->item(row, 1);
        if (keyItem && !keyItem->text().trimmed().isEmpty()) {
            ensureTrailingEmptyRow();
        }
    }

    emit dataChanged();
}

void KeyValueTable::ensureTrailingEmptyRow() {
    if (m_table->rowCount() == 0) {
        addRow(true, "", "", "");
        return;
    }
    int lastRow = m_table->rowCount() - 1;
    auto* keyItem = m_table->item(lastRow, 1);
    auto* valItem = m_table->item(lastRow, 2);
    if ((keyItem && !keyItem->text().isEmpty()) || (valItem && !valItem->text().isEmpty())) {
        addRow(true, "", "", "");
    }
}

void KeyValueTable::setHeaders(const QList<core::HttpHeader>& headers) {
    m_updating = true;
    m_table->setRowCount(0);
    for (const auto& h : headers) {
        addRow(h.enabled, h.name, h.value, h.description);
    }
    m_updating = false;
    ensureTrailingEmptyRow();
}

QList<core::HttpHeader> KeyValueTable::headers() const {
    QList<core::HttpHeader> list;
    for (int i = 0; i < m_table->rowCount(); ++i) {
        auto* chk = m_table->item(i, 0);
        auto* key = m_table->item(i, 1);
        auto* val = m_table->item(i, 2);
        auto* desc = m_showDescription ? m_table->item(i, 3) : nullptr;

        if (key && !key->text().trimmed().isEmpty()) {
            list.append(core::HttpHeader{
                .name = key->text().trimmed(),
                .value = val ? val->text() : "",
                .enabled = (chk && chk->checkState() == Qt::Checked),
                .description = desc ? desc->text() : ""
            });
        }
    }
    return list;
}

void KeyValueTable::setParams(const QList<core::HttpParam>& params) {
    m_updating = true;
    m_table->setRowCount(0);
    for (const auto& p : params) {
        addRow(p.enabled, p.key, p.value, p.description);
    }
    m_updating = false;
    ensureTrailingEmptyRow();
}

QList<core::HttpParam> KeyValueTable::params() const {
    QList<core::HttpParam> list;
    for (int i = 0; i < m_table->rowCount(); ++i) {
        auto* chk = m_table->item(i, 0);
        auto* key = m_table->item(i, 1);
        auto* val = m_table->item(i, 2);
        auto* desc = m_showDescription ? m_table->item(i, 3) : nullptr;

        if (key && !key->text().trimmed().isEmpty()) {
            list.append(core::HttpParam{
                .key = key->text().trimmed(),
                .value = val ? val->text() : "",
                .enabled = (chk && chk->checkState() == Qt::Checked),
                .description = desc ? desc->text() : ""
            });
        }
    }
    return list;
}

} // namespace poppy::gui
