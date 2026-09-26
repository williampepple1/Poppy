#include "CommandPaletteDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QApplication>
#include <QHeaderView>
#include <Theme.h>

namespace poppy::gui {

CommandPaletteDialog::CommandPaletteDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Command Palette");
    resize(700, 480);
    setModal(true);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(14, 14, 14, 14);
    mainLayout->setSpacing(10);

    // Search bar container
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("🔍  Type a command, search requests, or switch environments... (> for commands, @ for envs)");
    m_searchEdit->installEventFilter(this);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &CommandPaletteDialog::onSearchTextChanged);
    mainLayout->addWidget(m_searchEdit);

    // Results List
    m_listWidget = new QListWidget(this);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    connect(m_listWidget, &QListWidget::itemActivated, this, &CommandPaletteDialog::onItemActivated);
    connect(m_listWidget, &QListWidget::itemClicked, this, &CommandPaletteDialog::onItemActivated);
    mainLayout->addWidget(m_listWidget, 1);

    // Status / Tip bar
    m_statusLabel = new QLabel(this);
    mainLayout->addWidget(m_statusLabel);

    applyThemeStyles();
}

void CommandPaletteDialog::applyThemeStyles() {
    const bool dark = Theme::isDarkMode();

    setStyleSheet(QString(
        "QDialog { background-color: %1; border: 1px solid %2; border-radius: 10px; }"
    ).arg(dark ? "#0f1117" : "#ffffff", dark ? "#2b2f3e" : "#cbd5e1"));

    m_searchEdit->setStyleSheet(QString(
        "QLineEdit { font-size: 13px; font-weight: 500; padding: 10px 14px; border-radius: 8px; "
        "border: 1px solid %1; background-color: %2; color: %3; selection-background-color: #3b82f6; }"
        "QLineEdit:focus { border: 1px solid #3b82f6; }"
    ).arg(dark ? "#262936" : "#cbd5e1", dark ? "#161822" : "#f8fafc", dark ? "#f4f4f5" : "#0f172a"));

    m_listWidget->setStyleSheet(QString(
        "QListWidget { background-color: %1; border: 1px solid %2; border-radius: 8px; padding: 4px; outline: none; }"
        "QListWidget::item { border-radius: 6px; margin: 1px 0px; padding: 2px; }"
        "QListWidget::item:selected { background-color: %3; border: 1px solid %4; }"
    ).arg(dark ? "#13151f" : "#f8fafc",
          dark ? "#232634" : "#e2e8f0",
          dark ? "rgba(59, 130, 246, 0.16)" : "rgba(59, 130, 246, 0.12)",
          dark ? "#3b82f6" : "#2563eb"));

    m_statusLabel->setStyleSheet(dark ? "color: #71717a; font-size: 11px; padding: 2px 4px;" : "color: #64748b; font-size: 11px; padding: 2px 4px;");
}

void CommandPaletteDialog::setActions(const QList<PaletteEntry>& actions) {
    for (const auto& a : actions) {
        m_allEntries.append(a);
    }
    rebuildList("");
}

void CommandPaletteDialog::setEnvironments(const QStringList& envNames, const QString& currentEnv) {
    // "No Environment" option
    {
        PaletteEntry entry;
        entry.type = PaletteItemType::Environment;
        entry.title = "Switch Environment: No Environment";
        entry.subtitle = currentEnv.isEmpty() ? "Currently Active" : "Disable active environment variables";
        entry.category = "Environments";
        entry.envName = "";
        m_allEntries.append(entry);
    }

    for (const auto& name : envNames) {
        PaletteEntry entry;
        entry.type = PaletteItemType::Environment;
        entry.title = "Switch Environment: " + name;
        entry.subtitle = (name == currentEnv) ? "Currently Active" : "Active workspace environment";
        entry.category = "Environments";
        entry.envName = name;
        m_allEntries.append(entry);
    }
    rebuildList("");
}

