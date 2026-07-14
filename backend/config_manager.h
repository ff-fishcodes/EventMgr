#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "event_types.h"
#include <unordered_map>
#include <unordered_set>
#include <QMutex>

// ============================================================
// ConfigManager — 配置管理（线程安全）
// 管理降级和屏蔽两项配置，两者独立作用可叠加
// - 降级：改变 effectiveLevel，影响联动行为
// - 屏蔽：控制是否通知前端，不影响联动和日志
// ============================================================
class ConfigManager {
public:
    ConfigManager() {}
    ~ConfigManager() {}

    void setDowngrade(const EventId& id, EventLevel newLevel);
    void removeDowngrade(const EventId& id);
    EventLevel getEffectiveLevel(const EventId& id, EventLevel originalLevel) const;
    bool hasDowngrade(const EventId& id) const;

    void setShield(const EventId& id);
    void clearShield(const EventId& id);
    bool isShielded(const EventId& id) const;
    int getShieldCount() const;

    void clearAll();

private:
    std::unordered_map<EventId, EventLevel> downgradeMap_;
    std::unordered_set<EventId> shieldSet_;

    mutable QMutex mutex_;  // 保护配置读写并发
};

#endif // CONFIG_MANAGER_H
