#ifndef LINKAGE_ENGINE_H
#define LINKAGE_ENGINE_H

#include "event_types.h"
#include <functional>
#include <unordered_map>
#include <vector>
#include <utility>

// ============================================================
// LinkageEngine — 联动引擎（回调注册制）
//
// 三层注册模式：
//
//   1. registerHandler(type, handler)
//      → 全局 handler，匹配所有 protocolID 的该类型动作
//      → 适用于 LockUI / UnlockUI / Buzzer 等通用动作
//
//   2. registerHandler(type, protocolID, handler)
//      → 仅匹配指定 protocolID 的该类型动作
//      → 适用于 SendCommand，每个下位机单例独立注册
//
//   3. configureEvent(eventId, activeActions, clearActions)
//      → 启动时预配置"什么事件→干什么动作"
//      → 运行时 addEvent 自动查此表，Event 对象可以不携带 actions
//      → 若 findEvent 查不到，则 fallback 到 Event 自带的 actions
//
//   4. setLevelDefault(level, actions)
//      → 按等级绑定默认动作，所有该等级事件自动附加上
//      → 例如所有 Emergency 事件统一向指定设备发送停机指令
//
// 运行时 resolveActiveActions = 配置表 actions + 等级默认 actions
//
// 分发路由：
//   - 全局 handler（protocolID=-1）和 per-protocolID handler 两者都执行
//   - SendCommand 用 action.targetProtocolID（>0时），否则用 event.protocolID
// ============================================================
class LinkageEngine {
public:
    using ActionHandler = std::function<void(const Event&, const LinkageAction&)>;

    // ========= handler 注册 =========

    // 全局注册：匹配所有 protocolID
    void registerHandler(LinkageAction::Type type, ActionHandler handler);

    // 按 protocolID 注册：仅匹配指定下位机
    void registerHandler(LinkageAction::Type type, int protocolID,
                         ActionHandler handler);

    // ========= 事件联动配置 =========

    // 启动时一次性配置：指定某个 EventId 的联动动作
    // 运行时 addEvent 自动查此表，优先于 Event 自带的 actions
    void configureEvent(const EventId& eventId,
                        const std::vector<LinkageAction>& activeActions,
                        const std::vector<LinkageAction>& clearActions);

    // 按等级设置默认联动动作：所有该等级事件自动附加上这些 actions
    void setLevelDefault(EventLevel level,
                         const std::vector<LinkageAction>& activeActions);

    // ========= 执行 =========

    void executeActive(const Event& event);
    void executeCleared(const Event& event);

    // 清空所有已注册的 handler 和事件配置（用于测试）
    void clearAll();

private:
    // 解析 event 对应的 actions：配置表 + 等级默认 + fallback 到 event 自带
    std::vector<LinkageAction> resolveActiveActions(const Event& event);

    // 解析清除侧 actions：配置表 + 自动 mirror active 中的 LockUI → UnlockUI
    std::vector<LinkageAction> resolveClearActions(const Event& event);

    void executeActions(const Event& event,
                        const std::vector<LinkageAction>& actions);

    // 分发单个 action：按 targetProtocolID 路由
    void dispatchAction(const Event& event, const LinkageAction& action);

    // 获取分发用的 protocolID：SendCommand 用 targetProtocolID，其他用 event 源
    static int resolveProtocolID(const Event& event, const LinkageAction& action);

    // ========= handler 存储 =========
    // Key: (actionType, protocolID), protocolID=-1 表示全局
    using HandlerKey = std::pair<int, int>;

    struct KeyHash {
        std::size_t operator()(const HandlerKey& k) const {
            return std::hash<int>()(k.first) ^
                   (std::hash<int>()(k.second) << 1);
        }
    };

    std::unordered_map<HandlerKey, std::vector<ActionHandler>, KeyHash> handlers_;

    // ========= 事件联动配置表 =========
    // EventId → (activeActions, clearActions)
    std::unordered_map<EventId,
        std::pair<std::vector<LinkageAction>, std::vector<LinkageAction> > > eventConfig_;

    // ========= 等级默认动作 =========
    // EventLevel → 默认 active actions
    std::unordered_map<int, std::vector<LinkageAction> > levelDefaults_;
};

#endif // LINKAGE_ENGINE_H
