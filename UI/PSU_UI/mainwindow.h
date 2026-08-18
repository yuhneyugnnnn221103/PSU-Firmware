#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "measurement_controller.h"
#include "ina228_widget.h"
#include "custom_plot.h"
#include "csv_logger.h"

class QComboBox;
class QPushButton;
class QLabel;
class QVBoxLayout;
class QHBoxLayout;
class QTabWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onConnectClicked();
    void onRefreshPortsClicked();
    void onLogClicked();
    void onClearPlotsClicked();
    void onWindowSecChanged(int index);

    void onPowerEnableClicked();
    void onPowerDisableClicked();
    void onPowerCommandSent(bool enable);

    void onMeasurementUpdated(const MeasurementPacket &packet);
    void onSystemStatusUpdated(const SystemStatus &status);
    void onConnectionStateChanged(bool connected);
    void onCommunicationStatsChanged();
    void onLinkStaleChanged(bool stale);
    void onErrorOccurred(const QString &message);
    void onLoggingStateChanged(bool logging);

private:
    void setupUi();
    void setupTopBar(QVBoxLayout *layout);
    void setupTabs(QVBoxLayout *layout);
    QWidget *buildPanelTab();
    QWidget *buildTrendTab();
    // void setupMeasurementArea(QVBoxLayout *layout);
    void setupStatusArea(QVBoxLayout *layout);
    void setupConnections();

    void refreshSerialPorts();
    void updateConnectionUi(bool connected);

    void setupPowerControls(QHBoxLayout *layout, QWidget *parent);

private:
    Measurement_Controller m_controller;
    CSV_Logger  m_logger;

    QComboBox   *m_portCombo      = nullptr;
    QComboBox   *m_baudCombo      = nullptr;
    QPushButton *m_refreshButton  = nullptr;
    QPushButton *m_connectButton  = nullptr;
    QPushButton *m_logButton      = nullptr;

    // nút nhấn điều khiển cấp nguồn cho toàn bộ IC INA228
    QPushButton *m_powerOnButton  = nullptr;
    QPushButton *m_powerOffButton = nullptr;
    QLabel      *m_powerLabel     = nullptr;

    QTabWidget  *m_tabs            = nullptr;
    QComboBox   *m_windowCombo     = nullptr;
    QPushButton *m_clearPlotButton = nullptr;

    Custom_Plot *m_currentPlot = nullptr;
    Custom_Plot *m_vbusPlot    = nullptr;
    Custom_Plot *m_tempPlot    = nullptr;

    QLabel *m_connectionLabel = nullptr;
    QLabel *m_rxLabel         = nullptr;
    QLabel *m_rateLabel       = nullptr;
    QLabel *m_crcLabel        = nullptr;
    QLabel *m_frameErrorLabel = nullptr;
    QLabel *m_logLabel        = nullptr;

    Ina228_Widget *m_inaWidgets[Protocol::IC_COUNT] = {};
};

#endif // MAINWINDOW_H