#include "packet_parser.h"

namespace {

quint64 readBE(const QByteArray &d, int off, int size)
{
    quint64 v = 0;

    for (int i = 0; i < size; ++i)
        v = (v << 8) | static_cast<quint8>(d[off + i]);

    return v;
}

qint32 readBE32Signed(const QByteArray &d, int off)
{
    return static_cast<qint32>(static_cast<quint32>(readBE(d, off, 4)));
}

quint16 readBE16(const QByteArray &d, int off)
{
    return static_cast<quint16>(readBE(d, off, 2));
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

    if (m_buffer.size() > MAX_BUFFER)
    {
        m_droppedBytes += static_cast<quint64>(m_buffer.size() - Protocol::TLM_FRAME_SIZE);
        m_buffer = m_buffer.right(Protocol::TLM_FRAME_SIZE);
    }

    QByteArray frame;

    while (findFrame(frame))
        dispatch(frame);
}

bool Packet_Parser::findFrame(QByteArray &frame)
{
    static const QByteArray header =
        QByteArray(1, static_cast<char>(Protocol::HEADER1)) +
        QByteArray(1, static_cast<char>(Protocol::HEADER2));

    /* Can toi thieu 3 byte moi doc duoc CMD de biet do dai khung. */
    while (m_buffer.size() >= 3)
    {
        const int idx = m_buffer.indexOf(header);

        if (idx < 0)
        {
            /* Giu 1 byte cuoi: co the la HEADER1 cua khung bi cat. */
            m_droppedBytes += static_cast<quint64>(m_buffer.size() - 1);
            m_buffer = m_buffer.right(1);
            return false;
        }

        if (idx > 0)
        {
            m_droppedBytes += static_cast<quint64>(idx);
            m_buffer.remove(0, idx);
        }

        if (m_buffer.size() < 3)
            return false;

        const quint8 cmd = static_cast<quint8>(m_buffer[2]);
        const int    len = Protocol::frameLengthForCmd(cmd);

        if (len == 0)
        {
            /* CMD khong thuoc chieu STM -> PC: header gia. Bo 1 byte. */
            m_buffer.remove(0, 1);
            ++m_droppedBytes;
            emit frameError();
            continue;
        }

        if (m_buffer.size() < len)
            return false;                       /* cho them byte */

        const bool tailerOk =
            static_cast<quint8>(m_buffer[len - 2]) == Protocol::TAILER1 &&
            static_cast<quint8>(m_buffer[len - 1]) == Protocol::TAILER2;

        if (!tailerOk)
        {
            m_buffer.remove(0, 1);
            ++m_droppedBytes;
            emit frameError();
            continue;
        }

        frame = m_buffer.left(len);
        m_buffer.remove(0, len);
        return true;
    }

    return false;
}

void Packet_Parser::dispatch(const QByteArray &frame)
{
    const int len    = frame.size();
    const int crcOff = len - 4;

    const quint16 receivedCrc = readBE16(frame, crcOff);

    if (!(receivedCrc == 0 && Protocol::ACCEPT_ZERO_CRC))
    {
        const quint16 calc = Protocol::crc16Ccitt(frame, 2, crcOff - 2);

        if (calc != receivedCrc)
        {
            emit crcError();
            return;
        }
    }

    switch (static_cast<quint8>(frame[2]))
    {
    case Protocol::CMD_TELEMETRY: parseTelemetry(frame); break;
    case Protocol::CMD_LIMIT_ACK: parseLimitAck(frame);  break;
    case Protocol::CMD_STATUS:    parseStatus(frame);    break;
    default:                      emit frameError();     break;
    }
}

void Packet_Parser::parseTelemetry(const QByteArray &frame)
{
    MeasurementPacket packet;

    packet.cmd = Protocol::CMD_TELEMETRY;
    packet.crc = readBE16(frame, frame.size() - 4);

    const quint8 status = static_cast<quint8>(frame[Protocol::STATUS_OFFSET]);

    packet.status.raw = status;

    for (int i = 0; i < Protocol::IC_COUNT; ++i)
    {
        packet.status.fresh[i] = status & (1u << i);
        packet.status.fault[i] =
            status & (1u << (i + Protocol::ST_DCM_FAULT_SHIFT));

        const int base = Protocol::TLM_PAYLOAD_OFF + i * Protocol::TLM_BLOCK_SIZE;

        Ina228Data &d = packet.ic[i];

        d.current = static_cast<double>(
                        readBE32Signed(frame, base + Protocol::OFF_CURRENT))
                    * Protocol::CURRENT_LSB_A;

        d.vbus = static_cast<double>(readBE(frame, base + Protocol::OFF_VBUS, 4))
                 * Protocol::VBUS_LSB_V;

        d.temperature = static_cast<double>(
                            static_cast<qint16>(
                                readBE16(frame, base + Protocol::OFF_DIETEMP)))
                        * Protocol::TEMP_LSB_C;

        d.diag  = readBE16(frame, base + Protocol::OFF_DIAG);
        d.valid = packet.status.fresh[i];
    }

    packet.valid = true;

    emit packetReceived(packet);
}

void Packet_Parser::parseLimitAck(const QByteArray &frame)
{
    LimitAck ack;

    ack.channel = static_cast<quint8>(frame[Protocol::ACK_OFF_CH]);
    ack.status  = static_cast<quint8>(frame[Protocol::ACK_OFF_STATUS]);

    ack.sovlRaw = readBE16(frame, Protocol::ACK_OFF_SOVL);
    ack.bovlRaw = readBE16(frame, Protocol::ACK_OFF_BOVL);
    ack.buvlRaw = readBE16(frame, Protocol::ACK_OFF_BUVL);

    emit limitAckReceived(ack);
}

void Packet_Parser::parseStatus(const QByteArray &frame)
{
    StatusPacket st;

    st.raw       = static_cast<quint8>(frame[Protocol::STS_OFF_STATE]);
    st.tripMask  = static_cast<quint8>(frame[Protocol::STS_OFF_TRIPMASK]);
    st.dcmRaw    = static_cast<quint8>(frame[Protocol::STS_OFF_DCM]);
    st.freshMask = static_cast<quint8>(frame[Protocol::STS_OFF_FRESH]);
    st.cfgOkMask = static_cast<quint8>(frame[Protocol::STS_OFF_CFGOK]);
    st.pwrOn     = static_cast<quint8>(frame[Protocol::STS_OFF_PWR]) != 0;
    st.tripCount = static_cast<quint8>(frame[Protocol::STS_OFF_TRIPCNT]);

    emit statusReceived(st);
}