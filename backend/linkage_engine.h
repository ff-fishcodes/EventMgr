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
// 支持两种注册模式：
//   1. registerHandler(type, handler)
//      → 全局 handler，匹配所有 protocolID 的该类型动作
//      → 适用于 LockUI / UnlockUI / Buzzer 等通用动作
//
//   2. registerHandler(type, protocolID, handler)
//      → 仅匹配指定 protocolID 的该类型动作
//      → 适用于 SendCommand，每个下位机单例独立注册
//
// 分发时先查全局 handler，再查 protocolID 对应的 handler，两者都执行。
// ============================================================
class LinkageEngine {
public:
    // handler 签名: 接收事件和具体的联动动作
    using ActionHandler = std::function<void(const Event&, const LinkageAction&)>;

    // ========= 注册 =========

    // 全局注册：匹配所有 protocolID
    void registerHandler(LinkageAction::Type type, ActionHandler handler);

    // 按 protocolID 注册：仅匹配指定下位机的事件
    void registerHandler(LinkageAction::Type type, int protocolID,
                         ActionHandler handler);

    // ========= 执行 =========

    void executeActive(const Event& event);
    void executeCleared(const Event& event);

    // 清空所有已注册的 handler（用于测试）
    void clearAll();

private:
    void executeActions(const Event& event,
                        const std::vector<LinkageAction>& actions);

    // 分发单个 action 到所有匹配的 handler
    void dispatchAction(const Event& event, const LinkageAction& action);

    // 类型 key: (actionType, protocolID)
    // protocolID = -1 表示全局 handler（匹配所有 protocolID）
    using HandlerKey = std::pair<int, int>;

    struct KeyHash {
        std::size_t operator()(const HandlerKey& k) const {
            return std::hash<int>()(k.first) ^
                   (std::hash<int>()(k.second) << 1);
        }
    };

    std::unordered_map<HandlerKey, std::vector<ActionHandler>, KeyHash> handlers_;
};

#endif // LINKAGE_ENGINE_H
