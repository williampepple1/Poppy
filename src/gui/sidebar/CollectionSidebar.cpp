#include "CollectionSidebar.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QClipboard>
#include <QGuiApplication>
#include <Theme.h>

namespace poppy::gui {

CollectionSidebar::CollectionSidebar(core::CollectionModel* model, QWidget* parent)
    : QWidget(parent), m_model(model) {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // 1. Top action buttons
    auto* topBtnLayout = new QHBoxLayout();
    m_openBtn = new QPushButton("Open Collection", this);
    connect(m_openBtn, &QPushButton::clicked, this, &CollectionSidebar::openCollectionRequested);
    topBtnLayout->addWidget(m_openBtn);

    m_addReqBtn = new QPushButton("+ Req", this);
    connect(m_addReqBtn, &QPushButton::clicked, this, &CollectionSidebar::onAddRequest);
    topBtnLayout->addWidget(m_addReqBtn);

    m_addFolderBtn = new QPushButton("+ Folder", this);
    connect(m_addFolderBtn, &QPushButton::clicked, this, &CollectionSidebar::onAddFolder);
    topBtnLayout->addWidget(m_addFolderBtn);

    mainLayout->addLayout(topBtnLayout);

    // 2. Environment Selector Bar
    auto* envLayout = new QHBoxLayout();
    m_envCombo = new QComboBox(this);
    m_envCombo->addItem("No Environment", "");
    connect(m_envCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        emit environmentChanged(m_envCombo->itemData(idx).toString());
    });
    envLayout->addWidget(m_envCombo, 1);

    m_manageEnvBtn = new QPushButton("Envs", this);
    connect(m_manageEnvBtn, &QPushButton::clicked, this, &CollectionSidebar::manageEnvironmentsRequested);
    envLayout->addWidget(m_manageEnvBtn);

    mainLayout->addLayout(envLayout);

    // 3. Tree Widget
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QTreeWidget::itemClicked, this, &CollectionSidebar::onItemClicked);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &CollectionSidebar::onContextMenu);
    mainLayout->addWidget(m_tree);

    connect(m_model, &core::CollectionModel::collectionLoaded, this, &CollectionSidebar::refreshTree);
    connect(m_model, &core::CollectionModel::itemModified, this, &CollectionSidebar::refreshTree);
}

QString CollectionSidebar::currentEnvironmentName() const {
    return m_envCombo->currentData().toString();
}

void CollectionSidebar::updateEnvironmentsCombo() {
    QString current = m_envCombo->currentData().toString();
    m_envCombo->clear();
    m_envCombo->addItem("No Environment", "");

    for (const auto& env : m_model->environments()) {
        m_envCombo->addItem(env.name(), env.name());
    }

    int idx = m_envCombo->findData(current);
    if (idx >= 0) m_envCombo->setCurrentIndex(idx);
}

void CollectionSidebar::refreshTree() {
    m_tree->clear();
    updateEnvironmentsCombo();

    auto* root = m_model->rootItem();
    if (!root) return;

    auto* rootWidget = new QTreeWidgetItem(m_tree);
    rootWidget->setText(0, "📁 " + root->name());
    rootWidget->setData(0, Qt::UserRole, QVariant::fromValue(static_cast<void*>(root)));
    rootWidget->setExpanded(true);

    populateChildren(rootWidget, root);
}

void CollectionSidebar::populateChildren(QTreeWidgetItem* parentWidget, core::CollectionItem* parentModel) {
    for (auto* child : parentModel->children()) {
        auto* childWidget = new QTreeWidgetItem(parentWidget);
        childWidget->setData(0, Qt::UserRole, QVariant::fromValue(static_cast<void*>(child)));

        if (child->type() == core::CollectionItemType::Folder) {
            childWidget->setText(0, "📁 " + child->name());
            childWidget->setExpanded(true);
            populateChildren(childWidget, child);
        } else if (child->type() == core::CollectionItemType::Request) {
            QString method = "GET";
            if (child->request()) {
                method = core::methodToString(child->request()->method);
            }
            childWidget->setText(0, QString("[%1]  %2").arg(method, child->name()));
            
            // Set method badge color
            if (child->request()) {
                QColor c = Theme::methodColor(child->request()->method);
                childWidget->setForeground(0, c);
            }
        }
    }
}

core::CollectionItem* CollectionSidebar::itemFromWidget(QTreeWidgetItem* widget) const {
    if (!widget) return nullptr;
    return static_cast<core::CollectionItem*>(widget->data(0, Qt::UserRole).value<void*>());
}

