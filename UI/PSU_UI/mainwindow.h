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
    void onPowerEnableClicked();
    void onPowerDisableClicked();
    void onPowerCommandSent(bool enable);
    void onLimitAckReceived(const LimitAck &ack);
    void onSetLimitsRequested(int ch, double sovlA, double bovlV, double buvlV);
    void onGetLimitsRequested(int ch);
    void onClearFaultsClicked();
    void onClearPlotsClicked();
    void onWindowSecChanged(int index);

    void onMeasurementUpdated(const MeasurementPacket &packet);
    void onSystemStatusUpdated(const SystemStatus &status);
    void onSafetyStatusUpdated(const StatusPacket &status);
    void onConnectionStateChanged(bool connected);
    void onCommunicationStatsChanged();
    void onLinkStaleChanged(bool stale);
    void onLoggingStateChanged(bool logging);
    void onErrorOccurred(const QString &message);

private:
    void setupUi();
    void setupTopBar(QVBoxLayout *layout);
    void setupTabs(QVBoxLayout *layout);
    QWidget *buildPanelTab();
    QWidget *buildTrendTab();
    QWidget *buildEventTab();
    void appendEventLog(const QString &text, bool isFault);
    void setupStatusArea(QVBoxLayout *layout);
    void setupConnections();

    void refreshSerialPorts();
    void updateConnectionUi(bool connected);
    void setupPowerControls(QHBoxLayout *layout, QWidget *parent);
    void updateDcmBadges();
    void showSafetyBanner(Protocol::SafeState state, Protocol::FaultCode fault,
                          quint8 tripCount);

private:
    Measurement_Controller m_controller;
    Csv_Logger             m_logger;

    QComboBox   *m_portCombo     = nullptr;
    QComboBox   *m_baudCombo     = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QPushButton *m_connectButton = nullptr;
    QPushButton *m_logButton     = nullptr;
    QPushButton *m_powerOnButton  = nullptr;
    QPushButton *m_powerOffButton = nullptr;
    QLabel      *m_powerLabel     = nullptr;
    QLabel      *m_safetyLabel    = nullptr;

    /* Luu trang thai Safety gan nhat de ket hop voi khung TELEMETRY:
     * bit DCM trong byte trang thai TELEMETRY CHUA duoc mask theo PWR_EN
     * o phia firmware (khac voi dcm_fault da mask trong safety.c), nen
     * GUI tu suy luan: chi coi la loi that khi dong thoi dang SAFE_ON. */
    Protocol::SafeState m_lastSafeState = Protocol::SafeState::Boot;
    SystemStatus         m_lastSystemStatus;

    QTabWidget  *m_tabs            = nullptr;
    QComboBox   *m_windowCombo     = nullptr;
    QPushButton *m_clearPlotButton = nullptr;

    class QPlainTextEdit *m_eventLog = nullptr;
    QPushButton *m_clearFaultButton = nullptr;

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