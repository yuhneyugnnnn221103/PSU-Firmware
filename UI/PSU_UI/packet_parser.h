#ifndef PACKET_PARSER_H
#define PACKET_PARSER_H

#include <QObject>
#include <QByteArray>

#include "ina228_data.h"

class Packet_Parser : public QObject
{
    Q_OBJECT
public:
    explicit Packet_Parser(QObject *parent = nullptr);

    /* Số byte bị bỏ do mất đồng bộ, hữu ích khi debug đường truyền */
    quint64 droppedBytes() const { return m_droppedBytes; }

public slots:
    void processData(const QByteArray &data);
    void reset();                       /* gọi khi mở/đóng cổng */

signals:
    void packetReceived(const MeasurementPacket &packet);

    void crcError();
    void frameError();

private:
    bool findFrame(QByteArray &frame);
    bool parseFrame(const QByteArray &frame, MeasurementPacket &packet);
    void parseIna228(const QByteArray &frame, int base, Ina228Data &data) const;
    static void parseStatus(quint8 status, SystemStatus &out);

private:
    QByteArray m_buffer;
    quint64    m_droppedBytes = 0;

    static constexpr int MAX_BUFFER = 64 * 1024;
};

#endif // PACKET_PARSER_H