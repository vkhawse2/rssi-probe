#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include <atomic>
#include <cstdint>

#ifdef Q_OS_WIN
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#endif

// Matches a BLE advertisement's local name against the connected device's
// name. Pure Qt, unit-tested: the app links to the headphone over Classic
// RFCOMM and only knows its Classic address, but e.g. the WH-1000XM4's BLE
// advertisements carry a different (random static) address while naming the
// device "WH-1000XM4" (sometimes "LE_WH-1000XM4"). The watcher latches onto
// the right advertiser by name when the address filter misses.
bool rssiAdvertisementNameMatches(const QString& advertised, const QString& hint);

// RssiWatcher reports live Bluetooth signal strength (RSSI, in dBm) for one
// device by listening to its BLE advertisements. Windows only: the Sony
// protocol link on Windows is Classic RFCOMM, which exposes no RSSI, so the
// watcher runs a separate WinRT advertisement watcher. The connected
// device's Classic address is tried first, but a headphone's BLE advertising
// address is usually a different random static address, so the watcher falls
// back to latching onto the advertiser whose local name matches the device
// name. On other platforms every method is a no-op and no samples are ever
// emitted.
class RssiWatcher : public QObject {
    Q_OBJECT
public:
    explicit RssiWatcher(QObject* parent = nullptr);
    ~RssiWatcher() override;

    // "AA:BB:CC:DD:EE:FF". Takes effect on the next start().
    void setAddress(const QString& mac);
    // "WH-1000XM4". Name fallback used when the address never matches an
    // advertisement; the first name-matching advertiser is latched.
    void setNameHint(const QString& name);
    void start();
    void stop();
    bool isRunning() const { return _running; }

signals:
    void rssiSample(double dbm);

private:
#ifdef Q_OS_WIN
    void onAdvertisementReceived(
        const winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementReceivedEventArgs& args);
    // Fires (on the Qt thread) when the WinRT watcher reaches Stopped; it
    // completes a start that had to wait out an in-flight Stop().
    void onWatcherStopped();
    // Periodic check (Qt thread): if the latched BLE address has gone
    // quiet, its random static address may have rotated, so drop back to
    // the Classic address + name fallback and let the next advertisement
    // re-acquire the device.
    void onSampleWatchdogTimeout();

    winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher _watcher;
    uint64_t _address = 0;
    bool _latched = false;
    bool _pendingStart = false;
    QTimer _sampleWatchdog;
    // Written from the WinRT advertisement thread, read on the Qt thread.
    std::atomic<qint64> _lastSampleMs{-1};
#endif
    bool _running = false;
    QString _mac;
    QString _nameHint;
};
