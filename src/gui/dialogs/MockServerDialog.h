#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QTableWidget>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QSplitter>
#include <network/MockServer.h>
#include <core/CollectionModel.h>

namespace poppy::gui {

class MockServerDialog : public QDialog {
    Q_OBJECT
public:
    explicit MockServerDialog(core::CollectionModel* collectionModel = nullptr, QWidget* parent = nullptr);
    ~MockServerDialog() override;

private slots:
    void onToggleServer();
    void onImportCollection();
    void onAddRoute();
    void onRemoveRoute();
    void onRouteSelected();
    void onRouteDataChanged();
    void onRequestReceived(const poppy::network::MockRequestLog& log);
    void onLogSelected();
    void onClearLog();

private:
    void setupUi();
    void updateServerStatus();
    void refreshRoutesTable();

    network::MockServer* m_server{nullptr};
    core::CollectionModel* m_collectionModel{nullptr};

    QSpinBox* m_portSpin{nullptr};
    QPushButton* m_toggleServerBtn{nullptr};
    QLabel* m_statusBadge{nullptr};
    QPushButton* m_importCollectionBtn{nullptr};

    // Routes Tab
    QTableWidget* m_routesTable{nullptr};
    QComboBox* m_methodCombo{nullptr};
    QLineEdit* m_pathEdit{nullptr};
    QSpinBox* m_statusCodeSpin{nullptr};
    QSpinBox* m_delaySpin{nullptr};
    QLineEdit* m_contentTypeEdit{nullptr};
    QPlainTextEdit* m_responseBodyEdit{nullptr};
    int m_selectedRouteIndex{-1};
    bool m_updatingForm{false};

    // Logs Tab
    QTableWidget* m_logsTable{nullptr};
    QPlainTextEdit* m_logDetailViewer{nullptr};
};

} // namespace poppy::gui
