#include "FindReplaceDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QRegularExpression>
#include <QMessageBox>
#include <QSet>

namespace poppy::gui {

FindReplaceDialog::FindReplaceDialog(core::CollectionModel* model, QWidget* parent)
    : QDialog(parent), m_model(model) {
    setWindowTitle("Find & Replace Across Collection");
    resize(720, 520);
    setMinimumSize(600, 420);

    setupUi();
}

void FindReplaceDialog::setInitialFindText(const QString& text) {
    if (m_findEdit && !text.isEmpty()) {
        m_findEdit->setText(text);
        m_findEdit->selectAll();
        onFindAll();
    }
}

void FindReplaceDialog::setupUi() {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(12);

    // Grid: Find / Replace inputs
    auto* inputGrid = new QGridLayout();
    inputGrid->setSpacing(8);

    auto* findLbl = new QLabel("Find:", this);
    findLbl->setStyleSheet("font-weight: bold; font-size: 12px;");
    m_findEdit = new QLineEdit(this);
    m_findEdit->setPlaceholderText("Text to search across collection...");
    connect(m_findEdit, &QLineEdit::returnPressed, this, &FindReplaceDialog::onFindAll);

    m_findBtn = new QPushButton("Find All", this);
    m_findBtn->setObjectName("primaryBtn");
    m_findBtn->setFixedWidth(110);
    connect(m_findBtn, &QPushButton::clicked, this, &FindReplaceDialog::onFindAll);

    inputGrid->addWidget(findLbl, 0, 0);
    inputGrid->addWidget(m_findEdit, 0, 1);
    inputGrid->addWidget(m_findBtn, 0, 2);

    auto* replaceLbl = new QLabel("Replace with:", this);
    replaceLbl->setStyleSheet("font-weight: bold; font-size: 12px;");
    m_replaceEdit = new QLineEdit(this);
    m_replaceEdit->setPlaceholderText("Replacement text...");
    connect(m_replaceEdit, &QLineEdit::returnPressed, this, &FindReplaceDialog::onReplaceAll);

    m_replaceAllBtn = new QPushButton("Replace All", this);
    m_replaceAllBtn->setFixedWidth(110);
    connect(m_replaceAllBtn, &QPushButton::clicked, this, &FindReplaceDialog::onReplaceAll);

    inputGrid->addWidget(replaceLbl, 1, 0);
    inputGrid->addWidget(m_replaceEdit, 1, 1);
    inputGrid->addWidget(m_replaceAllBtn, 1, 2);

    rootLayout->addLayout(inputGrid);

    // Options Row
    auto* optionsLayout = new QHBoxLayout();
    m_matchCaseCheck = new QCheckBox("Match Case", this);
    m_wholeWordCheck = new QCheckBox("Whole Word", this);

    optionsLayout->addWidget(m_matchCaseCheck);
    optionsLayout->addWidget(m_wholeWordCheck);
    optionsLayout->addSpacing(16);

    auto* scopeGroup = new QGroupBox("Search Scope", this);
    auto* scopeLayout = new QHBoxLayout(scopeGroup);
    scopeLayout->setContentsMargins(8, 4, 8, 4);
    m_checkUrl = new QCheckBox("URLs", this);
    m_checkUrl->setChecked(true);
    m_checkName = new QCheckBox("Names", this);
    m_checkName->setChecked(true);
    m_checkHeaders = new QCheckBox("Headers", this);
    m_checkHeaders->setChecked(true);
    m_checkParams = new QCheckBox("Params", this);
    m_checkParams->setChecked(true);
    m_checkBody = new QCheckBox("Bodies", this);
    m_checkBody->setChecked(true);
    m_checkScripts = new QCheckBox("Scripts", this);
    m_checkScripts->setChecked(true);

    scopeLayout->addWidget(m_checkUrl);
    scopeLayout->addWidget(m_checkName);
    scopeLayout->addWidget(m_checkHeaders);
    scopeLayout->addWidget(m_checkParams);
    scopeLayout->addWidget(m_checkBody);
    scopeLayout->addWidget(m_checkScripts);

    optionsLayout->addWidget(scopeGroup, 1);
    rootLayout->addLayout(optionsLayout);

    // Results Tree
    m_resultsTree = new QTreeWidget(this);
    m_resultsTree->setHeaderLabels({"Request", "Field", "Snippet / Occurrence"});
    m_resultsTree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_resultsTree->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_resultsTree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_resultsTree->setColumnWidth(0, 200);
    m_resultsTree->setColumnWidth(1, 110);
    m_resultsTree->setAlternatingRowColors(true);
    m_resultsTree->setRootIsDecorated(false);
    connect(m_resultsTree, &QTreeWidget::itemDoubleClicked, this, &FindReplaceDialog::onItemDoubleClicked);

    rootLayout->addWidget(m_resultsTree, 1);

    // Bottom Bar
    auto* bottomLayout = new QHBoxLayout();
    m_statusLabel = new QLabel("Enter search term and click 'Find All'", this);
    m_statusLabel->setStyleSheet("color: #a1a1aa; font-size: 11px;");
    bottomLayout->addWidget(m_statusLabel, 1);

    m_replaceSelectedBtn = new QPushButton("Replace Selected", this);
    m_replaceSelectedBtn->setEnabled(false);
    connect(m_replaceSelectedBtn, &QPushButton::clicked, this, &FindReplaceDialog::onReplaceSelected);
    bottomLayout->addWidget(m_replaceSelectedBtn);

    connect(m_resultsTree, &QTreeWidget::itemSelectionChanged, this, [this]() {
        m_replaceSelectedBtn->setEnabled(!m_resultsTree->selectedItems().isEmpty());
    });

    auto* closeBtn = new QPushButton("Close", this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    bottomLayout->addWidget(closeBtn);

    rootLayout->addLayout(bottomLayout);
}

bool FindReplaceDialog::matches(const QString& text, const QString& search) const {
    if (text.isEmpty() || search.isEmpty()) return false;
    if (m_wholeWordCheck->isChecked()) {
        QRegularExpression::PatternOptions opts = m_matchCaseCheck->isChecked()
            ? QRegularExpression::NoPatternOption
            : QRegularExpression::CaseInsensitiveOption;
        QRegularExpression re(QString("\\b%1\\b").arg(QRegularExpression::escape(search)), opts);
        return text.contains(re);
    }
    return text.contains(search, m_matchCaseCheck->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive);
}

QString FindReplaceDialog::performReplace(const QString& text, const QString& search, const QString& replacement) const {
    if (text.isEmpty() || search.isEmpty()) return text;
    if (m_wholeWordCheck->isChecked()) {
        QRegularExpression::PatternOptions opts = m_matchCaseCheck->isChecked()
            ? QRegularExpression::NoPatternOption
            : QRegularExpression::CaseInsensitiveOption;
        QRegularExpression re(QString("\\b%1\\b").arg(QRegularExpression::escape(search)), opts);
        QString res = text;
        res.replace(re, replacement);
        return res;
    }
    QString res = text;
    res.replace(search, replacement, m_matchCaseCheck->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive);
    return res;
}

core::CollectionItem* FindReplaceDialog::itemFor(const FindMatch& match) const {
    if (!m_model || match.itemPath.isEmpty()) return nullptr;
    return m_model->findItemByPath(match.itemPath);
}

void FindReplaceDialog::onFindAll() {
    QString search = m_findEdit->text();
    m_resultsTree->clear();
    m_currentMatches.clear();

    if (search.isEmpty() || !m_model) {
        m_statusLabel->setText("Search query is empty.");
        return;
    }

    auto items = m_model->allRequestItems();
    QSet<core::CollectionItem*> matchedItems;

    for (auto* item : items) {
        if (!item || !item->request()) continue;
        auto* req = item->request();

        // 1. Name
        if (m_checkName->isChecked() && matches(req->name, search)) {
            m_currentMatches.append(FindMatch{item->path(), "Name", -1, "", req->name});
            matchedItems.insert(item);
        }

        // 2. URL
        if (m_checkUrl->isChecked() && matches(req->url, search)) {
            m_currentMatches.append(FindMatch{item->path(), "URL", -1, "", req->url});
            matchedItems.insert(item);
        }

        // 3. Params
        if (m_checkParams->isChecked()) {
            for (int i = 0; i < req->queryParams.size(); ++i) {
                if (matches(req->queryParams[i].key, search)) {
                    m_currentMatches.append(FindMatch{item->path(), "Param Key", i, "key", req->queryParams[i].key});
                    matchedItems.insert(item);
                }
                if (matches(req->queryParams[i].value, search)) {
                    m_currentMatches.append(FindMatch{item->path(), "Param Value", i, "value", req->queryParams[i].value});
                    matchedItems.insert(item);
                }
            }
        }

        // 4. Headers
        if (m_checkHeaders->isChecked()) {
            for (int i = 0; i < req->headers.size(); ++i) {
                if (matches(req->headers[i].name, search)) {
                    m_currentMatches.append(FindMatch{item->path(), "Header Name", i, "name", req->headers[i].name});
                    matchedItems.insert(item);
                }
                if (matches(req->headers[i].value, search)) {
                    m_currentMatches.append(FindMatch{item->path(), "Header Value", i, "value", req->headers[i].value});
                    matchedItems.insert(item);
                }
            }
        }

        // 5. Body
        if (m_checkBody->isChecked()) {
            if (matches(req->bodyContent, search)) {
                // Short preview
                QString preview = req->bodyContent.simplified().left(120);
                m_currentMatches.append(FindMatch{item->path(), "Body", -1, "", preview});
                matchedItems.insert(item);
            }
            if (matches(req->graphqlQuery, search)) {
                QString preview = req->graphqlQuery.simplified().left(120);
                m_currentMatches.append(FindMatch{item->path(), "GraphQL Query", -1, "", preview});
                matchedItems.insert(item);
            }
        }

        // 6. Scripts
        if (m_checkScripts->isChecked()) {
            if (matches(req->scripts.preRequestScript, search)) {
                m_currentMatches.append(FindMatch{item->path(), "Pre-request Script", -1, "pre", req->scripts.preRequestScript.simplified().left(120)});
                matchedItems.insert(item);
            }
            if (matches(req->scripts.postResponseScript, search)) {
                m_currentMatches.append(FindMatch{item->path(), "Post-response Script", -1, "post", req->scripts.postResponseScript.simplified().left(120)});
                matchedItems.insert(item);
            }
            if (matches(req->scripts.tests, search)) {
                m_currentMatches.append(FindMatch{item->path(), "Tests Script", -1, "tests", req->scripts.tests.simplified().left(120)});
                matchedItems.insert(item);
            }
        }
    }

    // Populate Tree
    for (int i = 0; i < m_currentMatches.size(); ++i) {
        const auto& m = m_currentMatches[i];
        auto* live = itemFor(m);
        auto* treeItem = new QTreeWidgetItem(m_resultsTree);
        treeItem->setText(0, live ? live->name() : QString());
        treeItem->setText(1, m.field);
        treeItem->setText(2, m.originalSnippet);
        treeItem->setData(0, Qt::UserRole, i);
    }

    m_statusLabel->setText(QString("Found %1 matches across %2 requests.")
                               .arg(m_currentMatches.size())
                               .arg(matchedItems.size()));
}

void FindReplaceDialog::onReplaceAll() {
    QString search = m_findEdit->text();
    QString replacement = m_replaceEdit->text();

    if (search.isEmpty() || !m_model) return;

    if (m_currentMatches.isEmpty()) {
        onFindAll();
        if (m_currentMatches.isEmpty()) {
            QMessageBox::information(this, "No Matches", "No occurrences found to replace.");
            return;
        }
    }

    int count = 0;
    QSet<core::CollectionItem*> modifiedItems;

    for (const auto& m : m_currentMatches) {
        auto* liveItem = itemFor(m);
        if (!liveItem || !liveItem->request()) continue;
        auto* req = liveItem->request();
        bool changed = false;

        if (m.field == "Name") {
            req->name = performReplace(req->name, search, replacement);
            liveItem->setName(req->name);
            changed = true;
        } else if (m.field == "URL") {
            req->url = performReplace(req->url, search, replacement);
            changed = true;
        } else if (m.field == "Param Key" && m.index >= 0 && m.index < req->queryParams.size()) {
            req->queryParams[m.index].key = performReplace(req->queryParams[m.index].key, search, replacement);
            changed = true;
        } else if (m.field == "Param Value" && m.index >= 0 && m.index < req->queryParams.size()) {
            req->queryParams[m.index].value = performReplace(req->queryParams[m.index].value, search, replacement);
            changed = true;
        } else if (m.field == "Header Name" && m.index >= 0 && m.index < req->headers.size()) {
            req->headers[m.index].name = performReplace(req->headers[m.index].name, search, replacement);
            changed = true;
        } else if (m.field == "Header Value" && m.index >= 0 && m.index < req->headers.size()) {
            req->headers[m.index].value = performReplace(req->headers[m.index].value, search, replacement);
            changed = true;
        } else if (m.field == "Body") {
            req->bodyContent = performReplace(req->bodyContent, search, replacement);
            changed = true;
        } else if (m.field == "GraphQL Query") {
            req->graphqlQuery = performReplace(req->graphqlQuery, search, replacement);
            changed = true;
        } else if (m.field == "Pre-request Script") {
            req->scripts.preRequestScript = performReplace(req->scripts.preRequestScript, search, replacement);
            changed = true;
        } else if (m.field == "Post-response Script") {
            req->scripts.postResponseScript = performReplace(req->scripts.postResponseScript, search, replacement);
            changed = true;
        } else if (m.field == "Tests Script") {
            req->scripts.tests = performReplace(req->scripts.tests, search, replacement);
            changed = true;
        }

        if (changed) {
            count++;
            modifiedItems.insert(liveItem);
        }
    }

    for (auto* item : modifiedItems) {
        m_model->saveRequest(item);
    }

    emit collectionModified();
    QMessageBox::information(this, "Replace Complete",
                             QString("Replaced %1 occurrences across %2 requests.")
                                 .arg(count)
                                 .arg(modifiedItems.size()));

    // Refresh search results
    onFindAll();
}

void FindReplaceDialog::onReplaceSelected() {
    auto selectedItems = m_resultsTree->selectedItems();
    if (selectedItems.isEmpty() || !m_model) return;

    QString search = m_findEdit->text();
    QString replacement = m_replaceEdit->text();
    if (search.isEmpty()) return;

    int count = 0;
    QSet<core::CollectionItem*> modifiedItems;

    for (auto* treeItem : selectedItems) {
        int idx = treeItem->data(0, Qt::UserRole).toInt();
        if (idx < 0 || idx >= m_currentMatches.size()) continue;

        const auto& m = m_currentMatches[idx];
        auto* liveItem = itemFor(m);
        if (!liveItem || !liveItem->request()) continue;
        auto* req = liveItem->request();
        bool changed = false;

        if (m.field == "Name") {
            req->name = performReplace(req->name, search, replacement);
            liveItem->setName(req->name);
            changed = true;
        } else if (m.field == "URL") {
            req->url = performReplace(req->url, search, replacement);
            changed = true;
        } else if (m.field == "Param Key" && m.index >= 0 && m.index < req->queryParams.size()) {
            req->queryParams[m.index].key = performReplace(req->queryParams[m.index].key, search, replacement);
            changed = true;
        } else if (m.field == "Param Value" && m.index >= 0 && m.index < req->queryParams.size()) {
            req->queryParams[m.index].value = performReplace(req->queryParams[m.index].value, search, replacement);
            changed = true;
        } else if (m.field == "Header Name" && m.index >= 0 && m.index < req->headers.size()) {
            req->headers[m.index].name = performReplace(req->headers[m.index].name, search, replacement);
            changed = true;
        } else if (m.field == "Header Value" && m.index >= 0 && m.index < req->headers.size()) {
            req->headers[m.index].value = performReplace(req->headers[m.index].value, search, replacement);
            changed = true;
        } else if (m.field == "Body") {
            req->bodyContent = performReplace(req->bodyContent, search, replacement);
            changed = true;
        } else if (m.field == "GraphQL Query") {
            req->graphqlQuery = performReplace(req->graphqlQuery, search, replacement);
            changed = true;
        } else if (m.field == "Pre-request Script") {
            req->scripts.preRequestScript = performReplace(req->scripts.preRequestScript, search, replacement);
            changed = true;
        } else if (m.field == "Post-response Script") {
            req->scripts.postResponseScript = performReplace(req->scripts.postResponseScript, search, replacement);
            changed = true;
        } else if (m.field == "Tests Script") {
            req->scripts.tests = performReplace(req->scripts.tests, search, replacement);
            changed = true;
        }

        if (changed) {
            count++;
            modifiedItems.insert(liveItem);
        }
    }

    for (auto* item : modifiedItems) {
        m_model->saveRequest(item);
    }

    emit collectionModified();
    onFindAll();
}

void FindReplaceDialog::onItemDoubleClicked(QTreeWidgetItem* item, int) {
    if (!item) return;
    int idx = item->data(0, Qt::UserRole).toInt();
    if (idx >= 0 && idx < m_currentMatches.size()) {
        emit requestSelected(itemFor(m_currentMatches[idx]));
    }
}

} // namespace poppy::gui
