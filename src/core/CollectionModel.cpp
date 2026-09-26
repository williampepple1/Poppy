#include "CollectionModel.h"
#include "BruParser.h"
#include "BruWriter.h"
#include "OpenCollectionParser.h"
#include "OpenCollectionWriter.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <algorithm>

namespace poppy::core {

CollectionItem::CollectionItem(CollectionItemType type, QString name, QString path, CollectionItem* parent)
    : m_type(type), m_name(std::move(name)), m_path(std::move(path)), m_parent(parent) {}

CollectionItem::~CollectionItem() {
    qDeleteAll(m_children);
}

void CollectionItem::appendChild(CollectionItem* child) {
    insertChild(-1, child);
}

void CollectionItem::insertChild(int index, CollectionItem* child) {
    if (!child) return;
    child->setParent(this);
    if (index < 0 || index > m_children.size()) {
        m_children.append(child);
    } else {
        m_children.insert(index, child);
    }
}

void CollectionItem::removeChild(CollectionItem* child) {
    m_children.removeOne(child);
}

int CollectionItem::seq() const {
    if (m_request) return m_request->seq;
    return m_seq;
}

void CollectionItem::setSeq(int seq) {
    m_seq = seq;
    if (m_request) {
        m_request->seq = seq;
    }
}

void CollectionItem::sortChildrenBySeq() {
    std::stable_sort(m_children.begin(), m_children.end(), [](const CollectionItem* a, const CollectionItem* b) {
        if (!a || !b) return a != nullptr;
        if (a->seq() != b->seq()) return a->seq() < b->seq();
        return a->name().localeAwareCompare(b->name()) < 0;
    });
    for (auto* child : m_children) {
        child->sortChildrenBySeq();
    }
}

void CollectionItem::setRequest(const RequestModel& req) {
    m_request = std::make_unique<RequestModel>(req);
}

AuthModel CollectionItem::effectiveAuth() const {
    const CollectionItem* node = (m_type == CollectionItemType::Request) ? m_parent : this;
    while (node) {
        if (node->m_auth.type != AuthType::None && node->m_auth.type != AuthType::Inherit) {
            return node->m_auth;
        }
        node = node->m_parent;
    }
    return {};
}

void CollectionItem::applyInheritedHeaders(RequestModel& req) const {
    QList<const CollectionItem*> chain;
    const CollectionItem* node = (m_type == CollectionItemType::Request) ? m_parent : this;
    while (node) {
        chain.prepend(node);
        node = node->m_parent;
    }

    QList<HttpHeader> inherited;
    for (const CollectionItem* ancestor : chain) {
        for (const HttpHeader& header : ancestor->m_headers) {
            if (!header.enabled || header.name.isEmpty()) continue;
            bool replaced = false;
            for (HttpHeader& existing : inherited) {
                if (existing.name.compare(header.name, Qt::CaseInsensitive) == 0) {
                    existing = header;
                    replaced = true;
                    break;
                }
            }
            if (!replaced) inherited.append(header);
        }
    }

    for (const HttpHeader& header : inherited) {
        bool present = false;
        for (const HttpHeader& existing : req.headers) {
            if (existing.name.compare(header.name, Qt::CaseInsensitive) == 0) {
                present = true;
                break;
            }
        }
        if (!present) req.headers.append(header);
    }
}

RequestModel CollectionItem::requestForExecution() const {
    RequestModel req = m_request ? *m_request : RequestModel{};
    if (req.auth.type == AuthType::Inherit) {
        req.auth = effectiveAuth();
    }
    applyInheritedHeaders(req);
    return req;
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
    QString collName = dir.dirName();
    QString openCollYml = dir.filePath(QStringLiteral("opencollection.yml"));
    QString openCollYaml = dir.filePath(QStringLiteral("opencollection.yaml"));
    QString collYml = dir.filePath(QStringLiteral("collection.yml"));

    if (QFile::exists(openCollYml) || QFile::exists(openCollYaml)) {
        QString ocPath = QFile::exists(openCollYml) ? openCollYml : openCollYaml;
        auto ocInfo = OpenCollectionParser::parseCollectionFile(ocPath);
        if (!ocInfo.name.isEmpty()) collName = ocInfo.name;
    } else if (QFile::exists(collYml)) {
        auto ocInfo = OpenCollectionParser::parseCollectionFile(collYml);
        if (!ocInfo.name.isEmpty()) collName = ocInfo.name;
    }

    m_rootItem = std::make_unique<CollectionItem>(CollectionItemType::Collection, collName, m_rootPath);
    if (QFile::exists(openCollYml) || QFile::exists(openCollYaml)) {
        QString ocPath = QFile::exists(openCollYml) ? openCollYml : openCollYaml;
        auto ocInfo = OpenCollectionParser::parseCollectionFile(ocPath);
        m_rootItem->setVariables(ocInfo.vars);
        m_rootItem->setHeaders(ocInfo.headers);
        if (ocInfo.auth.type != AuthType::None) {
            m_rootItem->setAuth(ocInfo.auth);
        }
    } else if (QFile::exists(collYml)) {
        auto ocInfo = OpenCollectionParser::parseCollectionFile(collYml);
        m_rootItem->setVariables(ocInfo.vars);
        m_rootItem->setHeaders(ocInfo.headers);
        if (ocInfo.auth.type != AuthType::None) {
            m_rootItem->setAuth(ocInfo.auth);
        }
    }

    // Watch directory
    QStringList existingPaths = m_fileWatcher.directories();
    if (!existingPaths.isEmpty()) {
        m_fileWatcher.removePaths(existingPaths);
    }
    m_fileWatcher.addPath(m_rootPath);

    scanDirectory(m_rootPath, m_rootItem.get());
    reloadEnvironments();
    const QString envDir = QDir(m_rootPath).filePath(QStringLiteral("environments"));
    if (QDir(envDir).exists()) {
        m_fileWatcher.addPath(QFileInfo(envDir).canonicalFilePath());
    }

    m_suppressWatchReload = false;
    emit collectionLoaded();
    return true;
}

void CollectionModel::reload() {
    if (!m_rootPath.isEmpty()) {
        openDirectory(m_rootPath);
    }
}

void CollectionModel::closeCollection() {
    emit collectionAboutToReload();
    m_rootPath.clear();
    m_rootItem.reset();
    m_environments.clear();
    QStringList existingPaths = m_fileWatcher.directories();
    if (!existingPaths.isEmpty()) {
        m_fileWatcher.removePaths(existingPaths);
    }
    emit collectionLoaded();
}

void CollectionModel::scanDirectory(const QString& dirPath, CollectionItem* parentItem) {
    QDir dir(dirPath);
    QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::DirsFirst | QDir::Name);

