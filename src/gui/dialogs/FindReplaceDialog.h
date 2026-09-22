#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QTreeWidget>
#include <QLabel>
#include <core/CollectionModel.h>

namespace poppy::gui {

struct FindMatch {
    core::CollectionItem* item{nullptr};
    QString field; // "Name", "URL", "Header", "Param", "Body", "Script"
    int index{-1}; // Index within list if header/param, or -1
    QString subKey; // "name" or "value" or "key"
    QString originalSnippet;
};

class FindReplaceDialog : public QDialog {
    Q_OBJECT
public:
    explicit FindReplaceDialog(core::CollectionModel* model, QWidget* parent = nullptr);
    ~FindReplaceDialog() override = default;

    void setInitialFindText(const QString& text);

signals:
    void requestSelected(core::CollectionItem* item);
    void collectionModified();

private slots:
    void onFindAll();
    void onReplaceAll();
    void onReplaceSelected();
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);

private:
    void setupUi();
    bool matches(const QString& text, const QString& search) const;
    QString performReplace(const QString& text, const QString& search, const QString& replacement) const;

    core::CollectionModel* m_model{nullptr};

    QLineEdit* m_findEdit{nullptr};
    QLineEdit* m_replaceEdit{nullptr};
    QPushButton* m_findBtn{nullptr};
    QPushButton* m_replaceAllBtn{nullptr};
    QPushButton* m_replaceSelectedBtn{nullptr};

    QCheckBox* m_matchCaseCheck{nullptr};
    QCheckBox* m_wholeWordCheck{nullptr};

    QCheckBox* m_checkUrl{nullptr};
    QCheckBox* m_checkName{nullptr};
    QCheckBox* m_checkHeaders{nullptr};
    QCheckBox* m_checkParams{nullptr};
    QCheckBox* m_checkBody{nullptr};
    QCheckBox* m_checkScripts{nullptr};

    QTreeWidget* m_resultsTree{nullptr};
    QLabel* m_statusLabel{nullptr};

    QList<FindMatch> m_currentMatches;
};

} // namespace poppy::gui
