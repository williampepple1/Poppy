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

static bool s_isDarkTheme = true;

bool Theme::isDarkMode() {
    return s_isDarkTheme;
}

void Theme::setDarkMode(bool dark) {
    s_isDarkTheme = dark;
}

bool Theme::toggleTheme() {
    s_isDarkTheme = !s_isDarkTheme;
    return s_isDarkTheme;
}

QString Theme::lightStyleSheet() {
    return R"(
        QMainWindow, QWidget {
            background-color: #ffffff;
            color: #1e293b;
            font-family: "Segoe UI", "Inter", -apple-system, sans-serif;
            font-size: 13px;
        }

        /* Splitter */
        QSplitter::handle {
            background-color: #e2e8f0;
        }
        QSplitter::handle:horizontal {
            width: 2px;
        }
        QSplitter::handle:vertical {
            height: 2px;
        }

        /* LineEdit & TextEdit */
        QLineEdit, QTextEdit, QPlainTextEdit {
            background-color: #f8fafc;
            color: #1e293b;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 6px 10px;
            selection-background-color: #6366f1;
            selection-color: #ffffff;
        }
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {
            border: 1px solid #6366f1;
            background-color: #ffffff;
        }

        /* PushButton */
        QPushButton {
            background-color: #f1f5f9;
            color: #334155;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 6px 14px;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: #e2e8f0;
            color: #0f172a;
        }
        QPushButton:pressed {
            background-color: #cbd5e1;
        }
        QPushButton#primaryBtn {
            background-color: #10b981;
            color: #ffffff;
            border: none;
            font-weight: bold;
        }
        QPushButton#primaryBtn:hover {
            background-color: #059669;
        }

        /* ComboBox */
        QComboBox {
            background-color: #f8fafc;
            color: #1e293b;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 6px 10px;
        }
        QComboBox::drop-down {
            border: none;
            width: 20px;
        }
        QComboBox QAbstractItemView {
            background-color: #ffffff;
            color: #1e293b;
            selection-background-color: #e0e7ff;
            selection-color: #312e81;
            border: 1px solid #cbd5e1;
        }

        /* TabWidget */
        QTabWidget::pane {
            border: 1px solid #e2e8f0;
            border-radius: 4px;
            background-color: #ffffff;
        }
        QTabBar::tab {
            background-color: #f1f5f9;
            color: #64748b;
            padding: 8px 16px;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            background-color: #ffffff;
            color: #0f172a;
            font-weight: bold;
            border-bottom: 2px solid #6366f1;
        }
        QTabBar::tab:hover:!selected {
            background-color: #e2e8f0;
            color: #334155;
        }

        /* TreeView & TableWidget */
        QTreeView, QTableWidget {
            background-color: #ffffff;
            color: #1e293b;
            border: 1px solid #e2e8f0;
            border-radius: 6px;
            gridline-color: #f1f5f9;
        }
        QTreeView::item:hover, QTableWidget::item:hover {
            background-color: #f8fafc;
        }
        QTreeView::item:selected, QTableWidget::item:selected {
            background-color: #e0e7ff;
            color: #312e81;
        }
        QHeaderView::section {
            background-color: #f8fafc;
            color: #64748b;
            border: none;
            border-bottom: 1px solid #e2e8f0;
            padding: 6px;
            font-weight: 600;
        }

        /* ScrollBar */
        QScrollBar:vertical {
            background: #f8fafc;
            width: 8px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #cbd5e1;
            min-height: 20px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover {
            background: #94a3b8;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar:horizontal {
            background: #f8fafc;
            height: 8px;
            margin: 0px;
        }
        QScrollBar::handle:horizontal {
            background: #cbd5e1;
            min-width: 20px;
            border-radius: 4px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #94a3b8;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
        }

        /* Menu */
        QMenuBar {
            background-color: #f8fafc;
            color: #1e293b;
            border-bottom: 1px solid #e2e8f0;
        }
        QMenuBar::item:selected {
            background-color: #e2e8f0;
            border-radius: 4px;
        }
        QMenu {
            background-color: #ffffff;
            color: #1e293b;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 4px;
        }
        QMenu::item {
            padding: 6px 24px 6px 12px;
            border-radius: 4px;
        }
        QMenu::item:selected {
            background-color: #e0e7ff;
            color: #312e81;
        }
        QMenu::separator {
            height: 1px;
            background-color: #e2e8f0;
            margin: 4px 0px;
        }

        /* Tooltip */
        QToolTip {
            background-color: #ffffff;
            color: #1e293b;
            border: 1px solid #cbd5e1;
            border-radius: 4px;
            padding: 6px;
        }
    )";
}

} // namespace poppy::gui
