#include "csv_logger.h"

#include <QDateTime>
#include <QFileInfo>
#include <QDir>

namespace {

/* Số thực dạng locale C, 9 chữ số có nghĩa. */
inline QString num(double v)
{
    return QString::number(v, 'g', 9);
}

} // namespace

Csv_Logger::Csv_Logger(QObject *parent)
    : QObject{parent}
{
}

Csv_Logger::~Csv_Logger()
{
    if (m_file.isOpen())
    {
        m_stream.flush();
        m_file.close();
    }
}

QString Csv_Logger::suggestedFileName()
{
    return QStringLiteral("ina228_log_%1.xlsx")
    .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
}

bool Csv_Logger::start(const QString &filePath)
{
    if (m_file.isOpen())
        stop();

    m_file.setFileName(filePath);

    /* Truncate: mỗi phiên log là một file riêng. */
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
        emit errorOccurred(tr("Cannot open log file %1: %2")
                               .arg(filePath, m_file.errorString()));
        return false;
    }

    m_stream.setDevice(&m_file);
    m_stream.setRealNumberNotation(QTextStream::SmartNotation);

    m_rows = 0;
    m_clock.start();

    writeHeader();

    emit loggingStateChanged(true);
    return true;
}

void Csv_Logger::stop()
{
    if (!m_file.isOpen())
        return;

    m_stream.flush();
    m_file.close();

    emit loggingStateChanged(false);
}

void Csv_Logger::writeHeader()
{
    m_stream << "timestamp,elapsed_s,status_raw";

    for (int i = 1; i <= Protocol::IC_COUNT; ++i)
    {
        m_stream << ",ic" << i << "_fresh"
                 << ",ic" << i << "_fault"
                 << ",ic" << i << "_current_A"
                 << ",ic" << i << "_vbus_V"
                 << ",ic" << i << "_temp_C"
                 << ",ic" << i << "_diag_hex"
                 << ",ic" << i << "_faults";
    }

    m_stream << '\n';
}

void Csv_Logger::logPacket(const MeasurementPacket &packet)
{
    if (!m_file.isOpen() || !packet.valid)
        return;

    m_stream << QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
             << ',' << num(m_clock.elapsed() / 1000.0)
             << ',' << static_cast<int>(packet.status.raw);

    for (int i = 0; i < Protocol::IC_COUNT; ++i)
    {
        const Ina228Data &d = packet.ic[i];

        m_stream << ',' << (packet.status.fresh[i] ? 1 : 0)
                 << ',' << (packet.status.fault[i] ? 1 : 0);

        /* Kênh không fresh: ghi ô rỗng thay vì số cũ, để phân tích sau
         * không nhầm dữ liệu treo thành dữ liệu thật. */
        if (!d.valid)
        {
            m_stream << ",,,,";
        }
        else
        {
            m_stream << ',' << num(d.current)
            << ',' << num(d.vbus)
            << ',' << num(d.temperature)
            << ",0x" << QString::number(d.diag, 16).rightJustified(4, QLatin1Char('0'))
            << ",\"" << Protocol::diagFaultNames(d.diag).join(QLatin1Char('|')) << '"';
        }
    }

    m_stream << '\n';

    ++m_rows;

    if ((m_rows % FLUSH_EVERY) == 0)
        m_stream.flush();
}