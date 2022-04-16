#include "PacketCapture.hpp"

#ifdef Q_OS_WIN
#   include "WinDivert.hpp"
#else
#   include "PCap.hpp"
#endif

#include <QDebug>

#ifndef Q_OS_LINUX

// Little Endian only

#   pragma pack(1)

struct iphdr
{
    uint8_t ihl:4;
    uint8_t version:4;
    uint8_t tos;
    uint16_t tot_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t check;
    uint32_t saddr;
    uint32_t daddr;
};
static_assert(sizeof(iphdr) == 20);

struct tcphdr
{
    uint16_t source;
    uint16_t dest;
    uint32_t seq;
    uint32_t ack_seq;
    uint16_t res1:4;
    uint16_t doff:4;
    uint16_t fin:1;
    uint16_t syn:1;
    uint16_t rst:1;
    uint16_t psh:1;
    uint16_t ack:1;
    uint16_t urg:1;
    uint16_t res2:2;
    uint16_t window;
    uint16_t check;
    uint16_t urg_ptr;
};
static_assert(sizeof(tcphdr) == 20);

#   pragma pack()

#else
#   include <netinet/ip.h>
#   include <netinet/tcp.h>
#endif

using namespace std;

unique_ptr<PacketCapture> PacketCapture::create()
{
#ifdef Q_OS_WIN
    return make_unique<WinDivert>();
#else
    return make_unique<PCap>();
#endif
}

PacketCapture::PacketCapture()
{
}
PacketCapture::~PacketCapture()
{
}

void PacketCapture::reset()
{
    m_hasConnection = false;
    m_reassembly.clear();
}

void PacketCapture::processPacket(const uint8_t *packet, qsizetype len)
{
    if (len < sizeof(iphdr))
    {
        qCritical() << "Packet too short";
        return;
    }
    const auto ipHeader = reinterpret_cast<const iphdr *>(packet);
    packet += ipHeader->ihl * sizeof(uint32_t);
    len -= ipHeader->ihl * sizeof(uint32_t);
    if (ipHeader->version != 4)
    {
        // Not IPv4
        return;
    }
    if (ipHeader->protocol != 6)
    {
        // Not TCP
        return;
    }

    if (len < sizeof(tcphdr))
    {
        qCritical() << "Packet too short";
        return;
    }
    const auto tcpHeader = reinterpret_cast<const tcphdr *>(packet);
    packet += tcpHeader->doff * sizeof(uint32_t);
    len -= tcpHeader->doff * sizeof(uint32_t);

    if (len < 0)
    {
        qCritical() << "Packet too short";
        return;
    }

    const uint32_t srcIp = ntohl(ipHeader->saddr);
    const uint32_t dstIp = ntohl(ipHeader->daddr);
    const uint16_t dstPort = ntohs(tcpHeader->dest);
    const uint32_t seq = ntohl(tcpHeader->seq);

    if (!m_hasConnection || tcpHeader->syn)
    {
#ifdef QT_DEBUG
        if (tcpHeader->syn)
            qDebug() << "TCP connection started";
#endif
        m_hasConnection = true;
        m_srcIp = srcIp;
        m_dstIp = dstIp;
        m_dstPort = dstPort;
        m_seq = seq + (tcpHeader->syn ? 1 : 0);
        m_reassembly.clear();
    }

    if (m_srcIp != srcIp || m_dstIp != dstIp)
    {
#ifdef QT_DEBUG
        qDebug() << "Ignoring different IP (connections with more servers?)";
#endif
        return;
    }

    if (m_dstPort != dstPort)
    {
#ifdef QT_DEBUG
        qDebug() << "Ignoring different TCP destination port (too many connections with server?)";
#endif
        return;
    }

    if (tcpHeader->fin || tcpHeader->rst)
    {
#ifdef QT_DEBUG
        qDebug() << "TCP connection finished / reset";
#endif
        reset();
        return;
    }

    if (!tcpHeader->psh && len == 6 && memcmp(packet, "\0\0\0\0\0", 6) == 0)
    {
        // 6 bytes is padding here
        return;
    }

    if (len == 0)
    {
        return;
    }

    if (!maybeNewPacket(seq, packet, len, true))
    {
        auto &[storedSeq, arr] = m_reassembly.emplace_back();
        storedSeq = seq;
        arr.insert(arr.begin(), packet, packet + len);

#ifdef QT_DEBUG
        qDebug() << "TCP packet buffered for reassembly, seq:" << seq;
#endif

        return;
    }

    if (!m_reassembly.empty())
    {
        for (auto &&[storedSeq, arr] : m_reassembly)
        {
            maybeNewPacket(storedSeq, arr.data(), arr.size(), false);
        }
        m_reassembly.clear();
#ifdef QT_DEBUG
        qDebug() << "Cleared all buffered TCP packets";
#endif
    }
}

bool PacketCapture::maybeNewPacket(uint32_t seq, const uint8_t *data, uint32_t dataSize, bool currPkt)
{
    // FIXME: What if sequence integer overflows?

    if (m_seq < seq || m_seq >= seq + dataSize)
        return false;

    const uint32_t seqDiff = m_seq - seq;

#ifdef QT_DEBUG
    if (!currPkt)
        qDebug() << "TCP packet reassembly finished, seq:" << seq << "+" << seqDiff;
    else if (seqDiff > 0)
        qDebug() << "TCP packet with offset, seq:" << seq << "+" << seqDiff;
#else
    Q_UNUSED(currPkt)
#endif

    const auto size = dataSize - seqDiff;
    emit newPacket(data + seqDiff, size);
    m_seq += size;

    return true;
}
