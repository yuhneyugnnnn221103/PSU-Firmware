#include "ina228_widget.h"
#include "protocol.h"

#include <QGroupBox>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFont>
#include <cmath>

namespace {

const char *LED_ON_GREEN = "background:#2e7d32; color:white; border-radius:7px;"
                           " padding:2px 8px; font-weight:bold;";
const char *LED_ON_RED   = "background:#c62828; color:white; border-radius:7px;"
                         " padding:2px 8px; font-weight:bold;";
const char *LED_OFF      = "background:#9e9e9e; color:#eeeeee; border-radius:7px;"
                      " padding:2px 8px;";

/* Định dạng giá trị với tiền tố SI tự động (µ / m / đơn vị / k) */
QString fmt(double v, const QString &unit, int prec = 3)
{
    const double a = std::fabs(v);

    if (!std::isfinite(v))
        return QStringLiteral("---");

    if (a >= 1e3)
        return QStringLiteral("%1 k%2").arg(v / 1e3, 0, 'f', prec).arg(unit);
    if (a >= 1.0 || a == 0.0)
        return QStringLiteral("%1 %2").arg(v, 0, 'f', prec).arg(unit);
    if (a >= 1e-3)
        return QStringLiteral("%1 m%2").arg(v * 1e3, 0, 'f', prec).arg(unit);

    return QStringLiteral("%1 µ%2").arg(v * 1e6, 0, 'f', prec).arg(unit);
}

} // namespace

Ina228_Widget::Ina228_Widget(int channel, QWidget *parent)
    : QWidget{parent}, m_channel(channel)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    const quint8 addr = (channel >= 0 && channel < Protocol::IC_COUNT)
                            ? Protocol::I2C_ADDR[channel] : 0;

    m_box = new QGroupBox(tr("INA228 #%1  (0x%2)")
                              .arg(channel + 1)
                              .arg(addr, 2, 16, QLatin1Char('0'))
                              .toUpper(),
                          this);
    outer->addWidget(m_box);

    auto *boxLayout = new QVBoxLayout(m_box);
    boxLayout->setContentsMargins(10, 8, 10, 8);
    boxLayout->setSpacing(6);

    /* --- Hàng badge trạng thái --- */
    auto *badges = new QHBoxLayout();
    badges->setSpacing(6);

    m_freshLed = new QLabel(tr("FRESH"), m_box);
    m_faultLed = new QLabel(tr("DCM FAULT"), m_box);

    badges->addWidget(m_freshLed);
    badges->addWidget(m_faultLed);
    badges->addStretch();

    boxLayout->addLayout(badges);

    /* --- Bảng giá trị --- */
    auto *grid = new QGridLayout();
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(4);
    grid->setColumnStretch(1, 1);

    int r = 0;
    m_current = addRow(grid, r++, tr("Current"));
    m_vbus    = addRow(grid, r++, tr("Vbus"));
    // m_power   = addRow(grid, r++, tr("Power"));
    // m_vshunt  = addRow(grid, r++, tr("Vshunt"));
    m_temp    = addRow(grid, r++, tr("Die temp"));
    // m_energy  = addRow(grid, r++, tr("Energy"));
    // m_charge  = addRow(grid, r++, tr("Charge"));

    /* Ba đại lượng chính để font lớn hơn cho dễ đọc từ xa */
    QFont big = m_current->font();
    big.setPointSizeF(big.pointSizeF() * 1.35);
    big.setBold(true);
    m_current->setFont(big);
    m_vbus->setFont(big);
    m_temp->setFont(big);

    boxLayout->addLayout(grid);
    boxLayout->addStretch();

    clearValues();
    refreshBadges();
}

QLabel *Ina228_Widget::addRow(QGridLayout *grid, int row, const QString &name)
{
    auto *nameLabel = new QLabel(name, m_box);
    nameLabel->setStyleSheet(QStringLiteral("color:#616161;"));

    auto *valueLabel = new QLabel(QStringLiteral("---"), m_box);
    valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    grid->addWidget(nameLabel,  row, 0);
    grid->addWidget(valueLabel, row, 1);

    return valueLabel;
}

void Ina228_Widget::setData(const Ina228Data &data)
{
    if (!data.valid)
    {
        clearValues();
        return;
    }

    m_current->setText(fmt(data.current,     QStringLiteral("A"), 4));
    m_vbus   ->setText(fmt(data.vbus,        QStringLiteral("V"), 4));
    // m_power  ->setText(fmt(data.power,       QStringLiteral("W"), 4));
    // m_vshunt ->setText(fmt(data.vshunt,      QStringLiteral("V"), 3));
    // m_energy ->setText(fmt(data.energy,      QStringLiteral("J"), 3));
    // m_charge ->setText(fmt(data.charge,      QStringLiteral("C"), 3));

    m_temp->setText(QStringLiteral("%1 °C").arg(data.temperature, 0, 'f', 2));
}

void Ina228_Widget::clearValues()
{
    const QString dash = QStringLiteral("---");

    for (QLabel *l : { m_current, m_vbus, m_temp })
    {
        if (l)
            l->setText(dash);
    }
}

void Ina228_Widget::setFresh(bool fresh)
{
    if (m_fresh == fresh)
        return;

    m_fresh = fresh;
    refreshBadges();
}

void Ina228_Widget::setFault(bool fault)
{
    if (m_fault == fault)
        return;

    m_fault = fault;
    refreshBadges();
}

void Ina228_Widget::setStale(bool stale)
{
    if (m_stale == stale)
        return;

    m_stale = stale;

    if (m_stale)
        clearValues();

    refreshBadges();
}

void Ina228_Widget::refreshBadges()
{
    const bool fresh = m_fresh && !m_stale;

    m_freshLed->setStyleSheet(fresh ? LED_ON_GREEN : LED_OFF);
    m_faultLed->setStyleSheet(m_fault ? LED_ON_RED : LED_OFF);

    m_freshLed->setText(m_stale ? tr("NO LINK")
                                : (m_fresh ? tr("FRESH") : tr("STALE")));

    /* Viền đỏ khi có lỗi DCM, xám khi mất liên lạc */
    if (m_stale)
        m_box->setStyleSheet(QStringLiteral("QGroupBox{border:1px solid #bdbdbd;"
                                            "border-radius:6px;margin-top:8px;"
                                            "color:#9e9e9e;}"
                                            "QGroupBox::title{subcontrol-origin:margin;"
                                            "left:10px;padding:0 4px;}"));
    else if (m_fault)
        m_box->setStyleSheet(QStringLiteral("QGroupBox{border:2px solid #c62828;"
                                            "border-radius:6px;margin-top:8px;}"
                                            "QGroupBox::title{subcontrol-origin:margin;"
                                            "left:10px;padding:0 4px;color:#c62828;}"));
    else
        m_box->setStyleSheet(QStringLiteral("QGroupBox{border:1px solid #90a4ae;"
                                            "border-radius:6px;margin-top:8px;}"
                                            "QGroupBox::title{subcontrol-origin:margin;"
                                            "left:10px;padding:0 4px;}"));
}