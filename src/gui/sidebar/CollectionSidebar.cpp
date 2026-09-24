#include "CollectionSidebar.h"
#include "BrunoTreeItemDelegate.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QClipboard>
#include <QGuiApplication>
#include <QDropEvent>
#include <Theme.h>
#include <functional>
#include <components/KeyValueTable.h>
#include <core/codegen/CodeGenerator.h>
#include <QDialog>

namespace poppy::gui {

namespace {

class CollectionTreeWidget : public QTreeWidget {
public:
    explicit CollectionTreeWidget(QWidget* parent = nullptr) : QTreeWidget(parent) {}
    std::function<void(QTreeWidgetItem*)> afterDrop;

protected:
    void startDrag(Qt::DropActions supportedActions) override {
        const auto selected = selectedItems();
        m_dragged = selected.isEmpty() ? currentItem() : selected.first();
        QTreeWidget::startDrag(supportedActions);
    }

    void dropEvent(QDropEvent* event) override {
        QTreeWidgetItem* dragged = m_dragged;
        if (!dragged) {
            const auto selected = selectedItems();
            dragged = selected.isEmpty() ? currentItem() : selected.first();
        }
        m_dragged = nullptr;
        QTreeWidget::dropEvent(event);
        if (dragged && afterDrop) {
            afterDrop(dragged);
        }
    }

private:
    QTreeWidgetItem* m_dragged{nullptr};
};

} // namespace

CollectionSidebar::CollectionSidebar(core::CollectionModel* model, core::HistoryManager* historyManager, QWidget* parent)
    : QWidget(parent), m_model(model), m_historyManager(historyManager) {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    m_tabs = new QTabWidget(this);
    m_tabs->setStyleSheet(
        "QTabWidget::pane { border: none; border-top: 1px solid #232429; background: transparent; }"
        "QTabBar::tab { background: transparent; color: #9ca3af; padding: 7px 14px; font-weight: 600; border: none; border-bottom: 2px solid transparent; font-size: 12px; }"
        "QTabBar::tab:selected { color: #f59e0b; border-bottom: 2px solid #f59e0b; }"
        "QTabBar::tab:hover:!selected { color: #ffffff; background: rgba(255, 255, 255, 0.03); border-top-left-radius: 4px; border-top-right-radius: 4px; }"
    );

    // Tab 1: Collections
    auto* colContainer = new QWidget(this);
    setupCollectionsTab(colContainer);
    m_tabs->addTab(colContainer, "📁 Collections");

    // Tab 2: History
    auto* histContainer = new QWidget(this);
    setupHistoryTab(histContainer);
    m_tabs->addTab(histContainer, "🕒 History");

    mainLayout->addWidget(m_tabs);

    if (m_model) {
        connect(m_model, &core::CollectionModel::collectionLoaded, this, &CollectionSidebar::refreshTree);
        connect(m_model, &core::CollectionModel::itemModified, this, &CollectionSidebar::refreshTree);
    }

    if (m_historyManager) {
        setHistoryManager(m_historyManager);
    }
}

void CollectionSidebar::setHistoryManager(core::HistoryManager* manager) {
    m_historyManager = manager;
    if (m_historyManager) {
        connect(m_historyManager, &core::HistoryManager::entryAdded, this, &CollectionSidebar::refreshHistory);
        connect(m_historyManager, &core::HistoryManager::historyCleared, this, &CollectionSidebar::refreshHistory);
        refreshHistory();
    }
}

void CollectionSidebar::setupCollectionsTab(QWidget* container) {
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    // 1. Top action buttons
    auto* topBtnLayout = new QHBoxLayout();
    topBtnLayout->setSpacing(4);
    m_openBtn = new QPushButton("📂 Open", container);
    m_openBtn->setToolTip("Open Bruno or Poppy collection folder");
    connect(m_openBtn, &QPushButton::clicked, this, &CollectionSidebar::openCollectionRequested);
    topBtnLayout->addWidget(m_openBtn);

    m_addReqBtn = new QPushButton("+ Request", container);
    m_addReqBtn->setToolTip("Create a new request");
    connect(m_addReqBtn, &QPushButton::clicked, this, &CollectionSidebar::onAddRequest);
    topBtnLayout->addWidget(m_addReqBtn);

    m_addFolderBtn = new QPushButton("+ Folder", container);
    m_addFolderBtn->setToolTip("Create a new subfolder");
    connect(m_addFolderBtn, &QPushButton::clicked, this, &CollectionSidebar::onAddFolder);
    topBtnLayout->addWidget(m_addFolderBtn);

    layout->addLayout(topBtnLayout);

    // 1b. Collection search filter
    m_collectionFilterEdit = new QLineEdit(container);
    m_collectionFilterEdit->setPlaceholderText("🔍  Search requests...");
    m_collectionFilterEdit->setClearButtonEnabled(true);
    connect(m_collectionFilterEdit, &QLineEdit::textChanged, this, &CollectionSidebar::onCollectionFilterChanged);
    layout->addWidget(m_collectionFilterEdit);

    // 2. Environment Selector Bar
    auto* envLayout = new QHBoxLayout();
    envLayout->setSpacing(4);
    m_envCombo = new QComboBox(container);
    m_envCombo->addItem("No Environment", "");
    connect(m_envCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        emit environmentChanged(m_envCombo->itemData(idx).toString());
    });
    envLayout->addWidget(m_envCombo, 1);

