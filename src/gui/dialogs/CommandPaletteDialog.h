#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QLabel>
#include <QString>
#include <QList>
#include <functional>
#include <core/CollectionModel.h>
#include <core/RequestModel.h>

namespace poppy::gui {

enum class PaletteItemType {
    Action,
    Environment,
    Request
};

struct PaletteEntry {
    PaletteItemType type{PaletteItemType::Action};
    QString title;
    QString subtitle;
    QString shortcut;
    QString category; // "Commands", "Environments", "Requests"
    core::HttpMethod method{core::HttpMethod::GET};
    QString envName;
    core::CollectionItem* requestItem{nullptr};
    std::function<void()> action;
};

class CommandPaletteDialog : public QDialog {
    Q_OBJECT
public:
    explicit CommandPaletteDialog(QWidget* parent = nullptr);

    void setActions(const QList<PaletteEntry>& actions);
    void setEnvironments(const QStringList& envNames, const QString& currentEnv);
    void setCollection(core::CollectionModel* model);

    PaletteEntry selectedEntry() const { return m_chosenEntry; }

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onSearchTextChanged(const QString& text);
    void onItemActivated(QListWidgetItem* item);

private:
    void rebuildList(const QString& query);
    void collectRequests(core::CollectionItem* item, const QString& parentPath);
    void applyThemeStyles();

    QLineEdit* m_searchEdit;
    QListWidget* m_listWidget;
    QLabel* m_statusLabel;

    QList<PaletteEntry> m_allEntries;
    QList<PaletteEntry> m_filteredEntries;
    PaletteEntry m_chosenEntry;
};

} // namespace poppy::gui
