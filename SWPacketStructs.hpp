#pragma once

#include <cstdint>

enum class OpCode : uint16_t
{
    WorldChange = 0x0402,
    ObjectCreate = 0x0415,
    Damage = 0x0613,
    Akasic = 0x067b,
    MazeEnd = 0x1175,
    Party = 0x1209,
    Force = 0x2e09,
};

namespace Packet {

#pragma pack(1)

struct Header
{
    uint16_t magic;
    uint16_t size;
    uint8_t type;
};

struct WorldChange
{
    uint32_t id;
    uint8_t unknown1[20];
    uint16_t worldId;
    uint8_t unknown2[63];
};

struct PartyHeader
{
    uint8_t unknown01[4];
    uint32_t partyHostId;
    uint8_t unknown02[10];
    uint8_t partyPlayerCount;
};
struct PartyData
{
    uint32_t playerId;
    uint16_t nickSize;
    char16_t nick[];
};
constexpr auto PartyDataUnknownSize = 32;

struct DamageMonster
{
    uint32_t monsterId;
    uint8_t unknown01;
    uint8_t damageType;
    uint32_t totalDmg;
    uint32_t soulstoneDmg;
    uint32_t remainHp;
    uint8_t unknown02[22];
};
struct DamagePlayer
{
    uint32_t playerId;
    uint8_t unknown01[20];
    uint32_t skillId;
    uint8_t unknown02[2];
    uint16_t maxCombo;
    uint8_t unknown03[2];
};

struct ObjectCreate
{
    uint8_t unknown01;
    uint32_t id;
    uint8_t unknown02[33];
    uint32_t owner_id;
    uint8_t unknown03[54];
    uint32_t db2;
};

struct Akasic
{
    uint32_t owner_id;
    uint32_t id;
};

#pragma pack()

}
