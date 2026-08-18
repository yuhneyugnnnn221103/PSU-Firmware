#include "mainwindow.h"

#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QStatusBar>
#include <QTabWidget>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>

namespace {

/* Màu cho 4 kênh, dùng chung cho cả 3 đồ thị */
const QColor CH_COLOR[Protocol::IC_COUNT] = {
    QColor(0x1E, 0x88, 0xE5),   /* xanh dương */
    QColor(0x43, 0xA0, 0x47),   /* xanh lá    */
    QColor(0xFB, 0x8C, 0x00),   /* cam        */
    QColor(0x8E, 0x24, 0xAA)    /* tím        */
};

} //namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
    setupConnections();

    refreshSerialPorts();
    updateConnectionUi(false);
    onLoggingStateChanged(false);
    onCommunicationStatsChanged();
}

/* ---------------------------------------------------------------- UI build */

void MainWindow::setupUi()
{
    setWindowTitle(tr("INA228 Measurement Monitor"));
    resize(1080, 800);

    auto *central = new QWidget(this);
    setCentralWidget(central);

    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    setupTopBar(mainLayout);
    setupTabs(mainLayout);
    setupStatusArea(mainLayout);

    statusBar();      /* tạo sẵn để showMessage() hoạt động */
}

void MainWindow::setupTopBar(QVBoxLayout *mainLayout)
{
    auto *topWidget = new QWidget(this);

    auto *layout = new QHBoxLayout(topWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    m_portCombo = new QComboBox(topWidget);
    m_portCombo->setMinimumWidth(210);

    m_refreshButton = new QPushButton(tr("Refresh"), topWidget);

    m_baudCombo = new QComboBox(topWidget);
    m_baudCombo->setMinimumWidth(100);
    for (int baud : { 115200, 230400, 460800, 921600 })
        m_baudCombo->addItem(QString::number(baud), baud);

    m_connectButton = new QPushButton(tr("Connect"), topWidget);
    m_logButton     = new QPushButton(tr("Start log"), topWidget);

    layout->addWidget(new QLabel(tr("Port:"), topWidget));
    layout->addWidget(m_portCombo);
    layout->addWidget(m_refreshButton);
    layout->addSpacing(10);
    layout->addWidget(new QLabel(tr("Baudrate:"), topWidget));
    layout->addWidget(m_baudCombo);
    layout->addSpacing(10);
    layout->addWidget(m_connectButton);
    layout->addSpacing(10);
    layout->addWidget(m_logButton);
    layout->addSpacing(16);

    setupPowerControls(layout, topWidget);

    layout->addStretch();

    mainLayout->addWidget(topWidget);
}

void MainWindow::setupPowerControls(QHBoxLayout *layout, QWidget *parent)
{
    auto *sep = new QFrame(parent);
    sep->setFrameShape(QFrame::VLine);
    sep->setFrameShadow(QFrame::Sunken);
    layout->addWidget(sep);

    m_powerOnButton  = new QPushButton(tr("Power ON"),  parent);
    m_powerOffButton = new QPushButton(tr("Power OFF"), parent);

    m_powerOnButton->setStyleSheet(
        QStringLiteral("QPushButton{background:#2e7d32;color:white;"
                       "font-weight:bold;padding:4px 14px;border-radius:4px;}"
                       "QPushButton:disabled{background:#c8e6c9;color:#eeeeee;}"));

    m_powerOffButton->setStyleSheet(
        QStringLiteral("QPushButton{background:#c62828;color:white;"
                       "font-weight:bold;padding:4px 14px;border-radius:4px;}"
                       "QPushButton:disabled{background:#ffcdd2;color:#eeeeee;}"));

    m_powerLabel = new QLabel(tr("Power: --"), parent);
    m_powerLabel->setMinimumWidth(96);

    layout->addWidget(new QLabel(tr("INA228 supply:"), parent));
    layout->addWidget(m_powerOnButton);
    layout->addWidget(m_powerOffButton);
    layout->addWidget(m_powerLabel);
}

void MainWindow::setupTabs(QVBoxLayout *mainLayout)
{
    m_tabs = new QTabWidget(this);

    m_tabs->addTab(buildPanelTab(), tr("Panel"));
    m_tabs->addTab(buildTrendTab(), tr("Trends"));

    mainLayout->addWidget(m_tabs, 1);
}

QWidget *MainWindow::buildPanelTab()
{
    auto *page = new QWidget(this);

    auto *grid = new QGridLayout(page);
    grid->setContentsMargins(10, 10, 10, 10);
    grid->setSpacing(10);

    for (int i = 0; i < Protocol::IC_COUNT; ++i)
    {
        m_inaWidgets[i] = new Ina228_Widget(i, page);

        const int row = i / 2;
        const int col = i % 2;

        grid->addWidget(m_inaWidgets[i], row, col);
        grid->setRowStretch(row, 1);
        grid->setColumnStretch(col, 1);
    }

    return page;
}

QWidget *MainWindow::buildTrendTab()
{
    auto *page = new QWidget(this);

    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    /* --- Thanh điều khiển đồ thị --- */
    auto *bar = new QHBoxLayout();
    bar->setSpacing(8);

    m_windowCombo = new QComboBox(page);
    m_windowCombo->addItem(tr("30 s"),  30);
    m_windowCombo->addItem(tr("1 min"), 60);
    m_windowCombo->addItem(tr("5 min"), 300);
    m_windowCombo->addItem(tr("15 min"), 900);
    m_windowCombo->setCurrentIndex(1);

    m_clearPlotButton = new QPushButton(tr("Clear"), page);

    bar->addWidget(new QLabel(tr("Time window:"), page));
    bar->addWidget(m_windowCombo);
    bar->addWidget(m_clearPlotButton);
    bar->addStretch();

    layout->addLayout(bar);

    /* --- Ba đồ thị xếp dọc --- */
    m_currentPlot = new Custom_Plot(tr("Current"), QStringLiteral("A"),  page);
    m_vbusPlot    = new Custom_Plot(tr("Bus voltage"), QStringLiteral("V"), page);
    m_tempPlot    = new Custom_Plot(tr("Die temperature"), QStringLiteral("°C"), page);

    for (int i = 0; i < Protocol::IC_COUNT; ++i)
    {
        const QString name = tr("CH%1").arg(i + 1);

        m_currentPlot->addSeries(name, CH_COLOR[i]);
        m_vbusPlot   ->addSeries(name, CH_COLOR[i]);
        m_tempPlot   ->addSeries(name, CH_COLOR[i]);
    }

    layout->addWidget(m_currentPlot, 1);
    layout->addWidget(m_vbusPlot,    1);
    layout->addWidget(m_tempPlot,    1);

    return page;
}

void MainWindow::setupStatusArea(QVBoxLayout *mainLayout)
{
    auto *frame = new QFrame(this);
    frame->setFrameShape(QFrame::StyledPanel);

    auto *layout = new QHBoxLayout(frame);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(18);

    m_connectionLabel = new QLabel(tr("Disconnected"), frame);
    m_rxLabel         = new QLabel(frame);
    m_rateLabel       = new QLabel(frame);
    m_crcLabel        = new QLabel(frame);
    m_frameErrorLabel = new QLabel(frame);
    m_logLabel        = new QLabel(frame);

    layout->addWidget(m_connectionLabel);
    layout->addWidget(m_rxLabel);
    layout->addWidget(m_rateLabel);
    layout->addWidget(m_crcLabel);
    layout->addWidget(m_frameErrorLabel);
    layout->addStretch();
    layout->addWidget(m_logLabel);

    mainLayout->addWidget(frame);
}

void MainWindow::setupConnections()
{
    connect(m_connectButton, &QPushButton::clicked,
            this, &MainWindow::onConnectClicked);

    connect(m_refreshButton, &QPushButton::clicked,
            this, &MainWindow::onRefreshPortsClicked);

    connect(m_logButton, &QPushButton::clicked,
            this, &MainWindow::onLogClicked);

    connect(m_clearPlotButton, &QPushButton::clicked,
            this, &MainWindow::onClearPlotsClicked);

    connect(m_powerOnButton, &QPushButton::clicked,
            this, &MainWindow::onPowerEnableClicked);

    connect(m_powerOffButton, &QPushButton::clicked,
            this, &MainWindow::onPowerDisableClicked);

    connect(&m_controller, &Measurement_Controller::powerCommandSent,
            this, &MainWindow::onPowerCommandSent);

    connect(m_windowCombo, &QComboBox::currentIndexChanged,
            this, &MainWindow::onWindowSecChanged);

    connect(&m_controller, &Measurement_Controller::measurementUpdated,
            this, &MainWindow::onMeasurementUpdated);

    connect(&m_controller, &Measurement_Controller::systemStatusUpdated,
            this, &MainWindow::onSystemStatusUpdated);

    connect(&m_controller, &Measurement_Controller::connectionStateChanged,
            this, &MainWindow::onConnectionStateChanged);

    connect(&m_controller, &Measurement_Controller::communicationStatsChanged,
            this, &MainWindow::onCommunicationStatsChanged);

    connect(&m_controller, &Measurement_Controller::linkStaleChanged,
            this, &MainWindow::onLinkStaleChanged);

    connect(&m_controller, &Measurement_Controller::errorOccurred,
            this, &MainWindow::onErrorOccurred);

    /* Ghi log ngay từ controller, không qua UI, để không mất mẫu
     * khi cửa sổ bị thu nhỏ hoặc tab Trends đang ẩn. */
    connect(&m_controller, &Measurement_Controller::measurementUpdated,
            &m_logger, &CSV_Logger::logPacket);

    connect(&m_logger, &CSV_Logger::loggingStateChanged,
            this, &MainWindow::onLoggingStateChanged);

    connect(&m_logger, &CSV_Logger::errorOccurred,
            this, &MainWindow::onErrorOccurred);
}

/* ------------------------------------------------------------------- slots */

void MainWindow::onConnectClicked()
{
    if (m_controller.isConnected())
    {
        m_controller.closePort();
        return;
    }

    const QString portName = m_portCombo->currentData().toString();

    if (portName.isEmpty())
    {
        onErrorOccurred(tr("No serial port selected"));
        return;
    }

    const qint32 baudRate = m_baudCombo->currentData().toInt();

    if (!m_controller.openPort(portName, baudRate))
        onErrorOccurred(tr("Failed to open %1").arg(portName));
}

void MainWindow::onRefreshPortsClicked()
{
    refreshSerialPorts();
}

void MainWindow::onLogClicked()
{
    if (m_logger.isLogging())
    {
        m_logger.stop();
        return;
    }

    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);

    const QString suggested =
        QDir(dir).filePath(CSV_Logger::suggestedFileName());

    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save CSV log"), suggested, tr("CSV files (*.csv)"));

    if (path.isEmpty())
        return;

    m_logger.start(path);
}

