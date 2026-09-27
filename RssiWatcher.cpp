#include "RssiWatcher.h"

#include <QDateTime>
#include <QDebug>
#include <QMetaObject>

#include <functional>

#ifdef Q_OS_WIN
namespace Adv = winrt::Windows::Devices::Bluetooth::Advertisement;

// Parses "AA:BB:CC:DD:EE:FF" into the WinRT BluetoothAddress; 0 when the
// string is not a valid MAC.
static uint64_t parseBleAddress(const QString& mac) {
    const QStringList parts = mac.split(':');
    if (parts.size() != 6) return 0;
    uint64_t addr = 0;
    for (const QString& part : parts) {
        bool ok = true;
        addr = (addr << 8) | static_cast<uint64_t>(part.toUInt(&ok, 16) & 0xFF);
        if (!ok) return 0;
    }
    return addr;
}
#endif

bool rssiAdvertisementNameMatches(const QString& advertised, const QString& hint) {
    const QString a = advertised.trimmed();
    const QString h = hint.trimmed();
    if (a.isEmpty() || h.isEmpty()) return false;
    return a.compare(h, Qt::CaseInsensitive) == 0
        || a.contains(h, Qt::CaseInsensitive)
        || h.contains(a, Qt::CaseInsensitive);
}

RssiWatcher::RssiWatcher(QObject* parent) : QObject(parent) {
#ifdef Q_OS_WIN
    // Register once; the handler filters by device address. std::bind with
    // _2 adapts the member function (args only) to the two-parameter
    // TypedEventHandler delegate.
    _watcher.Received(
        std::bind(&RssiWatcher::onAdvertisementReceived, this, std::placeholders::_2));
    // Stop() completes asynchronously (Started -> Stopping -> Stopped) and
    // Start() issued while the watcher is Stopping throws. A toggle-off /
    // toggle-on sequence that lands in that window used to swallow the
    // failure and leave the detector in "Detecting" forever; the Stopped
    // handler below completes the deferred start instead.
    _watcher.Stopped([this](const auto&, const auto&) {
        QMetaObject::invokeMethod(this, [this] { onWatcherStopped(); },
                                  Qt::QueuedConnection);
    });
    // If the latched BLE address goes quiet (address rotation, or the
    // advertisements thinning out), fall back to re-acquiring by name
    // rather than sitting silent until the detector gives up.
    _sampleWatchdog.setInterval(5000);
    connect(&_sampleWatchdog, &QTimer::timeout, this,
            &RssiWatcher::onSampleWatchdogTimeout);
#endif
}

RssiWatcher::~RssiWatcher() {
    stop();
}

void RssiWatcher::setAddress(const QString& mac) {
    // Called on every device snapshot (twice a second); only the actual
    // device change may disturb the latched BLE address. Resetting the
    // latch unconditionally starved the detector whenever advertisements
    // were sparse (e.g. while A2DP audio keeps the radio busy).
    if (mac == _mac) return;
    _mac = mac;
#ifdef Q_OS_WIN
    _address = parseBleAddress(mac);
    _latched = false;
#endif
}

void RssiWatcher::setNameHint(const QString& name) {
    _nameHint = name;
}

