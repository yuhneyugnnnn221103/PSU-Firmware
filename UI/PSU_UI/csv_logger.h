#ifndef CSV_LOGGER_H
#define CSV_LOGGER_H

#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QElapsedTimer>

#include "ina228_data.h"

/**
 * Ghi log CSV từng khung đo.
 *
 * Định dạng số dùng locale C (dấu chấm thập phân) để Excel/pandas/MATLAB
 * đều đọc được, không phụ thuộc locale hệ thống của máy chạy ứng dụng.
 */

class Csv_Logger : public QObject
{
    Q_OBJECT
public:
    explicit Csv_Logger(QObject *parent = nullptr);
    ~Csv_Logger() override;

    bool start(const QString &filePath);
    void stop();

    bool    isLogging() const { return m_file.isOpen(); }
    QString filePath()  const { return m_file.fileName(); }
    quint64 rowCount()  const { return m_rows; }

    /* Tên file gợi ý: ina228_log_YYYYMMDD_HHMMSS.csv */
    static QString suggestedFileName();

public slots:
    void logPacket(const MeasurementPacket &packet);

signals:
    void loggingStateChanged(bool logging);
    void errorOccurred(const QString &message);

private:
    void writeHeader();

private:
    QFile         m_file;
    QTextStream   m_stream;
    QElapsedTimer m_clock;
    quint64       m_rows = 0;

    static constexpr quint64 FLUSH_EVERY = 20;

signals:
};

#endif // CSV_LOGGER_H
