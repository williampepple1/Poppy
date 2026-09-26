#include "VariableHoverPopup.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QKeyEvent>
#include <QGuiApplication>
#include <QScreen>
#include <Theme.h>

namespace poppy::gui {

VariableHoverPopup::VariableHoverPopup(QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating, false);

    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    connect(m_hideTimer, &QTimer::timeout, this, [this]() {
        if (!isUserInteracting()) {
            hide();
        }
    });

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    // 1. Header row
    auto* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(8);

    m_tokenBadge = new QLabel(this);
    m_tokenBadge->setStyleSheet("font-family: Consolas, monospace; font-weight: 700; font-size: 13px;");
    headerLayout->addWidget(m_tokenBadge);

    m_scopeBadge = new QLabel(this);
    m_scopeBadge->setAlignment(Qt::AlignCenter);
    headerLayout->addWidget(m_scopeBadge);

    headerLayout->addStretch();

    m_closeBtn = new QPushButton("✕", this);
    m_closeBtn->setFixedSize(20, 20);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setToolTip("Close");
    connect(m_closeBtn, &QPushButton::clicked, this, &QWidget::hide);
    headerLayout->addWidget(m_closeBtn);

    mainLayout->addLayout(headerLayout);

    // 2. Value Input
    auto* valLabel = new QLabel("Value:", this);
    valLabel->setStyleSheet("font-size: 11px; font-weight: 600; color: #a1a1aa;");
    mainLayout->addWidget(valLabel);

    m_valueEdit = new QLineEdit(this);
    m_valueEdit->installEventFilter(this);
    m_valueEdit->setPlaceholderText("Enter variable value...");
    connect(m_valueEdit, &QLineEdit::returnPressed, this, &VariableHoverPopup::onSaveClicked);
    mainLayout->addWidget(m_valueEdit);

    // 3. Target Scope and Secret Row
    auto* targetRow = new QHBoxLayout();
    targetRow->setSpacing(8);

    auto* scopeLabel = new QLabel("Save to:", this);
    scopeLabel->setStyleSheet("font-size: 11px; font-weight: 600; color: #a1a1aa;");
    targetRow->addWidget(scopeLabel);

    m_targetScopeCombo = new QComboBox(this);
    m_targetScopeCombo->setMinimumWidth(150);
    targetRow->addWidget(m_targetScopeCombo, 1);

    m_isSecretCheck = new QCheckBox("Secret", this);
    m_isSecretCheck->setToolTip("Store in .secret.env (masked)");
    targetRow->addWidget(m_isSecretCheck);

    mainLayout->addLayout(targetRow);

    // 4. Action Buttons
    auto* actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(8);

    m_feedbackLabel = new QLabel("Press Enter to save", this);
    m_feedbackLabel->setStyleSheet("font-size: 10px; color: #71717a;");
    actionLayout->addWidget(m_feedbackLabel, 1);

    m_manageBtn = new QPushButton("⚙️ Envs...", this);
    m_manageBtn->setToolTip("Open Environment Manager");
    connect(m_manageBtn, &QPushButton::clicked, this, [this]() {
        hide();
        emit manageEnvironmentsRequested();
    });
    actionLayout->addWidget(m_manageBtn);

    m_saveBtn = new QPushButton("💾 Save", this);
    m_saveBtn->setCursor(Qt::PointingHandCursor);
    connect(m_saveBtn, &QPushButton::clicked, this, &VariableHoverPopup::onSaveClicked);
    actionLayout->addWidget(m_saveBtn);

    mainLayout->addLayout(actionLayout);

    setFixedWidth(340);
    applyThemeStyles();
}

void VariableHoverPopup::applyThemeStyles() {
    const bool dark = Theme::isDarkMode();

    m_closeBtn->setStyleSheet(QString(
        "QPushButton { background: transparent; border: none; color: %1; font-weight: bold; border-radius: 10px; }"
        "QPushButton:hover { background-color: %2; color: #ef4444; }"
    ).arg(dark ? "#71717a" : "#94a3b8", dark ? "rgba(255, 255, 255, 0.1)" : "rgba(0, 0, 0, 0.08)"));

    m_valueEdit->setStyleSheet(QString(
        "QLineEdit { font-family: Consolas, monospace; font-size: 12px; padding: 6px 8px; border-radius: 6px; "
        "border: 1px solid %1; background-color: %2; color: %3; selection-background-color: #3b82f6; }"
        "QLineEdit:focus { border: 1px solid #3b82f6; }"
    ).arg(dark ? "#2d3142" : "#cbd5e1", dark ? "#161822" : "#ffffff", dark ? "#f4f4f5" : "#0f172a"));

    m_targetScopeCombo->setStyleSheet(QString(
        "QComboBox { font-size: 11px; padding: 4px 8px; border-radius: 5px; "
        "border: 1px solid %1; background-color: %2; color: %3; }"
    ).arg(dark ? "#2d3142" : "#cbd5e1", dark ? "#161822" : "#ffffff", dark ? "#f4f4f5" : "#0f172a"));

    m_isSecretCheck->setStyleSheet(QString("QCheckBox { font-size: 11px; color: %1; }").arg(dark ? "#a1a1aa" : "#475569"));

    m_manageBtn->setStyleSheet(QString(
        "QPushButton { font-size: 11px; padding: 4px 8px; border-radius: 5px; "
        "background-color: %1; color: %2; border: 1px solid %3; }"
        "QPushButton:hover { background-color: %4; }"
    ).arg(dark ? "#1c1f2b" : "#f1f5f9", dark ? "#cbd5e1" : "#334155", dark ? "#2e3346" : "#cbd5e1", dark ? "#282c3f" : "#e2e8f0"));

    m_saveBtn->setStyleSheet(
        "QPushButton { font-size: 11px; font-weight: 700; padding: 5px 12px; border-radius: 5px; "
        "background-color: #3b82f6; color: #ffffff; border: 1px solid #2563eb; }"
        "QPushButton:hover { background-color: #2563eb; }"
        "QPushButton:pressed { background-color: #1d4ed8; }"
    );
}

