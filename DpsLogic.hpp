#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QTimer>

#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <memory>

class DpsLogic : public QObject
{
    Q_OBJECT

public:
    struct PlayerStats
    {
        uint16_t maxCombo = 0;
        uint64_t hits = 0;
        uint64_t damage = 0;
        uint64_t damageReceived = 0;
        uint64_t misses = 0;
        uint64_t crits = 0;
        uint64_t soulstones = 0;
    };

    using IterateCallback = std::function<void(
        uint32_t idx,
        const QString playerName,
        uint8_t characterClass,
        uint64_t totalDamage,
        const PlayerStats &playerStats
    )>;

public:
    DpsLogic(QObject *parent = nullptr);
    ~DpsLogic();

    inline bool isValid() const;
    inline bool isSuspended() const;
    inline bool isAutoResume() const;

    void suspend(bool autoResume);
    void resume();

    void reset();

    double getTime() const;

    inline uint32_t getWorldId() const;
    inline uint32_t getNumPlayers() const;
    void iterate(const IterateCallback &cb) const;

public:
    void worldChange(uint32_t id, uint32_t worldId);
    void ownerId(uint32_t id, uint32_t ownerId);
    void damage(uint32_t srcId, uint16_t combo, uint32_t dstId, uint32_t dmg, uint32_t ssDmg, bool miss, bool crit);
    void mazeEnd();
    void partyMember(uint32_t id, const QString &nick, uint8_t characterClass);

private:
    void doUpdate(bool forceRestart);

    void makeValid();

    inline bool isSuspendedCantResume() const;

signals:
    void update();

private:
    QElapsedTimer m_elapsedTimer;
    QTimer m_timer;

    double m_suspendTime = 0.0;
    int64_t m_suspendPoint = 0;
    bool m_suspended = false;
    bool m_autoResume = true;

    uint32_t m_myId = 0;
    uint32_t m_curWorldId = 0;
    uint32_t m_worldId = 0;

    std::unordered_map<uint32_t, std::pair<QString, uint8_t>> m_players;
    std::unordered_map<uint32_t, std::unique_ptr<PlayerStats>> m_playerStats;
    std::unordered_map<uint32_t, uint32_t> m_ownerIds;

    std::unordered_set<uint32_t> m_cityIds;
};

inline bool DpsLogic::isValid() const
{
    return m_elapsedTimer.isValid();
}
inline bool DpsLogic::isSuspended() const
{
    return m_suspended;
}
inline bool DpsLogic::isAutoResume() const
{
    return m_autoResume;
}

inline uint32_t DpsLogic::getWorldId() const
{
    return (m_worldId == 0) ? m_curWorldId : m_worldId;
}
inline uint32_t DpsLogic::getNumPlayers() const
{
    return m_playerStats.size();
}
