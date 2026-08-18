#ifndef Ina228_Widget_H
#define Ina228_Widget_H

#include <QWidget>

#include "ina228_data.h"

class QLabel;
class QGroupBox;
class QGridLayout;

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
    void setStale(bool stale);      /* mất liên lạc ở mức đường truyền */

    void clearValues();

private:
    QLabel *addRow(QGridLayout *grid, int row, const QString &name);
    void    refreshBadges();

private:
    int  m_channel = 0;
    bool m_fresh   = false;
    bool m_fault   = false;
    bool m_stale   = true;

    QGroupBox *m_box       = nullptr;
    QLabel    *m_freshLed  = nullptr;
    QLabel    *m_faultLed  = nullptr;

    QLabel *m_current = nullptr;
    QLabel *m_vbus    = nullptr;
    // QLabel *m_power   = nullptr;
    // QLabel *m_vshunt  = nullptr;
    QLabel *m_temp    = nullptr;
    // QLabel *m_energy  = nullptr;
    // QLabel *m_charge  = nullptr;
};

#endif // Ina228_Widget_H
