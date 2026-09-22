#include "CollectionModel.h"
#include "BruParser.h"
#include "BruWriter.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTimer>

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
    m_reloadDebounce.setSingleShot(true);
    m_reloadDebounce.setInterval(400);
    connect(&m_reloadDebounce, &QTimer::timeout, this, [this]() {
        if (!m_rootPath.isEmpty()) {
            reload();
        }
    });
    m_suppressClearTimer.setSingleShot(true);
    m_suppressClearTimer.setInterval(750);
    connect(&m_suppressClearTimer, &QTimer::timeout, this, [this]() {
        m_suppressWatchReload = false;
    });
    connect(&m_fileWatcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString& path) {
        emit directoryChangedOnDisk(path);
        scheduleReloadFromDisk();
    });
}

CollectionModel::~CollectionModel() = default;

bool CollectionModel::openDirectory(const QString& dirPath) {
    QDir dir(dirPath);
    if (!dir.exists()) return false;

    m_suppressWatchReload = true;
    m_reloadDebounce.stop();

    emit collectionAboutToReload();

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

    m_suppressWatchReload = false;
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
        if (entry.isDir()) {
            const QString dirName = entry.fileName();
            if (dirName.startsWith('.') || dirName == "environments") continue;

            auto* folderItem = new CollectionItem(CollectionItemType::Folder, dirName, entry.canonicalFilePath());
            QString folderBru = QDir(entry.canonicalFilePath()).filePath(QStringLiteral("folder.bru"));
            if (QFile::exists(folderBru)) {
                folderItem->setVariables(BruParser::parseVarsFile(folderBru));
            }
            parentItem->appendChild(folderItem);
            m_fileWatcher.addPath(entry.canonicalFilePath());
            scanDirectory(entry.canonicalFilePath(), folderItem);
        } else if (entry.isFile() && entry.fileName().endsWith(QLatin1String(".bru"), Qt::CaseInsensitive)) {
            const QString fileName = entry.fileName();
            // Bruno folder/collection metadata files are not requests
            if (fileName.compare(QLatin1String("folder.bru"), Qt::CaseInsensitive) == 0) continue;
            if (fileName.compare(QLatin1String("collection.bru"), Qt::CaseInsensitive) == 0) continue;

            RequestModel req = BruParser::parseFile(entry.canonicalFilePath());
            QString reqName = req.name.isEmpty() ? entry.completeBaseName() : req.name;
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

    suppressDiskWatcher();
    QString parentDir = targetParent->path();
    QString filePath = BruWriter::uniqueFilePath(parentDir, BruWriter::safeFileStem(name), ".bru");

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

    suppressDiskWatcher();
    QString parentDir = targetParent->path();
    QString safeName = BruWriter::safeFileStem(name, QStringLiteral("folder"));
    QDir dir(parentDir);
    if (!dir.mkdir(safeName)) {
        return nullptr;
    }

    QString folderPath = dir.filePath(safeName);
    auto* item = new CollectionItem(CollectionItemType::Folder, safeName, QFileInfo(folderPath).canonicalFilePath());
    targetParent->appendChild(item);
    m_fileWatcher.addPath(folderPath);
    emit itemModified(item);
    return item;
}

bool CollectionModel::saveRequest(CollectionItem* item) {
    if (!item || item->type() != CollectionItemType::Request || !item->request()) {
        return false;
    }
    suppressDiskWatcher();
    return BruWriter::writeToFile(item->path(), *item->request());
}

bool CollectionModel::saveFolderVariables(CollectionItem* folder) {
    if (!folder || folder->type() == CollectionItemType::Request) {
        return false;
    }
    suppressDiskWatcher();
    return BruWriter::writeFolderFile(folder->path(), folder->name(), folder->variables());
}

void CollectionModel::notifyItemTreeDeleted(CollectionItem* item) {
    if (!item) return;
    const auto children = item->children();
    for (auto* child : children) {
        notifyItemTreeDeleted(child);
    }
    emit itemAboutToBeDeleted(item);
}

bool CollectionModel::deleteItem(CollectionItem* item) {
    if (!item || item == m_rootItem.get()) return false;

    suppressDiskWatcher();
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
    notifyItemTreeDeleted(item);
    emit itemModified(nullptr);
    delete item;
    return true;
}

bool CollectionModel::renameItem(CollectionItem* item, const QString& newName) {
    if (!item || item == m_rootItem.get()) return false;

    QFileInfo fi(item->path());
    QString parentDir = fi.dir().path();
    const QString oldPath = item->path();
    suppressDiskWatcher();

    if (item->type() == CollectionItemType::Folder) {
        QString safeName = BruWriter::safeFileStem(newName, QStringLiteral("folder"));
        QString newPath = QDir(parentDir).filePath(safeName);
        const QString oldCanon = QFileInfo(oldPath).canonicalFilePath();
        auto sameAsOld = [&](const QString& candidate) {
            const QString c = QFileInfo(candidate).canonicalFilePath();
            if (!c.isEmpty() && !oldCanon.isEmpty()) return c == oldCanon;
            return QDir::cleanPath(candidate) == QDir::cleanPath(oldPath);
        };
        if (QFileInfo::exists(newPath) && !sameAsOld(newPath)) {
            int n = 1;
            QString candidate;
            do {
                candidate = QDir(parentDir).filePath(QStringLiteral("%1 (%2)").arg(safeName).arg(n++));
            } while (QFileInfo::exists(candidate));
            newPath = candidate;
        }
        if (!sameAsOld(newPath)) {
            QDir dir;
            if (!dir.rename(oldPath, newPath)) return false;
            item->setPath(QFileInfo(newPath).canonicalFilePath());
            rewriteDescendantPaths(item, oldPath, item->path());
            if (m_fileWatcher.directories().contains(oldPath)) {
                m_fileWatcher.removePath(oldPath);
            }
            m_fileWatcher.addPath(item->path());
        }
        item->setName(newName);
        BruWriter::writeFolderFile(item->path(), newName, item->variables());
    } else {
        QString newPath = BruWriter::uniqueFilePath(parentDir, BruWriter::safeFileStem(newName), ".bru", oldPath);
        if (QDir::cleanPath(newPath) != QDir::cleanPath(oldPath)
            && QFileInfo(newPath).canonicalFilePath() != QFileInfo(oldPath).canonicalFilePath()) {
            QFile file(oldPath);
            if (!file.rename(newPath)) return false;
            item->setPath(newPath);
        }
        item->setName(newName);
        if (item->request()) {
            item->request()->name = newName;
            saveRequest(item);
        }
    }

    emit itemModified(item);
    return true;
}

bool CollectionModel::moveItem(CollectionItem* item, CollectionItem* newParent) {
    if (!item || item == m_rootItem.get()) return false;
    if (!newParent) newParent = m_rootItem.get();
    if (!newParent || newParent->type() == CollectionItemType::Request) return false;
    if (item->parent() == newParent) return true;

    for (auto* p = newParent; p; p = p->parent()) {
        if (p == item) return false;
    }

    const QString oldPath = item->path();
    QFileInfo fi(oldPath);
    QString destPath = QDir(newParent->path()).filePath(fi.fileName());
    if (QFileInfo::exists(destPath)) return false;

    suppressDiskWatcher();
    bool renamed = false;
    if (item->type() == CollectionItemType::Folder) {
        renamed = QDir().rename(oldPath, destPath);
    } else {
        renamed = QFile::rename(oldPath, destPath);
    }
    if (!renamed) return false;

    if (item->parent()) {
        item->parent()->removeChild(item);
    }
    newParent->appendChild(item);

    const QString canonicalDest = QFileInfo(destPath).canonicalFilePath();
    item->setPath(canonicalDest.isEmpty() ? destPath : canonicalDest);
    if (item->type() == CollectionItemType::Folder) {
        rewriteDescendantPaths(item, oldPath, item->path());
        if (m_fileWatcher.directories().contains(oldPath)) {
            m_fileWatcher.removePath(oldPath);
        }
        m_fileWatcher.addPath(item->path());
    }

    emit itemModified(item);
    return true;
}

CollectionItem* CollectionModel::findItemByPath(const QString& path) const {
    if (path.isEmpty() || !m_rootItem) return nullptr;
    const QString canonical = QFileInfo(path).canonicalFilePath();
    const QString needle = canonical.isEmpty() ? QDir::cleanPath(path) : canonical;
    if (QDir::cleanPath(m_rootItem->path()) == needle || m_rootItem->path() == path) {
        return m_rootItem.get();
    }
    return findItemByPathRecursive(m_rootItem.get(), needle);
}

CollectionItem* CollectionModel::findItemByPathRecursive(CollectionItem* item, const QString& canonicalPath) const {
    if (!item) return nullptr;
    const QString itemCanon = QFileInfo(item->path()).canonicalFilePath();
    if (!itemCanon.isEmpty() && itemCanon == canonicalPath) return item;
    if (QDir::cleanPath(item->path()) == canonicalPath) return item;
    for (auto* child : item->children()) {
        if (auto* found = findItemByPathRecursive(child, canonicalPath)) return found;
    }
    return nullptr;
}

void CollectionModel::rewriteDescendantPaths(CollectionItem* item, const QString& oldPrefix, const QString& newPrefix) {
    if (!item) return;
    for (auto* child : item->children()) {
        QString p = child->path();
        const bool bounded = (p == oldPrefix)
            || p.startsWith(oldPrefix + QLatin1Char('/'))
            || p.startsWith(oldPrefix + QLatin1Char('\\'));
        if (bounded) {
            child->setPath(newPrefix + p.mid(oldPrefix.size()));
        }
        rewriteDescendantPaths(child, oldPrefix, newPrefix);
    }
}

void CollectionModel::scheduleReloadFromDisk() {
    if (m_suppressWatchReload || m_rootPath.isEmpty()) return;
    m_reloadDebounce.start();
}

void CollectionModel::suppressDiskWatcher() {
    m_suppressWatchReload = true;
    m_reloadDebounce.stop();
    m_suppressClearTimer.start();
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

static void collectAllRequestItemsRecursively(CollectionItem* item, QList<CollectionItem*>& list) {
    if (!item) return;
    if (item->type() == CollectionItemType::Request && item->request()) {
        list.append(item);
    }
    for (auto* child : item->children()) {
        collectAllRequestItemsRecursively(child, list);
    }
}

QList<CollectionItem*> CollectionModel::allRequestItems() const {
    QList<CollectionItem*> list;
    collectAllRequestItemsRecursively(m_rootItem.get(), list);
    return list;
}

} // namespace poppy::core
