#include "serial_manager.h"

#include <QSerialPortInfo>

Serial_Manager::Serial_Manager(QObject *parent)
    : QObject{parent}
{
    connect(&m_serial, &QSerialPort::readyRead,
            this, &Serial_Manager::onReadyRead);

    connect(&m_serial, &QSerialPort::errorOccurred,
            this, &Serial_Manager::onError);
}

Serial_Manager::~Serial_Manager()
{
    if (m_serial.isOpen())
        m_serial.close();          /* không emit signal trong destructor */
}

bool Serial_Manager::open(const QString &portName, qint32 baudRate)
{
    if (m_serial.isOpen())
        close();

    m_serial.setPortName(portName);
    m_serial.setBaudRate(baudRate);
    m_serial.setDataBits(QSerialPort::Data8);
    m_serial.setParity(QSerialPort::NoParity);
    m_serial.setStopBits(QSerialPort::OneStop);
    m_serial.setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serial.open(QIODevice::ReadWrite))
    {
        emit errorOccurred(tr("%1: %2").arg(portName, m_serial.errorString()));
        return false;
    }

    /* Xoá rác còn tồn trong driver buffer trước khi bắt đầu đồng bộ khung */
    m_serial.clear(QSerialPort::AllDirections);

    /* FT232R: kéo DTR/RTS lên để tương thích các thiết kế dùng chúng làm enable */
    m_serial.setDataTerminalReady(true);
    m_serial.setRequestToSend(true);

    emit connected();
    return true;
}

void Serial_Manager::close()
{
    if (!m_serial.isOpen())
        return;

    m_serial.close();
    emit disconnected();
}

bool Serial_Manager::isOpen() const
{
    return m_serial.isOpen();
}

QString Serial_Manager::portName() const
{
    return m_serial.portName();
}

qint64 Serial_Manager::write(const QByteArray &data)
{
    if (!m_serial.isOpen())
        return -1;

    return m_serial.write(data);
}

QVector<QPair<QString, QString>> Serial_Manager::availablePorts()
{
    QVector<QPair<QString, QString>> list;

    const auto infos = QSerialPortInfo::availablePorts();

    for (const QSerialPortInfo &info : infos)
    {
        QString label = info.portName();

        if (!info.description().isEmpty())
            label += QStringLiteral(" - ") + info.description();

        list.append(qMakePair(info.portName(), label));
    }

    return list;
}

void Serial_Manager::onReadyRead()
{
    const QByteArray data = m_serial.readAll();

    if (!data.isEmpty())
        emit dataReceived(data);
}

void Serial_Manager::onError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError)
        return;

    emit errorOccurred(m_serial.errorString());

    /* Lỗi nghiêm trọng (rút cáp USB, driver biến mất) -> phải đóng cổng,
     * nếu không isOpen() vẫn trả true và UI kẹt ở trạng thái "Connected". */
    switch (error)
    {
    case QSerialPort::ResourceError:
    case QSerialPort::DeviceNotFoundError:
    case QSerialPort::PermissionError:
    case QSerialPort::UnsupportedOperationError:
        close();
        break;
    default:
        break;
    }
}