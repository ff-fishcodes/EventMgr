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

void LinkageEngine::setLevelDefault(EventLevel level,
                                     const std::vector<LinkageAction>& activeActions) {
    levelDefaults_[static_cast<int>(level)] = activeActions;
}

// ============================================================
// 解析 actions：配置表 + 等级默认 + Event 自带兜底
// ============================================================

std::vector<LinkageAction> LinkageEngine::resolveActiveActions(const Event& event) {
    std::vector<LinkageAction> result;

    // 1. 事件级配置表（或 Event 自带 actions）
    std::unordered_map<EventId,
        std::pair<std::vector<LinkageAction>, std::vector<LinkageAction> > >::const_iterator
        it = eventConfig_.find(event.id);
    const std::vector<LinkageAction>& base =
        (it != eventConfig_.end()) ? it->second.first : event.activeActions;
    result.insert(result.end(), base.begin(), base.end());

    // 2. 等级默认动作（如所有 Emergency 统一向指定设备发停机指令）
    std::unordered_map<int, std::vector<LinkageAction> >::const_iterator
        levelIt = levelDefaults_.find(static_cast<int>(event.effectiveLevel));
    if (levelIt != levelDefaults_.end()) {
        result.insert(result.end(), levelIt->second.begin(), levelIt->second.end());
    }

    return result;
}

std::vector<LinkageAction> LinkageEngine::resolveClearActions(const Event& event) {
    std::vector<LinkageAction> result;

    std::unordered_map<EventId,
        std::pair<std::vector<LinkageAction>, std::vector<LinkageAction> > >::const_iterator
        it = eventConfig_.find(event.id);

    const std::vector<LinkageAction>* explicitClear = NULL;
    const std::vector<LinkageAction>* activeSrc    = NULL;

    if (it != eventConfig_.end()) {
        explicitClear = &it->second.second;
        activeSrc     = &it->second.first;
    } else {
        explicitClear = &event.clearActions;
        activeSrc     = &event.activeActions;
    }

    // 1. 复制显式配置的清除动作
    result.insert(result.end(), explicitClear->begin(), explicitClear->end());

    // 2. 自动 mirror：active 中每个 LockUI 自动生成对应的 UnlockUI
    for (std::vector<LinkageAction>::const_iterator i = activeSrc->begin();
         i != activeSrc->end(); ++i) {
        if (i->type == LinkageAction::LockUI) {
            result.push_back(LinkageAction(
                LinkageAction::UnlockUI, i->target, "", i->targetProtocolID));
        }
    }

    return result;
}

// ============================================================
// 执行入口
// ============================================================

void LinkageEngine::executeActive(const Event& event) {
    if (event.effectiveLevel == EventLevel::Info) return;   // 提示等级不联动
    executeActions(event, resolveActiveActions(event));
}

void LinkageEngine::executeCleared(const Event& event) {
    if (event.effectiveLevel == EventLevel::Info) return;   // 提示等级不联动
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
    levelDefaults_.clear();
}
