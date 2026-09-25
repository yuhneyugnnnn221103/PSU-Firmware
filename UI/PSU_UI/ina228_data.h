#ifndef INA228_DATA_H
#define INA228_DATA_H

#include <QtGlobal>
#include <QDateTime>

#include "protocol.h"

/* ---- Mot kenh trong khung telemetry (0x01) ---- */
struct Ina228Data
{
    double  current     = 0.0;   /* A    */
    double  vbus        = 0.0;   /* V    */
    double  temperature = 0.0;   /* degC */

    quint16 diag = 0;            /* sticky | (diag_alrt & FAULT_MASK) */

    bool valid = false;          /* = bit fresh trong byte status */

    bool hasFault() const { return (diag & Protocol::FAULT_MASK) != 0; }
};

struct SystemStatus
{
    quint8 raw = 0;

    bool fresh[Protocol::IC_COUNT] = { false, false, false, false };
    bool fault[Protocol::IC_COUNT] = { false, false, false, false };  /* DCM */
};

struct MeasurementPacket
{
    quint8       cmd = 0;
    SystemStatus status;
    Ina228Data   ic[Protocol::IC_COUNT];

    quint16 crc   = 0;
    bool    valid = false;
};

/* ---- Khung STATUS (0x03) ---- */
struct StatusPacket
{
    quint8 raw       = 0;   /* byte STS_OFF_STATE nguyen ban */
    quint8 tripMask  = 0;   /* bit i = kenh i gop phan gay trip lan gan nhat */
    quint8 dcmRaw    = 0;   /* anh chup FT_SECn, CHUA mask theo trang thai nguon */
    quint8 freshMask = 0;   /* bit i = kenh i con tuoi (ina228_is_fresh)     */
    quint8 cfgOkMask = 0;   /* bit i = kenh i cau hinh dung                  */
    bool   pwrOn     = false; /* trang thai LOGIC that su cua PWR_EN         */
    quint8 tripCount = 0;   /* so lan trip lien tiep, >= 3 la LOCKOUT        */

    Protocol::SafeState state() const
    {
        return static_cast<Protocol::SafeState>((raw >> 4) & 0x0F);
    }
    Protocol::FaultCode fault() const
    {
        return static_cast<Protocol::FaultCode>(raw & 0x0F);
    }
};

/* ---- Khung LIMIT_ACK (0x8A) ---- */
struct LimitAck
{
    quint8 channel = 0;
    quint8 status  = Protocol::ACK_OK;

    quint16 sovlRaw = 0;
    quint16 bovlRaw = 0;
    quint16 buvlRaw = 0;

    bool ok() const { return status == Protocol::ACK_OK; }

    /* Gia tri IC thuc su nhan, sau khi luong tu hoa */
    double sovlAmp()  const { return Protocol::sovlRawToAmp(sovlRaw); }
    double bovlVolt() const { return Protocol::busvRawToVolt(bovlRaw); }
    double buvlVolt() const { return Protocol::busvRawToVolt(buvlRaw); }

    bool sovlDisabled() const { return sovlRaw == Protocol::SOVL_DISABLED; }
    bool bovlDisabled() const { return bovlRaw == Protocol::BOVL_DISABLED; }
    bool buvlDisabled() const { return buvlRaw == Protocol::BUVL_DISABLED; }
};

#endif // INA228_DATA_H