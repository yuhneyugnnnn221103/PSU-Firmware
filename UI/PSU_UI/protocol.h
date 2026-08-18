#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QtGlobal>
#include <QByteArray>

/* ============================================================================
 *  LAYOUT KHUNG TRUYỀN - phải khớp firmware STM32
 * ----------------------------------------------------------------------------
 *  Firmware đã tự sign-extend và dịch bit trước khi gửi, nên PC KHÔNG dịch
 *  bit và KHÔNG mở rộng dấu theo 20/24/40-bit nữa. Mỗi trường chỉ là một số
 *  nguyên big-endian có kích thước cố định:
 *
 *      offset  size  kiểu       nội dung (đã hiệu chỉnh ở firmware)
 *      ------  ----  ---------  ---------------------------------------------
 *        0      4    int32      CURRENT   (20-bit đã sign-extend)
 *        4      4    uint32     VBUS      (20-bit)
 *        8      2    int16      DIETEMP   (16-bit)
 * ========================================================================== */

namespace Protocol
{

/* ---- Khung ---- */
constexpr quint8 HEADER1 = 0xAB;
constexpr quint8 HEADER2 = 0xCD;
constexpr quint8 TAILER1 = 0xE1;
constexpr quint8 TAILER2 = 0xE2;

constexpr quint8 CMD_MONITOR = 0x01;

constexpr int IC_COUNT = 4;

/* ----------------------------------------------------------------------------
 * Mô tả một trường số nguyên big-endian trên đường truyền.
 *   offset   : vị trí byte đầu trong block của 1 IC
 *   size     : số byte truyền (2 / 4 / 8)
 *   isSigned : mở rộng dấu từ bit cao nhất của chính `size` byte đó
 * -------------------------------------------------------------------------- */
struct Field
{
    int  offset;
    int  size;
    bool isSigned;
};

constexpr Field F_CURRENT { 0,  4, true  };   /* int32  */
constexpr Field F_VBUS    { 4,  4, false };   /* uint32 */
constexpr Field F_TEMP    { 8,  2, true  };   /* int16  */
// constexpr Field F_POWER   { 10, 4, false };   /* uint32 */
// constexpr Field F_VSHUNT  { 14, 4, true  };   /* int32  */
// constexpr Field F_ENERGY  { 18, 8, false };   /* uint64 */
// constexpr Field F_CHARGE  { 26, 8, true  };   /* int64  */

constexpr int IC_SIZE = 10;

/* ---- Offset trong khung: dẫn xuất, không hard-code ---- */
constexpr int CMD_OFFSET    = 2;
constexpr int STATUS_OFFSET = 3;
constexpr int IC1_OFFSET    = 4;
constexpr int CRC_OFFSET    = IC1_OFFSET + IC_COUNT * IC_SIZE;
constexpr int CRC_SIZE      = 2;
constexpr int TAILER_OFFSET = CRC_OFFSET + CRC_SIZE;
constexpr int FRAME_SIZE    = TAILER_OFFSET + 2;

constexpr int CRC_START  = CMD_OFFSET;
constexpr int CRC_LENGTH = CRC_OFFSET - CRC_START;

static_assert(F_TEMP.offset + F_TEMP.size == IC_SIZE,
              "Bang Field khong khop IC_SIZE");
static_assert(FRAME_SIZE == 48, "Khung phai dai 48 byte");

/* ---- Byte trạng thái ---- */
constexpr quint8 INA_FRESH_MASK = 0x0F;
constexpr quint8 FT_FAULT_MASK  = 0xF0;
constexpr int    FT_FAULT_SHIFT = 4;

/* ---- Hệ số quy đổi INA228 (ADCRANGE=1, Rshunt=1 mOhm, CURRENT_LSB=78.125 uA) ---- */
constexpr double CURRENT_LSB_A = 78.125e-6;
constexpr double VBUS_LSB_V    = 195.3125e-6;
constexpr double VSHUNT_LSB_V  = 78.125e-9;
constexpr double TEMP_LSB_C    = 7.8125e-3;
constexpr double POWER_LSB_W   = 250.0e-6;     /* 3.2 * CURRENT_LSB */
constexpr double ENERGY_LSB_J  = 4.0e-3;       /* 16 * POWER_LSB    */
constexpr double CHARGE_LSB_C  = CURRENT_LSB_A;

/* ---- Địa chỉ I2C (A1-A0), chỉ dùng để hiển thị ---- */
constexpr quint8 I2C_ADDR[IC_COUNT] = { 0x40, 0x44, 0x41, 0x45 };

/* ---- CRC16-CCITT (init 0xFFFF, poly 0x1021, không reflect) ---- */
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

/* Firmware đang để CRC = 0 trong giai đoạn bring-up.
 * Đặt false ngay khi firmware tính CRC thật. */
constexpr bool ACCEPT_ZERO_CRC = true;

/* CRC truyền big-endian? Đổi thành false nếu firmware gửi little-endian. */
constexpr bool CRC_BIG_ENDIAN = true;


/* ============================================================================
 *  KHUNG LỆNH PC -> STM32 (8 byte)
 * ----------------------------------------------------------------------------
 *      0   0xAB        HEADER1
 *      1   0xCD        HEADER2
 *      2   0x88        CMD_POWER_CTRL
 *      3   0x00/0x01   0 = tắt nguồn, 1 = bật nguồn mạch INA228
 *      4   CRC hi      CRC16-CCITT trên byte [2..3]
 *      5   CRC lo
 *      6   0xE1        TAILER1
 *      7   0xE2        TAILER2
 *
 *  CRC dùng cùng thuật toán và cùng quy ước phạm vi với khung nhận:
 *  bắt đầu từ byte CMD, kết thúc ngay trước ô CRC.
 * ========================================================================== */

constexpr quint8 CMD_POWER_CTRL = 0x88;

constexpr quint8 POWER_DISABLE = 0x00;
constexpr quint8 POWER_ENABLE  = 0x01;

constexpr int CMD_FRAME_SIZE   = 8;
constexpr int CMD_CTRL_OFFSET  = 3;
constexpr int CMD_CRC_OFFSET   = 4;
constexpr int CMD_TAILER_OFFSET = 6;

constexpr int CMD_CRC_START  = 2;
constexpr int CMD_CRC_LENGTH = CMD_CRC_OFFSET - CMD_CRC_START;   /* = 2 */

static_assert(CMD_TAILER_OFFSET + 2 == CMD_FRAME_SIZE,
              "Khung lenh phai dai 8 byte");


inline QByteArray buildPowerControlFrame(bool enable)
{
    QByteArray f(CMD_FRAME_SIZE, '\0');

    f[0] = static_cast<char>(HEADER1);
    f[1] = static_cast<char>(HEADER2);
    f[2] = static_cast<char>(CMD_POWER_CTRL);
    f[CMD_CTRL_OFFSET] =
        static_cast<char>(enable ? POWER_ENABLE : POWER_DISABLE);

    // const quint16 crc = crc16Ccitt(f, CMD_CRC_START, CMD_CRC_LENGTH);
    const quint16 crc = 0;

    if (CRC_BIG_ENDIAN)
    {
        f[CMD_CRC_OFFSET]     = static_cast<char>(crc >> 8);
        f[CMD_CRC_OFFSET + 1] = static_cast<char>(crc & 0xFF);
    }
    else
    {
        f[CMD_CRC_OFFSET]     = static_cast<char>(crc & 0xFF);
        f[CMD_CRC_OFFSET + 1] = static_cast<char>(crc >> 8);
    }

    f[CMD_TAILER_OFFSET]     = static_cast<char>(TAILER1);
    f[CMD_TAILER_OFFSET + 1] = static_cast<char>(TAILER2);

    return f;
}

} // namespace Protocol

#endif // PROTOCOL_H