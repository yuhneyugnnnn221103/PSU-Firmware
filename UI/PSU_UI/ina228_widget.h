#ifndef INA228_WIDGET_H
#define INA228_WIDGET_H

#include <QWidget>

#include "ina228_data.h"

class QLabel;
class QGroupBox;
class QGridLayout;
class QPushButton;
class QDoubleSpinBox;

/**
 * The hien thi mot kenh INA228.
 *
 * Bo cuc hai cot:
 *   - trai : badge trang thai, Current / Vbus / Die temp, dong giai ma DIAG
 *   - phai : nguong canh bao (qua dong / qua ap / thap ap) + Apply, Read
 *
 * Widget khong tu gui lenh; no phat signal de MainWindow chuyen xuong
 * Measurement_Controller. Ket qua ACK quay ve qua applyAck().
 */
class Ina228_Widget : public QWidget
{
    Q_OBJECT
public:
    explicit Ina228_Widget(int channel, QWidget *parent = nullptr);

    int channel() const { return m_channel; }

public slots:
    void setData(const Ina228Data &data);

    void setFresh(bool fresh);
    void setFault(bool fault);
    void setStale(bool stale);            /* mat lien lac o muc duong truyen */

    void applyAck(const LimitAck &ack);
    void setLimitsEnabled(bool enabled);

    void clearValues();

signals:
    void setLimitsRequested(int channel, double sovlA, double bovlV, double buvlV);
    void getLimitsRequested(int channel);

private:
    QWidget *buildMeasureColumn();
    QWidget *buildLimitColumn();
    QLabel  *addRow(QGridLayout *grid, int row, const QString &name);
    void     refreshBadges();

private:
    int  m_channel = 0;
    bool m_fresh   = false;
    bool m_fault   = false;
    bool m_stale   = true;

    QGroupBox *m_box      = nullptr;
    QLabel    *m_freshLed = nullptr;
    QLabel    *m_faultLed = nullptr;

    /* Cot trai. m_diag hien ten cac bit loi giai ma tu DIAG_ALRT, chiem
     * ca hai cot cua luoi nen khong tao qua addRow(). */
    QLabel *m_current = nullptr;
    QLabel *m_vbus    = nullptr;
    QLabel *m_temp    = nullptr;
    QLabel *m_diag    = nullptr;

    /* Cot phai */
    QDoubleSpinBox *m_ocp   = nullptr;
    QDoubleSpinBox *m_ovp   = nullptr;
    QDoubleSpinBox *m_uvp   = nullptr;
    QPushButton    *m_apply = nullptr;
    QPushButton    *m_read  = nullptr;
    QLabel         *m_ackLabel = nullptr;
};

#endif // INA228_WIDGET_H