    m_manageEnvBtn = new QPushButton("⚙️ Envs", container);
    m_manageEnvBtn->setToolTip("Manage collection environments");
    connect(m_manageEnvBtn, &QPushButton::clicked, this, &CollectionSidebar::manageEnvironmentsRequested);
    envLayout->addWidget(m_manageEnvBtn);

    layout->addLayout(envLayout);

    // 3. Tree Widget
    auto* tree = new CollectionTreeWidget(container);
    m_tree = tree;
    m_tree->setHeaderHidden(true);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setDragDropMode(QAbstractItemView::InternalMove);
    m_tree->setDragEnabled(true);
    m_tree->setAcceptDrops(true);
    m_tree->setDropIndicatorShown(true);
    m_tree->setItemDelegate(new BrunoTreeItemDelegate(m_tree));
    tree->afterDrop = [this](QTreeWidgetItem* dragged) {
        onTreeItemDropped(dragged);
    };
    connect(m_tree, &QTreeWidget::itemClicked, this, &CollectionSidebar::onItemClicked);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &CollectionSidebar::onContextMenu);
    layout->addWidget(m_tree, 1);
}

void CollectionSidebar::setupHistoryTab(QWidget* container) {
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    // Top action bar: Filter + Clear
    auto* topLayout = new QHBoxLayout();
    m_historyFilterEdit = new QLineEdit(container);
    m_historyFilterEdit->setPlaceholderText("Filter history...");
    connect(m_historyFilterEdit, &QLineEdit::textChanged, this, &CollectionSidebar::onHistoryFilterChanged);
    topLayout->addWidget(m_historyFilterEdit, 1);

    m_clearHistoryBtn = new QPushButton("Clear", container);
    m_clearHistoryBtn->setToolTip("Clear all request history");
    connect(m_clearHistoryBtn, &QPushButton::clicked, this, &CollectionSidebar::onClearHistory);
    topLayout->addWidget(m_clearHistoryBtn);

    layout->addLayout(topLayout);

    // History Tree
    m_historyTree = new QTreeWidget(container);
    m_historyTree->setHeaderHidden(true);
    m_historyTree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_historyTree->setItemDelegate(new BrunoTreeItemDelegate(m_historyTree));
    connect(m_historyTree, &QTreeWidget::itemClicked, this, &CollectionSidebar::onHistoryItemClicked);
    connect(m_historyTree, &QTreeWidget::customContextMenuRequested, this, &CollectionSidebar::onHistoryContextMenu);
    layout->addWidget(m_historyTree, 1);
}

QString CollectionSidebar::currentEnvironmentName() const {
    return m_envCombo->currentData().toString();
}

void CollectionSidebar::updateEnvironmentsCombo() {
    if (!m_model) return;
    QString current = m_envCombo->currentData().toString();
    m_envCombo->clear();
    m_envCombo->addItem("No Environment", "");

    for (const auto& env : m_model->environments()) {
        m_envCombo->addItem(env.name(), env.name());
    }

    int idx = m_envCombo->findData(current);
    if (idx >= 0) m_envCombo->setCurrentIndex(idx);
}

