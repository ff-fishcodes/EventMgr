#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "event_types.h"
#include <functional>
#include <string>
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
    static ConfigManager& instance();
    static void setInstance(ConfigManager* mgr);

    ConfigManager() {}
    ~ConfigManager() {}

    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    // ========= 降级相关 =========

    // 设置降级：默认降至最低等级 Info，也可显式指定目标等级
    void setDowngrade(const EventId& id,
                      EventLevel newLevel = EventLevel::Info);

    // 取消降级
    void removeDowngrade(const EventId& id);

    // 获取有效等级：若有降级配置则返回降级后等级，否则返回 originalLevel
    EventLevel getEffectiveLevel(const EventId& id, EventLevel originalLevel) const;

    // 查询是否存在降级配置
    bool hasDowngrade(const EventId& id) const;

    // ========= 屏蔽相关 =========

    // 设置屏蔽：该事件的 UI 通知将被抑制
    void setShield(const EventId& id);

    // 取消屏蔽
    void clearShield(const EventId& id);

    // 查询是否被屏蔽
    bool isShielded(const EventId& id) const;

    // 获取当前被屏蔽的事件数量（用于前端屏蔽计数提示）
    int getShieldCount() const;

    // ========= 联动开关相关 =========

    void setActionEnabled(const EventId& eventId,
                          const std::string& actionName,
                          bool isActive, bool enabled);
    bool isActionEnabled(const EventId& eventId,
                         const std::string& actionName,
                         bool isActive) const;

    // ========= 批量操作 =========

    // 清空所有配置（用于测试/重启）
    void clearAll();

private:
    struct ActionSwitchKey {
        EventId eventId;
        std::string actionName;

        ActionSwitchKey(const EventId& id, const std::string& name)
            : eventId(id), actionName(name) {}

        bool operator==(const ActionSwitchKey& other) const {
            return eventId == other.eventId && actionName == other.actionName;
        }
    };

    struct ActionSwitchKeyHash {
        size_t operator()(const ActionSwitchKey& key) const {
            const size_t h1 = std::hash<std::string>()(key.eventId);
            const size_t h2 = std::hash<std::string>()(key.actionName);
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };

    // 降级映射: EventId → 降级后的等级
    std::unordered_map<EventId, EventLevel> downgradeMap_;

    // 屏蔽集合: 被屏蔽的 EventId
    std::unordered_set<EventId> shieldSet_;

    // 仅保存关闭项；未出现的联动默认启用。
    std::unordered_set<ActionSwitchKey, ActionSwitchKeyHash>
        disabledActiveActions_;
    std::unordered_set<ActionSwitchKey, ActionSwitchKeyHash>
        disabledClearActions_;

    mutable QMutex mutex_;  // 保护配置读写并发

    static ConfigManager* instance_;
};

#endif // CONFIG_MANAGER_H
