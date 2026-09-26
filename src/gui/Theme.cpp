#include "Theme.h"
#include <QMap>
#include <QSettings>

namespace poppy::gui {

namespace {

struct ThemePalette {
    QString id;
    QString name;
    QString category;
    bool isDark{true};

    // Backgrounds & Surfaces
    QString bg;
    QString surface;
    QString surfaceAlt;
    QString surfaceHover;
    QString border;
    QString borderLight;

    // Text
    QString text;
    QString textMuted;
    QString textDim;

    // Accent
    QString accent;
    QString accentHover;
    QString accentPressed;
    QString accentText;

    // Selection
    QString selectionBg;
    QString selectionFg;

    // Table & Tree Headers
    QString headerBg;
    QString headerText;

    // Scrollbars
    QString scrollHandle;
    QString scrollHover;

    // Menus
    QString menuBg;
    QString menuHover;
    QString menuText;

    // Status bar & Telemetry
    QString statusBarBg;
    QString statusBarFg;
    QString telemetryBg;
    QString telemetryFg;

    // Open Requests Tab Bar
    QString tabBarBg;
    QString tabActiveBg;
    QString tabActiveBorder;
    QString tabActiveText;
    QString tabInactiveText;
    QString closeTabIcon;
    QString closeTabHover;
};

const QList<ThemePalette>& allPalettes() {
    static const QList<ThemePalette> s_palettes = {
        // ── 1. DARK & MONOCHROME SHADES ──
        ThemePalette{
            .id = "obsidian",
            .name = "Obsidian Dark",
            .category = "Black & White Shades",
            .isDark = true,
            .bg = "#0c0d10",
            .surface = "#14161c",
            .surfaceAlt = "#181920",
            .surfaceHover = "#22242e",
            .border = "#252834",
            .borderLight = "#1f212a",
            .text = "#f9fafb",
            .textMuted = "#9496a1",
            .textDim = "#6b7280",
            .accent = "#f59e0b",
            .accentHover = "#fbbf24",
            .accentPressed = "#b45309",
            .accentText = "#0c0a09",
            .selectionBg = "#262936",
            .selectionFg = "#ffffff",
            .headerBg = "#14151b",
            .headerText = "#9ca3af",
            .scrollHandle = "#2a2d3a",
            .scrollHover = "#3d4254",
            .menuBg = "#14161c",
            .menuHover = "#22242e",
            .menuText = "#f3f4f6",
            .statusBarBg = "#0a0a0d",
            .statusBarFg = "#71717a",
            .telemetryBg = "#181920",
            .telemetryFg = "#94a3b8",
            .tabBarBg = "#111215",
            .tabActiveBg = "#18191e",
            .tabActiveBorder = "#282932",
            .tabActiveText = "#ffffff",
            .tabInactiveText = "#9496a1",
            .closeTabIcon = ":/icons/close_tab.png",
            .closeTabHover = ":/icons/close_tab_hover.png"
        },
        ThemePalette{
            .id = "midnight",
            .name = "Midnight Slate",
            .category = "Black & White Shades",
            .isDark = true,
            .bg = "#0b0f19",
            .surface = "#111827",
            .surfaceAlt = "#162032",
            .surfaceHover = "#1f293d",
            .border = "#1f2c42",
            .borderLight = "#172133",
            .text = "#f8fafc",
            .textMuted = "#94a3b8",
            .textDim = "#64748b",
            .accent = "#38bdf8",
            .accentHover = "#7dd3fc",
            .accentPressed = "#0284c7",
            .accentText = "#071324",
            .selectionBg = "#1e2f4d",
            .selectionFg = "#ffffff",
            .headerBg = "#101624",
            .headerText = "#94a3b8",
            .scrollHandle = "#26354f",
            .scrollHover = "#384d73",
            .menuBg = "#111827",
            .menuHover = "#1f293d",
            .menuText = "#f8fafc",
            .statusBarBg = "#080c14",
            .statusBarFg = "#64748b",
            .telemetryBg = "#162032",
            .telemetryFg = "#94a3b8",
            .tabBarBg = "#0e1422",
            .tabActiveBg = "#162035",
            .tabActiveBorder = "#263757",
            .tabActiveText = "#ffffff",
            .tabInactiveText = "#94a3b8",
            .closeTabIcon = ":/icons/close_tab.png",
            .closeTabHover = ":/icons/close_tab_hover.png"
        },
        ThemePalette{
            .id = "carbon",
            .name = "Carbon Charcoal",
            .category = "Black & White Shades",
            .isDark = true,
            .bg = "#121214",
            .surface = "#18181c",
            .surfaceAlt = "#202026",
            .surfaceHover = "#2c2c34",
            .border = "#2d2d38",
            .borderLight = "#22222a",
            .text = "#f4f4f5",
            .textMuted = "#a1a1aa",
            .textDim = "#71717a",
            .accent = "#10b981",
            .accentHover = "#34d399",
            .accentPressed = "#059669",
            .accentText = "#022c22",
            .selectionBg = "#2a3038",
            .selectionFg = "#ffffff",
            .headerBg = "#16161a",
            .headerText = "#a1a1aa",
            .scrollHandle = "#32323e",
            .scrollHover = "#454556",
            .menuBg = "#18181c",
            .menuHover = "#2c2c34",
            .menuText = "#f4f4f5",
            .statusBarBg = "#0e0e10",
            .statusBarFg = "#71717a",
            .telemetryBg = "#202026",
            .telemetryFg = "#a1a1aa",
            .tabBarBg = "#141417",
            .tabActiveBg = "#1e1e24",
            .tabActiveBorder = "#353540",
            .tabActiveText = "#ffffff",
            .tabInactiveText = "#a1a1aa",
            .closeTabIcon = ":/icons/close_tab.png",
            .closeTabHover = ":/icons/close_tab_hover.png"
        },
        ThemePalette{
            .id = "espresso",
            .name = "Warm Espresso",
            .category = "Black & White Shades",
            .isDark = true,
            .bg = "#141210",
            .surface = "#1c1917",
            .surfaceAlt = "#24201c",
            .surfaceHover = "#302b26",
            .border = "#36302b",
            .borderLight = "#292420",
            .text = "#fafaf9",
            .textMuted = "#a8a29e",
            .textDim = "#78716c",
            .accent = "#d97706",
            .accentHover = "#f59e0b",
            .accentPressed = "#b45309",
            .accentText = "#1c1917",
            .selectionBg = "#3d3228",
            .selectionFg = "#ffffff",
            .headerBg = "#181513",
            .headerText = "#a8a29e",
            .scrollHandle = "#3d3630",
            .scrollHover = "#524840",
            .menuBg = "#1c1917",
            .menuHover = "#302b26",
            .menuText = "#fafaf9",
            .statusBarBg = "#0f0d0b",
            .statusBarFg = "#78716c",
            .telemetryBg = "#24201c",
            .telemetryFg = "#a8a29e",
            .tabBarBg = "#171412",
            .tabActiveBg = "#231f1c",
            .tabActiveBorder = "#3d3630",
            .tabActiveText = "#ffffff",
            .tabInactiveText = "#a8a29e",
            .closeTabIcon = ":/icons/close_tab.png",
            .closeTabHover = ":/icons/close_tab_hover.png"
        },
        ThemePalette{
            .id = "monochrome",
            .name = "Pure Monochrome",
            .category = "Black & White Shades",
            .isDark = true,
            .bg = "#000000",
            .surface = "#0d0d0d",
            .surfaceAlt = "#171717",
            .surfaceHover = "#262626",
            .border = "#333333",
            .borderLight = "#202020",
            .text = "#ffffff",
            .textMuted = "#a3a3a3",
            .textDim = "#737373",
            .accent = "#ffffff",
            .accentHover = "#e5e5e5",
            .accentPressed = "#a3a3a3",
            .accentText = "#000000",
            .selectionBg = "#383838",
            .selectionFg = "#ffffff",
            .headerBg = "#0f0f0f",
            .headerText = "#a3a3a3",
            .scrollHandle = "#333333",
            .scrollHover = "#4d4d4d",
            .menuBg = "#0d0d0d",
            .menuHover = "#262626",
            .menuText = "#ffffff",
            .statusBarBg = "#000000",
            .statusBarFg = "#737373",
            .telemetryBg = "#171717",
            .telemetryFg = "#a3a3a3",
            .tabBarBg = "#080808",
            .tabActiveBg = "#1a1a1a",
            .tabActiveBorder = "#404040",
            .tabActiveText = "#ffffff",
            .tabInactiveText = "#a3a3a3",
            .closeTabIcon = ":/icons/close_tab.png",
            .closeTabHover = ":/icons/close_tab_hover.png"
        },

        // ── 2. LIGHT & CLEAN SHADES ──
        ThemePalette{
            .id = "snow",
            .name = "Snow White",
            .category = "Black & White Shades",
            .isDark = false,
            .bg = "#f8fafc",
            .surface = "#ffffff",
            .surfaceAlt = "#f1f5f9",
            .surfaceHover = "#e2e8f0",
            .border = "#e2e8f0",
            .borderLight = "#cbd5e1",
            .text = "#0f172a",
            .textMuted = "#64748b",
            .textDim = "#94a3b8",
            .accent = "#2563eb",
            .accentHover = "#1d4ed8",
            .accentPressed = "#1e40af",
            .accentText = "#ffffff",
            .selectionBg = "#dbeafe",
            .selectionFg = "#1e3a8a",
            .headerBg = "#f1f5f9",
            .headerText = "#475569",
            .scrollHandle = "#cbd5e1",
            .scrollHover = "#94a3b8",
            .menuBg = "#ffffff",
            .menuHover = "#f1f5f9",
            .menuText = "#0f172a",
            .statusBarBg = "#f8fafc",
            .statusBarFg = "#64748b",
            .telemetryBg = "#ffffff",
            .telemetryFg = "#475569",
            .tabBarBg = "#f8fafc",
            .tabActiveBg = "#ffffff",
            .tabActiveBorder = "#cbd5e1",
            .tabActiveText = "#0f172a",
            .tabInactiveText = "#64748b",
            .closeTabIcon = ":/icons/close_tab_light.png",
            .closeTabHover = ":/icons/close_tab_light.png"
        },
        ThemePalette{
            .id = "nordic",
            .name = "Nordic Light",
            .category = "Black & White Shades",
            .isDark = false,
            .bg = "#eef2f6",
            .surface = "#f8fafc",
            .surfaceAlt = "#e5ebf2",
            .surfaceHover = "#dbe2eb",
            .border = "#d4dbe4",
            .borderLight = "#c2cbd6",
            .text = "#1e293b",
            .textMuted = "#475569",
            .textDim = "#64748b",
            .accent = "#0d9488",
            .accentHover = "#0f766e",
            .accentPressed = "#115e59",
            .accentText = "#ffffff",
            .selectionBg = "#ccfbf1",
            .selectionFg = "#134e4a",
            .headerBg = "#e5ebf2",
            .headerText = "#334155",
            .scrollHandle = "#cbd5e1",
            .scrollHover = "#94a3b8",
            .menuBg = "#f8fafc",
            .menuHover = "#dbe2eb",
            .menuText = "#1e293b",
            .statusBarBg = "#eef2f6",
            .statusBarFg = "#475569",
            .telemetryBg = "#f8fafc",
            .telemetryFg = "#334155",
            .tabBarBg = "#e5ebf2",
            .tabActiveBg = "#f8fafc",
            .tabActiveBorder = "#cbd5e1",
            .tabActiveText = "#1e293b",
            .tabInactiveText = "#475569",
            .closeTabIcon = ":/icons/close_tab_light.png",
            .closeTabHover = ":/icons/close_tab_light.png"
        },
        ThemePalette{
            .id = "sepia",
            .name = "Warm Paper",
            .category = "Black & White Shades",
            .isDark = false,
            .bg = "#f7f4ed",
            .surface = "#fffdfa",
            .surfaceAlt = "#f0ebe1",
            .surfaceHover = "#e8e1d5",
            .border = "#ded7c8",
            .borderLight = "#cfc6b4",
            .text = "#2c2724",
            .textMuted = "#6b635b",
            .textDim = "#8c8277",
            .accent = "#c2410c",
            .accentHover = "#9a3412",
            .accentPressed = "#7c2d12",
            .accentText = "#ffffff",
            .selectionBg = "#ffedd5",
            .selectionFg = "#7c2d12",
            .headerBg = "#f0ebe1",
            .headerText = "#574f47",
            .scrollHandle = "#d6cdbe",
            .scrollHover = "#b8ab97",
            .menuBg = "#fffdfa",
            .menuHover = "#e8e1d5",
            .menuText = "#2c2724",
            .statusBarBg = "#f7f4ed",
            .statusBarFg = "#6b635b",
            .telemetryBg = "#fffdfa",
            .telemetryFg = "#574f47",
            .tabBarBg = "#ede7db",
            .tabActiveBg = "#fffdfa",
            .tabActiveBorder = "#ded7c8",
            .tabActiveText = "#2c2724",
            .tabInactiveText = "#6b635b",
            .closeTabIcon = ":/icons/close_tab_light.png",
            .closeTabHover = ":/icons/close_tab_light.png"
        },

        // ── 3. VIBRANT & DEVELOPER THEMES ──
        ThemePalette{
            .id = "cyberpunk",
            .name = "Cyberpunk Neon",
            .category = "Vibrant & Developer",
            .isDark = true,
            .bg = "#0a0714",
            .surface = "#130f24",
            .surfaceAlt = "#1a1433",
            .surfaceHover = "#281e4d",
            .border = "#30225c",
            .borderLight = "#221742",
            .text = "#f4f0ff",
            .textMuted = "#a899cc",
            .textDim = "#6e6090",
            .accent = "#ff007f",
            .accentHover = "#ff3399",
            .accentPressed = "#cc0066",
            .accentText = "#ffffff",
            .selectionBg = "#421245",
            .selectionFg = "#00f0ff",
            .headerBg = "#151028",
            .headerText = "#a899cc",
            .scrollHandle = "#362669",
            .scrollHover = "#523a9e",
            .menuBg = "#130f24",
            .menuHover = "#281e4d",
            .menuText = "#f4f0ff",
            .statusBarBg = "#080510",
            .statusBarFg = "#6e6090",
            .telemetryBg = "#1a1433",
            .telemetryFg = "#a899cc",
            .tabBarBg = "#0e091d",
            .tabActiveBg = "#1b1436",
            .tabActiveBorder = "#4a3382",
            .tabActiveText = "#00f0ff",
            .tabInactiveText = "#a899cc",
            .closeTabIcon = ":/icons/close_tab.png",
            .closeTabHover = ":/icons/close_tab_hover.png"
        },
        ThemePalette{
            .id = "nord",
            .name = "Nord Aurora",
            .category = "Vibrant & Developer",
            .isDark = true,
            .bg = "#242933",
            .surface = "#2e3440",
            .surfaceAlt = "#3b4252",
            .surfaceHover = "#434c5e",
            .border = "#4c566a",
            .borderLight = "#3b4252",
            .text = "#eceff4",
            .textMuted = "#d8dee9",
            .textDim = "#e5e9f0",
            .accent = "#88c0d0",
            .accentHover = "#8fbcbb",
            .accentPressed = "#81a1c1",
            .accentText = "#242933",
            .selectionBg = "#3b4252",
            .selectionFg = "#88c0d0",
            .headerBg = "#2e3440",
            .headerText = "#d8dee9",
            .scrollHandle = "#434c5e",
            .scrollHover = "#4c566a",
            .menuBg = "#2e3440",
            .menuHover = "#434c5e",
            .menuText = "#eceff4",
            .statusBarBg = "#1e222a",
            .statusBarFg = "#d8dee9",
            .telemetryBg = "#3b4252",
            .telemetryFg = "#eceff4",
            .tabBarBg = "#21252e",
            .tabActiveBg = "#2e3440",
            .tabActiveBorder = "#4c566a",
            .tabActiveText = "#88c0d0",
            .tabInactiveText = "#d8dee9",
            .closeTabIcon = ":/icons/close_tab.png",
            .closeTabHover = ":/icons/close_tab_hover.png"
        },
        ThemePalette{
            .id = "dracula",
            .name = "Dracula",
            .category = "Vibrant & Developer",
            .isDark = true,
            .bg = "#1e1f29",
            .surface = "#282a36",
            .surfaceAlt = "#343746",
            .surfaceHover = "#44475a",
            .border = "#44475a",
            .borderLight = "#343746",
            .text = "#f8f8f2",
            .textMuted = "#6272a4",
            .textDim = "#4b567a",
            .accent = "#bd93f9",
            .accentHover = "#d1b2ff",
            .accentPressed = "#9965f4",
            .accentText = "#282a36",
            .selectionBg = "#44475a",
            .selectionFg = "#50fa7b",
            .headerBg = "#21222c",
            .headerText = "#6272a4",
            .scrollHandle = "#44475a",
            .scrollHover = "#6272a4",
            .menuBg = "#282a36",
            .menuHover = "#44475a",
            .menuText = "#f8f8f2",
            .statusBarBg = "#191a21",
            .statusBarFg = "#6272a4",
            .telemetryBg = "#343746",
            .telemetryFg = "#f8f8f2",
            .tabBarBg = "#1b1c24",
            .tabActiveBg = "#282a36",
            .tabActiveBorder = "#6272a4",
            .tabActiveText = "#ff79c6",
            .tabInactiveText = "#6272a4",
            .closeTabIcon = ":/icons/close_tab.png",
            .closeTabHover = ":/icons/close_tab_hover.png"
        },
        ThemePalette{
            .id = "tokyo",
            .name = "Tokyo Night",
            .category = "Vibrant & Developer",
            .isDark = true,
            .bg = "#16161e",
            .surface = "#1a1b26",
            .surfaceAlt = "#24283b",
            .surfaceHover = "#2f354f",
            .border = "#2f354f",
            .borderLight = "#23283b",
            .text = "#c0caf5",
            .textMuted = "#7aa2f7",
            .textDim = "#565f89",
            .accent = "#bb9af7",
            .accentHover = "#cbb0ff",
            .accentPressed = "#9d7cd8",
            .accentText = "#1a1b26",
            .selectionBg = "#283457",
            .selectionFg = "#7dcfff",
            .headerBg = "#181924",
            .headerText = "#7aa2f7",
            .scrollHandle = "#3b4261",
            .scrollHover = "#565f89",
            .menuBg = "#1a1b26",
            .menuHover = "#2f354f",
            .menuText = "#c0caf5",
            .statusBarBg = "#13141c",
            .statusBarFg = "#565f89",
            .telemetryBg = "#24283b",
            .telemetryFg = "#c0caf5",
            .tabBarBg = "#14141b",
            .tabActiveBg = "#1f2335",
            .tabActiveBorder = "#3b4261",
            .tabActiveText = "#7aa2f7",
            .tabInactiveText = "#565f89",
            .closeTabIcon = ":/icons/close_tab.png",
            .closeTabHover = ":/icons/close_tab_hover.png"
        }
    };
    return s_palettes;
}

static QString s_currentThemeId = "obsidian";

const ThemePalette& paletteForId(const QString& id) {
    const auto& list = allPalettes();
    for (const auto& p : list) {
        if (p.id.compare(id, Qt::CaseInsensitive) == 0) {
            return p;
        }
    }
    return list.first(); // fallback to obsidian
}

QString generateStyleSheet(const ThemePalette& p) {
    return QString(R"(
        QMainWindow, QWidget {
            background-color: %1;
            color: %2;
            font-family: "Inter", "Segoe UI", -apple-system, system-ui, sans-serif;
            font-size: 13px;
        }

        /* Splitter */
        QSplitter::handle {
            background-color: %3;
        }
        QSplitter::handle:horizontal {
            width: 1px;
        }
        QSplitter::handle:vertical {
            height: 1px;
        }

        /* Integrated URL Bar Container */
        QFrame#urlBarContainer {
            background-color: %4;
            border: 1px solid %5;
            border-radius: 8px;
            padding: 3px 5px;
        }
        QFrame#urlBarContainer:focus-within {
            border: 1px solid %6;
        }
        QFrame#urlBarDivider {
            background-color: %5;
            width: 1px;
            max-width: 1px;
            margin: 4px 4px;
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
            background-color: %7;
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
            border-top: 5px solid %8;
            margin-right: 4px;
        }

