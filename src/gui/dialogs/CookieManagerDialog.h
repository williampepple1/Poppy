#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <core/CookieJar.h>

namespace poppy::gui {

class CookieManagerDialog : public QDialog {
    Q_OBJECT
public:
    explicit CookieManagerDialog(const QString& cookieFilePath, QWidget* parent = nullptr);

private slots:
    void onFilterChanged(const QString& text);
    void onAddCookie();
    void onDeleteCookie();
    void onClearAll();
    void onRefresh();
    void onSaveAndClose();

private:
    void populateTable(const QString& filter = QString());

    QString m_filePath;
    core::CookieJar m_jar;

    QLineEdit* m_filterEdit;
    QTableWidget* m_table;
    QPushButton* m_addBtn;
    QPushButton* m_deleteBtn;
    QPushButton* m_clearBtn;
    QPushButton* m_refreshBtn;
    QPushButton* m_saveBtn;
    QLabel* m_statusLabel;
};

} // namespace poppy::gui
