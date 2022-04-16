#include "SWPacketCapture.hpp"
#include "SWPacketStructs.hpp"

#include <QtEndian>
#include <QDebug>

using namespace std;
using namespace Packet;

constexpr auto g_headerSize = sizeof(Header);

SWPacketCapture::SWPacketCapture(QObject *parent)
    : QObject(parent)
{
}
SWPacketCapture::~SWPacketCapture()
{
}

void SWPacketCapture::newPacket(const uint8_t *data, qsizetype len)
{
    while (len > 0)
    {
        if (m_data.size() < g_headerSize)
        {
            const auto toReserve = max<qsizetype>(len, g_headerSize);
            if (m_data.capacity() < toReserve)
                m_data.reserve(toReserve);

            const int toCopy = min<qsizetype>(len, g_headerSize);
            m_data.insert(m_data.end(), data, data + toCopy);
            data += toCopy;
            len -= toCopy;
        }

        if (m_data.size() < g_headerSize)
        {
#ifdef QT_DEBUG
            qDebug() << "Segmented packet, waiting for header:" << m_data.size();
#endif
            break;
        }

        auto header = reinterpret_cast<const Header *>(m_data.data());

        if (header->magic != 2)
        {
#ifdef QT_DEBUG
            qWarning() << "Invalid packet:" << header->magic;
#endif
            m_data.clear();
            break;
        }

        if (m_data.capacity() < header->size)
        {
            m_data.reserve(header->size);
            header = reinterpret_cast<const Header *>(m_data.data());
        }

        const int toCopy = min<qsizetype>(len, header->size - m_data.size());
        if (toCopy > 0)
        {
            m_data.insert(m_data.end(), data, data + toCopy);
            data += toCopy;
            len -= toCopy;
        }

        if (m_data.size() < header->size)
        {
#ifdef QT_DEBUG
            qDebug() << "Segmented packet:" << m_data.size() << header->size;
#endif
            break;
        }

        if (header->type != 1)
        {
            m_data.clear();
            continue;
        }

        qsizetype dataSize = header->size - sizeof(Header);
        if (dataSize < sizeof(uint16_t))
        {
#ifdef QT_DEBUG
            qWarning() << "Packet too short";
#endif
            m_data.clear();
            break;
        }

        auto data = m_data.data() + sizeof(Header);

        decrypt(data, dataSize);

        const auto op = static_cast<OpCode>(qbswap(*reinterpret_cast<const uint16_t *>(data)));
        data += sizeof(OpCode);
        dataSize -= sizeof(OpCode);

        switch (op)
        {
            case OpCode::WorldChange:
                processWorldChangePacket(data, dataSize);
                break;
            case OpCode::ObjectCreate:
                processObjectCreatePacket(data, dataSize);
                break;
            case OpCode::Damage:
                processDamagePacket(data, dataSize);
                break;
            case OpCode::Akasic:
                processAkasicPacket(data, dataSize);
                break;
            case OpCode::MazeEnd:
                emit mazeEnd();
                break;
            case OpCode::Party:
            case OpCode::Force:
                processPartyPacket(data, dataSize);
                break;
            default:
            {
#if defined(QT_DEBUG) && 0
                const auto opInt = static_cast<uint16_t>(op);
                if (opInt != 0x0106)
                {
                    qDebug().noquote().nospace() << QString("0x%1").arg(opInt, 4, 16, QLatin1Char('0')) << ", size: " << dataSize;
                }
#endif
                break;
            }
        }

        m_data.clear();
    }
}

void SWPacketCapture::decrypt(uint8_t *data, qsizetype size)
{
    constexpr uint8_t xorTable[3] = {0x60, 0x3B, 0x0B};

    for (qsizetype i = 0; i < size; ++i)
        data[i] ^= xorTable[i % sizeof(xorTable)];
}

void SWPacketCapture::processWorldChangePacket(uint8_t *data, qsizetype len)
{
    if (len < sizeof(WorldChange))
        return;

    const auto packet = reinterpret_cast<const WorldChange *>(data);
    emit worldChange(packet->id, packet->worldId);
}
void SWPacketCapture::processObjectCreatePacket(uint8_t *data, qsizetype len)
{
    if (len < sizeof(ObjectCreate))
        return;

    const auto packet = reinterpret_cast<const ObjectCreate *>(data);
    emit ownerId(packet->id, packet->owner_id);
}
void SWPacketCapture::processDamagePacket(uint8_t *data, qsizetype len)
{
    if (len < sizeof(uint8_t))
        return;

    const auto nMonsters = data[0];
    data += sizeof(uint8_t);
    len -= sizeof(uint8_t);

    if (len < sizeof(DamageMonster) * nMonsters + sizeof(DamagePlayer))
        return;

    const auto damagePlayer = reinterpret_cast<const DamagePlayer *>(data + sizeof(DamageMonster) * nMonsters);

    for (uint32_t i = 0; i < nMonsters; ++i)
    {
        const auto damageMonster = reinterpret_cast<const DamageMonster *>(data);
        data += sizeof(DamageMonster);
        len -= sizeof(DamageMonster);

        const bool isMiss = (damageMonster->damageType & 0x01);
        const bool isCrit = (damageMonster->damageType & 0x04);
        emit damage(
            damagePlayer->playerId,
            damagePlayer->maxCombo,
            damageMonster->monsterId,
            damageMonster->totalDmg,
            damageMonster->soulstoneDmg,
            isMiss,
            isCrit
        );
    }
}
void SWPacketCapture::processAkasicPacket(uint8_t *data, qsizetype len)
{
    if (len < sizeof(Akasic))
        return;

    const auto packet = reinterpret_cast<const Akasic *>(data);
    emit ownerId(packet->id, packet->owner_id);
}
void SWPacketCapture::processPartyPacket(uint8_t *data, qsizetype len)
{
    if (len < sizeof(PartyHeader))
        return;

    const auto partyHeader = reinterpret_cast<const PartyHeader *>(data);
    data += sizeof(PartyHeader);
    len -= sizeof(PartyHeader);

    for (uint32_t i = 0; i < partyHeader->partyPlayerCount; ++i)
    {
        if (len < sizeof(PartyData))
            return;

        const auto partyData = reinterpret_cast<const PartyData *>(data);
        data += sizeof(PartyData);
        len -= sizeof(PartyData);

        if (len < partyData->nickSize)
            return;

        const auto nick = QString::fromUtf16(partyData->nick, partyData->nickSize / sizeof(char16_t));
        data += partyData->nickSize;
        len -= partyData->nickSize;

        if (len < sizeof(uint8_t) * 2)
            return;

        const auto characterClass = reinterpret_cast<const uint8_t *>(data)[1];

        data += PartyDataUnknownSize;
        len -= PartyDataUnknownSize;

        emit partyMember(partyData->playerId, nick, characterClass);
    }
}
