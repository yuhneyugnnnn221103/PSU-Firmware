#include "measurement_controller.h"

Measurement_Controller::Measurement_Controller(QObject *parent)
    : QObject{parent}
{
    connect(&m_serialManager, &Serial_Manager::dataReceived,
            &m_packetParser,  &Packet_Parser::processData);

    connect(&m_packetParser,  &Packet_Parser::packetReceived,
            this,             &Measurement_Controller::onPacketReceived);

    connect(&m_packetParser,  &Packet_Parser::limitAckReceived,
            this,             &Measurement_Controller::onLimitAckReceived);

    connect(&m_packetParser,  &Packet_Parser::statusReceived,
            this,             &Measurement_Controller::onStatusReceived);

    connect(&m_serialManager, &Serial_Manager::connected,
            this,             &Measurement_Controller::onSerialConnected);

    connect(&m_serialManager, &Serial_Manager::disconnected,
            this,             &Measurement_Controller::onSerialDisconnected);

    connect(&m_serialManager, &Serial_Manager::errorOccurred,
            this,             &Measurement_Controller::onSerialError);

    /* Tách riêng hai loại lỗi để thống kê đúng */
    connect(&m_packetParser,  &Packet_Parser::crcError,
            this,             &Measurement_Controller::onCrcError);

    connect(&m_packetParser,  &Packet_Parser::frameError,
            this,             &Measurement_Controller::onFrameError);

    m_watchdog.setInterval(WATCHDOG_PERIOD_MS);
    connect(&m_watchdog, &QTimer::timeout,
            this, &Measurement_Controller::onWatchdogTick);
}

bool Measurement_Controller::openPort(const QString &portName, qint32 baudRate)
{
    if (m_serialManager.isOpen())
        m_serialManager.close();

    m_packetParser.reset();          /* bỏ byte rác của phiên trước */
    resetStats();

    return m_serialManager.open(portName, baudRate);
}

void Measurement_Controller::closePort()
{
    m_serialManager.close();
}

bool Measurement_Controller::isConnected() const
{
    return m_serialManager.isOpen();
}

bool Measurement_Controller::sendFrame(const QByteArray &frame, const QString &what)
{
    if (!m_serialManager.isOpen())
    {
        emit errorOccurred(tr("Port is not open"));
        return false;
    }

    if (m_serialManager.write(frame) != frame.size())
    {
        emit errorOccurred(tr("Failed to send %1").arg(what));
        return false;
    }

    return true;
}

bool Measurement_Controller::sendPowerControl(bool enable)
{
    if (!sendFrame(Protocol::buildPowerControlFrame(enable),
                   tr("power command")))
        return false;

    emit powerCommandSent(enable);
    return true;
}

bool Measurement_Controller::sendSetLimits(quint8 channel,
                                           double sovlA, double bovlV, double buvlV)
{
    return sendFrame(
        Protocol::buildSetLimitFrame(channel, sovlA, bovlV, buvlV),
        tr("limit command"));
}

bool Measurement_Controller::sendGetLimits(quint8 channel)
{
    return sendFrame(Protocol::buildGetLimitFrame(channel),
                     tr("read-limit command"));
}

bool Measurement_Controller::sendClearFault(quint8 channelMask)
{
    return sendFrame(Protocol::buildClearFaultFrame(channelMask),
                     tr("clear-fault command"));
}

void Measurement_Controller::onLimitAckReceived(const LimitAck &ack)
{
    emit limitAckReceived(ack);
}

void Measurement_Controller::onStatusReceived(const StatusPacket &status)
{
    m_safetyStatus = status;
    emit safetyStatusUpdated(m_safetyStatus);
}

const Ina228Data &Measurement_Controller::inaData(int index) const
{
    static const Ina228Data invalidData{};

    if (index < 0 || index >= Protocol::IC_COUNT)
        return invalidData;

    return m_measurement.ic[index];
}

void Measurement_Controller::resetStats()
{
    m_rxFrames    = 0;
    m_validFrames = 0;
    m_crcErrors   = 0;
    m_frameErrors = 0;

    m_framesAtLastTick = 0;
    m_frameRateHz      = 0.0;
    m_measurement      = MeasurementPacket{};
    m_safetyStatus     = StatusPacket{};

    emit communicationStatsChanged();
    emit measurementUpdated(m_measurement);
    emit systemStatusUpdated(m_measurement.status);
    emit safetyStatusUpdated(m_safetyStatus);
}

void Measurement_Controller::onPacketReceived(const MeasurementPacket &packet)
{
    m_measurement = packet;

    ++m_rxFrames;

    if (packet.valid)
        ++m_validFrames;

    m_sinceLastFrame.restart();
    setStale(false);

    emit measurementUpdated(m_measurement);
    emit systemStatusUpdated(m_measurement.status);
    emit communicationStatsChanged();
}

void Measurement_Controller::onSerialConnected()
{
    m_sinceLastFrame.start();
    m_stale = true;                       /* chưa có khung nào */
    m_watchdog.start();

    emit connectionStateChanged(true);
    emit linkStaleChanged(true);
}

void Measurement_Controller::onSerialDisconnected()
{
    m_watchdog.stop();
    setStale(true);

    emit connectionStateChanged(false);
}

void Measurement_Controller::onSerialError(const QString &message)
{
    emit errorOccurred(message);
}

void Measurement_Controller::onCrcError()
{
    ++m_crcErrors;
    emit communicationStatsChanged();
}

void Measurement_Controller::onFrameError()
{
    ++m_frameErrors;
    emit communicationStatsChanged();
}

void Measurement_Controller::onWatchdogTick()
{
    const double dt = WATCHDOG_PERIOD_MS / 1000.0;

    m_frameRateHz      = static_cast<double>(m_rxFrames - m_framesAtLastTick) / dt;
    m_framesAtLastTick = m_rxFrames;

    if (m_sinceLastFrame.isValid() && m_sinceLastFrame.elapsed() > STALE_TIMEOUT_MS)
        setStale(true);

    emit communicationStatsChanged();
}

void Measurement_Controller::setStale(bool stale)
{
    if (m_stale == stale)
        return;

    m_stale = stale;
    emit linkStaleChanged(m_stale);
}