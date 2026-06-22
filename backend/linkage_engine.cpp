#include "linkage_engine.h"

namespace {
    // protocolID 为 -1 表示全局 handler
    const int GLOBAL_PROTOCOL = -1;
}

void LinkageEngine::registerHandler(LinkageAction::Type type,
                                     ActionHandler handler) {
    // 全局注册：protocolID = -1
    HandlerKey key(static_cast<int>(type), GLOBAL_PROTOCOL);
    handlers_[key].push_back(handler);
}

void LinkageEngine::registerHandler(LinkageAction::Type type,
                                     int protocolID,
                                     ActionHandler handler) {
    HandlerKey key(static_cast<int>(type), protocolID);
    handlers_[key].push_back(handler);
}

void LinkageEngine::executeActive(const Event& event) {
    executeActions(event, event.activeActions);
}

void LinkageEngine::executeCleared(const Event& event) {
    executeActions(event, event.clearActions);
}

void LinkageEngine::executeActions(const Event& event,
                                    const std::vector<LinkageAction>& actions) {
    for (std::vector<LinkageAction>::const_iterator it = actions.begin();
         it != actions.end(); ++it) {
        dispatchAction(event, *it);
    }
}

void LinkageEngine::dispatchAction(const Event& event,
                                    const LinkageAction& action) {
    int typeKey = static_cast<int>(action.type);

    // 1. 先查全局 handler（protocolID = -1）
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

    // 2. 再查该 protocolID 对应的 handler
    HandlerKey deviceKey(typeKey, event.protocolID);
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
}
