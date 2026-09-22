#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <memory>
#include <QFileSystemWatcher>
#include <QTimer>
#include "RequestModel.h"
#include "EnvironmentModel.h"

namespace poppy::core {

enum class CollectionItemType {
    Collection,
    Folder,
    Request
};

class CollectionItem {
public:
    CollectionItem(CollectionItemType type, QString name, QString path, CollectionItem* parent = nullptr);
    ~CollectionItem();

    CollectionItemType type() const { return m_type; }
    const QString& name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }
    const QString& path() const { return m_path; }
    void setPath(const QString& path) { m_path = path; }

    CollectionItem* parent() const { return m_parent; }
    void setParent(CollectionItem* p) { m_parent = p; }

    const QList<CollectionItem*>& children() const { return m_children; }
    void appendChild(CollectionItem* child);
    void insertChild(int index, CollectionItem* child);
    void removeChild(CollectionItem* child);
    void sortChildrenBySeq();

    int seq() const;
    void setSeq(int seq);

    RequestModel* request() { return m_request.get(); }
    const RequestModel* request() const { return m_request.get(); }
    void setRequest(const RequestModel& req);

    const QMap<QString, QString>& variables() const { return m_variables; }
    void setVariables(const QMap<QString, QString>& vars) { m_variables = vars; }
    void setVariable(const QString& key, const QString& value) { m_variables[key] = value; }

    // Aggregate variables from this folder and all ancestor folders
    QMap<QString, QString> effectiveVariables() const {
        QMap<QString, QString> result;
        if (m_parent) {
            result = m_parent->effectiveVariables();
        }
        for (auto it = m_variables.begin(); it != m_variables.end(); ++it) {
            result[it.key()] = it.value();
        }
        return result;
    }

    int row() const;

private:
    CollectionItemType m_type;
    QString m_name;
    QString m_path;
    CollectionItem* m_parent{nullptr};
    QList<CollectionItem*> m_children;
    std::unique_ptr<RequestModel> m_request;
    QMap<QString, QString> m_variables;
    int m_seq{1};
};

class CollectionModel : public QObject {
    Q_OBJECT
public:
    explicit CollectionModel(QObject* parent = nullptr);
    ~CollectionModel() override;

    bool openDirectory(const QString& dirPath);
    void reload();

    CollectionItem* rootItem() const { return m_rootItem.get(); }
    const QString& rootPath() const { return m_rootPath; }
    QString name() const { return m_rootItem ? m_rootItem->name() : QString(); }
    QList<RequestModel> allRequests() const;
    QList<CollectionItem*> allRequestItems() const;

    // Request & Folder operations
    CollectionItem* addRequest(CollectionItem* parent, const QString& name, const RequestModel& req);
    CollectionItem* addFolder(CollectionItem* parent, const QString& name);
    bool saveRequest(CollectionItem* item);
    bool saveFolderVariables(CollectionItem* folder);
    bool deleteItem(CollectionItem* item);
    bool renameItem(CollectionItem* item, const QString& newName);
    bool moveItem(CollectionItem* item, CollectionItem* newParent, int insertIndex = -1);
    CollectionItem* findItemByPath(const QString& path) const;

    // Environments discovered in collection
    const QList<EnvironmentModel>& environments() const { return m_environments; }
    QList<EnvironmentModel>& environments() { return m_environments; }
    void reloadEnvironments();
    bool saveEnvironment(const EnvironmentModel& env);

signals:
    void collectionAboutToReload();
    void collectionLoaded();
    void directoryChangedOnDisk(const QString& path);
    void itemModified(CollectionItem* item);
    void itemAboutToBeDeleted(CollectionItem* item);

private:
    void scanDirectory(const QString& dirPath, CollectionItem* parentItem);
    void notifyItemTreeDeleted(CollectionItem* item);
    void rewriteDescendantPaths(CollectionItem* item, const QString& oldPrefix, const QString& newPrefix);
    void persistSiblingOrder(CollectionItem* parent);
    CollectionItem* findItemByPathRecursive(CollectionItem* item, const QString& canonicalPath) const;
    void scheduleReloadFromDisk();
    void suppressDiskWatcher();

    QString m_rootPath;
    std::unique_ptr<CollectionItem> m_rootItem;
    QList<EnvironmentModel> m_environments;
    QFileSystemWatcher m_fileWatcher;
    QTimer m_reloadDebounce;
    QTimer m_suppressClearTimer;
    bool m_suppressWatchReload{false};
};

} // namespace poppy::core
