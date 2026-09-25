#include "NetworkWaterfallWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QToolTip>
#include <QFontMetrics>
#include <algorithm>
#include <cmath>

namespace poppy::gui {

NetworkWaterfallWidget::NetworkWaterfallWidget(QWidget* parent)
    : QWidget(parent) {
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void NetworkWaterfallWidget::clear() {
    m_segments.clear();
    m_totalLatencyMs = 0.0;
    m_hoveredSegmentIndex = -1;
    update();
}

void NetworkWaterfallWidget::setTimings(double dnsMs, double connectMs, double sslMs, double ttfbMs, double totalLatencyMs) {
    m_segments.clear();
    m_totalLatencyMs = std::max(0.0, totalLatencyMs);

    if (m_totalLatencyMs <= 0.0 && (dnsMs > 0 || connectMs > 0 || ttfbMs > 0)) {
        m_totalLatencyMs = std::max({dnsMs, connectMs, sslMs, ttfbMs});
    }

    if (m_totalLatencyMs <= 0.0) {
        update();
        return;
    }

    // Libcurl timing metrics are cumulative timestamps from start of transfer
    double dnsDur = std::max(0.0, dnsMs);
    double tcpDur = std::max(0.0, connectMs > dnsMs ? (connectMs - dnsMs) : connectMs);
    double sslDur = (sslMs > connectMs) ? (sslMs - connectMs) : 0.0;
    double preWait = (sslMs > 0.0) ? sslMs : ((connectMs > 0.0) ? connectMs : dnsMs);
    double waitDur = std::max(0.0, ttfbMs > preWait ? (ttfbMs - preWait) : 0.0);
    double dlDur = std::max(0.0, m_totalLatencyMs > ttfbMs ? (m_totalLatencyMs - ttfbMs) : 0.0);

    // If curl didn't report detailed breakdown but totalLatencyMs is known
    double sumDurs = dnsDur + tcpDur + sslDur + waitDur + dlDur;
    if (sumDurs <= 0.0 && m_totalLatencyMs > 0.0) {
        waitDur = m_totalLatencyMs;
        sumDurs = waitDur;
    }

    auto addSegment = [this, sumDurs](const QString& name, double dur, const QColor& color) {
        if (dur <= 0.001) return;
        TimingSegment seg;
        seg.name = name;
        seg.durationMs = dur;
        seg.percentage = (sumDurs > 0.0) ? (dur / sumDurs * 100.0) : 0.0;
        seg.color = color;
        m_segments.append(seg);
    };

    // Color theme: Cyan, Blue, Purple, Amber, Emerald
    addSegment("DNS Lookup", dnsDur, QColor("#06b6d4"));
    addSegment("TCP Connect", tcpDur, QColor("#3b82f6"));
    addSegment("SSL / TLS", sslDur, QColor("#a855f7"));
    addSegment("TTFB / Server", waitDur, QColor("#f59e0b"));
    addSegment("Download", dlDur, QColor("#10b981"));

    update();
}

QSize NetworkWaterfallWidget::sizeHint() const {
    return QSize(280, 26);
}

QSize NetworkWaterfallWidget::minimumSizeHint() const {
    return QSize(160, 22);
}

void NetworkWaterfallWidget::paintEvent(QPaintEvent* /*event*/) {
    if (m_segments.isEmpty() || m_totalLatencyMs <= 0.0) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int barHeight = 8;
    const int barY = 2;
    const int availableWidth = width();
    const double radius = 4.0;

    // Outer track background
    QRectF trackRect(0, barY, availableWidth, barHeight);
    QPainterPath trackPath;
    trackPath.addRoundedRect(trackRect, radius, radius);
    painter.fillPath(trackPath, QColor(255, 255, 255, 18));

    // Calculate total duration for normalization
    double totalDurs = 0.0;
    for (const auto& s : m_segments) {
        totalDurs += s.durationMs;
    }
    if (totalDurs <= 0.0) return;

    painter.save();
    painter.setClipPath(trackPath);

    double currentX = 0.0;
    for (int i = 0; i < m_segments.size(); ++i) {
        const auto& seg = m_segments[i];
        double segWidth = (seg.durationMs / totalDurs) * availableWidth;
        // Ensure minimal visibility for active phases
        if (segWidth < 3.0 && seg.durationMs > 0.0) segWidth = 3.0;

        QRectF segRect(currentX, barY, segWidth, barHeight);
        QColor col = seg.color;
        if (m_hoveredSegmentIndex == i) {
            col = col.lighter(125);
        }
        painter.fillRect(segRect, col);
        currentX += segWidth;
    }
    painter.restore();

    // Render compact legend dots below the bar
    QFont legendFont = painter.font();
    legendFont.setPointSize(8);
    painter.setFont(legendFont);

    int textX = 2;
    const int textY = barY + barHeight + 11;
    QFontMetrics fm(legendFont);

    for (int i = 0; i < m_segments.size(); ++i) {
        const auto& seg = m_segments[i];
        if (textX + 50 > availableWidth) break;

        // Dot
        painter.setPen(Qt::NoPen);
        painter.setBrush(seg.color);
        painter.drawEllipse(QPointF(textX + 3.0, textY - 3.5), 3.0, 3.0);

        textX += 9;

        // Label
        QString label = QString("%1: %2ms").arg(seg.name).arg(static_cast<int>(std::round(seg.durationMs)));
        painter.setPen(QColor(161, 161, 170));
        painter.drawText(textX, textY, label);

        textX += fm.horizontalAdvance(label) + 8;
    }
}

void NetworkWaterfallWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_segments.isEmpty() || m_totalLatencyMs <= 0.0) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    double totalDurs = 0.0;
    for (const auto& s : m_segments) totalDurs += s.durationMs;
    if (totalDurs <= 0.0) return;

    double availableWidth = width();
    double mouseX = event->position().x();
    double currentX = 0.0;
    int foundIdx = -1;

    for (int i = 0; i < m_segments.size(); ++i) {
        double segWidth = (m_segments[i].durationMs / totalDurs) * availableWidth;
        if (segWidth < 3.0 && m_segments[i].durationMs > 0.0) segWidth = 3.0;

        if (mouseX >= currentX && mouseX <= currentX + segWidth) {
            foundIdx = i;
            break;
        }
        currentX += segWidth;
    }

    if (foundIdx != m_hoveredSegmentIndex) {
        m_hoveredSegmentIndex = foundIdx;
        update();

        if (foundIdx >= 0 && foundIdx < m_segments.size()) {
            const auto& s = m_segments[foundIdx];
            QString tip = QString(
                "<b>%1</b><br/>"
                "Duration: <b>%2 ms</b> (%3% of roundtrip)<br/>"
                "Total Latency: %4 ms"
            ).arg(s.name)
             .arg(s.durationMs, 0, 'f', 1)
             .arg(s.percentage, 0, 'f', 1)
             .arg(m_totalLatencyMs, 0, 'f', 1);
            QToolTip::showText(event->globalPosition().toPoint(), tip, this);
        } else {
            QToolTip::hideText();
        }
    }

    QWidget::mouseMoveEvent(event);
}

void NetworkWaterfallWidget::leaveEvent(QEvent* event) {
    if (m_hoveredSegmentIndex != -1) {
        m_hoveredSegmentIndex = -1;
        update();
        QToolTip::hideText();
    }
    QWidget::leaveEvent(event);
}

} // namespace poppy::gui
