#include "CollectionModel.h"
#include "BruParser.h"
#include "BruWriter.h"
#include <QDir>
#include <QFileInfo>

namespace poppy::core {

CollectionItem::CollectionItem(CollectionItemType type, QString name, QString path, CollectionItem* parent)
    : m_type(type), m_name(std::move(name)), m_path(std::move(path)), m_parent(parent) {}

CollectionItem::~CollectionItem() {
    qDeleteAll(m_children);
}

void CollectionItem::appendChild(CollectionItem* child) {
    if (child) {
        child->setParent(this);
        m_children.append(child);
    }
}

void CollectionItem::removeChild(CollectionItem* child) {
    m_children.removeOne(child);
}

void CollectionItem::setRequest(const RequestModel& req) {
    m_request = std::make_unique<RequestModel>(req);
}

int CollectionItem::row() const {
    if (m_parent) {
        return m_parent->m_children.indexOf(const_cast<CollectionItem*>(this));
    }
    return 0;
}

CollectionModel::CollectionModel(QObject* parent) : QObject(parent) {
    connect(&m_fileWatcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString& path) {
        emit directoryChangedOnDisk(path);
    });
}

CollectionModel::~CollectionModel() = default;

bool CollectionModel::openDirectory(const QString& dirPath) {
    QDir dir(dirPath);
    if (!dir.exists()) return false;

    m_rootPath = dir.canonicalPath();
    m_rootItem = std::make_unique<CollectionItem>(CollectionItemType::Collection, dir.dirName(), m_rootPath);

    // Watch directory
    QStringList existingPaths = m_fileWatcher.directories();
    if (!existingPaths.isEmpty()) {
        m_fileWatcher.removePaths(existingPaths);
    }
    m_fileWatcher.addPath(m_rootPath);

    scanDirectory(m_rootPath, m_rootItem.get());
    reloadEnvironments();

    emit collectionLoaded();
    return true;
}

void CollectionModel::reload() {
    if (!m_rootPath.isEmpty()) {
        openDirectory(m_rootPath);
    }
}

void CollectionModel::scanDirectory(const QString& dirPath, CollectionItem* parentItem) {
    QDir dir(dirPath);
    QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::DirsFirst | QDir::Name);

    for (const auto& entry : entries) {
        if (entry.fileName().startsWith('.')) continue; // ignore hidden (.git, etc.)
        if (entry.fileName() == "environments") continue; // handled separately

        if (entry.isDir()) {
            auto* folderItem = new CollectionItem(CollectionItemType::Folder, entry.fileName(), entry.canonicalFilePath());
            parentItem->appendChild(folderItem);
            m_fileWatcher.addPath(entry.canonicalFilePath());
            scanDirectory(entry.canonicalFilePath(), folderItem);
        } else if (entry.isFile() && entry.suffix().toLower() == "bru") {
            RequestModel req = BruParser::parseFile(entry.canonicalFilePath());
            QString reqName = req.name.isEmpty() ? entry.baseName() : req.name;
            auto* reqItem = new CollectionItem(CollectionItemType::Request, reqName, entry.canonicalFilePath());
            reqItem->setRequest(req);
            parentItem->appendChild(reqItem);
        }
    }
}

void CollectionModel::reloadEnvironments() {
    m_environments.clear();
    if (m_rootPath.isEmpty()) return;

    QDir envDir(m_rootPath + "/environments");
    if (!envDir.exists()) return;

    QFileInfoList entries = envDir.entryInfoList(QStringList() << "*.env", QDir::Files);
    for (const auto& entry : entries) {
        if (entry.fileName().endsWith(".secret.env")) continue; // skip secret file directly; it merges into the base env

        QString envName = entry.baseName();
        EnvironmentModel env = EnvironmentModel::loadFromEnvFile(entry.canonicalFilePath(), envName);

        // Check if matching secret file exists
        QString secretPath = envDir.filePath(envName + ".secret.env");
        if (QFile::exists(secretPath)) {
            env.loadSecretsFromEnvFile(secretPath);
        }

        m_environments.append(env);
    }
}

CollectionItem* CollectionModel::addRequest(CollectionItem* parent, const QString& name, const RequestModel& req) {
    CollectionItem* targetParent = parent ? parent : m_rootItem.get();
    if (!targetParent) return nullptr;

    QString parentDir = targetParent->path();
    QString fileName = name.toLower().replace(' ', '-') + ".bru";
    QString filePath = QDir(parentDir).filePath(fileName);

    RequestModel copy = req;
    copy.name = name;
    if (!BruWriter::writeToFile(filePath, copy)) {
        return nullptr;
    }

    auto* item = new CollectionItem(CollectionItemType::Request, name, filePath);
    item->setRequest(copy);
    targetParent->appendChild(item);
    emit itemModified(item);
    return item;
}

CollectionItem* CollectionModel::addFolder(CollectionItem* parent, const QString& name) {
    CollectionItem* targetParent = parent ? parent : m_rootItem.get();
    if (!targetParent) return nullptr;

    QString parentDir = targetParent->path();
    QDir dir(parentDir);
    if (!dir.mkdir(name)) {
        return nullptr;
    }

    QString folderPath = dir.canonicalPath() + "/" + name;
    auto* item = new CollectionItem(CollectionItemType::Folder, name, folderPath);
    targetParent->appendChild(item);
    m_fileWatcher.addPath(folderPath);
    emit itemModified(item);
    return item;
}

bool CollectionModel::saveRequest(CollectionItem* item) {
    if (!item || item->type() != CollectionItemType::Request || !item->request()) {
        return false;
    }
    return BruWriter::writeToFile(item->path(), *item->request());
}

bool CollectionModel::deleteItem(CollectionItem* item) {
    if (!item || item == m_rootItem.get()) return false;

    if (item->type() == CollectionItemType::Folder) {
        QDir dir(item->path());
        if (!dir.removeRecursively()) return false;
    } else {
        QFile file(item->path());
        if (!file.remove()) return false;
    }

    if (item->parent()) {
        item->parent()->removeChild(item);
    }
    emit itemModified(nullptr);
    delete item;
    return true;
}

bool CollectionModel::renameItem(CollectionItem* item, const QString& newName) {
    if (!item || item == m_rootItem.get()) return false;

    QFileInfo fi(item->path());
    QString parentDir = fi.dir().path();

    if (item->type() == CollectionItemType::Folder) {
        QDir dir(item->path());
        QString newPath = parentDir + "/" + newName;
        if (!dir.rename(item->path(), newPath)) return false;
        item->setName(newName);
        item->setPath(newPath);
    } else {
        QString newPath = parentDir + "/" + newName.toLower().replace(' ', '-') + ".bru";
        QFile file(item->path());
        if (!file.rename(newPath)) return false;
        item->setName(newName);
        item->setPath(newPath);
        if (item->request()) {
            item->request()->name = newName;
            saveRequest(item);
        }
    }

    emit itemModified(item);
    return true;
}

static void collectAllRequestsRecursively(const CollectionItem* item, QList<RequestModel>& list) {
    if (!item) return;
    if (item->type() == CollectionItemType::Request && item->request()) {
        list.append(*item->request());
    }
    for (const auto* child : item->children()) {
        collectAllRequestsRecursively(child, list);
    }
}

QList<RequestModel> CollectionModel::allRequests() const {
    QList<RequestModel> list;
    collectAllRequestsRecursively(m_rootItem.get(), list);
    return list;
}

} // namespace poppy::core
