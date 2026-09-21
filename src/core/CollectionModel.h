#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <memory>
#include <QFileSystemWatcher>
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
    void removeChild(CollectionItem* child);

    RequestModel* request() { return m_request.get(); }
    const RequestModel* request() const { return m_request.get(); }
    void setRequest(const RequestModel& req);

    int row() const;

private:
    CollectionItemType m_type;
    QString m_name;
    QString m_path;
    CollectionItem* m_parent{nullptr};
    QList<CollectionItem*> m_children;
    std::unique_ptr<RequestModel> m_request;
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

    // Request & Folder operations
    CollectionItem* addRequest(CollectionItem* parent, const QString& name, const RequestModel& req);
    CollectionItem* addFolder(CollectionItem* parent, const QString& name);
    bool saveRequest(CollectionItem* item);
    bool deleteItem(CollectionItem* item);
    bool renameItem(CollectionItem* item, const QString& newName);

    // Environments discovered in collection
    const QList<EnvironmentModel>& environments() const { return m_environments; }
    QList<EnvironmentModel>& environments() { return m_environments; }
    void reloadEnvironments();

signals:
    void collectionLoaded();
    void directoryChangedOnDisk(const QString& path);
    void itemModified(CollectionItem* item);

private:
    void scanDirectory(const QString& dirPath, CollectionItem* parentItem);

    QString m_rootPath;
    std::unique_ptr<CollectionItem> m_rootItem;
    QList<EnvironmentModel> m_environments;
    QFileSystemWatcher m_fileWatcher;
};

} // namespace poppy::core
