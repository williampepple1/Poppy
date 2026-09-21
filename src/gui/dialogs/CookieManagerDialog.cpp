#include "CookieManagerDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QCheckBox>
#include <QDateTimeEdit>
#include <QDateTime>

namespace poppy::gui {

CookieManagerDialog::CookieManagerDialog(const QString& cookieFilePath, QWidget* parent)
    : QDialog(parent), m_filePath(cookieFilePath) {
    setWindowTitle("Cookie Manager");
    resize(850, 480);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // 1. Top filter & actions
    auto* topLayout = new QHBoxLayout();
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText("Filter cookies by domain or name...");
    connect(m_filterEdit, &QLineEdit::textChanged, this, &CookieManagerDialog::onFilterChanged);
    topLayout->addWidget(m_filterEdit, 1);

    m_refreshBtn = new QPushButton("Refresh", this);
    connect(m_refreshBtn, &QPushButton::clicked, this, &CookieManagerDialog::onRefresh);
    topLayout->addWidget(m_refreshBtn);

    mainLayout->addLayout(topLayout);

    // 2. Cookie Table
    m_table = new QTableWidget(this);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({"Domain", "Path", "Name", "Value", "Expires", "Secure", "HttpOnly"});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    mainLayout->addWidget(m_table, 1);

    // 3. Status label
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: #71717a; font-size: 11px;");
    mainLayout->addWidget(m_statusLabel);

    // 4. Bottom action buttons
    auto* btnLayout = new QHBoxLayout();

    m_addBtn = new QPushButton("+ Add Cookie", this);
    connect(m_addBtn, &QPushButton::clicked, this, &CookieManagerDialog::onAddCookie);
    btnLayout->addWidget(m_addBtn);

    m_deleteBtn = new QPushButton("Delete Selected", this);
    connect(m_deleteBtn, &QPushButton::clicked, this, &CookieManagerDialog::onDeleteCookie);
    btnLayout->addWidget(m_deleteBtn);

    m_clearBtn = new QPushButton("Clear All", this);
    connect(m_clearBtn, &QPushButton::clicked, this, &CookieManagerDialog::onClearAll);
    btnLayout->addWidget(m_clearBtn);

    btnLayout->addStretch();

    m_saveBtn = new QPushButton("Save & Close", this);
    m_saveBtn->setObjectName("primaryBtn");
    connect(m_saveBtn, &QPushButton::clicked, this, &CookieManagerDialog::onSaveAndClose);
    btnLayout->addWidget(m_saveBtn);

    auto* cancelBtn = new QPushButton("Cancel", this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    mainLayout->addLayout(btnLayout);

    onRefresh();
}

void CookieManagerDialog::onRefresh() {
    m_jar.loadFromFile(m_filePath);
    populateTable(m_filterEdit->text());
}

void CookieManagerDialog::onFilterChanged(const QString& text) {
    populateTable(text);
}

void CookieManagerDialog::populateTable(const QString& filter) {
    m_table->setRowCount(0);
    QString cleanFilter = filter.trimmed().toLower();

    int row = 0;
    for (const auto& c : m_jar.cookies()) {
        if (!cleanFilter.isEmpty()) {
            if (!c.domain.toLower().contains(cleanFilter) && !c.name.toLower().contains(cleanFilter)) {
                continue;
            }
        }

        m_table->insertRow(row);
        m_table->setItem(row, 0, new QTableWidgetItem(c.domain));
        m_table->setItem(row, 1, new QTableWidgetItem(c.path));
        m_table->setItem(row, 2, new QTableWidgetItem(c.name));
        m_table->setItem(row, 3, new QTableWidgetItem(c.value));

        QString expStr = (c.expires == 0) ? "Session" : QDateTime::fromSecsSinceEpoch(c.expires).toString("yyyy-MM-dd HH:mm");
        m_table->setItem(row, 4, new QTableWidgetItem(expStr));
        m_table->setItem(row, 5, new QTableWidgetItem(c.secure ? "Yes" : "No"));
        m_table->setItem(row, 6, new QTableWidgetItem(c.httpOnly ? "Yes" : "No"));

        // Store domain, path, name in UserRole for identification
        m_table->item(row, 0)->setData(Qt::UserRole, c.domain);
        m_table->item(row, 0)->setData(Qt::UserRole + 1, c.path);
        m_table->item(row, 0)->setData(Qt::UserRole + 2, c.name);

        row++;
    }

    m_statusLabel->setText(QString("Showing %1 of %2 cookies | File: %3").arg(row).arg(m_jar.count()).arg(m_filePath));
}

void CookieManagerDialog::onAddCookie() {
    QDialog addDlg(this);
    addDlg.setWindowTitle("Add Cookie");
    addDlg.resize(400, 300);

    auto* formLayout = new QFormLayout(&addDlg);

    auto* domainEdit = new QLineEdit(&addDlg);
    domainEdit->setPlaceholderText("e.g. .example.com or localhost");
    formLayout->addRow("Domain:", domainEdit);

    auto* pathEdit = new QLineEdit("/", &addDlg);
    formLayout->addRow("Path:", pathEdit);

    auto* nameEdit = new QLineEdit(&addDlg);
    nameEdit->setPlaceholderText("cookie_name");
    formLayout->addRow("Name:", nameEdit);

    auto* valEdit = new QLineEdit(&addDlg);
    valEdit->setPlaceholderText("cookie_value");
    formLayout->addRow("Value:", valEdit);

    auto* subdomainsCb = new QCheckBox("Include Subdomains", &addDlg);
    subdomainsCb->setChecked(true);
    formLayout->addRow("", subdomainsCb);

    auto* secureCb = new QCheckBox("Secure (HTTPS only)", &addDlg);
    formLayout->addRow("", secureCb);

    auto* httpOnlyCb = new QCheckBox("HttpOnly", &addDlg);
    formLayout->addRow("", httpOnlyCb);

    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &addDlg);
    connect(btnBox, &QDialogButtonBox::accepted, &addDlg, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &addDlg, &QDialog::reject);
    formLayout->addRow(btnBox);

    if (addDlg.exec() == QDialog::Accepted) {
        if (domainEdit->text().trimmed().isEmpty() || nameEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "Validation Error", "Domain and Name are required.");
            return;
        }

        core::Cookie c;
        c.domain = domainEdit->text().trimmed();
        c.path = pathEdit->text().trimmed().isEmpty() ? "/" : pathEdit->text().trimmed();
        c.name = nameEdit->text().trimmed();
        c.value = valEdit->text();
        c.includeSubdomains = subdomainsCb->isChecked();
        c.secure = secureCb->isChecked();
        c.httpOnly = httpOnlyCb->isChecked();
        c.expires = 0; // Session

        m_jar.addOrUpdateCookie(c);
        populateTable(m_filterEdit->text());
    }
}

void CookieManagerDialog::onDeleteCookie() {
    int row = m_table->currentRow();
    if (row < 0) return;

    QString domain = m_table->item(row, 0)->data(Qt::UserRole).toString();
    QString path = m_table->item(row, 0)->data(Qt::UserRole + 1).toString();
    QString name = m_table->item(row, 0)->data(Qt::UserRole + 2).toString();

    m_jar.removeCookie(domain, path, name);
    populateTable(m_filterEdit->text());
}

void CookieManagerDialog::onClearAll() {
    if (m_jar.count() == 0) return;

    auto res = QMessageBox::question(this, "Clear All Cookies",
        "Are you sure you want to remove all cookies from the cookie jar?",
        QMessageBox::Yes | QMessageBox::No);
    if (res == QMessageBox::Yes) {
        m_jar.clear();
        populateTable();
    }
}

void CookieManagerDialog::onSaveAndClose() {
    if (!m_jar.saveToFile(m_filePath)) {
        QMessageBox::warning(this, "Save Failed", QString("Could not save cookie jar to:\n%1").arg(m_filePath));
        return;
    }
    accept();
}

} // namespace poppy::gui
