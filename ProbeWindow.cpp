#include "ProbeWindow.h"

#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QtMath>
#include <numeric>

namespace {
QString stateColor(MovementDetector::State s) {
    switch (s) {
    case MovementDetector::State::Moving: return QStringLiteral("#2e7d32");
    case MovementDetector::State::Still: return QStringLiteral("#1565c0");
    default: return QStringLiteral("#616161");
    }
}
} // namespace

ProbeWindow::ProbeWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("RSSI Probe — movement detector playground"));
    resize(720, 640);

    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);
    setCentralWidget(central);

    // ---- Device discovery ----
    auto* devBox = new QGroupBox(QStringLiteral("1. Find your headphones"), central);
    auto* devLayout = new QVBoxLayout(devBox);
    auto* scanRow = new QHBoxLayout();
    _scanButton = new QPushButton(QStringLiteral("Scan for devices"), devBox);
    _scanButton->setCheckable(true);
    scanRow->addWidget(_scanButton);
    _selectedLabel = new QLabel(QStringLiteral("No device selected"), devBox);
    scanRow->addWidget(_selectedLabel, 1);
    devLayout->addLayout(scanRow);
    _deviceList = new QListWidget(devBox);
    _deviceList->setMinimumHeight(110);
    devLayout->addWidget(_deviceList);
#ifndef Q_OS_WIN
    auto* note = new QLabel(
        QStringLiteral("BLE watching is Windows-only; this build can only show the UI."), devBox);
    note->setStyleSheet(QStringLiteral("color:#9e9e9e;"));
    devLayout->addWidget(note);