void RssiWatcher::start() {
#ifdef Q_OS_WIN
    if (_address == 0) return;
    using Adv::BluetoothLEAdvertisementWatcherStatus;
    const auto status = _watcher.Status();
    // Note: the WinRT status enum has no "Starting": Created, Started,
    // Stopping, Stopped, Aborted are the only states.
    const bool live = status == BluetoothLEAdvertisementWatcherStatus::Started;
    if (_running && live) return;  // already up
    if (!_running && live) {
        // We lost track of a live watcher (a previous Stop threw); bring
        // it down first so the Stopped handler restarts it cleanly.
        qDebug() << "RssiWatcher: recycling untracked live watcher";
        _pendingStart = true;
        try {
            _watcher.Stop();
        } catch (const winrt::hresult_error&) {
        }
        return;
    }
    if (status == BluetoothLEAdvertisementWatcherStatus::Stopping) {
        // The previous Stop() has not finished yet; the Stopped handler
        // completes this start. Calling Start() here would throw and wedge
        // the detector in "Detecting".
        _pendingStart = true;
        qDebug() << "RssiWatcher: start deferred until the watcher stops";
        return;
    }
    try {
        // Active scanning also delivers scan-response packets, giving more
        // RSSI samples per second for the movement heuristic.
        _watcher.ScanningMode(Adv::BluetoothLEScanningMode::Active);
        _watcher.Start();
        _running = true;
        _pendingStart = false;
        _lastSampleMs.store(-1, std::memory_order_relaxed);
        _sampleWatchdog.start();
        qDebug() << "RssiWatcher: scan started";
    } catch (const winrt::hresult_error& e) {
        qDebug() << "RssiWatcher: Start failed:"
                 << QString::number(static_cast<uint>(e.code()), 16)
                 << QString::fromWCharArray(e.message().c_str());
        _running = false;
    }
#else
    Q_UNUSED(_mac);
#endif
}

void RssiWatcher::stop() {
#ifdef Q_OS_WIN
    _pendingStart = false;
    _sampleWatchdog.stop();
    using Adv::BluetoothLEAdvertisementWatcherStatus;
    const auto status = _watcher.Status();
    if (status != BluetoothLEAdvertisementWatcherStatus::Started &&
        status != BluetoothLEAdvertisementWatcherStatus::Stopping) {
        _running = false;
        return;
    }
    try {
        _watcher.Stop();
    } catch (const winrt::hresult_error& e) {
        qDebug() << "RssiWatcher: Stop failed:"
                 << QString::number(static_cast<uint>(e.code()), 16);
    }
    _running = false;
    qDebug() << "RssiWatcher: stop requested";
#else
    Q_UNUSED(_mac);
#endif
}

#ifdef Q_OS_WIN
void RssiWatcher::onWatcherStopped() {
    // Runs on the Qt thread (queued from the WinRT Stopped event).
    if (_pendingStart) {
        _pendingStart = false;
        qDebug() << "RssiWatcher: completing deferred start";
        start();
    }
}

void RssiWatcher::onSampleWatchdogTimeout() {
    // Runs on the Qt thread.
    const qint64 last = _lastSampleMs.load(std::memory_order_relaxed);
    const qint64 quietMs =
        last < 0 ? 6000 : QDateTime::currentMSecsSinceEpoch() - last;
    if (quietMs >= 5000 && _latched) {
        qDebug() << "RssiWatcher: latched address quiet for" << quietMs
                 << "ms, dropping back to name re-acquisition";
        _latched = false;
        _address = parseBleAddress(_mac);
    }
}
#endif

#ifdef Q_OS_WIN
void RssiWatcher::onAdvertisementReceived(const Adv::BluetoothLEAdvertisementReceivedEventArgs& args) {
    const uint64_t addr = args.BluetoothAddress();
    const double dbm = static_cast<double>(args.RawSignalStrengthInDBm());
    // Runs on a WinRT thread: the atomic is the only cross-thread state
    // touched here; Qt queues the signal to the receiver's thread.
    if (_address != 0 && addr == _address) {
        _lastSampleMs.store(QDateTime::currentMSecsSinceEpoch(), std::memory_order_relaxed);
        emit rssiSample(dbm);
        return;
    }
    // The app links over Classic RFCOMM and only knows the Classic address,
    // but a headphone's BLE advertising address is usually a different
    // random static address (WH-1000XM4: C2:D6:3C:33:26:CB). Fall back to
    // the advertisement's local name and latch the first match, after which
    // the address filter above takes over.
    if (!_latched && !_nameHint.isEmpty()) {
        const QString localName =
            QString::fromWCharArray(args.Advertisement().LocalName().c_str());
        if (rssiAdvertisementNameMatches(localName, _nameHint)) {
            _latched = true;
            _address = addr;
            _lastSampleMs.store(QDateTime::currentMSecsSinceEpoch(), std::memory_order_relaxed);
            emit rssiSample(dbm);
        }
    }
}
#endif
