#include "WinDivert.hpp"

#include <QThread>

using namespace std;

WinDivert::WinDivert()
{
}
WinDivert::~WinDivert()
{
    if (m_handle != INVALID_HANDLE_VALUE)
    {
        if (m_thread)
            m_thread->requestInterruption();

        WinDivertClose(m_handle);
    }

    if (m_thread)
    {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
    }
}

bool WinDivert::init(uint16_t port)
{
    m_handle = WinDivertOpen(
        QString("tcp.SrcPort == %1").arg(port).toLatin1().constData(),
        WINDIVERT_LAYER_NETWORK,
        0,
        WINDIVERT_FLAG_SNIFF
    );
    if (m_handle == INVALID_HANDLE_VALUE)
        return false;

    m_thread = QThread::create(bind(&WinDivert::receivePacketThread, this));
    m_thread->start();

    return true;
}

void WinDivert::receivePacketThread()
{
    auto packet = make_unique<uint8_t[]>(WINDIVERT_MTU_MAX);
    WINDIVERT_ADDRESS addr = {};
    UINT addrLen = sizeof(addr);
    while (!m_thread->isInterruptionRequested())
    {
        UINT recvLen = 0;
        const BOOL ok = WinDivertRecvEx(
            m_handle,
            packet.get(),
            WINDIVERT_MTU_MAX,
            &recvLen,
            0,
            &addr,
            &addrLen,
            nullptr
        );
        if (ok)
        {
            processPacket(packet.get(), recvLen);
        }
    }
}
