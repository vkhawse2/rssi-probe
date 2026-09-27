#pragma once

#include <QDateTime>
#include <QMainWindow>
#include <QMap>
#include <QTimer>
#include <deque>
#include <utility>

#include "BleDiscovery.h"
#include "MovementDetector.h"
#include "RssiWatcher.h"

class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

// Standalone RSSI probe: watch live Bluetooth signal values from a chosen
// device and see exactly what the app's MovementDetector makes of them, so
// thresholds can be tuned from real data before touching the app.
class ProbeWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit ProbeWindow(QWidget* parent = nullptr);

private slots:
    void onScanToggled(bool on);
    void onDeviceSeen(const QString& address, const QString& name, double rssiDbm);
    void onDevicePicked(QListWidgetItem* item);
    void onProbeToggled(bool on);
    void onRssiSample(double dbm);
    void onDetectorState(MovementDetector::State state);
    void onApplyThresholds();
    void onStatsTick();

private:
    void rebuildDetector();
    void updateStateLabel();
    void logSample(double dbm);

    BleDiscovery _discovery;
    RssiWatcher _rssi;
    MovementDetector* _detector = nullptr;

    QString _targetAddress;
    QMap<QString, QListWidgetItem*> _rows; // address -> list row

    // Display-side stats (same window as the detector, for the readout only;
    // the detector itself remains the authority for the state).
    std::deque<std::pair<qint64, double>> _stats;
    double _currentDbm = 0.0;
    bool _haveSample = false;
    qint64 _probeStartMs = 0;
    int _totalSamples = 0;

    QListWidget* _deviceList = nullptr;
    QLabel* _selectedLabel = nullptr;
    QPushButton* _scanButton = nullptr;
    QPushButton* _probeButton = nullptr;
    QLabel* _stateLabel = nullptr;
    QLabel* _rssiLabel = nullptr;
    QLabel* _varLabel = nullptr;
    QLabel* _samplesLabel = nullptr;
    QLabel* _rateLabel = nullptr;
    QLabel* _minMaxLabel = nullptr;
    QSpinBox* _windowSpin = nullptr;
    QDoubleSpinBox* _bandSpin = nullptr;
    QSpinBox* _minSamplesSpin = nullptr;
    QSpinBox* _staleSpin = nullptr;
    QPlainTextEdit* _log = nullptr;
    QTimer _statsTimer;
};
