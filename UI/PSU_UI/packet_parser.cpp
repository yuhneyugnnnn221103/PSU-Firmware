#include "packet_parser.h"
#include "protocol.h"

namespace {

/* Ghép big-endian, tối đa 8 byte */
quint64 readBE(const QByteArray &d, int off, int size)
{
    quint64 v = 0;

    for (int i = 0; i < size; ++i)
        v = (v << 8) | static_cast<quint8>(d[off + i]);

    return v;
}

/* Giải mã 1 trường: ghép BE -> mở rộng dấu theo size -> nhân LSB.
 *
 * Firmware đã sign-extend và dịch bit sẵn, nên ở đây chỉ cần diễn giải
 * `size` byte như một số nguyên có/không dấu đúng bề rộng đó. */
double decodeField(const QByteArray &f, int base,
                   const Protocol::Field &fd, double lsb)
{
    const quint64 raw = readBE(f, base + fd.offset, fd.size);

    if (!fd.isSigned)
        return static_cast<double>(raw) * lsb;

    if (fd.size >= 8)
        return static_cast<double>(static_cast<qint64>(raw)) * lsb;

    const int      bits    = fd.size * 8;
    const quint64  signBit = 1ULL << (bits - 1);

    const qint64 s = (raw & signBit)
                         ? static_cast<qint64>(raw) - static_cast<qint64>(signBit << 1)
                         : static_cast<qint64>(raw);

    return static_cast<double>(s) * lsb;
}

} // namespace

Packet_Parser::Packet_Parser(QObject *parent)
    : QObject{parent}
{
}

void Packet_Parser::reset()
{
    m_buffer.clear();
    m_droppedBytes = 0;
}

void Packet_Parser::processData(const QByteArray &data)
{
    m_buffer.append(data);

    /* Chặn buffer phình vô hạn khi nhận toàn rác */
    if (m_buffer.size() > MAX_BUFFER)
    {
        m_droppedBytes += static_cast<quint64>(m_buffer.size() - Protocol::FRAME_SIZE);
        m_buffer = m_buffer.right(Protocol::FRAME_SIZE);
    }

    QByteArray frame;

    while (findFrame(frame))
    {
        MeasurementPacket packet;

        if (parseFrame(frame, packet))
            emit packetReceived(packet);
    }
}

bool Packet_Parser::findFrame(QByteArray &frame)
{
    static const QByteArray header =
        QByteArray(1, static_cast<char>(Protocol::HEADER1)) +
        QByteArray(1, static_cast<char>(Protocol::HEADER2));

    while (m_buffer.size() >= Protocol::FRAME_SIZE)
    {
        const int idx = m_buffer.indexOf(header);

        if (idx < 0)
        {
            /* Không có header. Giữ lại 1 byte cuối vì nó có thể là HEADER1
             * của một khung bị cắt giữa hai lần readyRead(). */
            m_droppedBytes += static_cast<quint64>(m_buffer.size() - 1);
            m_buffer = m_buffer.right(1);
            return false;
        }

        if (idx > 0)
        {
            m_droppedBytes += static_cast<quint64>(idx);
            m_buffer.remove(0, idx);
        }

        if (m_buffer.size() < Protocol::FRAME_SIZE)
            return false;                       /* chờ thêm byte */

        const bool tailerOk =
            static_cast<quint8>(m_buffer[Protocol::TAILER_OFFSET])     == Protocol::TAILER1 &&
            static_cast<quint8>(m_buffer[Protocol::TAILER_OFFSET + 1]) == Protocol::TAILER2;

        if (!tailerOk)
        {
            /* Header giả. Bỏ đúng 1 byte để không nuốt mất header thật
             * có thể bắt đầu ngay byte kế tiếp. */
            m_buffer.remove(0, 1);
            ++m_droppedBytes;
            emit frameError();
            continue;
        }

        frame = m_buffer.left(Protocol::FRAME_SIZE);
        m_buffer.remove(0, Protocol::FRAME_SIZE);
        return true;
    }

    /* Còn ít hơn 1 khung: nếu đã có header ở đầu thì giữ nguyên chờ thêm */
    return false;
}

void Packet_Parser::parseStatus(quint8 status, SystemStatus &out)
{
    out.raw = status;

    for (int i = 0; i < Protocol::IC_COUNT; ++i)
    {
        out.fresh[i] = status & (1u << i);
        out.fault[i] = status & (1u << (i + Protocol::FT_FAULT_SHIFT));
    }
}

bool Packet_Parser::parseFrame(const QByteArray &frame, MeasurementPacket &packet)
{
    /* Header/tailer đã được findFrame() kiểm tra, không lặp lại. */

    const quint8 cmd = static_cast<quint8>(frame[Protocol::CMD_OFFSET]);

    if (cmd != Protocol::CMD_MONITOR)
    {
        emit frameError();
        return false;
    }

    const quint8 b0 = static_cast<quint8>(frame[Protocol::CRC_OFFSET]);
    const quint8 b1 = static_cast<quint8>(frame[Protocol::CRC_OFFSET + 1]);

    const quint16 receivedCrc = Protocol::CRC_BIG_ENDIAN
                                    ? static_cast<quint16>((b0 << 8) | b1)
                                    : static_cast<quint16>((b1 << 8) | b0);

    bool crcOk = true;

    if (receivedCrc == 0 && Protocol::ACCEPT_ZERO_CRC)
    {
        crcOk = true;                          /* firmware bring-up: CRC = 0 */
    }
    else
    {
        const quint16 calc = Protocol::crc16Ccitt(frame,
                                                  Protocol::CRC_START,
                                                  Protocol::CRC_LENGTH);
        crcOk = (calc == receivedCrc);
    }

    if (!crcOk)
    {
        emit crcError();
        return false;
    }

    packet = MeasurementPacket{};
    packet.cmd      = cmd;
    packet.crc      = receivedCrc;
    packet.crcValid = crcOk;

    parseStatus(static_cast<quint8>(frame[Protocol::STATUS_OFFSET]), packet.status);

    for (int i = 0; i < Protocol::IC_COUNT; ++i)
    {
        const int base = Protocol::IC1_OFFSET + i * Protocol::IC_SIZE;

        parseIna228(frame, base, packet.ic[i]);

        /* Bit fresh trong byte status là căn cứ cuối cùng về tính hợp lệ */
        packet.ic[i].valid = packet.status.fresh[i];
    }

    packet.valid = true;
    return true;
}

void Packet_Parser::parseIna228(const QByteArray &frame, int base, Ina228Data &data) const
{
    using namespace Protocol;

    data.current     = decodeField(frame, base, F_CURRENT, CURRENT_LSB_A);
    data.vbus        = decodeField(frame, base, F_VBUS,    VBUS_LSB_V);
    data.temperature = decodeField(frame, base, F_TEMP,    TEMP_LSB_C);
    // data.power       = decodeField(frame, base, F_POWER,   POWER_LSB_W);
    // data.vshunt      = decodeField(frame, base, F_VSHUNT,  VSHUNT_LSB_V);
    // data.energy      = decodeField(frame, base, F_ENERGY,  ENERGY_LSB_J);
    // data.charge      = decodeField(frame, base, F_CHARGE,  CHARGE_LSB_C);
}