void VariableHoverPopup::showForVariable(const QString& varName,
                                        const QPoint& globalPos,
                                        core::VariableResolver* resolver,
                                        core::CollectionModel* model,
                                        const QString& activeEnvName) {
    m_varName = varName;
    m_resolver = resolver;
    m_model = model;
    m_activeEnvName = activeEnvName;

    cancelHideTimer();
    applyThemeStyles();

    m_tokenBadge->setText(QString("{{%1}}").arg(varName));

    QString currentVal;
    QString currentScope = "Unresolved";
    if (m_resolver) {
        currentVal = m_resolver->lookupVariableWithScope(varName, &currentScope);
    }

    const bool dark = Theme::isDarkMode();
    if (currentScope.isEmpty() || currentScope == "Unresolved") {
        m_scopeBadge->setText("● Unresolved");
        m_scopeBadge->setStyleSheet(
            "background-color: rgba(239, 68, 68, 0.15); color: #ef4444; border: 1px solid rgba(239, 68, 68, 0.35); "
            "border-radius: 4px; padding: 2px 6px; font-size: 10px; font-weight: 700;"
        );
    } else {
        m_scopeBadge->setText(QString("● %1").arg(currentScope));
        m_scopeBadge->setStyleSheet(
            "background-color: rgba(16, 185, 129, 0.15); color: #10b981; border: 1px solid rgba(16, 185, 129, 0.35); "
            "border-radius: 4px; padding: 2px 6px; font-size: 10px; font-weight: 700;"
        );
    }

    m_valueEdit->setText(currentVal);
    m_valueEdit->selectAll();

    // Populate target scope combo
    m_targetScopeCombo->clear();
    if (!m_activeEnvName.isEmpty()) {
        m_targetScopeCombo->addItem(QString("Environment: %1").arg(m_activeEnvName), "env");
    }
    m_targetScopeCombo->addItem("Collection Variables", "collection");
    m_targetScopeCombo->addItem("Global Session", "global");

    m_isSecretCheck->setChecked(false);
    m_feedbackLabel->setText("Press Enter to save");
    m_feedbackLabel->setStyleSheet(dark ? "font-size: 10px; color: #71717a;" : "font-size: 10px; color: #64748b;");

    // Position popup below anchor, keeping on screen
    adjustSize();
    QPoint pos = globalPos + QPoint(0, 16);
    QScreen* screen = QGuiApplication::screenAt(globalPos);
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect scrGeom = screen->availableGeometry();
        if (pos.x() + width() > scrGeom.right()) {
            pos.setX(scrGeom.right() - width() - 8);
        }
        if (pos.y() + height() > scrGeom.bottom()) {
            pos.setY(globalPos.y() - height() - 8);
        }
        if (pos.x() < scrGeom.left()) pos.setX(scrGeom.left() + 8);
    }

    move(pos);
    show();
    raise();
}

void VariableHoverPopup::cancelHideTimer() {
    if (m_hideTimer->isActive()) {
        m_hideTimer->stop();
    }
}

void VariableHoverPopup::scheduleHide(int delayMs) {
    if (isUserInteracting()) return;
    m_hideTimer->start(delayMs);
}

bool VariableHoverPopup::isUserInteracting() const {
    if (!isVisible()) return false;
    if (m_valueEdit->hasFocus() || m_targetScopeCombo->hasFocus() || m_saveBtn->hasFocus()) return true;
    QPoint mousePos = QCursor::pos();
    return geometry().contains(mousePos);
}

void VariableHoverPopup::enterEvent(QEnterEvent* /*event*/) {
    cancelHideTimer();
}

void VariableHoverPopup::leaveEvent(QEvent* /*event*/) {
    scheduleHide(350);
}

bool VariableHoverPopup::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_valueEdit && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            hide();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void VariableHoverPopup::onSaveClicked() {
    if (m_varName.isEmpty()) return;

    QString val = m_valueEdit->text().trimmed();
    QString scope = m_targetScopeCombo->currentData().toString();
    bool isSecret = m_isSecretCheck->isChecked();

    emit variableSaved(m_varName, val, scope, isSecret);

    m_feedbackLabel->setText("✓ Saved!");
    m_feedbackLabel->setStyleSheet("font-size: 11px; font-weight: 700; color: #10b981;");

    QTimer::singleShot(500, this, [this]() {
        hide();
    });
}

void VariableHoverPopup::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const bool dark = Theme::isDarkMode();
    QRectF r = rect().adjusted(1, 1, -1, -1);
    QPainterPath path;
    path.addRoundedRect(r, 8, 8);

    painter.fillPath(path, QColor(dark ? "#12141c" : "#ffffff"));
    painter.setPen(QPen(QColor(dark ? "#262938" : "#cbd5e1"), 1.2));
    painter.drawPath(path);
}

} // namespace poppy::gui