void CollectionSidebar::onItemClicked(QTreeWidgetItem* widget, int /*column*/) {
    auto* item = itemFromWidget(widget);
    if (item && item->type() == core::CollectionItemType::Request) {
        emit requestSelected(item);
    }
}

void CollectionSidebar::onContextMenu(const QPoint& pos) {
    auto* widget = m_tree->itemAt(pos);
    auto* item = itemFromWidget(widget);

    QMenu menu(this);
    if (item && item->type() == core::CollectionItemType::Request) {
        menu.addAction("Open Request", this, [this, item]() { emit requestSelected(item); });
        menu.addAction("Copy as cURL", this, &CollectionSidebar::onCopyAsCurl);
        menu.addSeparator();
        menu.addAction("Rename", this, &CollectionSidebar::onRenameItem);
        menu.addAction("Delete", this, &CollectionSidebar::onDeleteItem);
    } else {
        menu.addAction("New Request", this, &CollectionSidebar::onAddRequest);
        menu.addAction("New Folder", this, &CollectionSidebar::onAddFolder);
        if (item && item != m_model->rootItem()) {
            menu.addSeparator();
            menu.addAction("Rename Folder", this, &CollectionSidebar::onRenameItem);
            menu.addAction("Delete Folder", this, &CollectionSidebar::onDeleteItem);
        }
    }

    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

void CollectionSidebar::onAddRequest() {
    auto* current = itemFromWidget(m_tree->currentItem());
    core::CollectionItem* parentFolder = nullptr;
    if (current) {
        parentFolder = (current->type() == core::CollectionItemType::Request) ? current->parent() : current;
    } else {
        parentFolder = m_model->rootItem();
    }

    if (!parentFolder) {
        QMessageBox::information(this, "Open Collection", "Please open or create a collection first.");
        return;
    }

    bool ok = false;
    QString name = QInputDialog::getText(this, "New Request", "Request Name:", QLineEdit::Normal, "New Request", &ok);
    if (ok && !name.trimmed().isEmpty()) {
        core::RequestModel req;
        req.name = name.trimmed();
        req.method = core::HttpMethod::GET;
        req.url = "https://httpbin.org/get";
        auto* newItem = m_model->addRequest(parentFolder, name.trimmed(), req);
        refreshTree();
        if (newItem) {
            emit requestSelected(newItem);
        }
    }
}

void CollectionSidebar::onAddFolder() {
    auto* current = itemFromWidget(m_tree->currentItem());
    core::CollectionItem* parentFolder = nullptr;
    if (current) {
        parentFolder = (current->type() == core::CollectionItemType::Request) ? current->parent() : current;
    } else {
        parentFolder = m_model->rootItem();
    }

    if (!parentFolder) {
        QMessageBox::information(this, "Open Collection", "Please open or create a collection first.");
        return;
    }

    bool ok = false;
    QString name = QInputDialog::getText(this, "New Folder", "Folder Name:", QLineEdit::Normal, "New Folder", &ok);
    if (ok && !name.trimmed().isEmpty()) {
        m_model->addFolder(parentFolder, name.trimmed());
        refreshTree();
    }
}

void CollectionSidebar::onRenameItem() {
    auto* item = itemFromWidget(m_tree->currentItem());
    if (!item || item == m_model->rootItem()) return;

    bool ok = false;
    QString newName = QInputDialog::getText(this, "Rename", "New Name:", QLineEdit::Normal, item->name(), &ok);
    if (ok && !newName.trimmed().isEmpty()) {
        m_model->renameItem(item, newName.trimmed());
        refreshTree();
    }
}

void CollectionSidebar::onDeleteItem() {
    auto* item = itemFromWidget(m_tree->currentItem());
    if (!item || item == m_model->rootItem()) return;

    auto ans = QMessageBox::question(this, "Confirm Delete", QString("Delete '%1'?").arg(item->name()));
    if (ans == QMessageBox::Yes) {
        m_model->deleteItem(item);
        refreshTree();
    }
}

void CollectionSidebar::onCopyAsCurl() {
    auto* item = itemFromWidget(m_tree->currentItem());
    if (item && item->request()) {
        QClipboard* clipboard = QGuiApplication::clipboard();
        clipboard->setText(item->request()->toCurlCommand());
    }
}

} // namespace poppy::gui
