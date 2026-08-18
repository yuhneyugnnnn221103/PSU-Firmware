#ifndef MEASUREMENT_CONTROLLER_H
#define MEASUREMENT_CONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>

#include "ina228_data.h"
#include "packet_parser.h"
#include "serial_manager.h"

class Measurement_Controller : public QObject
{
    Q_OBJECT
public:
    explicit Measurement_Controller(QObject *parent = nullptr);
    ~Measurement_Controller() override = default;

    bool openPort(const QString &portName, qint32 baudRate);
    void closePort();
    bool isConnected() const;

    /* Gửi lệnh bật/tắt nguồn mạch INA228 (CMD 0x88).
     * Trả về false nếu cổng chưa mở hoặc ghi không hết khung. */
    bool sendPowerControl(bool enable);

    const MeasurementPacket &measurement() const { return m_measurement; }
    const Ina228Data        &inaData(int index) const;
    const SystemStatus      &systemStatus() const { return m_measurement.status; }

    quint64 rxFrames()    const { return m_rxFrames; }
    quint64 validFrames() const { return m_validFrames; }
    quint64 crcErrors()   const { return m_crcErrors; }
    quint64 frameErrors() const { return m_frameErrors; }
    double  frameRateHz() const { return m_frameRateHz; }
    bool    linkStale()   const { return m_stale; }

signals:
    void measurementUpdated(const MeasurementPacket &packet);
    void systemStatusUpdated(const SystemStatus &status);
    void connectionStateChanged(bool connected);
    void communicationStatsChanged();
    void linkStaleChanged(bool stale);
    void powerCommandSent(bool enable);
    void errorOccurred(const QString &message);

private slots:
    void onPacketReceived(const MeasurementPacket &packet);
    void onSerialConnected();
    void onSerialDisconnected();
    void onSerialError(const QString &message);
    void onCrcError();
    void onFrameError();
    void onWatchdogTick();

private:
    void setStale(bool stale);
    void resetStats();

private:
    Serial_Manager m_serialManager;
    Packet_Parser  m_packetParser;

    MeasurementPacket m_measurement;

    quint64 m_rxFrames    = 0;
    quint64 m_validFrames = 0;
    quint64 m_crcErrors   = 0;
    quint64 m_frameErrors = 0;

    /* Watchdog + đo tốc độ khung */
    QTimer        m_watchdog;
    QElapsedTimer m_sinceLastFrame;
    quint64       m_framesAtLastTick = 0;
    double        m_frameRateHz      = 0.0;
    bool          m_stale            = true;

    static constexpr int  WATCHDOG_PERIOD_MS = 500;
    static constexpr qint64 STALE_TIMEOUT_MS = 2000;
};

#endif // MEASUREMENT_CONTROLLER_H