#include "linkage_engine.h"

namespace {
    const int GLOBAL_PROTOCOL = -1;
}

// ============================================================
// handler 注册
// ============================================================

void LinkageEngine::registerHandler(LinkageAction::Type type,
                                     ActionHandler handler) {
    HandlerKey key(static_cast<int>(type), GLOBAL_PROTOCOL);
    handlers_[key].push_back(handler);
}

void LinkageEngine::registerHandler(LinkageAction::Type type,
                                     int protocolID,
                                     ActionHandler handler) {
    HandlerKey key(static_cast<int>(type), protocolID);
    handlers_[key].push_back(handler);
}

// ============================================================
// 事件联动配置
// ============================================================

void LinkageEngine::configureEvent(
        const EventId& eventId,
        const std::vector<LinkageAction>& activeActions,
        const std::vector<LinkageAction>& clearActions) {
    eventConfig_[eventId] = std::make_pair(activeActions, clearActions);
}

// ============================================================
// 解析 actions：配置表优先，Event 自带兜底
// ============================================================

const std::vector<LinkageAction>& LinkageEngine::resolveActiveActions(const Event& event) {
    std::unordered_map<EventId,
        std::pair<std::vector<LinkageAction>, std::vector<LinkageAction> > >::const_iterator
        it = eventConfig_.find(event.id);
    if (it != eventConfig_.end()) {
        return it->second.first;   // 预配置的 activeActions
    }
    return event.activeActions;    // fallback
}

const std::vector<LinkageAction>& LinkageEngine::resolveClearActions(const Event& event) {
    std::unordered_map<EventId,
        std::pair<std::vector<LinkageAction>, std::vector<LinkageAction> > >::const_iterator
        it = eventConfig_.find(event.id);
    if (it != eventConfig_.end()) {
        return it->second.second;  // 预配置的 clearActions
    }
    return event.clearActions;     // fallback
}

// ============================================================
// 执行入口
// ============================================================

void LinkageEngine::executeActive(const Event& event) {
    executeActions(event, resolveActiveActions(event));
}

void LinkageEngine::executeCleared(const Event& event) {
    executeActions(event, resolveClearActions(event));
}

void LinkageEngine::executeActions(const Event& event,
                                    const std::vector<LinkageAction>& actions) {
    for (std::vector<LinkageAction>::const_iterator it = actions.begin();
         it != actions.end(); ++it) {
        dispatchAction(event, *it);
    }
}

// ============================================================
// 分发：按 targetProtocolID 路由
// ============================================================

int LinkageEngine::resolveProtocolID(const Event& event, const LinkageAction& action) {
    // SendCommand 且指定了 targetProtocolID → 路由到目标下位机
    if (action.type == LinkageAction::SendCommand && action.targetProtocolID > 0) {
        return action.targetProtocolID;
    }
    // 否则使用事件源下位机
    return event.protocolID;
}

void LinkageEngine::dispatchAction(const Event& event,
                                    const LinkageAction& action) {
    int typeKey = static_cast<int>(action.type);

    // 1. 全局 handler（protocolID = -1）
    HandlerKey globalKey(typeKey, GLOBAL_PROTOCOL);
    std::unordered_map<HandlerKey, std::vector<ActionHandler>, KeyHash>::iterator
        globalIt = handlers_.find(globalKey);
    if (globalIt != handlers_.end()) {
        const std::vector<ActionHandler>& list = globalIt->second;
        for (std::vector<ActionHandler>::const_iterator hit = list.begin();
             hit != list.end(); ++hit) {
            (*hit)(event, action);
        }
    }

    // 2. per-protocolID handler
    int protocolID = resolveProtocolID(event, action);
    HandlerKey deviceKey(typeKey, protocolID);
    std::unordered_map<HandlerKey, std::vector<ActionHandler>, KeyHash>::iterator
        deviceIt = handlers_.find(deviceKey);
    if (deviceIt != handlers_.end()) {
        const std::vector<ActionHandler>& list = deviceIt->second;
        for (std::vector<ActionHandler>::const_iterator hit = list.begin();
             hit != list.end(); ++hit) {
            (*hit)(event, action);
        }
    }
}

void LinkageEngine::clearAll() {
    handlers_.clear();
    eventConfig_.clear();
}