void CollectionSidebar::setActiveEnvironment(const QString& name) {
    if (!m_envCombo) return;
    int idx = m_envCombo->findData(name);
    if (idx < 0) return;
    m_envCombo->blockSignals(true);
    m_envCombo->setCurrentIndex(idx);
    m_envCombo->blockSignals(false);
}

void CollectionSidebar::refreshTree() {
    m_tree->clear();
    updateEnvironmentsCombo();

    if (!m_model) return;
    auto* root = m_model->rootItem();
    if (!root) return;

    auto* rootWidget = new QTreeWidgetItem(m_tree);
    rootWidget->setText(0, "📁 " + root->name());
    rootWidget->setData(0, Qt::UserRole, QVariant::fromValue(static_cast<void*>(root)));
    rootWidget->setData(0, Qt::UserRole + 2, 0); // Folder / root
    rootWidget->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled);
    rootWidget->setExpanded(true);

    populateChildren(rootWidget, root);

    if (m_collectionFilterEdit && !m_collectionFilterEdit->text().trimmed().isEmpty()) {
        onCollectionFilterChanged(m_collectionFilterEdit->text());
    }
}

void CollectionSidebar::populateChildren(QTreeWidgetItem* parentWidget, core::CollectionItem* parentModel) {
    for (auto* child : parentModel->children()) {
        auto* childWidget = new QTreeWidgetItem(parentWidget);
        childWidget->setData(0, Qt::UserRole, QVariant::fromValue(static_cast<void*>(child)));

        if (child->type() == core::CollectionItemType::Folder) {
            childWidget->setText(0, "📁 " + child->name());
            childWidget->setData(0, Qt::UserRole + 2, 0); // Folder
            childWidget->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
            populateChildren(childWidget, child);
            childWidget->setExpanded(true);
        } else {
            childWidget->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
            childWidget->setText(0, child->name());
            childWidget->setData(0, Qt::UserRole + 2, 1); // Request
            auto* req = child->request();
            if (req) {
                childWidget->setData(0, Qt::UserRole + 1, static_cast<int>(req->method));
            } else {
                childWidget->setData(0, Qt::UserRole + 1, static_cast<int>(core::HttpMethod::GET));
            }
        }
    }
}

core::CollectionItem* CollectionSidebar::itemFromWidget(QTreeWidgetItem* widget) const {
    if (!widget) return nullptr;
    return static_cast<core::CollectionItem*>(widget->data(0, Qt::UserRole).value<void*>());
}

void CollectionSidebar::onTreeItemDropped(QTreeWidgetItem* widget) {
    if (!m_model || !widget) return;
    auto* item = itemFromWidget(widget);
    if (!item) {
        refreshTree();
        return;
    }
    QTreeWidgetItem* parentWidget = widget->parent();
    if (!parentWidget) {
        // Can't have siblings to the collection root
        refreshTree();
        return;
    }
    core::CollectionItem* newParent = itemFromWidget(parentWidget);
    if (!newParent || newParent->type() == core::CollectionItemType::Request) {
        refreshTree();
        return;
    }
    int insertIndex = parentWidget->indexOfChild(widget);
    if (!m_model->moveItem(item, newParent, insertIndex)) {
        refreshTree();
    }
}

void CollectionSidebar::onItemClicked(QTreeWidgetItem* item, int /*column*/) {
    auto* modelItem = itemFromWidget(item);
    if (modelItem && modelItem->type() == core::CollectionItemType::Request) {
        emit requestSelected(modelItem);
    }
}

