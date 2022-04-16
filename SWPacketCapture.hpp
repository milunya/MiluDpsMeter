#pragma once

#include <QObject>

class SWPacketCapture : public QObject
{
    Q_OBJECT

public:
    SWPacketCapture(QObject *parent = nullptr);
    ~SWPacketCapture();

    bool init();

    void newPacket(const uint8_t *data, qsizetype len);

private:
    void decrypt(uint8_t *data, qsizetype size);

    void processWorldChangePacket(uint8_t *data, qsizetype len);
    void processObjectCreatePacket(uint8_t *data, qsizetype len);
    void processDamagePacket(uint8_t *data, qsizetype len);
    void processAkasicPacket(uint8_t *data, qsizetype len);
    void processPartyPacket(uint8_t *data, qsizetype len);
    void processForcePacket(uint8_t *data, qsizetype len);

signals:
    void worldChange(uint32_t id, uint32_t worldId);
    void ownerId(uint32_t id, uint32_t ownerId);
    void damage(uint32_t srcId, uint16_t combo, uint32_t dstId, uint32_t dmg, uint32_t ssDmg, bool miss, bool crit);
    void mazeEnd();
    void partyMember(uint32_t id, const QString &nick, uint8_t characterClass);

private:
    std::vector<uint8_t> m_data;
};
