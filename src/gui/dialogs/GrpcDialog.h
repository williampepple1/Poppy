#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QSplitter>
#include <QCloseEvent>
#include <network/GrpcClient.h>
#include <components/KeyValueTable.h>

namespace poppy::gui {

class GrpcDialog : public QDialog {
    Q_OBJECT
public:
    explicit GrpcDialog(QWidget* parent = nullptr);
    ~GrpcDialog() override = default;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onLoadProtoClicked();
    void onServiceChanged(int index);
    void onMethodChanged(int index);
    void onGenerateExampleClicked();
    void onInvokeClicked();
    void onCallStarted();
    void onCallFinished(const poppy::network::GrpcResponse& response);

private:
    void setupUi();
    void updateMethodCombo();
    void updateStatusBadge(int code, const QString& name);

    network::GrpcClient* m_client{nullptr};
    uint64_t m_activeGeneration{0};
    bool m_callActive{false};
    bool m_closeWhenFinished{false};
    network::GrpcProtoDefinition m_protoDef;

    QLineEdit* m_endpointEdit{nullptr};
    QCheckBox* m_tlsCheck{nullptr};
    QPushButton* m_invokeBtn{nullptr};

    QPushButton* m_loadProtoBtn{nullptr};
    QComboBox* m_serviceCombo{nullptr};
    QComboBox* m_methodCombo{nullptr};
    QPushButton* m_examplePayloadBtn{nullptr};

    // Left Tabs
    QPlainTextEdit* m_requestPayloadEdit{nullptr};
    KeyValueTable* m_metadataTable{nullptr};
    QPlainTextEdit* m_protoViewer{nullptr};

    // Right Response Pane
    QLabel* m_statusBadge{nullptr};
    QLabel* m_latencyBadge{nullptr};
    QPlainTextEdit* m_responseViewer{nullptr};
    QTableWidget* m_responseHeadersTable{nullptr};
};

} // namespace poppy::gui
