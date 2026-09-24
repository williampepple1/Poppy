#include "Theme.h"

namespace poppy::gui {

QColor Theme::methodColor(core::HttpMethod method) {
    switch (method) {
        case core::HttpMethod::GET: return QColor("#22c55e");     // vibrant emerald
        case core::HttpMethod::POST: return QColor("#f59e0b");    // golden amber
        case core::HttpMethod::PUT: return QColor("#3b82f6");     // sky blue
        case core::HttpMethod::DELETE: return QColor("#ef4444");  // crimson red
        case core::HttpMethod::PATCH: return QColor("#a855f7");   // violet purple
        case core::HttpMethod::HEAD: return QColor("#06b6d4");    // cyan
        case core::HttpMethod::OPTIONS: return QColor("#94a3b8"); // slate
    }
    return QColor("#22c55e");
}

QString Theme::methodBadgeHtml(core::HttpMethod method) {
    QColor c = methodColor(method);
    QString name = core::methodToString(method);
    return QString("<span style=\"background-color:rgba(%1,%2,%3,0.18); color:%4; font-weight:bold; font-size:10px; padding:2px 7px; border-radius:4px; border:1px solid rgba(%1,%2,%3,0.4);\">%5</span>")
        .arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.name(), name);
}

QColor Theme::statusColor(int statusCode) {
    if (statusCode >= 200 && statusCode < 300) return QColor("#22c55e"); // green
    if (statusCode >= 300 && statusCode < 400) return QColor("#3b82f6"); // blue
    if (statusCode >= 400 && statusCode < 500) return QColor("#f59e0b"); // amber/orange
    if (statusCode >= 500) return QColor("#ef4444");                     // red
    return QColor("#6b7280");                                            // gray
}

