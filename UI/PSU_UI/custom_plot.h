#ifndef CUSTOM_PLOT_H
#define CUSTOM_PLOT_H

#include <QWidget>
#include <QVector>
#include <QPointF>
#include <QColor>
#include <QElapsedTimer>

class Custom_Plot : public QWidget
{
    Q_OBJECT
public:
    explicit Custom_Plot(const QString &title, const QString &unit, QWidget *parent = nullptr);

    void addSeries(const QString &name, const QColor &color);

    /* Thêm một mẫu; thời điểm lấy từ đồng hồ nội bộ của widget. */
    void addSample(int series, double value);

    /* Gọi một lần sau khi đã nạp đủ mẫu cho tất cả series của một khung,
     * để chỉ vẽ lại màn hình một lần thay vì mỗi series một lần. */
    void commit();

    void clearData();

    void setWindowSeconds(double seconds);
    double windowSeconds() const { return m_windowSec; }

    void setSeriesVisible(int series, bool visible);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct Series
    {
        QString         name;
        QColor          color;
        QVector<QPointF> points;      /* x = giây, y = giá trị */
        bool            visible = true;
        bool            hasLast = false;
        double          last    = 0.0;
    };

    void prune(double now);
    void computeRange(double &yMin, double &yMax) const;

private:
    QString m_title;
    QString m_unit;

    QVector<Series> m_series;

    QElapsedTimer m_clock;
    double        m_windowSec = 60.0;

    static constexpr int MARGIN_LEFT   = 64;
    static constexpr int MARGIN_RIGHT  = 10;
    static constexpr int MARGIN_TOP    = 24;
    static constexpr int MARGIN_BOTTOM = 26;

    /* Kích thước các thành phần của chú giải (legend) */
    static constexpr int LEGEND_LINE     = 12;   /* độ dài đoạn màu       */
    static constexpr int LEGEND_TEXT_GAP = 4;    /* đoạn màu -> chữ       */
    static constexpr int LEGEND_ITEM_GAP = 14;   /* khoảng cách giữa mục  */
};

#endif // CUSTOM_PLOT_H
