#include "PCap.hpp"

#include <QDebug>

#include <net/ethernet.h>

#pragma pack(1)

struct LinuxCookedCapture
{
    uint16_t packet_type;
    uint16_t address_type;
    uint16_t address_length;
    uint8_t source_address[6];
    uint16_t unused;
    uint16_t protocol;
};
static_assert(sizeof(LinuxCookedCapture) == 16);

#pragma pack()

/**/

PCap::PCap()
    : m_socketNotifier(QSocketNotifier::Read)
{
    connect(&m_socketNotifier, &QSocketNotifier::activated,
            this, &PCap::activated);
}
PCap::~PCap()
{
    closeHandle();
}

bool PCap::init(uint16_t port)
{
    closeHandle();

    char errbuf[PCAP_ERRBUF_SIZE] = {};
    m_handle = pcap_open_live(nullptr, 65535, false, 100, errbuf);
    if (!m_handle)
    {
        qCritical() << errbuf;
        return false;
    }

    const auto filterStr = QString("tcp port %1").arg(port).toLatin1();

    bpf_program fp = {};
    if (pcap_compile(m_handle, &fp, filterStr, true, PCAP_NETMASK_UNKNOWN) == -1)
    {
        qCritical() << pcap_geterr(m_handle);
        return false;
    }

    if (pcap_setfilter(m_handle, &fp) == -1)
    {
        qCritical() << pcap_geterr(m_handle);
        return false;
    }

    if (pcap_setnonblock(m_handle, true, errbuf) == -1)
    {
        qCritical() << pcap_geterr(m_handle);
        return false;
    }

    const int fd = pcap_get_selectable_fd(m_handle);
    if (fd < 0)
        return false;

    m_socketNotifier.setSocket(fd);
    m_socketNotifier.setEnabled(true);

    return true;
}

void PCap::closeHandle()
{
    if (!m_handle)
        return;

    pcap_close(m_handle);
    m_handle = nullptr;
}

void PCap::activated()
{
    pcap_pkthdr header = {};
    for (;;)
    {
        const auto packet = pcap_next(m_handle, &header);
        if (!packet)
            break;

        if (header.caplen < 1 || header.len < 1)
            continue;

        if (header.caplen < header.len)
        {
            qCritical() << "Pcap buffer to small";
            continue;
        }

        processPacket(packet, header.len);
    }
}

void PCap::processPacket(const uint8_t *packet, qsizetype len)
{
    if (len < sizeof(LinuxCookedCapture))
    {
        qCritical() << "Packet too short";
        return;
    }
    const auto linuxCookedCapture = reinterpret_cast<const LinuxCookedCapture *>(packet);
    if (ntohs(linuxCookedCapture->packet_type) != 0) // 0 = Unicast to us
    {
        return;
    }
    if (ntohs(linuxCookedCapture->address_length) != sizeof(LinuxCookedCapture::source_address))
    {
        return;
    }
    if (ntohs(linuxCookedCapture->protocol) != ETHERTYPE_IP)
    {
        return;
    }
    packet += sizeof(LinuxCookedCapture);
    len -= sizeof(LinuxCookedCapture);

    PacketCapture::processPacket(packet, len);
}
