#include "custom_plot.h"

#include <QPainter>
#include <QPaintEvent>
#include <QFontMetrics>
#include <cmath>
#include <limits>

Custom_Plot::Custom_Plot(const QString &title, const QString &unit, QWidget *parent)
    : QWidget{parent}, m_title(title), m_unit(unit)
{
    setMinimumHeight(160);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAutoFillBackground(false);

    m_clock.start();
}

void Custom_Plot::addSeries(const QString &name, const QColor &color)
{
    Series s;
    s.name  = name;
    s.color = color;
    m_series.append(s);
}

void Custom_Plot::addSample(int series, double value)
{
    if (series < 0 || series >= m_series.size())
        return;

    if (!std::isfinite(value))
        return;

    const double t = m_clock.elapsed() / 1000.0;

    Series &s = m_series[series];
    s.points.append(QPointF(t, value));
    s.last    = value;
    s.hasLast = true;
}

void Custom_Plot::commit()
{
    prune(m_clock.elapsed() / 1000.0);
    update();
}

void Custom_Plot::clearData()
{
    for (Series &s : m_series)
    {
        s.points.clear();
        s.hasLast = false;
    }
    update();
}

void Custom_Plot::setWindowSeconds(double seconds)
{
    if (seconds < 5.0)
        seconds = 5.0;

    m_windowSec = seconds;
    prune(m_clock.elapsed() / 1000.0);
    update();
}

void Custom_Plot::setSeriesVisible(int series, bool visible)
{
    if (series < 0 || series >= m_series.size())
        return;

    m_series[series].visible = visible;
    update();
}

void Custom_Plot::prune(double now)
{
    const double cutoff = now - m_windowSec;

    for (Series &s : m_series)
    {
        int drop = 0;
        while (drop < s.points.size() && s.points[drop].x() < cutoff)
            ++drop;

        /* Giữ lại 1 điểm trước mép trái để đường vẽ không bị cụt */
        if (drop > 0)
            s.points.remove(0, drop - 1);
    }
}

void Custom_Plot::computeRange(double &yMin, double &yMax) const
{
    yMin =  std::numeric_limits<double>::max();
    yMax = -std::numeric_limits<double>::max();

    bool any = false;

    for (const Series &s : m_series)
    {
        if (!s.visible)
            continue;

        for (const QPointF &p : s.points)
        {
            yMin = std::min(yMin, p.y());
            yMax = std::max(yMax, p.y());
            any  = true;
        }
    }

    if (!any)
    {
        yMin = 0.0;
        yMax = 1.0;
        return;
    }

    if (std::fabs(yMax - yMin) < 1e-12)
    {
        /* Đường phẳng: mở ra một dải quanh giá trị đó */
        const double pad = (std::fabs(yMax) > 1e-9) ? std::fabs(yMax) * 0.1 : 1.0;
        yMin -= pad;
        yMax += pad;
        return;
    }

    const double pad = (yMax - yMin) * 0.08;
    yMin -= pad;
    yMax += pad;
}