QString Theme::darkStyleSheet() {
    return R"(
        QMainWindow, QWidget {
            background-color: #131416;
            color: #e5e7eb;
            font-family: "Segoe UI", "Inter", -apple-system, sans-serif;
            font-size: 13px;
        }

        /* Splitter */
        QSplitter::handle {
            background-color: #222328;
        }
        QSplitter::handle:horizontal {
            width: 1px;
        }
        QSplitter::handle:vertical {
            height: 1px;
        }

        /* Integrated URL Bar Container */
        QFrame#urlBarContainer {
            background-color: #18191d;
            border: 1px solid #27282e;
            border-radius: 8px;
            padding: 2px 4px;
        }
        QFrame#urlBarContainer:focus-within {
            border: 1px solid #f59e0b;
        }
        QFrame#urlBarDivider {
            background-color: #27282e;
            width: 1px;
            max-width: 1px;
            margin: 4px 2px;
        }

        /* Method Combo inside URL Bar */
        QComboBox#methodCombo {
            background: transparent;
            border: none;
            font-weight: 800;
            font-size: 13px;
            padding-left: 6px;
            min-height: 26px;
        }
        QComboBox#methodCombo:hover {
            background-color: rgba(255, 255, 255, 0.05);
            border-radius: 4px;
        }
        QComboBox#methodCombo::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 16px;
            border-left: none;
        }
        QComboBox#methodCombo::down-arrow {
            image: none;
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-top: 5px solid #9ca3af;
            margin-right: 4px;
        }

        /* URL LineEdit inside URL Bar */
        QLineEdit#urlEdit {
            background: transparent;
            border: none;
            font-family: "JetBrains Mono", "Cascadia Code", "Fira Code", Consolas, monospace;
            font-size: 13px;
            color: #f3f4f6;
            padding: 6px 8px;
            selection-background-color: #f59e0b;
            selection-color: #0c0a09;
        }
        QLineEdit#urlEdit:focus {
            background: transparent;
            border: none;
        }

        /* Standard LineEdit & TextEdit */
        QLineEdit, QTextEdit, QPlainTextEdit {
            background-color: #18191d;
            color: #f3f4f6;
            border: 1px solid #27282e;
            border-radius: 6px;
            padding: 6px 10px;
            selection-background-color: #f59e0b;
            selection-color: #0c0a09;
        }
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {
            border: 1px solid #f59e0b;
            background-color: #1c1d22;
        }

        /* ComboBox */
        QComboBox {
            background-color: #1c1d22;
            color: #f3f4f6;
            border: 1px solid #27282e;
            border-radius: 6px;
            padding: 5px 12px;
            min-height: 22px;
            font-weight: 500;
        }
        QComboBox:hover {
            border-color: #3b3c44;
            background-color: #212228;
        }
        QComboBox:focus {
            border-color: #f59e0b;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 20px;
            border-left: none;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-top: 5px solid #9ca3af;
            margin-right: 6px;
        }
        QComboBox QAbstractItemView {
            background-color: #18191d;
            color: #f3f4f6;
            border: 1px solid #27282e;
            border-radius: 6px;
            selection-background-color: #272831;
            selection-color: #ffffff;
            outline: none;
            padding: 4px;
        }

        /* Standard PushButton */
        QPushButton {
            background-color: #1e1f24;
            color: #e5e7eb;
            border: 1px solid #2e2f35;
            border-radius: 6px;
            padding: 6px 14px;
            font-weight: 500;
            font-size: 12px;
        }
        QPushButton:hover {
            background-color: #27282f;
            border-color: #3f4048;
            color: #ffffff;
        }
        QPushButton:pressed {
            background-color: #18191d;
            border-color: #24252a;
        }
        QPushButton:disabled {
            background-color: #16171a;
            color: #52535a;
            border-color: #222327;
        }

        /* Primary Action Button (Send button) */
        QPushButton#primaryBtn {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #f59e0b, stop:1 #ea580c);
            color: #0c0a09;
            border: none;
            border-radius: 6px;
            padding: 7px 18px;
            font-weight: 700;
            font-size: 13px;
        }
        QPushButton#primaryBtn:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #fbbf24, stop:1 #f59e0b);
            color: #000000;
        }
        QPushButton#primaryBtn:pressed {
            background: #d97706;
            color: #000000;
        }
        QPushButton#primaryBtn:disabled {
            background: #252018;
            color: #615037;
            border: none;
        }

        /* Tab Widget (Flat Underline Style) */
        QTabWidget::pane {
            border: none;
            border-top: 1px solid #26272c;
            background-color: transparent;
        }
        QTabBar::tab {
            background-color: transparent;
            color: #9ca3af;
            border: none;
            border-bottom: 2px solid transparent;
            padding: 8px 16px;
            font-weight: 500;
            font-size: 12px;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            color: #ffffff;
            border-bottom: 2px solid #f59e0b;
            font-weight: 600;
        }
        QTabBar::tab:hover:!selected {
            color: #e5e7eb;
            background: rgba(255, 255, 255, 0.03);
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
        }
        QTabBar::close-button {
            image: url(:/icons/close_tab.png);
            subcontrol-position: right;
            subcontrol-origin: padding;
            margin-left: 6px;
            margin-right: 2px;
            padding: 3px;
            border-radius: 4px;
            background: transparent;
        }
        QTabBar::close-button:hover {
            image: url(:/icons/close_tab_hover.png);
            background: rgba(255, 255, 255, 0.12);
        }
        QTabBar::close-button:pressed {
            background: rgba(255, 255, 255, 0.22);
        }

        /* TreeView & TableView */
        QTreeView, QTableView, QTreeWidget, QTableWidget {
            background-color: #141518;
            color: #f3f4f6;
            border: 1px solid #232429;
            border-radius: 6px;
            gridline-color: #1f2025;
            selection-background-color: #252730;
            selection-color: #ffffff;
            outline: none;
        }
        QTreeView::item, QTableView::item {
            padding: 4px 6px;
            border-radius: 4px;
            min-height: 24px;
        }
        QTreeView::item:hover, QTableView::item:hover {
            background-color: #1c1d22;
        }
        QTreeView::item:selected, QTableView::item:selected {
            background-color: #272831;
            color: #ffffff;
        }
        QHeaderView::section {
            background-color: #17181c;
            color: #9ca3af;
            border: none;
            border-bottom: 1px solid #232429;
            border-right: 1px solid #232429;
            padding: 6px 8px;
            font-weight: 600;
            font-size: 11px;
            text-transform: uppercase;
        }

        /* CheckBox */
        QCheckBox {
            spacing: 8px;
            color: #e5e7eb;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border-radius: 4px;
            border: 1px solid #3d3e46;
            background-color: #1c1d22;
        }
        QCheckBox::indicator:hover {
            border-color: #f59e0b;
        }
        QCheckBox::indicator:checked {
            background-color: #f59e0b;
            border-color: #f59e0b;
        }

        /* ScrollBar */
        QScrollBar:vertical {
            border: none;
            background: transparent;
            width: 6px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #32333a;
            min-height: 24px;
            border-radius: 3px;
        }
        QScrollBar::handle:vertical:hover {
            background: #4a4b54;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar:horizontal {
            border: none;
            background: transparent;
            height: 6px;
            margin: 0px;
        }
        QScrollBar::handle:horizontal {
            background: #32333a;
            min-width: 24px;
            border-radius: 3px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #4a4b54;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
        }

        /* Menu */
        QMenuBar {
            background-color: #131416;
            color: #9ca3af;
            border-bottom: 1px solid #222328;
            padding: 2px 4px;
            font-size: 12px;
        }
        QMenuBar::item {
            padding: 4px 10px;
            border-radius: 4px;
        }
        QMenuBar::item:selected {
            background-color: #1f2025;
            color: #ffffff;
        }
        QMenu {
            background-color: #18191d;
            color: #e5e7eb;
            border: 1px solid #27282e;
            border-radius: 6px;
            padding: 4px;
        }
        QMenu::item {
            padding: 6px 24px 6px 12px;
            border-radius: 4px;
        }
        QMenu::item:selected {
            background-color: #272831;
            color: #ffffff;
        }
        QMenu::separator {
            height: 1px;
            background-color: #27282e;
            margin: 4px 6px;
        }

        /* Tooltip */
        QToolTip {
            background-color: #18191d;
            color: #f3f4f6;
            border: 1px solid #383940;
            border-radius: 5px;
            padding: 6px 10px;
            font-size: 12px;
        }

        /* Status Bar */
        QStatusBar {
            background-color: #131416;
            color: #9ca3af;
            border-top: 1px solid #222328;
            font-size: 12px;
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
            background-color: #f8fafc;
            color: #1e293b;
            font-family: "Segoe UI", "Inter", -apple-system, sans-serif;
            font-size: 13px;
        }

        /* Splitter */
        QSplitter::handle {
            background-color: #e2e8f0;
        }
        QSplitter::handle:horizontal {
            width: 1px;
        }
        QSplitter::handle:vertical {
            height: 1px;
        }

        /* Integrated URL Bar Container */
        QFrame#urlBarContainer {
            background-color: #ffffff;
            border: 1px solid #cbd5e1;
            border-radius: 8px;
            padding: 2px 4px;
        }
        QFrame#urlBarContainer:focus-within {
            border: 1px solid #d97706;
        }
        QFrame#urlBarDivider {
            background-color: #e2e8f0;
            width: 1px;
            max-width: 1px;
            margin: 4px 2px;
        }

        /* Method Combo inside URL Bar */
        QComboBox#methodCombo {
            background: transparent;
            border: none;
            font-weight: 800;
            font-size: 13px;
            padding-left: 6px;
            min-height: 26px;
        }
        QComboBox#methodCombo:hover {
            background-color: rgba(0, 0, 0, 0.04);
            border-radius: 4px;
        }
        QComboBox#methodCombo::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 16px;
            border-left: none;
        }
        QComboBox#methodCombo::down-arrow {
            image: none;
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-top: 5px solid #64748b;
            margin-right: 4px;
        }

        /* URL LineEdit inside URL Bar */
        QLineEdit#urlEdit {
            background: transparent;
            border: none;
            font-family: "JetBrains Mono", "Cascadia Code", "Fira Code", Consolas, monospace;
            font-size: 13px;
            color: #0f172a;
            padding: 6px 8px;
            selection-background-color: #fde68a;
            selection-color: #0f172a;
        }
        QLineEdit#urlEdit:focus {
            background: transparent;
            border: none;
        }

        /* Standard LineEdit & TextEdit */
        QLineEdit, QTextEdit, QPlainTextEdit {
            background-color: #ffffff;
            color: #0f172a;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 6px 10px;
            selection-background-color: #fde68a;
            selection-color: #0f172a;
        }
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {
            border: 1px solid #d97706;
            background-color: #ffffff;
        }

        /* PushButton */
        QPushButton {
            background-color: #ffffff;
            color: #334155;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 6px 14px;
            font-weight: 500;
            font-size: 12px;
        }
        QPushButton:hover {
            background-color: #f1f5f9;
            border-color: #94a3b8;
            color: #0f172a;
        }
        QPushButton:pressed {
            background-color: #e2e8f0;
        }
        QPushButton:disabled {
            background-color: #f8fafc;
            color: #94a3b8;
            border-color: #e2e8f0;
        }

        /* Primary Action Button */
        QPushButton#primaryBtn {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #f59e0b, stop:1 #ea580c);
            color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 7px 18px;
            font-weight: 700;
            font-size: 13px;
        }
        QPushButton#primaryBtn:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #fbbf24, stop:1 #f59e0b);
        }
        QPushButton#primaryBtn:pressed {
            background: #c2410c;
        }

        /* ComboBox */
        QComboBox {
            background-color: #ffffff;
            color: #1e293b;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 5px 10px;
            min-height: 22px;
        }
        QComboBox:hover {
            border-color: #94a3b8;
        }
        QComboBox:focus {
            border-color: #d97706;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 20px;
            border-left: none;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-top: 5px solid #64748b;
            margin-right: 6px;
        }
        QComboBox QAbstractItemView {
            background-color: #ffffff;
            color: #1e293b;
            selection-background-color: #fef3c7;
            selection-color: #78350f;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 4px;
            outline: none;
        }

        /* TabWidget */
        QTabWidget::pane {
            border: none;
            border-top: 1px solid #e2e8f0;
            background-color: transparent;
        }
        QTabBar::tab {
            background-color: transparent;
            color: #64748b;
            padding: 8px 16px;
            border: none;
            border-bottom: 2px solid transparent;
            margin-right: 2px;
            font-size: 12px;
            font-weight: 500;
        }
        QTabBar::tab:selected {
            color: #0f172a;
            font-weight: 600;
            border-bottom: 2px solid #d97706;
        }
        QTabBar::tab:hover:!selected {
            background-color: rgba(0, 0, 0, 0.03);
            color: #334155;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
        }
        QTabBar::close-button {
            image: url(:/icons/close_tab_light.png);
            subcontrol-position: right;
            subcontrol-origin: padding;
            margin-left: 6px;
            margin-right: 2px;
            padding: 3px;
            border-radius: 4px;
            background: transparent;
        }
        QTabBar::close-button:hover {
            image: url(:/icons/close_tab_light_hover.png);
            background: rgba(0, 0, 0, 0.08);
        }
        QTabBar::close-button:pressed {
            background: rgba(0, 0, 0, 0.15);
        }

        /* TreeView & TableWidget */
        QTreeView, QTableWidget, QTreeWidget, QTableView {
            background-color: #ffffff;
            color: #1e293b;
            border: 1px solid #e2e8f0;
            border-radius: 6px;
            gridline-color: #f1f5f9;
            outline: none;
        }
        QTreeView::item, QTableWidget::item {
            padding: 4px 6px;
            border-radius: 4px;
            min-height: 24px;
        }
        QTreeView::item:hover, QTableWidget::item:hover {
            background-color: #f8fafc;
        }
        QTreeView::item:selected, QTableWidget::item:selected {
            background-color: #fef3c7;
            color: #78350f;
        }
        QHeaderView::section {
            background-color: #f8fafc;
            color: #64748b;
            border: none;
            border-bottom: 1px solid #e2e8f0;
            border-right: 1px solid #e2e8f0;
            padding: 6px 8px;
            font-weight: 600;
            font-size: 11px;
            text-transform: uppercase;
        }

        /* ScrollBar */
        QScrollBar:vertical {
            background: transparent;
            width: 6px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #cbd5e1;
            min-height: 24px;
            border-radius: 3px;
        }
        QScrollBar::handle:vertical:hover {
            background: #94a3b8;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar:horizontal {
            background: transparent;
            height: 6px;
            margin: 0px;
        }
        QScrollBar::handle:horizontal {
            background: #cbd5e1;
            min-width: 24px;
            border-radius: 3px;
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
            color: #475569;
            border-bottom: 1px solid #e2e8f0;
            font-size: 12px;
            padding: 2px 4px;
        }
        QMenuBar::item {
            padding: 4px 10px;
            border-radius: 4px;
        }
        QMenuBar::item:selected {
            background-color: #e2e8f0;
            color: #0f172a;
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
            background-color: #f1f5f9;
            color: #0f172a;
        }
        QMenu::separator {
            height: 1px;
            background-color: #e2e8f0;
            margin: 4px 6px;
        }

        /* Tooltip */
        QToolTip {
            background-color: #ffffff;
            color: #1e293b;
            border: 1px solid #cbd5e1;
            border-radius: 5px;
            padding: 6px 10px;
            font-size: 12px;
        }

        /* Status Bar */
        QStatusBar {
            background-color: #f8fafc;
            color: #64748b;
            border-top: 1px solid #e2e8f0;
            font-size: 12px;
        }
    )";
}

} // namespace poppy::gui
