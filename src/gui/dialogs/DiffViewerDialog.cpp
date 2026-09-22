#include "DiffViewerDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QMessageBox>

namespace poppy::gui {

DiffViewerDialog::DiffViewerDialog(const QString& leftContent,
                                   const QString& rightContent,
                                   core::HistoryManager* historyManager,
                                   QWidget* parent)
    : QDialog(parent), m_historyManager(historyManager) {
    setWindowTitle("Response Diff Viewer - Compare Responses");
    resize(1100, 680);
    setupUi();

    setLeftText(leftContent);
    setRightText(rightContent);
    computeDiff();
}

void DiffViewerDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    // Top control bar
    auto* topBar = new QHBoxLayout();
    topBar->setSpacing(8);

    m_statsLabel = new QLabel("Ready to compare responses", this);
    m_statsLabel->setStyleSheet("font-weight: bold; color: #a1a1aa; font-size: 13px;");
    topBar->addWidget(m_statsLabel);
    topBar->addStretch();

    m_swapBtn = new QPushButton("⇄ Swap Sides", this);
    connect(m_swapBtn, &QPushButton::clicked, this, &DiffViewerDialog::onSwapPanes);
    topBar->addWidget(m_swapBtn);

    auto* recomputeBtn = new QPushButton("Refresh Diff", this);
    recomputeBtn->setStyleSheet("background-color: #3b82f6; color: #ffffff; font-weight: bold;");
    connect(recomputeBtn, &QPushButton::clicked, this, &DiffViewerDialog::computeDiff);
    topBar->addWidget(recomputeBtn);

    mainLayout->addLayout(topBar);

    // Splitter for side-by-side editors
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // Left pane container
    auto* leftWidget = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(4);

    auto* leftHeader = new QHBoxLayout();
    leftHeader->addWidget(new QLabel("<b>Response A (Base)</b>", leftWidget));
    leftHeader->addStretch();
    m_loadLeftBtn = new QPushButton("Load File...", leftWidget);
    connect(m_loadLeftBtn, &QPushButton::clicked, this, &DiffViewerDialog::onLoadLeftFile);
    leftHeader->addWidget(m_loadLeftBtn);

    if (m_historyManager && m_historyManager->count() > 0) {
        m_leftSourceCombo = new QComboBox(leftWidget);
        m_leftSourceCombo->addItem("Select History Item...");
        for (const auto& item : m_historyManager->items()) {
            m_leftSourceCombo->addItem(QString("%1 %2 (%3ms)").arg(core::methodToString(item.request.method), item.request.url).arg(item.responseTimeMs));
        }
        connect(m_leftSourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DiffViewerDialog::onLeftHistorySelected);
        leftHeader->addWidget(m_leftSourceCombo);
    }
    leftLayout->addLayout(leftHeader);

    m_leftEditor = new QPlainTextEdit(leftWidget);
    QFont font("Consolas", 10);
    if (!font.exactMatch()) font = QFont("Courier New", 10);
    m_leftEditor->setFont(font);
    m_leftEditor->setPlaceholderText("Paste or load base response body here...");
    leftLayout->addWidget(m_leftEditor);
    splitter->addWidget(leftWidget);

    // Right pane container
    auto* rightWidget = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(4);

    auto* rightHeader = new QHBoxLayout();
    rightHeader->addWidget(new QLabel("<b>Response B (Compared)</b>", rightWidget));
    rightHeader->addStretch();
    m_loadRightBtn = new QPushButton("Load File...", rightWidget);
    connect(m_loadRightBtn, &QPushButton::clicked, this, &DiffViewerDialog::onLoadRightFile);
    rightHeader->addWidget(m_loadRightBtn);