void Custom_Plot::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect full = rect();

    const QRect plotArea(MARGIN_LEFT,
                         MARGIN_TOP,
                         full.width()  - MARGIN_LEFT - MARGIN_RIGHT,
                         full.height() - MARGIN_TOP  - MARGIN_BOTTOM);

    if (plotArea.width() < 20 || plotArea.height() < 20)
        return;

    /* --- Nền --- */
    p.fillRect(full, QColor(0xFA, 0xFA, 0xFA));
    p.fillRect(plotArea, Qt::white);

    /* --- Tiêu đề ---
     * Đo bề rộng thật của title để biết legend được phép bắt đầu từ đâu. */
    QFont titleFont = p.font();
    titleFont.setBold(true);
    p.setFont(titleFont);

    const QString titleText =
        m_title + QStringLiteral(" [") + m_unit + QLatin1Char(']');

    const int titleWidth = QFontMetrics(titleFont).horizontalAdvance(titleText);

    p.setPen(QColor(0x37, 0x47, 0x4F));
    p.drawText(QRect(MARGIN_LEFT, 2, titleWidth, MARGIN_TOP - 4),
               Qt::AlignLeft | Qt::AlignVCenter,
               titleText);

    const int titleRight = MARGIN_LEFT + titleWidth;

    QFont small = p.font();
    small.setBold(false);
    small.setPointSizeF(qMax(6.5, small.pointSizeF() - 1.5));
    p.setFont(small);

    double yMin, yMax;
    computeRange(yMin, yMax);

    const double ySpan = yMax - yMin;
    const double now   = m_clock.elapsed() / 1000.0;
    const double tMin  = now - m_windowSec;

    auto mapX = [&](double t) {
        return plotArea.left()
        + (t - tMin) / m_windowSec * plotArea.width();
    };
    auto mapY = [&](double v) {
        return plotArea.bottom()
        - (v - yMin) / ySpan * plotArea.height();
    };

    /* --- Lưới ngang + nhãn trục Y --- */
    const int H_LINES = 4;
    p.setPen(QPen(QColor(0xE0, 0xE0, 0xE0), 1));

    for (int i = 0; i <= H_LINES; ++i)
    {
        const double v = yMin + ySpan * i / H_LINES;
        const int    y = static_cast<int>(std::lround(mapY(v)));

        p.setPen(QPen(QColor(0xE8, 0xE8, 0xE8), 1));
        p.drawLine(plotArea.left(), y, plotArea.right(), y);

        p.setPen(QColor(0x75, 0x75, 0x75));
        p.drawText(QRect(0, y - 8, MARGIN_LEFT - 6, 16),
                   Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(v, 'g', 4));
    }

    /* --- Lưới dọc + nhãn trục X --- */
    const int V_LINES = 6;
    for (int i = 0; i <= V_LINES; ++i)
    {
        const int x = plotArea.left() + plotArea.width() * i / V_LINES;

        p.setPen(QPen(QColor(0xF0, 0xF0, 0xF0), 1));
        p.drawLine(x, plotArea.top(), x, plotArea.bottom());

        const double rel = -m_windowSec * (V_LINES - i) / V_LINES;

        p.setPen(QColor(0x75, 0x75, 0x75));
        p.drawText(QRect(x - 30, plotArea.bottom() + 4, 60, 16),
                   Qt::AlignHCenter | Qt::AlignTop,
                   QStringLiteral("%1s").arg(rel, 0, 'f', 0));
    }

    /* --- Khung --- */
    p.setPen(QPen(QColor(0xBD, 0xBD, 0xBD), 1));
    p.drawRect(plotArea.adjusted(0, 0, -1, -1));

    /* --- Đường số 0 nếu nằm trong dải --- */
    if (yMin < 0.0 && yMax > 0.0)
    {
        const int y0 = static_cast<int>(std::lround(mapY(0.0)));
        p.setPen(QPen(QColor(0x90, 0xA4, 0xAE), 1, Qt::DashLine));
        p.drawLine(plotArea.left(), y0, plotArea.right(), y0);
    }

    /* --- Các đường dữ liệu --- */
    p.setClipRect(plotArea);

    for (const Series &s : m_series)
    {
        if (!s.visible || s.points.isEmpty())
            continue;

        /* Mới có 1 mẫu: vẽ chấm để thấy ngay là kênh đang có dữ liệu */
        if (s.points.size() == 1)
        {
            const QPointF &pt = s.points.first();
            p.setBrush(s.color);
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(mapX(pt.x()), mapY(pt.y())), 2.0, 2.0);
            p.setBrush(Qt::NoBrush);
            continue;
        }

        QPolygonF poly;
        poly.reserve(s.points.size());

        for (const QPointF &pt : s.points)
            poly.append(QPointF(mapX(pt.x()), mapY(pt.y())));

        p.setPen(QPen(s.color, 1.6));
        p.drawPolyline(poly);
    }

    p.setClipping(false);

    /* --- Chú giải: tên + giá trị hiện tại ---
     * Căn phải và bắt đầu sau title, nên không bao giờ đè lên title
     * dù title dài hay font/DPI thay đổi. */
    const QFontMetrics fm(p.font());

    struct LegendItem { const Series *s; QString text; int width; };

    QVector<LegendItem> items;
    int totalWidth = 0;

    for (const Series &s : m_series)
    {
        if (!s.visible)
            continue;

        const QString text = s.hasLast
                                 ? QStringLiteral("%1 %2").arg(s.name)
                                       .arg(s.last, 0, 'g', 4)
                                 : s.name;

        const int w = LEGEND_LINE + LEGEND_TEXT_GAP
                      + fm.horizontalAdvance(text) + LEGEND_ITEM_GAP;

        items.append({ &s, text, w });
        totalWidth += w;
    }

    /* Không đủ chỗ thì bỏ dần các mục cuối thay vì vẽ tràn ra ngoài. */
    const int available = plotArea.right() - (titleRight + LEGEND_ITEM_GAP);

    while (!items.isEmpty() && totalWidth > available)
    {
        totalWidth -= items.last().width;
        items.removeLast();
    }

    int lx = plotArea.right() - totalWidth;

    const int ly = 2;
    const int lh = MARGIN_TOP - 4;

    for (const LegendItem &it : items)
    {
        p.setPen(QPen(it.s->color, 3));
        p.drawLine(lx, ly + lh / 2, lx + LEGEND_LINE, ly + lh / 2);

        p.setPen(QColor(0x42, 0x42, 0x42));
        p.drawText(QRect(lx + LEGEND_LINE + LEGEND_TEXT_GAP, ly,
                         it.width - LEGEND_LINE - LEGEND_TEXT_GAP, lh),
                   Qt::AlignLeft | Qt::AlignVCenter, it.text);

        lx += it.width;
    }
}
