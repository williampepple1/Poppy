#pragma once

#include <QStyledItemDelegate>
#include <QPainter>
#include <core/RequestModel.h>
#include "Theme.h"

namespace poppy::gui {

class BrunoTreeItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit BrunoTreeItemDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::TextAntialiasing, true);

        const bool isDark = Theme::isDarkMode();
        const bool isSelected = option.state & QStyle::State_Selected;
        const bool isHovered = option.state & QStyle::State_MouseOver;

        // 1. Draw rounded background for selection / hover
        if (isSelected) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(isDark ? "#252730" : "#e0e7ff"));
            QRect bgRect = option.rect.adjusted(2, 1, -2, -1);
            painter->drawRoundedRect(bgRect, 5, 5);

            // Subtle left accent bar on selection
            painter->setBrush(QColor(isDark ? "#f59e0b" : "#d97706"));
            painter->drawRoundedRect(QRect(bgRect.left(), bgRect.top() + 3, 3, bgRect.height() - 6), 1, 1);
        } else if (isHovered) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(isDark ? "#1c1d22" : "#f1f5f9"));
            painter->drawRoundedRect(option.rect.adjusted(2, 1, -2, -1), 5, 5);
        }

        int itemType = index.data(Qt::UserRole + 2).toInt();

        if (itemType == 1) {
            // REQUEST ITEM
            int methodInt = index.data(Qt::UserRole + 1).toInt();
            auto method = static_cast<core::HttpMethod>(methodInt);
            QColor mColor = Theme::methodColor(method);
            QString mStr = core::methodToString(method);
            if (mStr == "DELETE") mStr = "DEL";
            else if (mStr == "OPTIONS") mStr = "OPT";

            // Draw compact pill badge
            const int badgeWidth = 36;
            const int badgeHeight = 16;
            QRect badgeRect(option.rect.left() + 6, option.rect.top() + (option.rect.height() - badgeHeight) / 2, badgeWidth, badgeHeight);

            // Badge background & border
            QColor bgTint(mColor.red(), mColor.green(), mColor.blue(), isDark ? 36 : 28);
            QColor borderTint(mColor.red(), mColor.green(), mColor.blue(), isDark ? 100 : 80);
            painter->setPen(QPen(borderTint, 1));
            painter->setBrush(bgTint);
            painter->drawRoundedRect(badgeRect, 3, 3);

            // Badge text
            QFont badgeFont = painter->font();
            badgeFont.setPixelSize(9);
            badgeFont.setBold(true);
            painter->setFont(badgeFont);
            painter->setPen(mColor);
            painter->drawText(badgeRect, Qt::AlignCenter, mStr);

            // Request Name Text
            const int textLeft = badgeRect.right() + 8;
            const int textWidth = option.rect.right() - textLeft - 4;
            if (textWidth > 0) {
                QRect textRect(textLeft, option.rect.top(), textWidth, option.rect.height());
                QFont nameFont = option.font;
                nameFont.setPixelSize(12);
                nameFont.setBold(isSelected);
                painter->setFont(nameFont);
                painter->setPen(isSelected ? (isDark ? QColor("#ffffff") : QColor("#0f172a"))
                                           : (isDark ? QColor("#e5e7eb") : QColor("#1e293b")));
                QString displayName = index.data(Qt::DisplayRole).toString();
                painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, painter->fontMetrics().elidedText(displayName, Qt::ElideRight, textRect.width()));
            }
        } else if (itemType == 2) {
            // HISTORY ITEM
            int methodInt = index.data(Qt::UserRole + 1).toInt();
            auto method = static_cast<core::HttpMethod>(methodInt);
            QColor mColor = Theme::methodColor(method);
            QString mStr = core::methodToString(method);
            if (mStr == "DELETE") mStr = "DEL";
            else if (mStr == "OPTIONS") mStr = "OPT";
            int statusCode = index.data(Qt::UserRole + 3).toInt();
            qint64 latency = index.data(Qt::UserRole + 4).toLongLong();
            QString urlOrName = index.data(Qt::DisplayRole).toString();

            // Method badge
            QRect mBadge(option.rect.left() + 6, option.rect.top() + 4, 34, 15);
            painter->setPen(QPen(QColor(mColor.red(), mColor.green(), mColor.blue(), 100), 1));
            painter->setBrush(QColor(mColor.red(), mColor.green(), mColor.blue(), isDark ? 36 : 28));
            painter->drawRoundedRect(mBadge, 3, 3);

            QFont bFont = painter->font();
            bFont.setPixelSize(8);
            bFont.setBold(true);
            painter->setFont(bFont);
            painter->setPen(mColor);
            painter->drawText(mBadge, Qt::AlignCenter, mStr);

            // Status code pill
            QColor sColor = Theme::statusColor(statusCode);
            QRect sBadge(mBadge.right() + 4, option.rect.top() + 4, 30, 15);
            painter->setPen(QPen(QColor(sColor.red(), sColor.green(), sColor.blue(), 100), 1));
            painter->setBrush(QColor(sColor.red(), sColor.green(), sColor.blue(), isDark ? 36 : 28));
            painter->drawRoundedRect(sBadge, 3, 3);
            painter->setPen(sColor);
            painter->drawText(sBadge, Qt::AlignCenter, statusCode > 0 ? QString::number(statusCode) : "ERR");

            // Latency
            const int latLeft = sBadge.right() + 6;
            const int latWidth = option.rect.right() - latLeft - 4;
            if (latWidth > 0) {
                QRect latRect(latLeft, option.rect.top() + 4, latWidth, 15);
                QFont latFont = painter->font();
                latFont.setPixelSize(9);
                latFont.setBold(false);
                painter->setFont(latFont);
                painter->setPen(QColor(isDark ? "#9ca3af" : "#64748b"));
                painter->drawText(latRect, Qt::AlignVCenter | Qt::AlignLeft, QString("%1 ms").arg(latency));
            }

            // URL on second line
            const int urlWidth = option.rect.width() - 12;
            if (urlWidth > 0) {
                QRect urlRect(option.rect.left() + 6, option.rect.top() + 21, urlWidth, 16);
                QFont urlFont = option.font;
                urlFont.setPixelSize(11);
                painter->setFont(urlFont);
                painter->setPen(isSelected ? (isDark ? QColor("#ffffff") : QColor("#0f172a"))
                                           : (isDark ? QColor("#d1d5db") : QColor("#334155")));
                painter->drawText(urlRect, Qt::AlignVCenter | Qt::AlignLeft, painter->fontMetrics().elidedText(urlOrName, Qt::ElideMiddle, urlRect.width()));
            }
        } else {
            // FOLDER OR ROOT ITEM
            const int fWidth = option.rect.width() - 8;
            if (fWidth > 0) {
                QRect textRect(option.rect.left() + 4, option.rect.top(), fWidth, option.rect.height());
                QFont fFont = option.font;
                fFont.setPixelSize(12);
                fFont.setBold(true);
                painter->setFont(fFont);
                painter->setPen(isSelected ? (isDark ? QColor("#ffffff") : QColor("#0f172a"))
                                           : (isDark ? QColor("#e5e7eb") : QColor("#1e293b")));
                QString text = index.data(Qt::DisplayRole).toString();
                painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, painter->fontMetrics().elidedText(text, Qt::ElideRight, textRect.width()));
            }
        }

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        int itemType = index.data(Qt::UserRole + 2).toInt();
        if (itemType == 2) {
            return QSize(option.rect.width(), 40); // 2-line row for history
        }
        return QSize(option.rect.width(), 28); // comfortable 28px row height
    }
};

} // namespace poppy::gui
