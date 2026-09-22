#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QTabWidget>
#include <core/CollectionModel.h>
#include <core/HistoryManager.h>

namespace poppy::gui {

class CollectionSidebar : public QWidget {
    Q_OBJECT
public:
    explicit CollectionSidebar(core::CollectionModel* model, core::HistoryManager* historyManager = nullptr, QWidget* parent = nullptr);

    void setHistoryManager(core::HistoryManager* manager);
    void refreshTree();
    void refreshHistory();
    void updateEnvironmentsCombo();
    void setActiveEnvironment(const QString& name);
    QString currentEnvironmentName() const;

signals:
    void requestSelected(core::CollectionItem* item);
    void historyItemSelected(const core::HistoryItem& item);
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
    void onFolderVariables();
    void onDuplicateRequest();
    void onCopyAsCurl();
    void onCollectionFilterChanged(const QString& query);

    // History slots
    void onHistoryItemClicked(QTreeWidgetItem* item, int column);
    void onHistoryContextMenu(const QPoint& pos);
    void onHistoryFilterChanged(const QString& query);
    void onClearHistory();

private:
    void setupCollectionsTab(QWidget* container);
    void setupHistoryTab(QWidget* container);
    void populateChildren(QTreeWidgetItem* parentWidget, core::CollectionItem* parentModel);
    core::CollectionItem* itemFromWidget(QTreeWidgetItem* widget) const;

    core::CollectionModel* m_model;
    core::HistoryManager* m_historyManager;

    QTabWidget* m_tabs;

    // Collections Tab widgets
    QLineEdit* m_collectionFilterEdit{nullptr};
    QComboBox* m_envCombo;
    QPushButton* m_manageEnvBtn;
    QPushButton* m_openBtn;
    QPushButton* m_addReqBtn;
    QPushButton* m_addFolderBtn;
    QTreeWidget* m_tree;

    // History Tab widgets
    QLineEdit* m_historyFilterEdit;
    QPushButton* m_clearHistoryBtn;
    QTreeWidget* m_historyTree;
};

} // namespace poppy::gui
