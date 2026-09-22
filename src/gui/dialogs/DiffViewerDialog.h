#pragma once

#include <QDialog>
#include <QPlainTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QComboBox>
#include <core/HistoryManager.h>
#include <core/ResponseModel.h>

namespace poppy::gui {

class DiffViewerDialog : public QDialog {
    Q_OBJECT
public:
    explicit DiffViewerDialog(const QString& leftContent = {},
                              const QString& rightContent = {},
                              core::HistoryManager* historyManager = nullptr,
                              QWidget* parent = nullptr);

    void setLeftText(const QString& text);
    void setRightText(const QString& text);

private slots:
    void computeDiff();
    void onLoadLeftFile();
    void onLoadRightFile();
    void onLeftHistorySelected(int index);
    void onRightHistorySelected(int index);
    void onSwapPanes();

private:
    void setupUi();
    void highlightDiff();

    core::HistoryManager* m_historyManager{nullptr};

    QComboBox* m_leftSourceCombo{nullptr};
    QComboBox* m_rightSourceCombo{nullptr};
    QPushButton* m_loadLeftBtn{nullptr};
    QPushButton* m_loadRightBtn{nullptr};
    QPushButton* m_swapBtn{nullptr};
    QLabel* m_statsLabel{nullptr};

    QPlainTextEdit* m_leftEditor{nullptr};
    QPlainTextEdit* m_rightEditor{nullptr};

    bool m_syncingScroll{false};
};

} // namespace poppy::gui
