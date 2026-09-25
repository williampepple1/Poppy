#pragma once

#include <QWidget>
#include <QString>
#include <QList>
#include <QColor>

namespace poppy::gui {

struct TimingSegment {
    QString name;
    double durationMs{0.0};
    double percentage{0.0};
    QColor color;
};

class NetworkWaterfallWidget : public QWidget {
    Q_OBJECT
public:
    explicit NetworkWaterfallWidget(QWidget* parent = nullptr);

    void setTimings(double dnsMs, double connectMs, double sslMs, double ttfbMs, double totalLatencyMs);
    void clear();
    bool hasData() const { return !m_segments.isEmpty() && m_totalLatencyMs > 0.0; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    QList<TimingSegment> m_segments;
    double m_totalLatencyMs{0.0};
    int m_hoveredSegmentIndex{-1};
};

} // namespace poppy::gui
