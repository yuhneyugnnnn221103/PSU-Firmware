#ifndef PACKET_PARSER_H
#define PACKET_PARSER_H

#include <QObject>
#include <QByteArray>

#include "ina228_data.h"

/**
 * Tach khung do dai THAY DOI tu dong byte noi tiep.
 *
 * Do dai duoc quyet dinh boi byte CMD (offset 2), giong het may trang thai
 * rx_feed() ben firmware. Vi tri CRC luon la len-4 va CRC phu [2 .. len-5]
 * voi moi loai khung.
 */
class Packet_Parser : public QObject
{
    Q_OBJECT
public:
    explicit Packet_Parser(QObject *parent = nullptr);

    quint64 droppedBytes() const { return m_droppedBytes; }

public slots:
    void processData(const QByteArray &data);
    void reset();

signals:
    void packetReceived(const MeasurementPacket &packet);
    void limitAckReceived(const LimitAck &ack);
    void statusReceived(const StatusPacket &status);

    void crcError();
    void frameError();

private:
    bool findFrame(QByteArray &frame);
    void dispatch(const QByteArray &frame);

    void parseTelemetry(const QByteArray &frame);
    void parseLimitAck(const QByteArray &frame);
    void parseStatus(const QByteArray &frame);

private:
    QByteArray m_buffer;
    quint64    m_droppedBytes = 0;

    static constexpr int MAX_BUFFER = 64 * 1024;
};

#endif // PACKET_PARSER_H