        /* URL LineEdit inside URL Bar */
        QLineEdit#urlEdit {
            background: transparent;
            border: none;
            font-family: "JetBrains Mono", "Cascadia Code", "Fira Code", Consolas, monospace;
            font-size: 13px;
            color: %2;
            padding: 6px 8px;
            selection-background-color: %6;
            selection-color: %9;
        }
        QLineEdit#urlEdit:focus {
            background: transparent;
            border: none;
        }

        /* Standard LineEdit & TextEdit */
        QLineEdit, QTextEdit, QPlainTextEdit {
            background-color: %4;
            color: %2;
            border: 1px solid %5;
            border-radius: 6px;
            padding: 6px 10px;
            selection-background-color: %6;
            selection-color: %9;
        }
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {
            border: 1px solid %6;
            background-color: %10;
        }

        /* ComboBox */
        QComboBox {
            background-color: %4;
            color: %2;
            border: 1px solid %5;
            border-radius: 6px;
            padding: 5px 12px;
            min-height: 22px;
            font-weight: 500;
        }
        QComboBox:hover {
            border-color: %6;
            background-color: %7;
        }
        QComboBox:focus {
            border-color: %6;
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
            border-top: 5px solid %8;
            margin-right: 6px;
        }
        QComboBox QAbstractItemView {
            background-color: %4;
            color: %2;
            border: 1px solid %5;
            border-radius: 6px;
            selection-background-color: %7;
            selection-color: %2;
            outline: none;
            padding: 4px;
        }

        /* Standard PushButton */
        QPushButton {
            background-color: %10;
            color: %2;
            border: 1px solid %5;
            border-radius: 6px;
            padding: 6px 14px;
            font-weight: 500;
            font-size: 12px;
        }
        QPushButton:hover {
            background-color: %7;
            border-color: %6;
            color: %2;
        }
        QPushButton:pressed {
            background-color: %4;
            border-color: %5;
        }
        QPushButton:disabled {
            background-color: %4;
            color: %8;
            border-color: %3;
        }

        /* Primary Action Button (Send button) */
        QPushButton#primaryBtn {
            background-color: %6;
            color: %9;
            border: 1px solid %6;
            border-radius: 6px;
            padding: 7px 18px;
            font-weight: 700;
            font-size: 13px;
        }
        QPushButton#primaryBtn:hover {
            background-color: %11;
            border-color: %11;
            color: %9;
        }
        QPushButton#primaryBtn:pressed {
            background-color: %12;
            border-color: %12;
            color: %9;
        }
        QPushButton#primaryBtn:disabled {
            background-color: %4;
            color: %8;
            border: 1px solid %5;
        }

        /* Tab Widget (Flat Underline Style) */
        QTabWidget::pane {
            border: none;
            border-top: 1px solid %5;
            background-color: transparent;
        }
        QTabBar::tab {
            background-color: transparent;
            color: %8;
            border: none;
            border-bottom: 2px solid transparent;
            padding: 8px 16px;
            font-weight: 500;
            font-size: 12px;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            color: %2;
            border-bottom: 2px solid %6;
            font-weight: 600;
        }
        QTabBar::tab:hover:!selected {
            color: %2;
            background: %7;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
        }

        /* TreeView & TableView */
        QTreeView, QTableView, QTreeWidget, QTableWidget {
            background-color: %4;
            color: %2;
            border: 1px solid %5;
            border-radius: 6px;
            gridline-color: %3;
            selection-background-color: %7;
            selection-color: %2;
            outline: none;
        }
        QTreeView::item, QTableView::item {
            padding: 5px 8px;
            border-radius: 4px;
            min-height: 26px;
        }
        QTreeView::item:hover, QTableView::item:hover {
            background-color: %7;
        }
        QTreeView::item:selected, QTableView::item:selected {
            background-color: %7;
            color: %2;
            font-weight: 500;
        }

        /* Table & Tree Headers */
        QHeaderView::section {
            background-color: %13;
            color: %14;
            border: none;
            border-bottom: 1px solid %5;
            border-right: 1px solid %3;
            padding: 6px 10px;
            font-weight: 600;
            font-size: 11px;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }

        /* Scrollbars (Modern Thin Style) */
        QScrollBar:vertical {
            background: transparent;
            width: 8px;
            margin: 0;
            border: none;
        }
        QScrollBar::handle:vertical {
            background-color: %15;
            min-height: 30px;
            border-radius: 4px;
            margin: 1px;
        }
        QScrollBar::handle:vertical:hover {
            background-color: %16;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QScrollBar:horizontal {
            background: transparent;
            height: 8px;
            margin: 0;
            border: none;
        }
        QScrollBar::handle:horizontal {
            background-color: %15;
            min-width: 30px;
            border-radius: 4px;
            margin: 1px;
        }
        QScrollBar::handle:horizontal:hover {
            background-color: %16;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0;
        }

        /* GroupBox */
        QGroupBox {
            border: 1px solid %5;
            border-radius: 8px;
            margin-top: 14px;
            padding-top: 16px;
            font-weight: 600;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 0 8px;
            color: %8;
            font-size: 12px;
        }

        /* Checkbox & Radio */
        QCheckBox, QRadioButton {
            spacing: 8px;
            color: %2;
            font-size: 12px;
        }
        QCheckBox::indicator, QRadioButton::indicator {
            width: 16px;
            height: 16px;
            border: 1px solid %5;
            border-radius: 4px;
            background-color: %4;
        }
        QCheckBox::indicator:hover, QRadioButton::indicator:hover {
            border-color: %6;
        }
        QCheckBox::indicator:checked {
            background-color: %6;
            border-color: %6;
            image: url(:/icons/check.png);
        }

        /* Menu Bar */
        QMenuBar {
            background-color: %1;
            color: %2;
            border-bottom: 1px solid %3;
            padding: 2px 6px;
        }
        QMenuBar::item {
            padding: 5px 10px;
            border-radius: 4px;
            background: transparent;
        }
        QMenuBar::item:selected {
            background-color: %7;
            color: %2;
        }

        /* Context Menus */
        QMenu {
            background-color: %17;
            color: %18;
            border: 1px solid %5;
            border-radius: 8px;
            padding: 4px;
        }
        QMenu::item {
            padding: 6px 24px 6px 12px;
            border-radius: 4px;
        }
        QMenu::item:selected {
            background-color: %7;
            color: %2;
        }
        QMenu::separator {
            height: 1px;
            background-color: %5;
            margin: 4px 6px;
        }

        /* Tooltip */
        QToolTip {
            background-color: %4;
            color: %2;
            border: 1px solid %5;
            border-radius: 6px;
            padding: 6px 10px;
            font-size: 12px;
        }

        /* Status Bar */
        QStatusBar {
            background-color: %19;
            color: %20;
            border-top: 1px solid %3;
            font-size: 12px;
        }

        /* Session Telemetry chip button */
        QPushButton#sessionTelemetryBtn {
            border: 1px solid %5;
            border-radius: 4px;
            padding: 2px 10px;
            font-size: 11px;
            background-color: %21;
            color: %22;
        }
        QPushButton#sessionTelemetryBtn:hover {
            background-color: %7;
            color: %2;
            border-color: %6;
        }
    )")
    .arg(p.bg, p.text, p.borderLight, p.surface, p.border)       // %1-%5
    .arg(p.accent, p.surfaceHover, p.textMuted, p.accentText, p.surfaceAlt) // %6-%10
    .arg(p.accentHover, p.accentPressed, p.headerBg, p.headerText, p.scrollHandle) // %11-%15
    .arg(p.scrollHover, p.menuBg, p.menuText, p.statusBarBg, p.statusBarFg) // %16-%20
    .arg(p.telemetryBg, p.telemetryFg);                          // %21-%22
}

} // namespace

