#pragma once

#include <QObject>
#include <QString>

#ifdef Q_OS_WIN
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#endif

// Passive BLE discovery for the probe tool: reports every advertising device
// (address, advertised local name, RSSI) so the user can pick their
// headphones instead of typing a MAC address. Windows only; elsewhere every
// method is a no-op and nothing is ever emitted.
class BleDiscovery : public QObject {
    Q_OBJECT
public:
    explicit BleDiscovery(QObject* parent = nullptr);
    ~BleDiscovery() override;

    void start();
    void stop();
    bool isRunning() const { return _running; }

    static QString formatAddress(uint64_t address);

signals:
    void deviceSeen(const QString& address, const QString& name, double rssiDbm);

private:
#ifdef Q_OS_WIN
    void onReceived(
        const winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementReceivedEventArgs& args);

    winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher _watcher;
#endif
    bool _running = false;
};
