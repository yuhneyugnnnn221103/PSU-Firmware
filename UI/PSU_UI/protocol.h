#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QtGlobal>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <cstring>

/* ============================================================================
 *  Giao thuc PMON - khop voi comm.h / comm.c cua firmware STM32H725
 *
 *  Moi khung deu co dang:  AB CD | CMD | ... | CRC16(BE) | E1 E2
 *  CRC16-CCITT (init 0xFFFF, poly 0x1021) phu byte [2 .. len-5],
 *  tuc tu CMD den ngay truoc o CRC. Quy tac nay dung cho CA HAI chieu.
 * ========================================================================== */

namespace Protocol
{

constexpr quint8 HEADER1 = 0xAB;
constexpr quint8 HEADER2 = 0xCD;
constexpr quint8 TAILER1 = 0xE1;
constexpr quint8 TAILER2 = 0xE2;

constexpr int IC_COUNT = 4;

/* ---- Ma lenh ---- */
constexpr quint8 CMD_TELEMETRY = 0x01;   /* STM -> PC  */
/* 0x02 (EVENT) da nghi huu - xem CMD_STATUS va CMD_TELEMETRY */

constexpr quint8 CMD_STATUS    = 0x03;

constexpr quint8 CMD_PWR_CTRL  = 0x88;   /* PC  -> STM */
constexpr quint8 CMD_SET_LIMIT = 0x89;   /* PC  -> STM */
constexpr quint8 CMD_LIMIT_ACK = 0x8A;   /* STM -> PC  */
constexpr quint8 CMD_CLR_FAULT = 0x8B;   /* PC  -> STM */
constexpr quint8 CMD_GET_LIMIT = 0x8C;   /* PC  -> STM */

/* ---- Kich thuoc khung ---- */
constexpr int TLM_BLOCK_SIZE  = 12;
constexpr int TLM_PAYLOAD_OFF = 4;
constexpr int TLM_FRAME_SIZE  = TLM_PAYLOAD_OFF + IC_COUNT * TLM_BLOCK_SIZE + 4;

constexpr int AUX_FRAME_SIZE  = 16;   /* 0x02 va 0x8A       */
constexpr int RX_FRAME_SIZE   = 8;    /* 0x88 / 0x8B / 0x8C */
constexpr int CFG_FRAME_SIZE  = 20;   /* 0x89               */

static_assert(TLM_FRAME_SIZE == 56, "Khung telemetry phai dai 56 byte");

/* ---- Telemetry: offset trong 1 block 12 byte ---- */
constexpr int OFF_CURRENT = 0;    /* int32  BE, firmware da sign-extend */
constexpr int OFF_VBUS    = 4;    /* uint32 BE                          */
constexpr int OFF_DIETEMP = 8;    /* int16  BE                          */
constexpr int OFF_DIAG    = 10;   /* uint16 BE: sticky | (alrt & FAULT) */

constexpr int STATUS_OFFSET = 3;

/* ---- LIMIT_ACK (0x8A) ---- */
constexpr int ACK_OFF_CH     = 3;
constexpr int ACK_OFF_STATUS = 4;
constexpr int ACK_OFF_SOVL   = 6;
constexpr int ACK_OFF_BOVL   = 8;
constexpr int ACK_OFF_BUVL   = 10;

/* ---- STATUS (0x03), dung AUX_FRAME_SIZE = 16, khop TLM_STS_OFF_* firmware ---- */
constexpr int STS_OFF_STATE    = 3;   /* bit7..4 = safe_state, bit3..0 = fault_code */
constexpr int STS_OFF_TRIPMASK = 4;
constexpr int STS_OFF_DCM      = 5;
constexpr int STS_OFF_FRESH    = 6;
constexpr int STS_OFF_CFGOK    = 7;
constexpr int STS_OFF_PWR      = 8;
constexpr int STS_OFF_TRIPCNT  = 9;

/** May trang thai bao ve o firmware (safety.h::safe_state_t). */
enum class SafeState : quint8
{
    Boot    = 0,
    Off     = 1,
    Arming  = 2,
    On      = 3,
    Tripped = 4,
    Lockout = 5
};

/** Ma loi gay trip (safety.h::fault_code_t). */
enum class FaultCode : quint8
{
    None      = 0,
    Overcur   = 1,
    Overvolt  = 2,
    Undervolt = 3,
    Overtemp  = 4,
    Dcm       = 5,
    Bus       = 6
};

inline QString safeStateText(SafeState s)
{
    switch (s)
    {
    case SafeState::Boot:    return QStringLiteral("BOOT");
    case SafeState::Off:     return QStringLiteral("OFF");
    case SafeState::Arming:  return QStringLiteral("ARMING");
    case SafeState::On:      return QStringLiteral("ON");
    case SafeState::Tripped: return QStringLiteral("TRIPPED");
    case SafeState::Lockout: return QStringLiteral("LOCKOUT");
    }
    return QStringLiteral("?");
}

inline QString faultCodeText(FaultCode c)
{
    switch (c)
    {
    case FaultCode::None:      return QStringLiteral("--");
    case FaultCode::Overcur:   return QStringLiteral("Qua dong / dong nguoc");
    case FaultCode::Overvolt:  return QStringLiteral("Qua ap bus");
    case FaultCode::Undervolt: return QStringLiteral("Sut ap bus");
    case FaultCode::Overtemp:  return QStringLiteral("Qua nhiet");
    case FaultCode::Dcm:       return QStringLiteral("Loi FT tu DCM");
    case FaultCode::Bus:       return QStringLiteral("Mat lien lac I2C");
    }
    return QStringLiteral("?");
}

constexpr quint8 ACK_OK      = 0;
constexpr quint8 ACK_I2C_ERR = 1;
constexpr quint8 ACK_PARAM   = 2;
constexpr quint8 ACK_NO_DEV  = 3;
constexpr quint8 ACK_REFUSED = 4;   /* tu choi PWR_CTRL(on) vi dang TRIPPED/LOCKOUT */

/** LIMIT_ACK voi channel nay la ACK toan cuc (vd PWR_CTRL bi tu choi),
 *  khong gan voi mot kenh INA228 cu the nao. */
constexpr quint8 ACK_CH_GLOBAL = 0xFF;

/* ---- SET_LIMIT (0x89) ---- */
constexpr int CFG_OFF_CH   = 3;
constexpr int CFG_OFF_SOVL = 4;    /* float32 BE, don vi A */
constexpr int CFG_OFF_BOVL = 8;    /* float32 BE, don vi V */
constexpr int CFG_OFF_BUVL = 12;   /* float32 BE, don vi V */
constexpr quint8 CFG_CH_ALL = 0xFF;

constexpr int RX_OFF_CTRL = 3;

/* ---- Byte trang thai telemetry ---- */
constexpr quint8 ST_FRESH_MASK      = 0x0F;
constexpr int    ST_DCM_FAULT_SHIFT = 4;

/* ---- He so quy doi (ADCRANGE=1, Rshunt = 1 mOhm) ---- */
constexpr double R_SHUNT_OHM   = 0.001;
constexpr double CURRENT_LSB_A = 78.125e-6;
constexpr double VBUS_LSB_V    = 195.3125e-6;
constexpr double TEMP_LSB_C    = 7.8125e-3;

/* Thanh ghi nguong chi 16-bit -> LSB gap 16 lan LSB do luong */
constexpr double SOVL_LSB_V  = 1.25e-6;
constexpr double BUSVL_LSB_V = 3.125e-3;

constexpr quint16 SOVL_DISABLED = 0x7FFF;
constexpr quint16 BOVL_DISABLED = 0x7FFF;
constexpr quint16 BUVL_DISABLED = 0x0000;

constexpr double SOVL_MAX_A  = 32767.0 * SOVL_LSB_V / R_SHUNT_OHM;   /* 40.959 A */
constexpr double BUSVL_MAX_V = 32767.0 * BUSVL_LSB_V;                /* 102.4 V  */

inline double sovlRawToAmp(quint16 raw)
{
    return static_cast<double>(static_cast<qint16>(raw)) * SOVL_LSB_V / R_SHUNT_OHM;
}

inline double busvRawToVolt(quint16 raw)
{
    return static_cast<double>(raw & 0x7FFF) * BUSVL_LSB_V;
}

constexpr quint8 I2C_ADDR[IC_COUNT] = { 0x40, 0x41, 0x44, 0x45 };

/* ============================================================================
 *  DIAG_ALRT (0x0B) - giai ma bit loi
 * ========================================================================== */
constexpr quint16 FLAG_ALATCH    = 1u << 15;
constexpr quint16 FLAG_CNVR      = 1u << 14;
constexpr quint16 FLAG_SLOWALERT = 1u << 13;
constexpr quint16 FLAG_APOL      = 1u << 12;
constexpr quint16 FLAG_ENERGYOF  = 1u << 11;
constexpr quint16 FLAG_CHARGEOF  = 1u << 10;
constexpr quint16 FLAG_MATHOF    = 1u <<  9;
constexpr quint16 FLAG_TMPOL     = 1u <<  7;
constexpr quint16 FLAG_SHNTOL    = 1u <<  6;
constexpr quint16 FLAG_SHNTUL    = 1u <<  5;
constexpr quint16 FLAG_BUSOL     = 1u <<  4;
constexpr quint16 FLAG_BUSUL     = 1u <<  3;
constexpr quint16 FLAG_POL       = 1u <<  2;
constexpr quint16 FLAG_CNVRF     = 1u <<  1;
constexpr quint16 FLAG_MEMSTAT   = 1u <<  0;

constexpr quint16 FAULT_MASK = FLAG_TMPOL | FLAG_SHNTOL | FLAG_SHNTUL |
                               FLAG_BUSOL | FLAG_BUSUL | FLAG_POL | FLAG_MATHOF;

/** Ten ngan cua tung bit loi dang bat, de hien tren the kenh. */
inline QStringList diagFaultNames(quint16 diag)
{
    QStringList out;

    if (diag & FLAG_SHNTOL)   out << QStringLiteral("OVERCURRENT");
    if (diag & FLAG_SHNTUL)   out << QStringLiteral("REVERSE I");
    if (diag & FLAG_BUSOL)    out << QStringLiteral("OVERVOLTAGE");
    if (diag & FLAG_BUSUL)    out << QStringLiteral("UNDERVOLTAGE");
    if (diag & FLAG_TMPOL)    out << QStringLiteral("OVERTEMP");
    if (diag & FLAG_POL)      out << QStringLiteral("POWER LIMIT");
    if (diag & FLAG_MATHOF)   out << QStringLiteral("MATH OVF");
    if (diag & FLAG_ENERGYOF) out << QStringLiteral("ENERGY OVF");
    if (diag & FLAG_CHARGEOF) out << QStringLiteral("CHARGE OVF");

    return out;
}

/** Mo ta day du, dung cho nhat ky su kien. */
inline QString diagDescription(quint16 diag)
{
    struct Item { quint16 bit; const char *text; };

    static const Item items[] = {
                                  { FLAG_SHNTOL,   "Qua dong (SHNTOL) - vuot nguong SOVL" },
                                  { FLAG_SHNTUL,   "Dong nguoc chieu (SHNTUL) - duoi nguong SUVL" },
                                  { FLAG_BUSOL,    "Qua ap bus (BUSOL) - vuot nguong BOVL" },
                                  { FLAG_BUSUL,    "Sut ap bus (BUSUL) - duoi nguong BUVL" },
                                  { FLAG_TMPOL,    "Qua nhiet die (TMPOL)" },
                                  { FLAG_POL,      "Vuot gioi han cong suat (POL)" },
                                  { FLAG_MATHOF,   "Tran phep toan noi bo (MATHOF)" },
                                  { FLAG_ENERGYOF, "Tran thanh ghi ENERGY" },
                                  { FLAG_CHARGEOF, "Tran thanh ghi CHARGE" },
                                  };

    QStringList parts;

    for (const Item &it : items)
        if (diag & it.bit)
            parts << QString::fromUtf8(it.text);

    if (parts.isEmpty())
        return QStringLiteral("Khong co loi");

    return parts.join(QStringLiteral("; "));
}

inline QString ackStatusText(quint8 status)
{
    switch (status)
    {
    case ACK_OK:      return QStringLiteral("OK");
    case ACK_I2C_ERR: return QStringLiteral("I2C ERROR");
    case ACK_PARAM:   return QStringLiteral("BAD PARAM");
    case ACK_NO_DEV:  return QStringLiteral("NO DEVICE");
    case ACK_REFUSED: return QStringLiteral("REFUSED (TRIPPED/LOCKOUT)");
    default:          return QStringLiteral("UNKNOWN (%1)").arg(status);
    }
}

/* ============================================================================
 *  CRC16-CCITT
 * ========================================================================== */
inline quint16 crc16Ccitt(const quint8 *data, int len, quint16 crc = 0xFFFF)
{
    for (int i = 0; i < len; ++i)
    {
        crc ^= static_cast<quint16>(data[i]) << 8;

        for (int b = 0; b < 8; ++b)
            crc = (crc & 0x8000) ? static_cast<quint16>((crc << 1) ^ 0x1021)
                                 : static_cast<quint16>(crc << 1);
    }
    return crc;
}

inline quint16 crc16Ccitt(const QByteArray &data, int offset, int len)
{
    return crc16Ccitt(reinterpret_cast<const quint8 *>(data.constData()) + offset, len);
}

/* Firmware da bat CRC. Chi dat true khi can bring-up voi CRC = 0. */
// constexpr bool ACCEPT_ZERO_CRC = true;
constexpr bool ACCEPT_ZERO_CRC = false;

/** Do dai khung theo byte CMD. Tra 0 neu CMD khong thuoc chieu STM -> PC. */
inline int frameLengthForCmd(quint8 cmd)
{
    switch (cmd)
    {
    case CMD_TELEMETRY: return TLM_FRAME_SIZE;
    case CMD_LIMIT_ACK: return AUX_FRAME_SIZE;
    case CMD_STATUS:    return AUX_FRAME_SIZE;
    default:            return 0;
    }
}

/* ============================================================================
 *  DUNG KHUNG LENH PC -> STM
 * ========================================================================== */
namespace detail {

inline void putBE16(QByteArray &f, int off, quint16 v)
{
    f[off]     = static_cast<char>(v >> 8);
    f[off + 1] = static_cast<char>(v & 0xFF);
}

inline void putBEFloat(QByteArray &f, int off, float v)
{
    quint32 u = 0;
    std::memcpy(&u, &v, sizeof(u));

    f[off]     = static_cast<char>(u >> 24);
    f[off + 1] = static_cast<char>(u >> 16);
    f[off + 2] = static_cast<char>(u >> 8);
    f[off + 3] = static_cast<char>(u);
}

/** Dien header, CMD, CRC va tailer cho khung do dai bat ky. */
inline void seal(QByteArray &f, quint8 cmd)
{
    const int len    = f.size();
    const int crcOff = len - 4;

    f[0] = static_cast<char>(HEADER1);
    f[1] = static_cast<char>(HEADER2);
    f[2] = static_cast<char>(cmd);

    putBE16(f, crcOff, crc16Ccitt(f, 2, crcOff - 2));

    f[len - 2] = static_cast<char>(TAILER1);
    f[len - 1] = static_cast<char>(TAILER2);
}

} // namespace detail

/** 0x88 - bat/tat nguon mach INA228. */
inline QByteArray buildPowerControlFrame(bool enable)
{
    QByteArray f(RX_FRAME_SIZE, '\0');
    f[RX_OFF_CTRL] = static_cast<char>(enable ? 1 : 0);
    detail::seal(f, CMD_PWR_CTRL);
    return f;
}

/**
 * 0x89 - dat nguong canh bao.
 * @param channel 0..3, hoac CFG_CH_ALL cho ca 4 kenh
 * @param sovlA   qua dong, A.   <= 0 = tat canh bao
 * @param bovlV   qua ap bus, V. <= 0 = tat
 * @param buvlV   sut ap bus, V. <= 0 = tat
 */
inline QByteArray buildSetLimitFrame(quint8 channel,
                                     double sovlA, double bovlV, double buvlV)
{
    QByteArray f(CFG_FRAME_SIZE, '\0');

    f[CFG_OFF_CH] = static_cast<char>(channel);

    detail::putBEFloat(f, CFG_OFF_SOVL, static_cast<float>(sovlA));
    detail::putBEFloat(f, CFG_OFF_BOVL, static_cast<float>(bovlV));
    detail::putBEFloat(f, CFG_OFF_BUVL, static_cast<float>(buvlV));

    detail::seal(f, CMD_SET_LIMIT);
    return f;
}

/** 0x8C - doc lai nguong cua mot kenh (hoac CFG_CH_ALL). */
inline QByteArray buildGetLimitFrame(quint8 channel)
{
    QByteArray f(RX_FRAME_SIZE, '\0');
    f[RX_OFF_CTRL] = static_cast<char>(channel);
    detail::seal(f, CMD_GET_LIMIT);
    return f;
}

/** 0x8B - xoa co loi sticky. mask bit0..3 ung voi kenh 1..4. */
inline QByteArray buildClearFaultFrame(quint8 channelMask)
{
    QByteArray f(RX_FRAME_SIZE, '\0');
    f[RX_OFF_CTRL] = static_cast<char>(channelMask & 0x0F);
    detail::seal(f, CMD_CLR_FAULT);
    return f;
}

} // namespace Protocol

#endif // PROTOCOL_H