QList<ThemeInfo> Theme::availableThemes() {
    QList<ThemeInfo> list;
    for (const auto& p : allPalettes()) {
        list.append(ThemeInfo{
            .id = p.id,
            .name = p.name,
            .category = p.category,
            .isDark = p.isDark,
            .previewAccent = p.accent,
            .previewBg = p.bg
        });
    }
    return list;
}

QString Theme::currentThemeId() {
    return s_currentThemeId;
}

QString Theme::currentThemeName() {
    return paletteForId(s_currentThemeId).name;
}

void Theme::setTheme(const QString& id) {
    for (const auto& p : allPalettes()) {
        if (p.id.compare(id, Qt::CaseInsensitive) == 0) {
            s_currentThemeId = p.id;
            return;
        }
    }
    s_currentThemeId = "obsidian";
}

QString Theme::currentStyleSheet() {
    return styleSheetForTheme(s_currentThemeId);
}

QString Theme::styleSheetForTheme(const QString& id) {
    const auto& p = paletteForId(id);
    return generateStyleSheet(p);
}

bool Theme::isDarkMode() {
    return paletteForId(s_currentThemeId).isDark;
}

void Theme::setDarkMode(bool dark) {
    if (dark) {
        if (!isDarkMode()) setTheme("obsidian");
    } else {
        if (isDarkMode()) setTheme("snow");
    }
}