    if (m_historyManager && m_historyManager->count() > 0) {
        m_rightSourceCombo = new QComboBox(rightWidget);
        m_rightSourceCombo->addItem("Select History Item...");
        for (const auto& item : m_historyManager->items()) {
            m_rightSourceCombo->addItem(QString("%1 %2 (%3ms)").arg(core::methodToString(item.request.method), item.request.url).arg(item.responseTimeMs));
        }
        connect(m_rightSourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DiffViewerDialog::onRightHistorySelected);
        rightHeader->addWidget(m_rightSourceCombo);
    }
    rightLayout->addLayout(rightHeader);

    m_rightEditor = new QPlainTextEdit(rightWidget);
    m_rightEditor->setFont(font);
    m_rightEditor->setPlaceholderText("Paste or load compared response body here...");
    rightLayout->addWidget(m_rightEditor);
    splitter->addWidget(rightWidget);

    splitter->setSizes({500, 500});
    mainLayout->addWidget(splitter, 1);

    // Synchronize scrolling
    connect(m_leftEditor->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int val) {
        if (!m_syncingScroll && m_rightEditor) {
            m_syncingScroll = true;
            m_rightEditor->verticalScrollBar()->setValue(val);
            m_syncingScroll = false;
        }
    });

    connect(m_rightEditor->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int val) {
        if (!m_syncingScroll && m_leftEditor) {
            m_syncingScroll = true;
            m_leftEditor->verticalScrollBar()->setValue(val);
            m_syncingScroll = false;
        }
    });

    connect(m_leftEditor, &QPlainTextEdit::textChanged, this, &DiffViewerDialog::computeDiff);
    connect(m_rightEditor, &QPlainTextEdit::textChanged, this, &DiffViewerDialog::computeDiff);

    // Bottom close button
    auto* bottomBar = new QHBoxLayout();
    bottomBar->addStretch();
    auto* closeBtn = new QPushButton("Close", this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    bottomBar->addWidget(closeBtn);
    mainLayout->addLayout(bottomBar);
}

void DiffViewerDialog::setLeftText(const QString& text) {
    if (m_leftEditor) {
        m_leftEditor->setPlainText(text);
    }
}

void DiffViewerDialog::setRightText(const QString& text) {
    if (m_rightEditor) {
        m_rightEditor->setPlainText(text);
    }
}

void DiffViewerDialog::onSwapPanes() {
    QString left = m_leftEditor->toPlainText();
    QString right = m_rightEditor->toPlainText();
    m_leftEditor->blockSignals(true);
    m_rightEditor->blockSignals(true);
    m_leftEditor->setPlainText(right);
    m_rightEditor->setPlainText(left);
    m_leftEditor->blockSignals(false);
    m_rightEditor->blockSignals(false);
    computeDiff();
}

void DiffViewerDialog::onLoadLeftFile() {
    QString path = QFileDialog::getOpenFileName(this, "Open File for Response A", QString(), "All Files (*.*);;JSON (*.json);;Text (*.txt)");
    if (!path.isEmpty()) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            setLeftText(QString::fromUtf8(file.readAll()));
        }
    }
}

void DiffViewerDialog::onLoadRightFile() {
    QString path = QFileDialog::getOpenFileName(this, "Open File for Response B", QString(), "All Files (*.*);;JSON (*.json);;Text (*.txt)");
    if (!path.isEmpty()) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            setRightText(QString::fromUtf8(file.readAll()));
        }
    }
}

void DiffViewerDialog::onLeftHistorySelected(int index) {
    if (!m_historyManager || index <= 0) return;
    int hIdx = index - 1;
    if (hIdx >= 0 && hIdx < m_historyManager->count()) {
        setLeftText(m_historyManager->items()[hIdx].toResponseModel().bodyAsString());
    }
}

void DiffViewerDialog::onRightHistorySelected(int index) {
    if (!m_historyManager || index <= 0) return;
    int hIdx = index - 1;
    if (hIdx >= 0 && hIdx < m_historyManager->count()) {
        setRightText(m_historyManager->items()[hIdx].toResponseModel().bodyAsString());
    }
}