void CollectionSidebar::onContextMenu(const QPoint& pos) {
    auto* widgetItem = m_tree->itemAt(pos);
    auto* modelItem = itemFromWidget(widgetItem);

    QMenu menu(this);
    if (!modelItem) {
        menu.addAction("Add Request", this, &CollectionSidebar::onAddRequest);
        menu.addAction("Add Folder", this, &CollectionSidebar::onAddFolder);
    } else if (modelItem->type() == core::CollectionItemType::Folder || modelItem->type() == core::CollectionItemType::Collection) {
        menu.addAction("Add Request", this, &CollectionSidebar::onAddRequest);
        menu.addAction("Add Subfolder", this, &CollectionSidebar::onAddFolder);
        if (modelItem->type() == core::CollectionItemType::Folder) {
            menu.addSeparator();
            menu.addAction("Folder Variables...", this, &CollectionSidebar::onFolderVariables);
            menu.addAction("Rename", this, &CollectionSidebar::onRenameItem);
            menu.addAction("Delete", this, &CollectionSidebar::onDeleteItem);
        }
    } else if (modelItem->type() == core::CollectionItemType::Request) {
        menu.addAction("Rename", this, &CollectionSidebar::onRenameItem);
        menu.addAction("Duplicate", this, &CollectionSidebar::onDuplicateRequest);
        menu.addAction("Copy as cURL", this, &CollectionSidebar::onCopyAsCurl);
        menu.addAction("Copy as Fetch (JS)", [this, modelItem]() {
            if (modelItem && modelItem->request()) {
                QClipboard* cb = QGuiApplication::clipboard();
                cb->setText(core::CodeGenerator::generate(core::TargetLanguage::JavaScriptFetch, requestForExport(*modelItem->request(), modelItem)));
            }
        });
        menu.addAction("Copy as Python", [this, modelItem]() {
            if (modelItem && modelItem->request()) {
                QClipboard* cb = QGuiApplication::clipboard();
                cb->setText(core::CodeGenerator::generate(core::TargetLanguage::PythonRequests, requestForExport(*modelItem->request(), modelItem)));
            }
        });
        menu.addSeparator();
        menu.addAction("Delete", this, &CollectionSidebar::onDeleteItem);
    }

    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

void CollectionSidebar::onAddRequest() {
    auto* currentWidget = m_tree->currentItem();
    auto* parent = itemFromWidget(currentWidget);
    if (parent && parent->type() == core::CollectionItemType::Request) {
        parent = parent->parent();
    }
    if (!parent) parent = m_model->rootItem();
    if (!parent) return;

    bool ok;
    QString name = QInputDialog::getText(this, "New Request", "Request Name:", QLineEdit::Normal, "New Request", &ok);
    if (ok && !name.isEmpty()) {
        core::RequestModel newReq;
        newReq.name = name;
        auto* item = m_model->addRequest(parent, name, newReq);
        if (item) {
            refreshTree();
            emit requestSelected(item);
        }
    }
}

void CollectionSidebar::onAddFolder() {
    auto* currentWidget = m_tree->currentItem();
    auto* parent = itemFromWidget(currentWidget);
    if (parent && parent->type() == core::CollectionItemType::Request) {
        parent = parent->parent();
    }
    if (!parent) parent = m_model->rootItem();
    if (!parent) return;

    bool ok;
    QString name = QInputDialog::getText(this, "New Folder", "Folder Name:", QLineEdit::Normal, "New Folder", &ok);
    if (ok && !name.isEmpty()) {
        m_model->addFolder(parent, name);
        refreshTree();
    }
}

void CollectionSidebar::onRenameItem() {
    auto* currentWidget = m_tree->currentItem();
    auto* item = itemFromWidget(currentWidget);
    if (!item) return;

    bool ok;
    QString newName = QInputDialog::getText(this, "Rename Item", "New Name:", QLineEdit::Normal, item->name(), &ok);
    if (ok && !newName.isEmpty()) {
        if (!m_model->renameItem(item, newName)) {
            QMessageBox::warning(this, "Rename Failed", "Could not rename the item on disk.");
            return;
        }
        refreshTree();
    }
}

void CollectionSidebar::onDeleteItem() {
    auto* currentWidget = m_tree->currentItem();
    auto* item = itemFromWidget(currentWidget);
    if (!item) return;

    auto res = QMessageBox::question(this, "Delete Item",
        QString("Are you sure you want to delete '%1'?").arg(item->name()),
        QMessageBox::Yes | QMessageBox::No);
    if (res == QMessageBox::Yes) {
        m_model->deleteItem(item);
        refreshTree();
    }
}

void CollectionSidebar::onFolderVariables() {
    auto* currentWidget = m_tree->currentItem();
    auto* item = itemFromWidget(currentWidget);
    if (!item) return;

    QDialog dlg(this);
    dlg.setWindowTitle(QString("Folder Variables - %1").arg(item->name()));
    dlg.resize(480, 380);
    auto* layout = new QVBoxLayout(&dlg);

    auto* table = new KeyValueTable(false, &dlg);
    QList<core::HttpParam> params;
    for (auto it = item->variables().begin(); it != item->variables().end(); ++it) {
        params.append({.key = it.key(), .value = it.value(), .enabled = true});
    }
    table->setParams(params);
    layout->addWidget(table);

    auto* btnBox = new QHBoxLayout();
    btnBox->addStretch();
    auto* cancelBtn = new QPushButton("Cancel", &dlg);
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    auto* saveBtn = new QPushButton("Save Variables", &dlg);
    saveBtn->setStyleSheet("background-color: #3b82f6; color: #ffffff; font-weight: bold; padding: 6px 14px; border-radius: 4px;");
    connect(saveBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnBox->addWidget(cancelBtn);
    btnBox->addWidget(saveBtn);
    layout->addLayout(btnBox);

    if (dlg.exec() == QDialog::Accepted) {
        QMap<QString, QString> vars;
        for (const auto& p : table->params()) {
            if (p.enabled && !p.key.isEmpty()) {
                vars[p.key] = p.value;
            }
        }
        item->setVariables(vars);
        if (!m_model->saveFolderVariables(item)) {
            QMessageBox::warning(this, "Save Failed", "Could not write folder.bru for this folder.");
        }
    }
}

void CollectionSidebar::onDuplicateRequest() {
    auto* currentWidget = m_tree->currentItem();
    auto* item = itemFromWidget(currentWidget);
    if (!item || !item->request()) return;

    auto* parent = item->parent();
    if (!parent) parent = m_model->rootItem();
    if (!parent) return;

    QString newName = item->name() + " (copy)";
    core::RequestModel newReq = *item->request();
    newReq.name = newName;
    auto* newItem = m_model->addRequest(parent, newName, newReq);
    if (newItem) {
        refreshTree();
        emit requestSelected(newItem);
    }
}

core::RequestModel CollectionSidebar::requestForExport(core::RequestModel raw, core::CollectionItem* scope) const {
    if (raw.auth.type == core::AuthType::Inherit) {
        raw.auth = scope ? scope->effectiveAuth() : core::AuthModel{};
    }
    core::VariableResolver resolver;
    const QString envName = currentEnvironmentName();
    core::EnvironmentModel activeEnv(envName);
    if (m_model) {
        for (const auto& env : m_model->environments()) {
            if (env.name() == envName) {
                activeEnv = env;
                break;
            }
        }
        if (m_model->rootItem()) {
            resolver.setCollectionVariables(m_model->rootItem()->variables());
        }
    }
    resolver.setEnvironment(activeEnv);
    if (scope) {
        resolver.setFolderVariables(scope->effectiveVariables());
    }
    return resolver.resolveRequest(raw);
}

void CollectionSidebar::onCopyAsCurl() {
    auto* currentWidget = m_tree->currentItem();
    auto* item = itemFromWidget(currentWidget);
    if (item && item->request()) {
        QClipboard* clipboard = QGuiApplication::clipboard();
        clipboard->setText(requestForExport(*item->request(), item).toCurlCommand());
    }
}

// -------------------------------------------------------------
// History Implementation
// -------------------------------------------------------------

void CollectionSidebar::refreshHistory() {
    if (!m_historyManager || !m_historyTree) return;
    onHistoryFilterChanged(m_historyFilterEdit ? m_historyFilterEdit->text() : QString());
}

void CollectionSidebar::onHistoryFilterChanged(const QString& query) {
    if (!m_historyManager || !m_historyTree) return;

    m_historyTree->clear();
    auto items = m_historyManager->filter(query);

    for (const auto& item : items) {
        auto* treeItem = new QTreeWidgetItem(m_historyTree);
        treeItem->setData(0, Qt::UserRole, item.id);
        treeItem->setData(0, Qt::UserRole + 1, static_cast<int>(item.request.method));
        treeItem->setData(0, Qt::UserRole + 2, 2); // History item
        treeItem->setData(0, Qt::UserRole + 3, item.statusCode);
        treeItem->setData(0, Qt::UserRole + 4, static_cast<qint64>(item.responseTimeMs));
        treeItem->setData(0, Qt::UserRole + 5, item.timestamp.toString("HH:mm:ss"));

        QString urlOrName = item.request.url.isEmpty() ? item.request.name : item.request.url;
        treeItem->setText(0, urlOrName);
        treeItem->setToolTip(0, QString("%1 %2\nStatus: %3 %4\nLatency: %5 ms\nTime: %6")
            .arg(core::methodToString(item.request.method), item.request.url)
            .arg(item.statusCode).arg(item.statusText)
            .arg(item.responseTimeMs)
            .arg(item.timestamp.toString("yyyy-MM-dd HH:mm:ss")));

        m_historyTree->addTopLevelItem(treeItem);
    }
}

void CollectionSidebar::onHistoryItemClicked(QTreeWidgetItem* item, int /*column*/) {
    if (!item || !m_historyManager) return;
    QString id = item->data(0, Qt::UserRole).toString();
    for (const auto& it : m_historyManager->items()) {
        if (it.id == id) {
            emit historyItemSelected(it);
            break;
        }
    }
}

void CollectionSidebar::onHistoryContextMenu(const QPoint& pos) {
    auto* widgetItem = m_historyTree->itemAt(pos);
    if (!widgetItem || !m_historyManager) return;

    QString id = widgetItem->data(0, Qt::UserRole).toString();
    core::HistoryItem found;
    bool haveItem = false;
    for (const auto& it : m_historyManager->items()) {
        if (it.id == id) {
            found = it;
            haveItem = true;
            break;
        }
    }
    if (!haveItem) return;

    QMenu menu(this);
    menu.addAction("Load into Editor", [this, found]() {
        emit historyItemSelected(found);
    });
    menu.addAction("Copy URL", [found]() {
        QClipboard* cb = QGuiApplication::clipboard();
        cb->setText(found.request.url);
    });
    menu.addAction("Copy as cURL", [this, found]() {
        core::CollectionItem* scope = (m_model && !found.sourcePath.isEmpty()) ? m_model->findItemByPath(found.sourcePath) : nullptr;
        QClipboard* cb = QGuiApplication::clipboard();
        cb->setText(requestForExport(found.request, scope).toCurlCommand());
    });
    menu.addAction("Copy as Fetch (JS)", [this, found]() {
        core::CollectionItem* scope = (m_model && !found.sourcePath.isEmpty()) ? m_model->findItemByPath(found.sourcePath) : nullptr;
        QClipboard* cb = QGuiApplication::clipboard();
        cb->setText(core::CodeGenerator::generate(core::TargetLanguage::JavaScriptFetch, requestForExport(found.request, scope)));
    });
    menu.addAction("Copy as Python", [this, found]() {
        core::CollectionItem* scope = (m_model && !found.sourcePath.isEmpty()) ? m_model->findItemByPath(found.sourcePath) : nullptr;
        QClipboard* cb = QGuiApplication::clipboard();
        cb->setText(core::CodeGenerator::generate(core::TargetLanguage::PythonRequests, requestForExport(found.request, scope)));
    });
    menu.addSeparator();
    menu.addAction("Delete Entry", [this, id]() {
        m_historyManager->removeEntry(id);
        refreshHistory();
    });

    menu.exec(m_historyTree->viewport()->mapToGlobal(pos));
}

void CollectionSidebar::onClearHistory() {
    if (!m_historyManager || m_historyManager->count() == 0) return;

    auto res = QMessageBox::question(this, "Clear History",
        "Are you sure you want to clear all request execution history?",
        QMessageBox::Yes | QMessageBox::No);
    if (res == QMessageBox::Yes) {
        m_historyManager->clear();
    }
}

void CollectionSidebar::onCollectionFilterChanged(const QString& query) {
    if (!m_tree) return;
    QString q = query.trimmed();

    // Recursive helper that returns true if any child of 'item' matches
    std::function<bool(QTreeWidgetItem*)> applyFilter = [&](QTreeWidgetItem* item) -> bool {
        bool childVisible = false;
        for (int i = 0; i < item->childCount(); ++i) {
            childVisible |= applyFilter(item->child(i));
        }
        bool selfMatch = q.isEmpty() || item->text(0).contains(q, Qt::CaseInsensitive);
        if (!selfMatch && item->data(0, Qt::UserRole + 2).toInt() == 1) { // Request item
            auto method = static_cast<core::HttpMethod>(item->data(0, Qt::UserRole + 1).toInt());
            QString mStr = core::methodToString(method);
            if (mStr.contains(q, Qt::CaseInsensitive)) {
                selfMatch = true;
            }
        }
        bool visible = selfMatch || childVisible;
        item->setHidden(!visible);
        if (childVisible && !q.isEmpty()) item->setExpanded(true);
        return visible;
    };

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        applyFilter(m_tree->topLevelItem(i));
    }
}

} // namespace poppy::gui
