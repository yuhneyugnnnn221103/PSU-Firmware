#ifndef INA228_DATA_H
#define INA228_DATA_H

#include <QtGlobal>

#include "protocol.h"

struct Ina228Data
{
    double current     = 0.0;   /* A   */
    double vbus        = 0.0;   /* V   */
    // double vshunt      = 0.0;   /* V   */
    double temperature = 0.0;   /* degC */
    // double power       = 0.0;   /* W   */
    // double energy      = 0.0;   /* J   */
    // double charge      = 0.0;   /* C   */

    bool valid = false;         /* = bit fresh tương ứng trong byte status */
};

struct SystemStatus
{
    quint8 raw = 0;

    bool fresh[Protocol::IC_COUNT] = { false, false, false, false };
    bool fault[Protocol::IC_COUNT] = { false, false, false, false };
};

struct MeasurementPacket
{
    quint8 cmd = 0;

    SystemStatus status;

    Ina228Data ic[Protocol::IC_COUNT];

    quint16 crc      = 0;
    bool    crcValid = false;

    bool valid = false;
};

#endif // INA228_DATA_H