    for (const auto& entry : entries) {
        if (entry.isDir()) {
            const QString dirName = entry.fileName();
            if (dirName.startsWith('.') || dirName == "environments" || dirName == "node_modules") continue;

            auto* folderItem = new CollectionItem(CollectionItemType::Folder, dirName, entry.canonicalFilePath());
            QString folderBru = QDir(entry.canonicalFilePath()).filePath(QStringLiteral("folder.bru"));
            QString folderYml = QDir(entry.canonicalFilePath()).filePath(QStringLiteral("folder.yml"));
            QString folderYaml = QDir(entry.canonicalFilePath()).filePath(QStringLiteral("folder.yaml"));

            if (QFile::exists(folderBru)) {
                QFile fb(folderBru);
                if (fb.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    const QString folderContent = QString::fromUtf8(fb.readAll());
                    folderItem->setVariables(BruParser::parseVars(folderContent));
                    folderItem->setSeq(BruParser::parseMetaSeq(folderContent, 1));
                    folderItem->setAuth(BruParser::parse(folderContent).auth);
                }
            } else if (QFile::exists(folderYml) || QFile::exists(folderYaml)) {
                QString ymlPath = QFile::exists(folderYml) ? folderYml : folderYaml;
                auto fInfo = OpenCollectionParser::parseFolderFile(ymlPath);
                folderItem->setVariables(fInfo.vars);
                folderItem->setHeaders(fInfo.headers);
                folderItem->setSeq(fInfo.seq);
                folderItem->setAuth(fInfo.auth);
                if (!fInfo.name.isEmpty()) {
                    folderItem->setName(fInfo.name);
                }
            }
            parentItem->appendChild(folderItem);
            m_fileWatcher.addPath(entry.canonicalFilePath());
            scanDirectory(entry.canonicalFilePath(), folderItem);
        } else if (entry.isFile()) {
            const QString fileName = entry.fileName();
            bool isBru = fileName.endsWith(QLatin1String(".bru"), Qt::CaseInsensitive);
            bool isYml = fileName.endsWith(QLatin1String(".yml"), Qt::CaseInsensitive) ||
                         fileName.endsWith(QLatin1String(".yaml"), Qt::CaseInsensitive);

            if (!isBru && !isYml) continue;

            // Folder metadata files are not requests
            if (fileName.compare(QLatin1String("folder.bru"), Qt::CaseInsensitive) == 0 ||
                fileName.compare(QLatin1String("folder.yml"), Qt::CaseInsensitive) == 0 ||
                fileName.compare(QLatin1String("folder.yaml"), Qt::CaseInsensitive) == 0) {
                continue;
            }

            // Collection metadata files are not requests
            if (fileName.compare(QLatin1String("collection.bru"), Qt::CaseInsensitive) == 0) {
                parentItem->setVariables(BruParser::parseVarsFile(entry.canonicalFilePath()));
                parentItem->setAuth(BruParser::parseFile(entry.canonicalFilePath()).auth);
                continue;
            }
            if (fileName.compare(QLatin1String("opencollection.yml"), Qt::CaseInsensitive) == 0 ||
                fileName.compare(QLatin1String("opencollection.yaml"), Qt::CaseInsensitive) == 0 ||
                fileName.compare(QLatin1String("collection.yml"), Qt::CaseInsensitive) == 0 ||
                fileName.compare(QLatin1String("collection.yaml"), Qt::CaseInsensitive) == 0) {
                auto cInfo = OpenCollectionParser::parseCollectionFile(entry.canonicalFilePath());
                if (parentItem == m_rootItem.get() && !cInfo.name.isEmpty()) {
                    parentItem->setName(cInfo.name);
                }
                if (cInfo.auth.type != AuthType::None) {
                    parentItem->setAuth(cInfo.auth);
                }
                continue;
            }

            RequestModel req;
            if (isBru) {
                req = BruParser::parseFile(entry.canonicalFilePath());
            } else {
                YamlNode yNode = YamlNode::parseFile(entry.canonicalFilePath());
                if (!OpenCollectionParser::isOpenCollectionRequest(yNode)) {
                    continue; // Skip non-request YAML files (e.g. CI/CD or other config)
                }
                req = OpenCollectionParser::parseRequestFile(entry.canonicalFilePath());
            }

            QString reqName = req.name.isEmpty() ? entry.completeBaseName() : req.name;
            auto* reqItem = new CollectionItem(CollectionItemType::Request, reqName, entry.canonicalFilePath());
            reqItem->setRequest(req);
            reqItem->setSeq(req.seq);
            parentItem->appendChild(reqItem);
        }
    }
    parentItem->sortChildrenBySeq();
}

