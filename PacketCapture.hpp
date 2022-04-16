#pragma once

#include <QObject>

class PacketCapture : public QObject
{
    Q_OBJECT

public:
    static std::unique_ptr<PacketCapture> create();

public:
    PacketCapture();
    virtual ~PacketCapture();

    virtual bool init(uint16_t port) = 0;

    void reset();

protected:
    virtual void processPacket(const uint8_t *packet, qsizetype len);

private:
    bool maybeNewPacket(uint32_t seq, const uint8_t *data, uint32_t dataSize, bool currPkt);

signals:
    void newPacket(const uint8_t *data, qsizetype len); // Must be direct connection

private:
    bool m_hasConnection = false;
    uint32_t m_srcIp = 0;
    uint32_t m_dstIp = 0;
    uint16_t m_dstPort = 0;
    uint32_t m_seq = 0;
    std::vector<std::pair<uint32_t, std::vector<uint8_t>>> m_reassembly;
};