void DiffViewerDialog::computeDiff() {
    QString left = m_leftEditor->toPlainText();
    QString right = m_rightEditor->toPlainText();

    QStringList leftLines = left.split('\n');
    QStringList rightLines = right.split('\n');

    int maxLines = std::max(leftLines.size(), rightLines.size());
    int additions = 0;
    int deletions = 0;
    int modifications = 0;
    int identical = 0;

    QList<QTextEdit::ExtraSelection> leftSelections;
    QList<QTextEdit::ExtraSelection> rightSelections;

    QColor redBg(120, 20, 20, 100);    // Deletion
    QColor greenBg(20, 120, 20, 100);  // Addition
    QColor yellowBg(120, 100, 20, 80); // Modification

    int commonSize = std::min(leftLines.size(), rightLines.size());
    for (int i = 0; i < commonSize; ++i) {
        if (leftLines[i] == rightLines[i]) {
            identical++;
        } else {
            modifications++;

            // Highlight left line
            QTextEdit::ExtraSelection selL;
            selL.format.setBackground(yellowBg);
            selL.format.setProperty(QTextFormat::FullWidthSelection, true);
            QTextBlock blockL = m_leftEditor->document()->findBlockByLineNumber(i);
            if (blockL.isValid()) {
                selL.cursor = QTextCursor(blockL);
                leftSelections.append(selL);
            }

            // Highlight right line
            QTextEdit::ExtraSelection selR;
            selR.format.setBackground(yellowBg);
            selR.format.setProperty(QTextFormat::FullWidthSelection, true);
            QTextBlock blockR = m_rightEditor->document()->findBlockByLineNumber(i);
            if (blockR.isValid()) {
                selR.cursor = QTextCursor(blockR);
                rightSelections.append(selR);
            }
        }
    }

    // Extra lines in left -> deletions
    if (leftLines.size() > rightLines.size()) {
        for (int i = rightLines.size(); i < leftLines.size(); ++i) {
            deletions++;
            QTextEdit::ExtraSelection sel;
            sel.format.setBackground(redBg);
            sel.format.setProperty(QTextFormat::FullWidthSelection, true);
            QTextBlock block = m_leftEditor->document()->findBlockByLineNumber(i);
            if (block.isValid()) {
                sel.cursor = QTextCursor(block);
                leftSelections.append(sel);
            }
        }
    }

    // Extra lines in right -> additions
    if (rightLines.size() > leftLines.size()) {
        for (int i = leftLines.size(); i < rightLines.size(); ++i) {
            additions++;
            QTextEdit::ExtraSelection sel;
            sel.format.setBackground(greenBg);
            sel.format.setProperty(QTextFormat::FullWidthSelection, true);
            QTextBlock block = m_rightEditor->document()->findBlockByLineNumber(i);
            if (block.isValid()) {
                sel.cursor = QTextCursor(block);
                rightSelections.append(sel);
            }
        }
    }

    m_leftEditor->setExtraSelections(leftSelections);
    m_rightEditor->setExtraSelections(rightSelections);

    if (left.isEmpty() && right.isEmpty()) {
        m_statsLabel->setText("Paste or select two responses to see the difference.");
    } else if (left == right) {
        m_statsLabel->setText(QString("✔ Responses are identical (%1 lines)").arg(leftLines.size()));
        m_statsLabel->setStyleSheet("font-weight: bold; color: #10b981; font-size: 13px;");
    } else {
        m_statsLabel->setText(QString("Differences found: +%1 additions, -%2 deletions, ~%3 modified lines | A: %4 lines, B: %5 lines")
            .arg(additions).arg(deletions).arg(modifications).arg(leftLines.size()).arg(rightLines.size()));
        m_statsLabel->setStyleSheet("font-weight: bold; color: #f59e0b; font-size: 13px;");
    }
}

} // namespace poppy::gui
