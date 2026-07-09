#include "linkage_engine.h"

void LinkageEngine::registerAction(const std::string& name,
                                    ActionCallback callback) {
    actionTable_[name] = callback;
}

void LinkageEngine::setFallback(FallbackCallback callback) {
    fallback_ = callback;
}

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

std::vector<std::string> LinkageEngine::resolveActiveNames(const Event& event) {
    std::vector<std::string> result;

    std::unordered_map<EventId,
        std::pair<std::vector<std::string>, std::vector<std::string> > >::const_iterator
        it = eventConfig_.find(event.id);
    const std::vector<std::string>& base =
        (it != eventConfig_.end()) ? it->second.first : event.activeActions;
    result.insert(result.end(), base.begin(), base.end());

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

    const std::vector<std::string>& src =
        (it != eventConfig_.end()) ? it->second.second : event.clearActions;

    result.insert(result.end(), src.begin(), src.end());
    return result;
}

void LinkageEngine::executeActive(const Event& event) {
    if (event.effectiveLevel == EventLevel::Info) return;
    executeNames(resolveActiveNames(event));
}

void LinkageEngine::executeCleared(const Event& event) {
    if (event.effectiveLevel == EventLevel::Info) return;
    executeNames(resolveClearNames(event));
}

void LinkageEngine::executeNames(const std::vector<std::string>& names) {
    for (std::vector<std::string>::const_iterator it = names.begin();
         it != names.end(); ++it) {
        std::unordered_map<std::string, ActionCallback>::iterator
            found = actionTable_.find(*it);
        if (found != actionTable_.end()) {
            found->second();
        } else if (fallback_) {
            fallback_(*it);
        }
    }
}

void LinkageEngine::clearAll() {
    actionTable_.clear();
    eventConfig_.clear();
    levelDefaults_.clear();
}
