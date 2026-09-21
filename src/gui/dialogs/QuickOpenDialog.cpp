#include "QuickOpenDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QPainter>
#include <Theme.h>

namespace poppy::gui {

QuickOpenDialog::QuickOpenDialog(core::CollectionModel* model, QWidget* parent)
    : QDialog(parent), m_model(model) {
    setWindowTitle("Quick Open Request (Ctrl+P)");
    resize(640, 420);
    setModal(true);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search requests by name, URL, or method...");
    m_searchEdit->setStyleSheet("font-size: 14px; padding: 8px 12px; border-radius: 6px; border: 1px solid #3f3f46; background-color: #18181b; color: #f4f4f5;");
    m_searchEdit->installEventFilter(this);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &QuickOpenDialog::onFilterChanged);
    mainLayout->addWidget(m_searchEdit);

    m_listWidget = new QListWidget(this);
    m_listWidget->setStyleSheet(
        "QListWidget { background-color: #18181b; border: 1px solid #27272a; border-radius: 6px; }"
        "QListWidget::item { padding: 4px 8px; border-bottom: 1px solid #27272a; border-radius: 4px; }"
        "QListWidget::item:selected { background-color: #27272a; border: 1px solid #3b82f6; }"
    );
    connect(m_listWidget, &QListWidget::itemActivated, this, &QuickOpenDialog::onItemActivated);
    mainLayout->addWidget(m_listWidget, 1);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: #71717a; font-size: 11px;");
    mainLayout->addWidget(m_statusLabel);

    if (m_model && m_model->rootItem()) {
        collectEntries(m_model->rootItem(), "");
    }

    refreshList("");
    m_searchEdit->setFocus();
}

void QuickOpenDialog::collectEntries(core::CollectionItem* item, const QString& parentPath) {
    if (!item) return;

    if (item->type() == core::CollectionItemType::Request && item->request()) {
        QuickOpenEntry entry;
        entry.item = item;
        entry.name = item->name();
        entry.method = item->request()->method;
        entry.url = item->request()->url;
        entry.relativePath = parentPath.isEmpty() ? item->name() : (parentPath + " / " + item->name());
        m_entries.append(entry);
    }

    QString currentPath = parentPath;
    if (item->type() == core::CollectionItemType::Folder) {
        currentPath = parentPath.isEmpty() ? item->name() : (parentPath + " / " + item->name());
    }

    for (auto* child : item->children()) {
        collectEntries(child, currentPath);
    }
}

void QuickOpenDialog::refreshList(const QString& filter) {
    m_listWidget->clear();
    QString f = filter.trimmed().toLower();

    int matchCount = 0;
    for (const auto& entry : m_entries) {
        QString methodStr = core::methodToString(entry.method);
        bool match = f.isEmpty() ||
                     entry.name.toLower().contains(f) ||
                     entry.url.toLower().contains(f) ||
                     methodStr.toLower().contains(f) ||
                     entry.relativePath.toLower().contains(f);

        if (!match) continue;
        matchCount++;

        auto* listItem = new QListWidgetItem(m_listWidget);
        listItem->setData(Qt::UserRole, QVariant::fromValue(reinterpret_cast<quintptr>(entry.item)));
        listItem->setSizeHint(QSize(0, 48));

        auto* itemWidget = new QWidget();
        auto* itemLayout = new QHBoxLayout(itemWidget);
        itemLayout->setContentsMargins(6, 4, 6, 4);
        itemLayout->setSpacing(10);

        // Method Badge
        auto* badge = new QLabel(methodStr, itemWidget);
        QColor mc = Theme::methodColor(entry.method);
        badge->setStyleSheet(QString(
            "background-color: %1; color: #ffffff; font-size: 10px; font-weight: bold; border-radius: 3px; padding: 3px 6px; min-width: 44px; max-width: 50px;"
        ).arg(mc.name()));
        badge->setAlignment(Qt::AlignCenter);
        itemLayout->addWidget(badge);

        // Name & Path VBox
        auto* textVBox = new QVBoxLayout();
        textVBox->setSpacing(2);
        textVBox->setContentsMargins(0, 0, 0, 0);

        auto* nameLabel = new QLabel(entry.name, itemWidget);
        nameLabel->setStyleSheet("font-weight: bold; font-size: 12px; color: #f4f4f5;");
        textVBox->addWidget(nameLabel);

        QString detail = entry.relativePath;
        if (!entry.url.isEmpty()) {
            detail += " — " + entry.url;
        }
        auto* detailLabel = new QLabel(detail, itemWidget);
        detailLabel->setStyleSheet("font-size: 10px; color: #71717a;");
        textVBox->addWidget(detailLabel);

        itemLayout->addLayout(textVBox, 1);
        m_listWidget->setItemWidget(listItem, itemWidget);
    }

    if (m_listWidget->count() > 0) {
        m_listWidget->setCurrentRow(0);
    }

    m_statusLabel->setText(QString("%1 requests available | Press Enter to open, Esc to cancel").arg(matchCount));
}

void QuickOpenDialog::onFilterChanged(const QString& text) {
    refreshList(text);
}

void QuickOpenDialog::onItemActivated(QListWidgetItem* item) {
    if (!item) return;
    quintptr ptr = item->data(Qt::UserRole).value<quintptr>();
    m_selectedItem = reinterpret_cast<core::CollectionItem*>(ptr);
    accept();
}

bool QuickOpenDialog::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_searchEdit && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Down) {
            int row = m_listWidget->currentRow();
            if (row < m_listWidget->count() - 1) {
                m_listWidget->setCurrentRow(row + 1);
            }
            return true;
        } else if (keyEvent->key() == Qt::Key_Up) {
            int row = m_listWidget->currentRow();
            if (row > 0) {
                m_listWidget->setCurrentRow(row - 1);
            }
            return true;
        } else if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            onItemActivated(m_listWidget->currentItem());
            return true;
        } else if (keyEvent->key() == Qt::Key_Escape) {
            reject();
            return true;
        }
    }
    return QDialog::eventFilter(obj, event);
}

} // namespace poppy::gui
