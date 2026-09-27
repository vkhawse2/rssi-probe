#include "BleDiscovery.h"

#include <functional>

#ifdef Q_OS_WIN
namespace Adv = winrt::Windows::Devices::Bluetooth::Advertisement;
#endif

QString BleDiscovery::formatAddress(uint64_t address) {
    QStringList parts;
    for (int i = 5; i >= 0; --i)
        parts << QStringLiteral("%1").arg((address >> (i * 8)) & 0xFF, 2, 16, QChar('0')).toUpper();
    return parts.join(':');
}

BleDiscovery::BleDiscovery(QObject* parent) : QObject(parent) {
#ifdef Q_OS_WIN
    _watcher.Received(
        std::bind(&BleDiscovery::onReceived, this, std::placeholders::_2));
#endif
}

BleDiscovery::~BleDiscovery() {
    stop();
}

void BleDiscovery::start() {
#ifdef Q_OS_WIN
    if (_running) return;
    try {
        _watcher.ScanningMode(Adv::BluetoothLEScanningMode::Active);
        _watcher.Start();
        _running = true;
    } catch (const winrt::hresult_error&) {
        _running = false;
    }
#endif
}

void BleDiscovery::stop() {
#ifdef Q_OS_WIN
    if (!_running) return;
    try {
        _watcher.Stop();
    } catch (const winrt::hresult_error&) {
    }
    _running = false;
#endif
}

#ifdef Q_OS_WIN
void BleDiscovery::onReceived(const Adv::BluetoothLEAdvertisementReceivedEventArgs& args) {
    const uint64_t addr = args.BluetoothAddress();
    QString name = winrt::to_string(args.Advertisement().LocalName()).c_str();
    if (name.isEmpty()) name = QStringLiteral("(no name)");
    // Emitted on a WinRT thread; Qt queues it to the receiver's thread.
    emit deviceSeen(formatAddress(addr), name, static_cast<double>(args.RawSignalStrengthInDBm()));
}
#endif
