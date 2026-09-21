#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QLabel>
#include <core/CollectionModel.h>

namespace poppy::gui {

struct QuickOpenEntry {
    core::CollectionItem* item{nullptr};
    QString name;
    core::HttpMethod method{core::HttpMethod::GET};
    QString url;
    QString relativePath;
};

class QuickOpenDialog : public QDialog {
    Q_OBJECT
public:
    explicit QuickOpenDialog(core::CollectionModel* model, QWidget* parent = nullptr);

    core::CollectionItem* selectedItem() const { return m_selectedItem; }

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onFilterChanged(const QString& text);
    void onItemActivated(QListWidgetItem* item);

private:
    void collectEntries(core::CollectionItem* item, const QString& parentPath);
    void refreshList(const QString& filter);

    core::CollectionModel* m_model;
    QLineEdit* m_searchEdit;
    QListWidget* m_listWidget;
    QLabel* m_statusLabel;

    QList<QuickOpenEntry> m_entries;
    core::CollectionItem* m_selectedItem{nullptr};
};

} // namespace poppy::gui
