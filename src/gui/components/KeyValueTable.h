#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <core/RequestModel.h>

namespace poppy::gui {

class KeyValueTable : public QWidget {
    Q_OBJECT
public:
    explicit KeyValueTable(bool showDescription = true, QWidget* parent = nullptr);

    void setHeaders(const QList<core::HttpHeader>& headers);
    QList<core::HttpHeader> headers() const;

    void setParams(const QList<core::HttpParam>& params);
    QList<core::HttpParam> params() const;

    void setFormData(const QList<core::FormDataParam>& params);
    QList<core::FormDataParam> formData() const;
    void setAllowFiles(bool allow);

    void setKeyCompleterWords(const QStringList& words);

signals:
    void dataChanged();

private slots:
    void onCellChanged(int row, int column);
    void addRow(bool enabled = true, const QString& key = {}, const QString& value = {}, const QString& desc = {});
    void removeCurrentRow();

private:
    void ensureTrailingEmptyRow();

    QTableWidget* m_table;
    QPushButton* m_addRowBtn;
    QPushButton* m_addFileBtn{nullptr};
    QPushButton* m_removeRowBtn;
    bool m_showDescription;
    bool m_allowFiles{false};
    QStringList m_completerWords;
    bool m_updating{false};
};

} // namespace poppy::gui