void CollectionModel::reloadEnvironments() {
    m_environments.clear();
    if (m_rootPath.isEmpty()) return;

    QDir envDir(m_rootPath + "/environments");
    if (!envDir.exists()) return;

    // 1. Standard .env files
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

    // 2. OpenCollection YAML environments (*.yml, *.yaml)
    QFileInfoList ymlEntries = envDir.entryInfoList(QStringList() << "*.yml" << "*.yaml", QDir::Files);
    for (const auto& entry : ymlEntries) {
        EnvironmentModel env = OpenCollectionParser::parseEnvironmentFile(entry.canonicalFilePath());
        bool exists = false;
        for (const auto& existing : m_environments) {
            if (existing.name().compare(env.name(), Qt::CaseInsensitive) == 0) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            m_environments.append(env);
        }
    }
}

bool CollectionModel::saveEnvironment(const EnvironmentModel& env) {
    if (m_rootPath.isEmpty() || env.name().trimmed().isEmpty()) return false;
    suppressDiskWatcher();
    QDir dir(m_rootPath);
    if (!dir.mkpath(QStringLiteral("environments"))) return false;
    const QString envDir = QFileInfo(dir.filePath(QStringLiteral("environments"))).canonicalFilePath();
    if (!envDir.isEmpty() && !m_fileWatcher.directories().contains(envDir)) {
        m_fileWatcher.addPath(envDir);
    }

    const QString ymlPath = dir.filePath(QStringLiteral("environments/") + env.name() + QStringLiteral(".yml"));
    const QString yamlPath = dir.filePath(QStringLiteral("environments/") + env.name() + QStringLiteral(".yaml"));
    bool isOpenCollectionEnv = QFile::exists(ymlPath) || QFile::exists(yamlPath) ||
                               QFile::exists(dir.filePath(QStringLiteral("opencollection.yml"))) ||
                               QFile::exists(dir.filePath(QStringLiteral("opencollection.yaml")));

    bool ok = false;
    if (isOpenCollectionEnv) {
        QString targetPath = QFile::exists(yamlPath) ? yamlPath : ymlPath;
        ok = OpenCollectionWriter::writeEnvironmentFile(targetPath, env);
    } else {
        const QString envPath = dir.filePath(QStringLiteral("environments/") + env.name() + QStringLiteral(".env"));
        const QString secretPath = dir.filePath(QStringLiteral("environments/") + env.name() + QStringLiteral(".secret.env"));
        ok = env.saveToEnvFile(envPath);
        env.saveSecretsToEnvFile(secretPath);
    }

    bool found = false;
    for (auto& existing : m_environments) {
        if (existing.name() == env.name()) {
            existing = env;
            found = true;
            break;
        }
    }
    if (!found) {
        m_environments.append(env);
    }
    return ok;
}

