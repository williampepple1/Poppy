#pragma once

#include <QDialog>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <core/RequestModel.h>

namespace poppy::gui {

class ImportDialog : public QDialog {
    Q_OBJECT
public:
    explicit ImportDialog(const QString& defaultOutputDir, QWidget* parent = nullptr);

signals:
    void curlImported(const core::RequestModel& req);
    void collectionImported(const QString& targetDir);

private slots:
    void onImportCurl();
    void onImportPostman();
    void onImportOpenApi();

private:
    QString m_defaultOutputDir;
    QTabWidget* m_tabWidget;

    // cURL tab
    QPlainTextEdit* m_curlEdit;
    QPushButton* m_importCurlBtn;

    // Postman tab
    QLineEdit* m_postmanFileEdit;
    QLineEdit* m_postmanDestEdit;
    QPushButton* m_importPostmanBtn;

    // OpenAPI tab
    QLineEdit* m_openApiFileEdit;
    QLineEdit* m_openApiDestEdit;
    QPushButton* m_importOpenApiBtn;
};

} // namespace poppy::gui
