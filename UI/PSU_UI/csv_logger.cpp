#include "csv_logger.h"

#include <QDateTime>
#include <QFileInfo>
#include <QDir>

namespace {

/* Số thực dạng locale C, 5 chữ số có nghĩa. */
inline QString num(double v)
{
    return QString::number(v, 'g', 5);
}

} // namespace

CSV_Logger::CSV_Logger(QObject *parent)
    : QObject{parent}
{
}

CSV_Logger::~CSV_Logger()
{
    if (m_file.isOpen())
    {
        m_stream.flush();
        m_file.close();
    }
}

QString CSV_Logger::suggestedFileName()
{
    return QStringLiteral("ina228_log_%1.csv")
    .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
}

bool CSV_Logger::start(const QString &filePath)
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

void CSV_Logger::stop()
{
    if (!m_file.isOpen())
        return;

    m_stream.flush();
    m_file.close();

    emit loggingStateChanged(false);
}

void CSV_Logger::writeHeader()
{
    m_stream << "timestamp,elapsed_s,status_raw";

    for (int i = 1; i <= Protocol::IC_COUNT; ++i)
    {
        m_stream << ",ic" << i << "_fresh"
                 << ",ic" << i << "_fault"
                 << ",ic" << i << "_current_A"
                 << ",ic" << i << "_vbus_V"
                 // << ",ic" << i << "_vshunt_V"
                 // << ",ic" << i << "_power_W"
                 << ",ic" << i << "_temp_C";
                 // << ",ic" << i << "_energy_J"
                 // << ",ic" << i << "_charge_C";
    }

    m_stream << '\n';
}

void CSV_Logger::logPacket(const MeasurementPacket &packet)
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
            m_stream << ",,,,,,";
        }
        else
        {
            m_stream << ',' << num(d.current)
            << ',' << num(d.vbus)
            // << ',' << num(d.vshunt)
            // << ',' << num(d.power)
             << ',' << num(d.temperature);
            // << ',' << num(d.energy)
            // << ',' << num(d.charge);
        }
    }

    m_stream << '\n';

    ++m_rows;

    if ((m_rows % FLUSH_EVERY) == 0)
        m_stream.flush();
}