CollectionItem* CollectionModel::addRequest(CollectionItem* parent, const QString& name, const RequestModel& req) {
    CollectionItem* targetParent = parent ? parent : m_rootItem.get();
    if (!targetParent) return nullptr;

    suppressDiskWatcher();
    QString parentDir = targetParent->path();

    bool useYml = false;
    if (QFile::exists(QDir(m_rootPath).filePath(QStringLiteral("opencollection.yml"))) ||
        QFile::exists(QDir(m_rootPath).filePath(QStringLiteral("opencollection.yaml")))) {
        useYml = true;
    } else {
        QDir pDir(parentDir);
        QStringList ymls = pDir.entryList(QStringList() << "*.yml" << "*.yaml", QDir::Files);
        if (!ymls.isEmpty()) {
            useYml = true;
        }
    }

    QString ext = useYml ? QStringLiteral(".yml") : QStringLiteral(".bru");
    QString filePath = BruWriter::uniqueFilePath(parentDir, BruWriter::safeFileStem(name), ext);

    RequestModel copy = req;
    copy.name = name;
    bool written = false;
    if (useYml) {
        written = OpenCollectionWriter::writeRequestFile(filePath, copy);
    } else {
        written = BruWriter::writeToFile(filePath, copy);
    }
    if (!written) {
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
    saveFolderVariables(item);
    emit itemModified(item);
    return item;
}

bool CollectionModel::saveRequest(CollectionItem* item) {
    if (!item || item->type() != CollectionItemType::Request || !item->request()) {
        return false;
    }
    suppressDiskWatcher();
    if (item->path().endsWith(".yml", Qt::CaseInsensitive) || item->path().endsWith(".yaml", Qt::CaseInsensitive)) {
        return OpenCollectionWriter::writeRequestFile(item->path(), *item->request());
    }
    return BruWriter::writeToFile(item->path(), *item->request());
}

bool CollectionModel::saveFolderVariables(CollectionItem* folder) {
    if (!folder || folder->type() == CollectionItemType::Request) {
        return false;
    }
    suppressDiskWatcher();
    QString folderYml = QDir(folder->path()).filePath(QStringLiteral("folder.yml"));
    QString folderYaml = QDir(folder->path()).filePath(QStringLiteral("folder.yaml"));
    bool hasYml = QFile::exists(folderYml) || QFile::exists(folderYaml) ||
                  QFile::exists(QDir(m_rootPath).filePath(QStringLiteral("opencollection.yml"))) ||
                  QFile::exists(QDir(m_rootPath).filePath(QStringLiteral("opencollection.yaml")));
    if (hasYml) {
        return OpenCollectionWriter::writeFolderFile(folder->path(), folder->name(), folder->seq(), folder->auth(), folder->variables(), folder->headers());
    }
    if (folder == m_rootItem.get() && QFile::exists(QDir(folder->path()).filePath(QStringLiteral("collection.bru")))) {
        QFile collFile(QDir(folder->path()).filePath(QStringLiteral("collection.bru")));
        if (collFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&collFile);
            out << BruWriter::serializeFolder(folder->name(), folder->variables(), folder->seq());
        }
    }
    return BruWriter::writeFolderFile(folder->path(), folder->name(), folder->variables(), folder->seq());
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
        if (QFile::exists(QDir(item->path()).filePath(QStringLiteral("folder.yml")))) {
            OpenCollectionWriter::writeFolderFile(item->path(), newName, item->seq(), item->auth(), item->variables(), item->headers());
        } else {
            BruWriter::writeFolderFile(item->path(), newName, item->variables(), item->seq());
        }
    } else {
        QString ext = fi.suffix().isEmpty() ? QStringLiteral(".bru") : ("." + fi.suffix());
        QString newPath = BruWriter::uniqueFilePath(parentDir, BruWriter::safeFileStem(newName), ext, oldPath);
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

bool CollectionModel::moveItem(CollectionItem* item, CollectionItem* newParent, int insertIndex) {
    if (!item || item == m_rootItem.get()) return false;
    if (!newParent) newParent = m_rootItem.get();
    if (!newParent || newParent->type() == CollectionItemType::Request) return false;

    for (auto* p = newParent; p; p = p->parent()) {
        if (p == item) return false;
    }

    CollectionItem* oldParent = item->parent();
    const bool sameParent = (oldParent == newParent);

    auto clampIndex = [&](int idx, int count) {
        if (idx < 0 || idx > count) return count;
        return idx;
    };

    if (sameParent) {
        const int oldIndex = oldParent->children().indexOf(item);
        if (oldIndex < 0) return false;
        oldParent->removeChild(item);
        const int dest = clampIndex(insertIndex, oldParent->children().size());
        oldParent->insertChild(dest, item);
        persistSiblingOrder(oldParent);
        emit itemModified(item);
        return true;
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

    if (oldParent) {
        oldParent->removeChild(item);
    }
    const int dest = clampIndex(insertIndex, newParent->children().size());
    newParent->insertChild(dest, item);

    const QString canonicalDest = QFileInfo(destPath).canonicalFilePath();
    item->setPath(canonicalDest.isEmpty() ? destPath : canonicalDest);
    if (item->type() == CollectionItemType::Folder) {
        rewriteDescendantPaths(item, oldPath, item->path());
        if (m_fileWatcher.directories().contains(oldPath)) {
            m_fileWatcher.removePath(oldPath);
        }
        m_fileWatcher.addPath(item->path());
    }

    if (oldParent) persistSiblingOrder(oldParent);
    persistSiblingOrder(newParent);
    emit itemModified(item);
    return true;
}

void CollectionModel::persistSiblingOrder(CollectionItem* parent) {
    if (!parent) return;
    suppressDiskWatcher();
    const auto kids = parent->children();
    for (int i = 0; i < kids.size(); ++i) {
        auto* child = kids[i];
        child->setSeq(i + 1);
        if (child->type() == CollectionItemType::Request) {
            saveRequest(child);
        } else {
            saveFolderVariables(child);
        }
    }
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