#endif
    root->addWidget(devBox);

    // ---- Probe + live state ----
    auto* probeBox = new QGroupBox(QStringLiteral("2. Probe the signal"), central);
    auto* probeLayout = new QVBoxLayout(probeBox);
    auto* probeRow = new QHBoxLayout();
    _probeButton = new QPushButton(QStringLiteral("Start probe"), probeBox);
    _probeButton->setCheckable(true);
    _probeButton->setEnabled(false);
    probeRow->addWidget(_probeButton);
    _stateLabel = new QLabel(QStringLiteral("UNKNOWN"), probeBox);
    QFont stateFont = _stateLabel->font();
    stateFont.setPointSize(22);
    stateFont.setBold(true);
    _stateLabel->setFont(stateFont);
    _stateLabel->setAlignment(Qt::AlignCenter);
    probeRow->addWidget(_stateLabel, 1);
    probeLayout->addLayout(probeRow);

    auto* statsGrid = new QHBoxLayout();
    _rssiLabel = new QLabel(QStringLiteral("RSSI: —"), probeBox);
    _varLabel = new QLabel(QStringLiteral("Variance: —"), probeBox);
    _samplesLabel = new QLabel(QStringLiteral("Samples: 0"), probeBox);
    _rateLabel = new QLabel(QStringLiteral("Rate: —"), probeBox);
    _minMaxLabel = new QLabel(QStringLiteral("Min/Max: —"), probeBox);
    for (auto* l : {_rssiLabel, _varLabel, _samplesLabel, _rateLabel, _minMaxLabel}) {
        l->setTextInteractionFlags(Qt::TextSelectableByMouse);
        statsGrid->addWidget(l);
    }
    probeLayout->addLayout(statsGrid);
    root->addWidget(probeBox);

    // ---- Threshold tuning ----
    auto* tuneBox = new QGroupBox(QStringLiteral("3. Tune thresholds (applies live)"), central);
    auto* tuneLayout = new QHBoxLayout(tuneBox);
    auto addSpin = [&](const QString& label, QSpinBox*& out, int min, int max, int value,
                       const QString& suffix) {
        auto* col = new QVBoxLayout();
        col->addWidget(new QLabel(label, tuneBox));
        out = new QSpinBox(tuneBox);
        out->setRange(min, max);
        out->setValue(value);
        out->setSuffix(suffix);
        col->addWidget(out);
        tuneLayout->addLayout(col);
    };
    auto addDSpin = [&](const QString& label, QDoubleSpinBox*& out, double min, double max,
                        double step, double value, const QString& suffix) {
        auto* col = new QVBoxLayout();
        col->addWidget(new QLabel(label, tuneBox));
        out = new QDoubleSpinBox(tuneBox);
        out->setRange(min, max);
        out->setSingleStep(step);
        out->setValue(value);
        out->setSuffix(suffix);
        col->addWidget(out);
        tuneLayout->addLayout(col);
    };
    addSpin(QStringLiteral("Window"), _windowSpin, 3, 60, 12, QStringLiteral(" s"));
    addDSpin(QStringLiteral("Band ±"), _bandSpin, 1.0, 30.0, 0.5, 15.0,
             QStringLiteral(" dB"));
    addSpin(QStringLiteral("Min samples"), _minSamplesSpin, 2, 50, 4, QString());
    addSpin(QStringLiteral("Stale after"), _staleSpin, 5, 120, 20, QStringLiteral(" s"));
    auto* applyButton = new QPushButton(QStringLiteral("Apply"), tuneBox);
    tuneLayout->addWidget(applyButton);
    root->addWidget(tuneBox);

    // ---- Sample log ----
    auto* logBox = new QGroupBox(QStringLiteral("4. Sample log"), central);
    auto* logLayout = new QVBoxLayout(logBox);
    _log = new QPlainTextEdit(logBox);
    _log->setReadOnly(true);
    _log->setMaximumBlockCount(400);
    _log->setFont(QFont(QStringLiteral("Consolas"), 9));
    logLayout->addWidget(_log);
    auto* clearButton = new QPushButton(QStringLiteral("Clear log"), logBox);
    logLayout->addWidget(clearButton);
    root->addWidget(logBox, 1);

    connect(_scanButton, &QPushButton::toggled, this, &ProbeWindow::onScanToggled);
    connect(_probeButton, &QPushButton::toggled, this, &ProbeWindow::onProbeToggled);
    connect(_deviceList, &QListWidget::itemClicked, this, &ProbeWindow::onDevicePicked);
    connect(applyButton, &QPushButton::clicked, this, &ProbeWindow::onApplyThresholds);
    connect(clearButton, &QPushButton::clicked, _log, &QPlainTextEdit::clear);
    connect(&_discovery, &BleDiscovery::deviceSeen, this, &ProbeWindow::onDeviceSeen);
    connect(&_rssi, &RssiWatcher::rssiSample, this, &ProbeWindow::onRssiSample);

    _statsTimer.setInterval(500);
    connect(&_statsTimer, &QTimer::timeout, this, &ProbeWindow::onStatsTick);

    rebuildDetector();
    updateStateLabel();
}

void ProbeWindow::rebuildDetector() {
    const bool wasProbing = _probeButton->isChecked();
    if (wasProbing) {
        _rssi.stop();
    }
    delete _detector;
    MovementDetectorConfig cfg;
    cfg.bandDb = _bandSpin->value();
    cfg.dataWindowMs = _windowSpin->value() * 1000;
    cfg.minSamples = _minSamplesSpin->value();
    cfg.staleMs = _staleSpin->value() * 1000;
    _detector = new MovementDetector(cfg, this);
    connect(_detector, &MovementDetector::stateChanged, this, &ProbeWindow::onDetectorState);
    _stats.clear();
    _haveSample = false;
    _totalSamples = 0;
    updateStateLabel();
    if (wasProbing && !_targetAddress.isEmpty()) {
        _rssi.setAddress(_targetAddress);
        _rssi.start();
    }
}

void ProbeWindow::onScanToggled(bool on) {
    _scanButton->setText(on ? QStringLiteral("Stop scan") : QStringLiteral("Scan for devices"));
    if (on)
        _discovery.start();
    else
        _discovery.stop();
}

