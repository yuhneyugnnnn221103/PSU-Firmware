#ifndef SERIAL_MANAGER_H
#define SERIAL_MANAGER_H

#include <QObject>
#include <QSerialPort>
#include <QVector>
#include <QPair>
#include <QString>

class Serial_Manager : public QObject
{
    Q_OBJECT
public:
    explicit Serial_Manager(QObject *parent = nullptr);
    ~Serial_Manager() override;

    bool open(const QString &portName, qint32 baudRate);
    void close();
    bool isOpen() const;

    QString portName() const;

    qint64 write(const QByteArray &data);

    /* first = tên hệ thống (COM3 / ttyUSB0), second = mô tả để hiển thị */
    static QVector<QPair<QString, QString>> availablePorts();

signals:
    void dataReceived(const QByteArray &data);

    void connected();
    void disconnected();

    void errorOccurred(const QString &error);

private slots:
    void onReadyRead();
    void onError(QSerialPort::SerialPortError error);

private:
    QSerialPort m_serial;
};

#endif // SERIAL_MANAGER_H