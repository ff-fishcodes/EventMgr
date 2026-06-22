#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "event_types.h"
#include <unordered_map>
#include <unordered_set>

// ============================================================
// ConfigManager — 配置管理
// 管理降级和屏蔽两项配置，两者独立作用可叠加
// - 降级：改变 effectiveLevel，影响联动行为
// - 屏蔽：控制是否通知前端，不影响联动和日志
// ============================================================
class ConfigManager {
public:
    // ========= 降级相关 =========

    // 设置降级：将指定事件的报警等级降级为 newLevel
    void setDowngrade(const EventId& id, EventLevel newLevel);

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

    // ========= 批量操作 =========

    // 清空所有配置（用于测试/重启）
    void clearAll();

private:
    // 降级映射: EventId → 降级后的等级
    std::unordered_map<EventId, EventLevel> downgradeMap_;

    // 屏蔽集合: 被屏蔽的 EventId
    std::unordered_set<EventId> shieldSet_;
};

#endif // CONFIG_MANAGER_H
