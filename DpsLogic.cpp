#include "DpsLogic.hpp"

#include <QDebug>

using namespace std;

DpsLogic::DpsLogic(QObject *parent)
    : QObject(parent)
{
    m_timer.setInterval(40);
    connect(&m_timer, &QTimer::timeout,
            this, &DpsLogic::update);

    m_cityIds = {
        10002,
        10003,
        10021,
        10031,
        10041,
        11001,
        10051,
        10061,
    };
}
DpsLogic::~DpsLogic()
{
}

void DpsLogic::suspend(bool autoResume)
{
    if (isSuspended() || !isValid())
    {
        if (isAutoResume() && !autoResume)
            m_autoResume = false;
        return;
    }

    m_timer.stop();

    m_suspendPoint = m_elapsedTimer.nsecsElapsed();
    m_suspended = true;
    m_autoResume = autoResume;

    doUpdate(false);
}
void DpsLogic::resume()
{
    if (!isSuspended() || !isValid())
        return;

    m_timer.start();

    m_suspendTime += (m_elapsedTimer.nsecsElapsed() - m_suspendPoint) / 1e9;
    m_suspended = false;

    m_worldId = m_curWorldId;
}

void DpsLogic::reset()
{
    m_elapsedTimer.invalidate();
    m_timer.stop();

    m_suspendTime = 0.0;
    m_suspendPoint = 0;
    m_suspended = false;
    m_autoResume = true;

    m_worldId = 0;

    m_playerStats.clear();
}

double DpsLogic::getTime() const
{
    if (!isValid())
        return qQNaN();

    int64_t time;
    if (isSuspended())
        time = m_suspendPoint;
    else
        time = m_elapsedTimer.nsecsElapsed();
    return time / 1e9 - m_suspendTime;
}

void DpsLogic::iterate(const IterateCallback &cb) const
{
    if (!cb)
        return;

    vector<pair<uint64_t, uint32_t>> dmgPerPlayer;
    dmgPerPlayer.reserve(m_playerStats.size());

    uint64_t totalDamage = 0;
    for (auto &&[id, playerStats] : m_playerStats)
    {
        dmgPerPlayer.emplace_back(playerStats->damage, id);
        totalDamage += playerStats->damage;
    }

    stable_sort(dmgPerPlayer.begin(), dmgPerPlayer.end(), [](auto &&a, auto &&b) {
        return (b.first < a.first);
    });

    uint32_t idx = 0;
    for (auto &&dmgPerPlayer : dmgPerPlayer)
    {
        const auto id = dmgPerPlayer.second;
        const auto &playerStats = m_playerStats.find(id)->second;

        QString playerName;
        uint8_t characterClass = 0;

        const auto it = m_players.find(id);
        const auto playerFound = (it != m_players.end());

        if (playerFound)
            characterClass = it->second.second;

        if (id == m_myId)
            playerName = "[YOU]";
        else if (playerFound)
            playerName = it->second.first;
        else
            playerName = QString::number(id);

        cb(idx++, playerName, characterClass, totalDamage, *playerStats);
    }
}

void DpsLogic::worldChange(uint32_t id, uint32_t worldId)
{
    m_myId = id;
    m_curWorldId = worldId;

    const bool isCity = (m_cityIds.find(worldId) != m_cityIds.end());
    if (isCity)
    {
        suspend(true);
    }
    else if (!isSuspendedCantResume())
    {
        reset();
        m_worldId = m_curWorldId;
    }
    m_ownerIds.clear();

    doUpdate(false);
}
void DpsLogic::ownerId(uint32_t id, uint32_t ownerId)
{
    m_ownerIds[id] = ownerId;
}
void DpsLogic::damage(uint32_t srcId, uint16_t combo, uint32_t dstId, uint32_t dmg, uint32_t ssDmg, bool miss, bool crit)
{
    constexpr uint32_t notPlayerId = 1073741824;

    if (isSuspendedCantResume())
        return;

    bool isDamageFromPlayer = true;

    if (const auto it = m_ownerIds.find(srcId); it != m_ownerIds.end())
        srcId = it->second;

    if (srcId >= notPlayerId)
    {
        if (dstId >= notPlayerId)
            return;

        swap(srcId, dstId);
        isDamageFromPlayer = false;
    }

    auto &playerStats = m_playerStats[srcId];
    if (!playerStats)
        playerStats = make_unique<PlayerStats>();

    if (isDamageFromPlayer)
    {
        playerStats->maxCombo = max(playerStats->maxCombo, combo);
        if (dmg > 0)
        {
            playerStats->hits += 1;
            playerStats->damage += dmg;
            if (miss)
                playerStats->misses += 1;
            if (crit)
                playerStats->crits += 1;
            if (ssDmg > 0)
                playerStats->soulstones += 1;
        }
    }
    else
    {
        playerStats->damageReceived += dmg;
    }

    makeValid();
    resume();
    doUpdate(true);
}
void DpsLogic::mazeEnd()
{
    suspend(true);
}
void DpsLogic::partyMember(uint32_t id, const QString &nick, uint8_t characterClass)
{
    auto &player = m_players[id];

    if (player.first == nick && player.second == characterClass)
        return;

    player.first = nick;
    player.second = characterClass <= 8 ? characterClass : 0;

    doUpdate(false);
}

void DpsLogic::doUpdate(bool forceRestart)
{
    if (forceRestart || m_timer.isActive())
        m_timer.start();
    emit update();
}

void DpsLogic::makeValid()
{
    if (isValid())
        return;

    m_elapsedTimer.start();
    Q_ASSERT(isValid());
}

inline bool DpsLogic::isSuspendedCantResume() const
{
    return (isSuspended() && !isAutoResume());
}