bool Theme::toggleTheme() {
    if (isDarkMode()) {
        setTheme("snow");
    } else {
        setTheme("obsidian");
    }
    return isDarkMode();
}

QString Theme::themeTabBarStylesheet() {
    const auto& p = paletteForId(s_currentThemeId);
    return QString(
        "QTabBar { background: %1; border-bottom: 1px solid %2; qproperty-drawBase: 0; }"
        "QTabBar::tab {"
        "    background: transparent;"
        "    color: %3;"
        "    padding: 6px 12px 6px 14px;"
        "    margin-right: 3px;"
        "    margin-top: 2px;"
        "    border-top-left-radius: 6px;"
        "    border-top-right-radius: 6px;"
        "    border: 1px solid transparent;"
        "    border-bottom: 2px solid transparent;"
        "    font-size: 12px;"
        "    font-weight: 500;"
        "    min-width: 80px;"
        "    max-width: 220px;"
        "}"
        "QTabBar::tab:hover:!selected {"
        "    background: %4;"
        "    color: %5;"
        "    border: 1px solid %2;"
        "    border-bottom: 2px solid transparent;"
        "}"
        "QTabBar::tab:selected {"
        "    background: %6;"
        "    color: %7;"
        "    font-weight: 600;"
        "    border: 1px solid %8;"
        "    border-bottom: 2px solid %9;"
        "}"
        "QTabBar::close-button {"
        "    image: url(%10);"
        "    subcontrol-position: right;"
        "    subcontrol-origin: padding;"
        "    margin-left: 8px;"
        "    margin-right: 2px;"
        "    padding: 3px;"
        "    border-radius: 4px;"
        "    background: transparent;"
        "}"
        "QTabBar::close-button:hover {"
        "    image: url(%11);"
        "    background: rgba(255, 255, 255, 0.12);"
        "}"
        "QTabBar::close-button:pressed {"
        "    background: rgba(255, 255, 255, 0.22);"
        "}"
    ).arg(p.tabBarBg, p.border, p.tabInactiveText, p.surfaceHover, p.text,
         p.tabActiveBg, p.tabActiveText, p.tabActiveBorder, p.accent,
         p.closeTabIcon, p.closeTabHover);
}

QString Theme::themeRequestLabelColor() {
    return paletteForId(s_currentThemeId).text;
}

QString Theme::darkStyleSheet() {
    return styleSheetForTheme("obsidian");
}

QString Theme::lightStyleSheet() {
    return styleSheetForTheme("snow");
}

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

} // namespace poppy::gui
