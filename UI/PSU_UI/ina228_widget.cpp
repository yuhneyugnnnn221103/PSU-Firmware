#include "ina228_widget.h"
#include "protocol.h"

#include <QGroupBox>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QFont>
#include <QStringList>
#include <cmath>

namespace {

const char *LED_ON_GREEN = "background:#2e7d32; color:white; border-radius:7px;"
                           " padding:2px 8px; font-weight:bold;";
const char *LED_ON_RED   = "background:#c62828; color:white; border-radius:7px;"
                           " padding:2px 8px; font-weight:bold;";
const char *LED_OFF      = "background:#9e9e9e; color:#eeeeee; border-radius:7px;"
                           " padding:2px 8px;";

/* Dinh dang gia tri voi tien to SI tu dong */
QString fmt(double v, const QString &unit, int prec = 3)
{
    if (!std::isfinite(v))
        return QStringLiteral("---");

    const double a = std::fabs(v);

    if (a >= 1e3)
        return QStringLiteral("%1 k%2").arg(v / 1e3, 0, 'f', prec).arg(unit);
    if (a >= 1.0 || a == 0.0)
        return QStringLiteral("%1 %2").arg(v, 0, 'f', prec).arg(unit);
    if (a >= 1e-3)
        return QStringLiteral("%1 m%2").arg(v * 1e3, 0, 'f', prec).arg(unit);

    return QStringLiteral("%1 µ%2").arg(v * 1e6, 0, 'f', prec).arg(unit);
}

QString fmtLimit(double v, bool disabled, const QString &unit, int prec)
{
    return disabled ? QStringLiteral("off")
                    : QStringLiteral("%1 %2").arg(v, 0, 'f', prec).arg(unit);
}

QDoubleSpinBox *makeSpin(QWidget *parent, double maxValue,
                         int decimals, const QString &suffix)
{
    auto *sp = new QDoubleSpinBox(parent);

    sp->setRange(0.0, maxValue);
    sp->setDecimals(decimals);
    sp->setSuffix(suffix);
    sp->setSpecialValueText(QObject::tr("off"));   /* 0 = tat canh bao */
    sp->setAlignment(Qt::AlignRight);
    sp->setMinimumWidth(104);

    return sp;
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

    /* Hai cot: so do ben trai, nguong ben phai */
    auto *boxLayout = new QHBoxLayout(m_box);
    boxLayout->setContentsMargins(10, 8, 10, 8);
    boxLayout->setSpacing(10);

    boxLayout->addWidget(buildMeasureColumn(), 3);

    auto *sep = new QFrame(m_box);
    sep->setFrameShape(QFrame::VLine);
    sep->setFrameShadow(QFrame::Sunken);
    boxLayout->addWidget(sep);

    boxLayout->addWidget(buildLimitColumn(), 2);

    clearValues();
    refreshBadges();
    setLimitsEnabled(false);
}

QWidget *Ina228_Widget::buildMeasureColumn()
{
    auto *col = new QWidget(m_box);

    auto *layout = new QVBoxLayout(col);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    /* --- Badge trang thai --- */
    auto *badges = new QHBoxLayout();
    badges->setSpacing(6);

    m_freshLed = new QLabel(tr("FRESH"), col);
    m_faultLed = new QLabel(tr("DCM FAULT"), col);

    badges->addWidget(m_freshLed);
    badges->addWidget(m_faultLed);
    badges->addStretch();

    layout->addLayout(badges);

    /* --- Bang gia tri --- */
    auto *grid = new QGridLayout();
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(4);
    grid->setColumnStretch(1, 1);

    int r = 0;
    m_current = addRow(grid, r++, tr("Current"));
    m_vbus    = addRow(grid, r++, tr("Vbus"));
    m_temp    = addRow(grid, r++, tr("Die temp"));

    QFont big = m_current->font();
    big.setPointSizeF(big.pointSizeF() * 1.35);
    big.setBold(true);
    m_current->setFont(big);
    m_vbus->setFont(big);

    /* Dong giai ma DIAG_ALRT, trai ca hai cot */
    m_diag = new QLabel(QStringLiteral("--"), col);
    m_diag->setWordWrap(true);
    m_diag->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    grid->addWidget(m_diag, r, 0, 1, 2);

    layout->addLayout(grid);
    layout->addStretch();

    return col;
}

QWidget *Ina228_Widget::buildLimitColumn()
{
    auto *col = new QWidget(m_box);

    auto *layout = new QVBoxLayout(col);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto *title = new QLabel(tr("Alert thresholds"), col);
    title->setStyleSheet(QStringLiteral("color:#455a64;font-weight:bold;"));
    layout->addWidget(title);

    auto *grid = new QGridLayout();
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(3);

    m_ocp = makeSpin(col, Protocol::SOVL_MAX_A,  3, QStringLiteral(" A"));
    m_ovp = makeSpin(col, Protocol::BUSVL_MAX_V, 3, QStringLiteral(" V"));
    m_uvp = makeSpin(col, Protocol::BUSVL_MAX_V, 3, QStringLiteral(" V"));

    m_ocp->setToolTip(tr("Overcurrent (SOVL), max %1 A. 0 disables it.")
                          .arg(Protocol::SOVL_MAX_A, 0, 'f', 2));
    m_ovp->setToolTip(tr("Bus overvoltage (BOVL), max %1 V. 0 disables it.")
                          .arg(Protocol::BUSVL_MAX_V, 0, 'f', 1));
    m_uvp->setToolTip(tr("Bus undervoltage (BUVL), max %1 V. 0 disables it.")
                          .arg(Protocol::BUSVL_MAX_V, 0, 'f', 1));

    auto *lblOcp = new QLabel(tr("Over I"),  col);
    auto *lblOvp = new QLabel(tr("Over V"),  col);
    auto *lblUvp = new QLabel(tr("Under V"), col);

    for (QLabel *l : { lblOcp, lblOvp, lblUvp })
        l->setStyleSheet(QStringLiteral("color:#616161;"));

    grid->addWidget(lblOcp, 0, 0);
    grid->addWidget(m_ocp,  0, 1);
    grid->addWidget(lblOvp, 1, 0);
    grid->addWidget(m_ovp,  1, 1);
    grid->addWidget(lblUvp, 2, 0);
    grid->addWidget(m_uvp,  2, 1);
    grid->setColumnStretch(1, 1);

    layout->addLayout(grid);

    auto *buttons = new QHBoxLayout();
    buttons->setSpacing(4);

    m_apply = new QPushButton(tr("Apply"), col);
    m_read  = new QPushButton(tr("Read"),  col);

    buttons->addWidget(m_apply);
    buttons->addWidget(m_read);

    layout->addLayout(buttons);

    m_ackLabel = new QLabel(QStringLiteral("--"), col);
    m_ackLabel->setWordWrap(true);
    m_ackLabel->setStyleSheet(QStringLiteral("color:#757575;"));
    layout->addWidget(m_ackLabel);

    layout->addStretch();

    connect(m_apply, &QPushButton::clicked, this, [this] {
        m_ackLabel->setText(tr("waiting for ACK..."));
        m_ackLabel->setStyleSheet(QStringLiteral("color:#f57c00;"));

        emit setLimitsRequested(m_channel,
                                m_ocp->value(), m_ovp->value(), m_uvp->value());
    });

    connect(m_read, &QPushButton::clicked, this, [this] {
        m_ackLabel->setText(tr("waiting for ACK..."));
        m_ackLabel->setStyleSheet(QStringLiteral("color:#f57c00;"));

        emit getLimitsRequested(m_channel);
    });

    return col;
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

    m_current->setText(fmt(data.current, QStringLiteral("A"), 4));
    m_vbus   ->setText(fmt(data.vbus,    QStringLiteral("V"), 4));

    m_temp->setText(QStringLiteral("%1 °C").arg(data.temperature, 0, 'f', 2));

    const QStringList faults = Protocol::diagFaultNames(data.diag);

    if (faults.isEmpty())
    {
        m_diag->setText(tr("No alert"));
        m_diag->setStyleSheet(QStringLiteral("color:#9e9e9e;"));
    }
    else
    {
        m_diag->setText(faults.join(QStringLiteral(" · ")));
        m_diag->setStyleSheet(QStringLiteral(
            "color:white;background:#c62828;border-radius:4px;"
            "padding:3px 6px;font-weight:bold;"));
    }

    m_diag->setToolTip(QStringLiteral("DIAG_ALRT = 0x%1\n%2")
                           .arg(data.diag, 4, 16, QLatin1Char('0'))
                           .arg(Protocol::diagDescription(data.diag)));
}

void Ina228_Widget::clearValues()
{
    const QString dash = QStringLiteral("---");

    for (QLabel *l : { m_current, m_vbus, m_temp })
        if (l)
            l->setText(dash);

    if (m_diag)
    {
        m_diag->setText(dash);
        m_diag->setStyleSheet(QStringLiteral("color:#9e9e9e;"));
        m_diag->setToolTip(QString());
    }
}

void Ina228_Widget::applyAck(const LimitAck &ack)
{
    if (!ack.ok())
    {
        m_ackLabel->setText(Protocol::ackStatusText(ack.status));
        m_ackLabel->setStyleSheet(QStringLiteral("color:#c62828;font-weight:bold;"));
        return;
    }

    m_ackLabel->setText(tr("OK: %1 / %2 / %3")
                            .arg(fmtLimit(ack.sovlAmp(),  ack.sovlDisabled(),
                                          QStringLiteral("A"), 3))
                            .arg(fmtLimit(ack.bovlVolt(), ack.bovlDisabled(),
                                          QStringLiteral("V"), 3))
                            .arg(fmtLimit(ack.buvlVolt(), ack.buvlDisabled(),
                                          QStringLiteral("V"), 3)));

    m_ackLabel->setStyleSheet(QStringLiteral("color:#2e7d32;"));

    /* Dong bo o nhap ve dung gia tri IC dang giu (da luong tu hoa), de lan
     * Apply sau khong ghi lai con so cu con hien tren man hinh. */
    m_ocp->setValue(ack.sovlDisabled() ? 0.0 : ack.sovlAmp());
    m_ovp->setValue(ack.bovlDisabled() ? 0.0 : ack.bovlVolt());
    m_uvp->setValue(ack.buvlDisabled() ? 0.0 : ack.buvlVolt());
}

void Ina228_Widget::setLimitsEnabled(bool enabled)
{
    m_apply->setEnabled(enabled);
    m_read->setEnabled(enabled);

    if (!enabled)
    {
        m_ackLabel->setText(QStringLiteral("--"));
        m_ackLabel->setStyleSheet(QStringLiteral("color:#757575;"));
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