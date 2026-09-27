#include <QApplication>

#include "ProbeWindow.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("RSSI Probe"));
    app.setOrganizationName(QStringLiteral("SoundConnect"));
    ProbeWindow window;
    window.show();
    return app.exec();
}
