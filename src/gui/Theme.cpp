#include "Theme.h"

namespace poppy::gui {

QColor Theme::methodColor(core::HttpMethod method) {
    switch (method) {
        case core::HttpMethod::GET: return QColor("#10b981");     // emerald
        case core::HttpMethod::POST: return QColor("#f59e0b");    // amber
        case core::HttpMethod::PUT: return QColor("#3b82f6");     // blue
        case core::HttpMethod::DELETE: return QColor("#ef4444");  // red
        case core::HttpMethod::PATCH: return QColor("#8b5cf6");   // purple
        case core::HttpMethod::HEAD: return QColor("#06b6d4");    // cyan
        case core::HttpMethod::OPTIONS: return QColor("#64748b"); // slate
    }
    return QColor("#10b981");
}

QString Theme::methodBadgeHtml(core::HttpMethod method) {
    QColor c = methodColor(method);
    QString name = core::methodToString(method);
    return QString("<span style=\"background-color:%1; color:#0f172a; font-weight:bold; font-size:10px; padding:2px 6px; border-radius:3px;\">%2</span>")
        .arg(c.name(), name);
}

QColor Theme::statusColor(int statusCode) {
    if (statusCode >= 200 && statusCode < 300) return QColor("#10b981"); // green
    if (statusCode >= 300 && statusCode < 400) return QColor("#3b82f6"); // blue
    if (statusCode >= 400 && statusCode < 500) return QColor("#f59e0b"); // amber/orange
    if (statusCode >= 500) return QColor("#ef4444");                     // red
    return QColor("#6b7280");                                            // gray
}

QString Theme::darkStyleSheet() {
    return R"(
        QMainWindow, QWidget {
            background-color: #18181b;
            color: #f4f4f5;
            font-family: "Segoe UI", "Inter", -apple-system, sans-serif;
            font-size: 13px;
        }

        /* Splitter */
        QSplitter::handle {
            background-color: #27272a;
        }
        QSplitter::handle:horizontal {
            width: 2px;
        }
        QSplitter::handle:vertical {
            height: 2px;
        }

        /* LineEdit & TextEdit */
        QLineEdit, QTextEdit, QPlainTextEdit {
            background-color: #27272a;
            color: #f4f4f5;
            border: 1px solid #3f3f46;
            border-radius: 6px;
            padding: 6px 10px;
            selection-background-color: #4f46e5;
            selection-color: #ffffff;
        }
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {
            border: 1px solid #6366f1;
            background-color: #1f1f23;
        }

        /* ComboBox */
        QComboBox {
            background-color: #27272a;
            color: #f4f4f5;
            border: 1px solid #3f3f46;
            border-radius: 6px;
            padding: 6px 12px;
            min-height: 20px;
        }
        QComboBox:hover {
            border: 1px solid #52525b;
        }
        QComboBox:focus {
            border: 1px solid #6366f1;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 20px;
            border-left: none;
        }
        QComboBox QAbstractItemView {
            background-color: #27272a;
            color: #f4f4f5;
            border: 1px solid #3f3f46;
            border-radius: 6px;
            selection-background-color: #4f46e5;
            selection-color: #ffffff;
            outline: none;
            padding: 4px;
        }

        /* PushButton */
        QPushButton {
            background-color: #27272a;
            color: #f4f4f5;
            border: 1px solid #3f3f46;
            border-radius: 6px;
            padding: 7px 16px;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: #3f3f46;
            border-color: #52525b;
        }
        QPushButton:pressed {
            background-color: #18181b;
        }
        QPushButton:disabled {
            background-color: #202024;
            color: #71717a;
            border-color: #27272a;
        }

        /* Primary Action Button (Send button) */
        QPushButton#primaryBtn {
            background-color: #ec4899;
            color: #ffffff;
            border: 1px solid #db2777;
            font-weight: 600;
        }
        QPushButton#primaryBtn:hover {
            background-color: #f43f5e;
            border-color: #e11d48;
        }
        QPushButton#primaryBtn:pressed {
            background-color: #be123c;
        }

        /* Tab Widget */
        QTabWidget::pane {
            border: 1px solid #27272a;
            border-radius: 6px;
            background-color: #18181b;
            top: -1px;
        }
        QTabBar::tab {
            background-color: transparent;
            color: #a1a1aa;
            border: none;
            border-bottom: 2px solid transparent;
            padding: 8px 16px;
            font-weight: 500;
        }
        QTabBar::tab:selected {
            color: #f4f4f5;
            border-bottom: 2px solid #ec4899;
        }
        QTabBar::tab:hover:!selected {
            color: #e4e4e7;
        }

        /* TreeView & TableView */
        QTreeView, QTableView {
            background-color: #121214;
            color: #f4f4f5;
            border: 1px solid #27272a;
            border-radius: 6px;
            gridline-color: #27272a;
            selection-background-color: #27272a;
            selection-color: #f4f4f5;
            outline: none;
        }
        QTreeView::item, QTableView::item {
            padding: 6px 8px;
            border-radius: 4px;
        }
        QTreeView::item:hover, QTableView::item:hover {
            background-color: #1c1c20;
        }
        QTreeView::item:selected, QTableView::item:selected {
            background-color: #2e2e36;
        }
        QHeaderView::section {
            background-color: #18181b;
            color: #a1a1aa;
            border: none;
            border-bottom: 1px solid #27272a;
            padding: 6px 8px;
            font-weight: 600;
            font-size: 11px;
            text-transform: uppercase;
        }

        /* CheckBox */
        QCheckBox {
            spacing: 8px;
            color: #f4f4f5;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border-radius: 4px;
            border: 1px solid #52525b;
            background-color: #27272a;
        }
        QCheckBox::indicator:checked {
            background-color: #ec4899;
            border-color: #ec4899;
        }

        /* ScrollBar */
        QScrollBar:vertical {
            border: none;
            background: #18181b;
            width: 8px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #3f3f46;
            min-height: 20px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover {
            background: #52525b;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar:horizontal {
            border: none;
            background: #18181b;
            height: 8px;
            margin: 0px;
        }
        QScrollBar::handle:horizontal {
            background: #3f3f46;
            min-width: 20px;
            border-radius: 4px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #52525b;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
        }

        /* Menu */
        QMenuBar {
            background-color: #18181b;
            color: #f4f4f5;
            border-bottom: 1px solid #27272a;
        }
        QMenuBar::item:selected {
            background-color: #27272a;
            border-radius: 4px;
        }
        QMenu {
            background-color: #27272a;
            color: #f4f4f5;
            border: 1px solid #3f3f46;
            border-radius: 6px;
            padding: 4px;
        }
        QMenu::item {
            padding: 6px 24px 6px 12px;
            border-radius: 4px;
        }
        QMenu::item:selected {
            background-color: #3f3f46;
        }
        QMenu::separator {
            height: 1px;
            background-color: #3f3f46;
            margin: 4px 0px;
        }

        /* Tooltip */
        QToolTip {
            background-color: #27272a;
            color: #f4f4f5;
            border: 1px solid #52525b;
            border-radius: 4px;
            padding: 6px;
        }
    )";
}

} // namespace poppy::gui
