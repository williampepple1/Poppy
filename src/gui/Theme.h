#pragma once

#include <QString>
#include <QColor>
#include <core/RequestModel.h>

namespace poppy::gui {

class Theme {
public:
    static QString darkStyleSheet();
    static QColor methodColor(core::HttpMethod method);
    static QString methodBadgeHtml(core::HttpMethod method);
    static QColor statusColor(int statusCode);
};

} // namespace poppy::gui
