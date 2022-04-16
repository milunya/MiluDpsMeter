#include <QApplication>
#include <QFontDatabase>
#include <QMessageBox>
#include <QScreen>
#include <QDebug>

#include "SWPacketCapture.hpp"
#include "DpsLogic.hpp"
#include "PacketCapture.hpp"

#include "MainWindow.hpp"

int main(int argc, char *argv[])
{
    qunsetenv("XDG_CURRENT_DESKTOP");
    qunsetenv("QT_QPA_PLATFORMTHEME");
    qunsetenv("QT_STYLE_OVERRIDE");

    qInstallMessageHandler([](QtMsgType t, const QMessageLogContext &c, const QString &b) {
        fprintf(stderr, "%s\n", qUtf8Printable(qFormatLogMessage(t, c, b)));
        fflush(stderr);
    });

    QCoreApplication::setSetuidAllowed(true);
    QCoreApplication::setApplicationName("MiluDpsMeter");
    QCoreApplication::setApplicationVersion(MILU_DPS_METER_VERSION);

    QGuiApplication::setApplicationDisplayName("Milu DPS Meter");

    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::Round);

    QApplication app(argc, argv);

    QApplication::setStyle("windows");

    QPalette pal(QColor(44, 44, 44));
    QApplication::setPalette(pal);

    if (QFontDatabase::addApplicationFont(":/DejaVuSansMono-Bold.ttf") < 0)
        qWarning() << "Can't load font";

    QFont font("DejaVu Sans Mono");
    font.setHintingPreference(QFont::HintingPreference::PreferVerticalHinting);
    font.setPointSize(9);
    font.setBold(true);
    QApplication::setFont(font);

    auto packetCapture = PacketCapture::create();
    if (!packetCapture->init(15011))
    {
        qWarning() << "Error initializing packet capture";
#ifndef QT_DEBUG
        QMessageBox::warning(nullptr, QString(), "Can't grab packages, please run as root");
        return -1;
#endif
    }

    SWPacketCapture swPacketCapture;
    QObject::connect(
        packetCapture.get(), &PacketCapture::newPacket,
        &swPacketCapture, &SWPacketCapture::newPacket,
        Qt::DirectConnection
    );

    DpsLogic dpsLogic;
    QObject::connect(
        &swPacketCapture, &SWPacketCapture::worldChange,
        &dpsLogic, &DpsLogic::worldChange
    );
    QObject::connect(
        &swPacketCapture, &SWPacketCapture::ownerId,
        &dpsLogic, &DpsLogic::ownerId
    );
    QObject::connect(
        &swPacketCapture, &SWPacketCapture::damage,
        &dpsLogic, &DpsLogic::damage
    );
    QObject::connect(
        &swPacketCapture, &SWPacketCapture::mazeEnd,
        &dpsLogic, &DpsLogic::mazeEnd
    );
    QObject::connect(
        &swPacketCapture, &SWPacketCapture::partyMember,
        &dpsLogic, &DpsLogic::partyMember
    );

    MainWindow win(dpsLogic);
    win.move(app.primaryScreen()->availableSize().width() - win.width(), 0);
    win.show();

    QObject::connect(
        &win, &MainWindow::packetCaptureReset,
        packetCapture.get(), &PacketCapture::reset
    );

    return app.exec();
}
