#pragma once

#include "PacketCapture.hpp"

#include <QSocketNotifier>

#include <pcap/pcap.h>

class PCap : public PacketCapture
{
    Q_OBJECT

public:
    PCap();
    ~PCap();

    bool init(uint16_t port) override;

private:
    void closeHandle();

    void activated();

    void processPacket(const uint8_t *packet, qsizetype len) override;

signals:
    void newPacket(const uint8_t *data, int32_t len); // Must be direct connection

private:
    pcap_t *m_handle = nullptr;
    QSocketNotifier m_socketNotifier;
};
