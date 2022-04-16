#pragma once

#include "PacketCapture.hpp"

#include <windivert.h>

class QThread;

class WinDivert : public PacketCapture
{
    Q_OBJECT

public:
    WinDivert();
    ~WinDivert();

    bool init(uint16_t port) override;

private:
    void receivePacketThread();

private:
    HANDLE m_handle = INVALID_HANDLE_VALUE;
    QThread *m_thread = nullptr;
};
