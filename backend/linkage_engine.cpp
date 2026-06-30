#include "linkage_engine.h"
#include <sstream>

// ============================================================
// Action 注册
// ============================================================

void LinkageEngine::registerAction(int protocolID, const std::string& name,
                                    ActionCallback callback) {
    std::ostringstream key;
    key << protocolID << ":" << name;
    actionTable_[key.str()] = callback;
}

// ============================================================
// 事件联动配置
// ============================================================

void LinkageEngine::configureEvent(
        const EventId& eventId,
        const std::vector<std::string>& activeNames,
        const std::vector<std::string>& clearNames) {
    eventConfig_[eventId] = std::make_pair(activeNames, clearNames);
}

void LinkageEngine::setLevelDefault(EventLevel level,
                                     const std::vector<std::string>& names) {
    levelDefaults_[static_cast<int>(level)] = names;
}

// ============================================================
// 名字解析
// ============================================================

std::pair<int, std::string> LinkageEngine::parseName(const Event& event,
                                                      const std::string& name) {
    size_t colon = name.find(':');
    if (colon == std::string::npos) {
        // 无冒号 → 默认用事件的 protocolID
        return std::make_pair(event.protocolID, name);
    }

    // 冒号前为纯数字 → protocolID 前缀（如 "2:emergency_stop"）
    std::string prefix = name.substr(0, colon);
    bool isNumeric = !prefix.empty();
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (prefix[i] < '0' || prefix[i] > '9') { isNumeric = false; break; }
    }

    if (isNumeric) {
        int pid = 0;
        std::istringstream(prefix) >> pid;
        return std::make_pair(pid, name.substr(colon + 1));
    }

    // 冒号前非数字 → 整个字符串就是名字（如 "lock_ui:panel_main"）
    // protocolID 默认用 0（全局）
    return std::make_pair(0, name);
}

// ============================================================
// 解析 names：事件配置 + 等级默认 → 合并
// ============================================================

std::vector<std::string> LinkageEngine::resolveActiveNames(const Event& event) {
    std::vector<std::string> result;

    // 1. 事件级配置（或 Event 自带兜底）
    std::unordered_map<EventId,
        std::pair<std::vector<std::string>, std::vector<std::string> > >::const_iterator
        it = eventConfig_.find(event.id);
    const std::vector<std::string>& base =
        (it != eventConfig_.end()) ? it->second.first : event.activeActions;
    result.insert(result.end(), base.begin(), base.end());

    // 2. 等级默认
    std::unordered_map<int, std::vector<std::string> >::const_iterator
        levelIt = levelDefaults_.find(static_cast<int>(event.effectiveLevel));
    if (levelIt != levelDefaults_.end()) {
        result.insert(result.end(), levelIt->second.begin(), levelIt->second.end());
    }

    return result;
}

std::vector<std::string> LinkageEngine::resolveClearNames(const Event& event) {
    std::vector<std::string> result;

    std::unordered_map<EventId,
        std::pair<std::vector<std::string>, std::vector<std::string> > >::const_iterator
        it = eventConfig_.find(event.id);

    const std::vector<std::string>* explicitClear = NULL;
    const std::vector<std::string>* activeSrc     = NULL;

    if (it != eventConfig_.end()) {
        explicitClear = &it->second.second;
        activeSrc     = &it->second.first;
    } else {
        explicitClear = &event.clearActions;
        activeSrc     = &event.activeActions;
    }

    // 1. 显式清除动作
    result.insert(result.end(), explicitClear->begin(), explicitClear->end());

    // 2. 自动 mirror: "lock_ui:xxx" → "unlock_ui:xxx"
    for (std::vector<std::string>::const_iterator i = activeSrc->begin();
         i != activeSrc->end(); ++i) {
        if (i->compare(0, 8, "lock_ui:") == 0) {
            std::string unlockName = "unlock_ui:" + i->substr(8);
            // 保持前缀: 若原名字带 protocolID 前缀则保留
            result.push_back(unlockName);
        }
    }

    return result;
}

// ============================================================
// 执行入口
// ============================================================

void LinkageEngine::executeActive(const Event& event) {
    if (event.effectiveLevel == EventLevel::Info) return;
    executeNames(event, resolveActiveNames(event));
}

void LinkageEngine::executeCleared(const Event& event) {
    if (event.effectiveLevel == EventLevel::Info) return;
    executeNames(event, resolveClearNames(event));
}

void LinkageEngine::executeNames(const Event& event,
                                  const std::vector<std::string>& names) {
    for (std::vector<std::string>::const_iterator it = names.begin();
         it != names.end(); ++it) {
        // 解析名字 → (protocolID, localName)
        std::pair<int, std::string> resolved = parseName(event, *it);

        // 查 actionTable_
        std::ostringstream key;
        key << resolved.first << ":" << resolved.second;
        std::unordered_map<std::string, ActionCallback>::iterator
            found = actionTable_.find(key.str());
        if (found != actionTable_.end()) {
            found->second();   // 执行回调（lambda 已捕获所有参数）
        }
    }
}

void LinkageEngine::clearAll() {
    actionTable_.clear();
    eventConfig_.clear();
    levelDefaults_.clear();
}
