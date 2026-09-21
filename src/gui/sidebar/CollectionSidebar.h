#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QComboBox>
#include <QPushButton>
#include <core/CollectionModel.h>

namespace poppy::gui {

class CollectionSidebar : public QWidget {
    Q_OBJECT
public:
    explicit CollectionSidebar(core::CollectionModel* model, QWidget* parent = nullptr);

    void refreshTree();
    void updateEnvironmentsCombo();
    QString currentEnvironmentName() const;

signals:
    void requestSelected(core::CollectionItem* item);
    void environmentChanged(const QString& envName);
    void manageEnvironmentsRequested();
    void openCollectionRequested();

private slots:
    void onItemClicked(QTreeWidgetItem* item, int column);
    void onContextMenu(const QPoint& pos);
    void onAddRequest();
    void onAddFolder();
    void onRenameItem();
    void onDeleteItem();
    void onCopyAsCurl();

private:
    void populateChildren(QTreeWidgetItem* parentWidget, core::CollectionItem* parentModel);
    core::CollectionItem* itemFromWidget(QTreeWidgetItem* widget) const;

    core::CollectionModel* m_model;
    QComboBox* m_envCombo;
    QPushButton* m_manageEnvBtn;
    QPushButton* m_openBtn;
    QPushButton* m_addReqBtn;
    QPushButton* m_addFolderBtn;
    QTreeWidget* m_tree;
};

} // namespace poppy::gui