void ProbeWindow::onDeviceSeen(const QString& address, const QString& name, double rssiDbm) {
    QListWidgetItem* row = _rows.value(address, nullptr);
    if (!row) {
        row = new QListWidgetItem(_deviceList);
        row->setData(Qt::UserRole, address);
        _rows.insert(address, row);
    }
    row->setText(QStringLiteral("%1  —  %2  —  %3 dBm")
                     .arg(name, address, QString::number(qRound(rssiDbm))));
}

void ProbeWindow::onDevicePicked(QListWidgetItem* item) {
    _targetAddress = item->data(Qt::UserRole).toString();
    _selectedLabel->setText(QStringLiteral("Selected: %1").arg(_targetAddress));
    _probeButton->setEnabled(true);
}

void ProbeWindow::onProbeToggled(bool on) {
    _probeButton->setText(on ? QStringLiteral("Stop probe") : QStringLiteral("Start probe"));
    _scanButton->setEnabled(!on);
    if (on) {
        _probeStartMs = QDateTime::currentMSecsSinceEpoch();
        _rssi.setAddress(_targetAddress);
        _rssi.start();
        _statsTimer.start();
        _log->appendPlainText(QStringLiteral("--- probing %1 ---").arg(_targetAddress));
    } else {
        _rssi.stop();
        _statsTimer.stop();
    }
}

void ProbeWindow::onRssiSample(double dbm) {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    _currentDbm = dbm;
    _haveSample = true;
    ++_totalSamples;
    _stats.emplace_back(now, dbm);
    const qint64 cutoff = now - _windowSpin->value() * 1000;
    while (!_stats.empty() && _stats.front().first < cutoff)
        _stats.pop_front();
    if (_detector) _detector->addSample(dbm, now);
    logSample(dbm);
}

void ProbeWindow::logSample(double dbm) {
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"));
    _log->appendPlainText(QStringLiteral("%1  %2 dBm").arg(ts, QString::number(dbm, 'f', 1)));
}

void ProbeWindow::onDetectorState(MovementDetector::State) {
    updateStateLabel();
}

void ProbeWindow::updateStateLabel() {
    const auto s = _detector ? _detector->state() : MovementDetector::State::Unknown;
    _stateLabel->setText(_detector ? _detector->stateName().toUpper()
                                  : QStringLiteral("UNKNOWN"));
    _stateLabel->setStyleSheet(QStringLiteral("color:%1;").arg(stateColor(s)));
}

void ProbeWindow::onApplyThresholds() {
    rebuildDetector();
    _log->appendPlainText(QStringLiteral("--- thresholds applied ---"));
}

void ProbeWindow::onStatsTick() {
    if (!_haveSample) return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 cutoff = now - _windowSpin->value() * 1000;
    while (!_stats.empty() && _stats.front().first < cutoff)
        _stats.pop_front();

    double variance = 0.0, mn = _stats.front().second, mx = mn;
    if (_stats.size() >= 2) {
        double sum = 0.0;
        for (const auto& p : _stats) {
            sum += p.second;
            mn = qMin(mn, p.second);
            mx = qMax(mx, p.second);
        }
        const double mean = sum / _stats.size();
        double sq = 0.0;
        for (const auto& p : _stats)
            sq += (p.second - mean) * (p.second - mean);
        variance = sq / _stats.size();
    }
    const double elapsedS = qMax(1LL, (now - _probeStartMs) / 1000);
    _rssiLabel->setText(QStringLiteral("RSSI: %1 dBm").arg(QString::number(_currentDbm, 'f', 1)));
    _varLabel->setText(QStringLiteral("Variance: %1 dBm²").arg(QString::number(variance, 'f', 1)));
    _samplesLabel->setText(QStringLiteral("Samples: %1").arg(_stats.size()));
    _rateLabel->setText(
        QStringLiteral("Rate: %1/s").arg(QString::number(_totalSamples / elapsedS, 'f', 1)));
    _minMaxLabel->setText(QStringLiteral("Min/Max: %1 / %2 dBm")
                              .arg(QString::number(mn, 'f', 0), QString::number(mx, 'f', 0)));
}