void CommandPaletteDialog::setCollection(core::CollectionModel* model) {
    if (model && model->rootItem()) {
        collectRequests(model->rootItem(), "");
    }
    rebuildList("");
}

void CommandPaletteDialog::collectRequests(core::CollectionItem* item, const QString& parentPath) {
    if (!item) return;

    if (item->type() == core::CollectionItemType::Request && item->request()) {
        PaletteEntry entry;
        entry.type = PaletteItemType::Request;
        entry.title = item->name();
        entry.subtitle = parentPath.isEmpty() ? item->request()->url : (parentPath + " › " + item->request()->url);
        entry.category = "Requests";
        entry.method = item->request()->method;
        entry.requestItem = item;
        m_allEntries.append(entry);
    }

    QString currentPath = parentPath;
    if (item->type() == core::CollectionItemType::Folder) {
        currentPath = parentPath.isEmpty() ? item->name() : (parentPath + " / " + item->name());
    }

    for (auto* child : item->children()) {
        collectRequests(child, currentPath);
    }
}

void CommandPaletteDialog::onSearchTextChanged(const QString& text) {
    rebuildList(text);
}

void CommandPaletteDialog::rebuildList(const QString& query) {
    m_listWidget->clear();
    m_filteredEntries.clear();

    QString q = query.trimmed();
    enum FilterMode { All, ActionsOnly, EnvsOnly, RequestsOnly } mode = All;

    if (q.startsWith(">")) {
        mode = ActionsOnly;
        q = q.mid(1).trimmed();
    } else if (q.startsWith("@")) {
        mode = EnvsOnly;
        q = q.mid(1).trimmed();
    } else if (q.startsWith("#")) {
        mode = RequestsOnly;
        q = q.mid(1).trimmed();
    }

    const QStringList tokens = q.toLower().split(' ', Qt::SkipEmptyParts);
    const bool dark = Theme::isDarkMode();

    int totalMatches = 0;
    constexpr int kMaxDisplayed = 100;

    for (const auto& entry : m_allEntries) {
        if (mode == ActionsOnly && entry.type != PaletteItemType::Action) continue;
        if (mode == EnvsOnly && entry.type != PaletteItemType::Environment) continue;
        if (mode == RequestsOnly && entry.type != PaletteItemType::Request) continue;

        if (!tokens.isEmpty()) {
            QString fullHaystack = (entry.title + " " + entry.subtitle + " " + entry.category + " " + entry.shortcut + " " + core::methodToString(entry.method)).toLower();
            bool allTokensMatch = true;
            for (const auto& t : tokens) {
                if (!fullHaystack.contains(t)) {
                    allTokensMatch = false;
                    break;
                }
            }
            if (!allTokensMatch) continue;
        }

        totalMatches++;
        if (m_filteredEntries.size() >= kMaxDisplayed) continue;

        m_filteredEntries.append(entry);

        auto* listItem = new QListWidgetItem(m_listWidget);
        listItem->setSizeHint(QSize(0, 48));

        auto* itemWidget = new QWidget();
        auto* itemLayout = new QHBoxLayout(itemWidget);
        itemLayout->setContentsMargins(8, 4, 8, 4);
        itemLayout->setSpacing(10);

        // Left Pill Badge
        auto* badge = new QLabel(itemWidget);
        badge->setAlignment(Qt::AlignCenter);

        if (entry.type == PaletteItemType::Request) {
            QString mStr = core::methodToString(entry.method);
            badge->setText(mStr);
            QColor mc = Theme::methodColor(entry.method);
            badge->setStyleSheet(QString(
                "background-color: %1; color: #ffffff; font-size: 10px; font-weight: 800; border-radius: 4px; padding: 3px 6px; min-width: 48px; max-width: 52px;"
            ).arg(mc.name()));
        } else if (entry.type == PaletteItemType::Environment) {
            badge->setText("ENV");
            badge->setStyleSheet(
                "background-color: #06b6d4; color: #ffffff; font-size: 10px; font-weight: 800; border-radius: 4px; padding: 3px 6px; min-width: 48px; max-width: 52px;"
            );
        } else {
            badge->setText("CMD");
            badge->setStyleSheet(
                "background-color: #8b5cf6; color: #ffffff; font-size: 10px; font-weight: 800; border-radius: 4px; padding: 3px 6px; min-width: 48px; max-width: 52px;"
            );
        }
        itemLayout->addWidget(badge);

        // Center VBox (Title + Subtitle)
        auto* textVBox = new QVBoxLayout();
        textVBox->setSpacing(1);
        textVBox->setContentsMargins(0, 0, 0, 0);

        auto* titleLabel = new QLabel(entry.title, itemWidget);
        titleLabel->setStyleSheet(QString("font-weight: 600; font-size: 12px; color: %1;").arg(dark ? "#f4f4f5" : "#0f172a"));
        textVBox->addWidget(titleLabel);

        if (!entry.subtitle.isEmpty()) {
            auto* subLabel = new QLabel(entry.subtitle, itemWidget);
            subLabel->setStyleSheet(QString("font-size: 11px; color: %1;").arg(dark ? "#71717a" : "#64748b"));
            textVBox->addWidget(subLabel);
        }

        itemLayout->addLayout(textVBox, 1);

        // Right Shortcut / Category badge
        if (!entry.shortcut.isEmpty()) {
            auto* scLabel = new QLabel(entry.shortcut, itemWidget);
            scLabel->setStyleSheet(QString(
                "background-color: %1; color: %2; border: 1px solid %3; border-radius: 4px; padding: 2px 6px; font-size: 10px; font-family: Consolas, monospace;"
            ).arg(dark ? "#1f222e" : "#f1f5f9", dark ? "#a1a1aa" : "#475569", dark ? "#2e3242" : "#cbd5e1"));
            itemLayout->addWidget(scLabel);
        } else if (!entry.category.isEmpty()) {
            auto* catLabel = new QLabel(entry.category, itemWidget);
            catLabel->setStyleSheet(QString("color: %1; font-size: 10px; padding-right: 4px;").arg(dark ? "#52525b" : "#94a3b8"));
            itemLayout->addWidget(catLabel);
        }

        m_listWidget->setItemWidget(listItem, itemWidget);
    }

    if (m_listWidget->count() > 0) {
        m_listWidget->setCurrentRow(0);
    }

    if (totalMatches > kMaxDisplayed) {
        m_statusLabel->setText(QString("Showing %1 of %2 items · ↑↓ to navigate · Enter to select · Esc to dismiss")
            .arg(m_filteredEntries.size()).arg(totalMatches));
    } else {
        m_statusLabel->setText(QString("%1 items · ↑↓ to navigate · Enter to select · Esc to dismiss")
            .arg(m_filteredEntries.size()));
    }
}

void CommandPaletteDialog::onItemActivated(QListWidgetItem* item) {
    int row = m_listWidget->row(item);
    if (row >= 0 && row < m_filteredEntries.size()) {
        m_chosenEntry = m_filteredEntries[row];
        accept();
    }
}

bool CommandPaletteDialog::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_searchEdit && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            reject();
            return true;
        } else if (keyEvent->key() == Qt::Key_Down) {
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
        } else if (keyEvent->key() == Qt::Key_PageDown) {
            int row = std::min(m_listWidget->count() - 1, m_listWidget->currentRow() + 6);
            m_listWidget->setCurrentRow(row);
            return true;
        } else if (keyEvent->key() == Qt::Key_PageUp) {
            int row = std::max(0, m_listWidget->currentRow() - 6);
            m_listWidget->setCurrentRow(row);
            return true;
        } else if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (m_listWidget->currentItem()) {
                onItemActivated(m_listWidget->currentItem());
            }
            return true;
        }
    }
    return QDialog::eventFilter(obj, event);
}

} // namespace poppy::gui
