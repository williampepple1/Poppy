#pragma once

#include <QString>
#include <QColor>
#include <QList>
#include <core/RequestModel.h>

namespace poppy::gui {

struct ThemeInfo {
    QString id;
    QString name;
    QString category;
    bool isDark{true};
    QString previewAccent;
    QString previewBg;
};

class Theme {
public:
    static QList<ThemeInfo> availableThemes();
    static QString currentThemeId();
    static QString currentThemeName();
    static void setTheme(const QString& id);
    static QString currentStyleSheet();
    static QString styleSheetForTheme(const QString& id);

    static bool isDarkMode();
    static void setDarkMode(bool dark);
    static bool toggleTheme();

    static QString themeTabBarStylesheet();
    static QString themeRequestLabelColor();

    static QColor methodColor(core::HttpMethod method);
    static QString methodBadgeHtml(core::HttpMethod method);
    static QColor statusColor(int statusCode);

    // Backwards compatibility
    static QString darkStyleSheet();
    static QString lightStyleSheet();
};

} // namespace poppy::gui