void MainWindow::onClearPlotsClicked()
{
    m_currentPlot->clearData();
    m_vbusPlot->clearData();
    m_tempPlot->clearData();
}

void MainWindow::onPowerEnableClicked()
{
    m_controller.sendPowerControl(true);
}

void MainWindow::onPowerDisableClicked()
{
    /* Tắt nguồn có thể cắt điện tải đang chạy -> hỏi lại một lần.
     * Bật nguồn không cần hỏi vì hậu quả nhẹ hơn nhiều. */
    const auto answer = QMessageBox::question(
        this,
        tr("Disable supply"),
        tr("Turn OFF the INA228 board supply?\n"
           "Measurements will stop until it is turned back on."),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (answer != QMessageBox::Yes)
        return;

    m_controller.sendPowerControl(false);
}

void MainWindow::onPowerCommandSent(bool enable)
{
    /* Firmware không gửi ack, nên đây chỉ là lệnh ĐÃ GỬI,
     * không phải trạng thái nguồn thực tế đọc về. */
    m_powerLabel->setText(enable ? tr("Sent: ON") : tr("Sent: OFF"));
    m_powerLabel->setStyleSheet(enable
                                    ? QStringLiteral("color:#2e7d32;font-weight:bold;")
                                    : QStringLiteral("color:#c62828;font-weight:bold;"));

    statusBar()->showMessage(
        enable ? tr("Power enable command sent") : tr("Power disable command sent"),
        3000);
}

void MainWindow::onWindowSecChanged(int index)
{
    const double sec = m_windowCombo->itemData(index).toDouble();

    m_currentPlot->setWindowSeconds(sec);
    m_vbusPlot->setWindowSeconds(sec);
    m_tempPlot->setWindowSeconds(sec);
}

void MainWindow::onMeasurementUpdated(const MeasurementPacket &packet)
{
    for (int i = 0; i < Protocol::IC_COUNT; ++i) {
        m_inaWidgets[i]->setData(packet.ic[i]);

        if (!packet.ic[i].valid)
            continue;

        m_currentPlot->addSample(i, packet.ic[i].current);
        m_vbusPlot   ->addSample(i, packet.ic[i].vbus);
        m_tempPlot   ->addSample(i, packet.ic[i].temperature);
    }

    /* Vẽ lại một lần cho cả khung */
    m_currentPlot->commit();
    m_vbusPlot->commit();
    m_tempPlot->commit();
}

void MainWindow::onSystemStatusUpdated(const SystemStatus &status)
{
    for (int i = 0; i < Protocol::IC_COUNT; ++i)
    {
        m_inaWidgets[i]->setFresh(status.fresh[i]);
        m_inaWidgets[i]->setFault(status.fault[i]);
    }
}

void MainWindow::onConnectionStateChanged(bool connected)
{
    updateConnectionUi(connected);
}

void MainWindow::onLinkStaleChanged(bool stale)
{
    for (Ina228_Widget *w : m_inaWidgets)
        w->setStale(stale);
}

void MainWindow::onCommunicationStatsChanged()
{
    m_rxLabel->setText(tr("RX: %1 (valid %2)")
                           .arg(m_controller.rxFrames())
                           .arg(m_controller.validFrames()));

    m_rateLabel->setText(tr("Rate: %1 fps")
                             .arg(m_controller.frameRateHz(), 0, 'f', 1));

    m_crcLabel->setText(tr("CRC err: %1").arg(m_controller.crcErrors()));

    m_frameErrorLabel->setText(tr("Frame err: %1").arg(m_controller.frameErrors()));

    if (m_logger.isLogging())
        m_logLabel->setText(tr("LOG: %1 rows").arg(m_logger.rowCount()));
}

void MainWindow::onLoggingStateChanged(bool logging)
{
    m_logButton->setText(logging ? tr("Stop log") : tr("Start log"));

    if (logging)
    {
        m_logLabel->setText(tr("LOG: 0 rows"));
        m_logLabel->setStyleSheet(QStringLiteral("color:#c62828;font-weight:bold;"));
        statusBar()->showMessage(tr("Logging to %1").arg(m_logger.filePath()), 5000);
    }
    else
    {
        m_logLabel->setText(tr("LOG: off"));
        m_logLabel->setStyleSheet(QStringLiteral("color:#757575;"));
    }
}

void MainWindow::onErrorOccurred(const QString &message)
{
    statusBar()->showMessage(message, 5000);
}

/* ----------------------------------------------------------------- helpers */

void MainWindow::refreshSerialPorts()
{
    const QString previous = m_portCombo->currentData().toString();

    m_portCombo->clear();

    const auto ports = Serial_Manager::availablePorts();

    for (const auto &p : ports)
        m_portCombo->addItem(p.second, p.first);

    const int idx = m_portCombo->findData(previous);
    if (idx >= 0)
        m_portCombo->setCurrentIndex(idx);

    if (ports.isEmpty())
        statusBar()->showMessage(tr("No serial port found"), 3000);
}

void MainWindow::updateConnectionUi(bool connected)
{
    m_connectButton->setText(connected ? tr("Disconnect") : tr("Connect"));

    m_portCombo->setEnabled(!connected);
    m_baudCombo->setEnabled(!connected);
    m_refreshButton->setEnabled(!connected);

    m_connectionLabel->setText(connected ? tr("Connected") : tr("Disconnected"));
    m_connectionLabel->setStyleSheet(connected ? QStringLiteral("color:#2e7d32;font-weight:bold;")
                                               : QStringLiteral("color:#c62828;font-weight:bold;"));

    if (!connected)
        onLinkStaleChanged(true);
}