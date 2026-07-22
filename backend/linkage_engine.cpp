#include "linkage_engine.h"
#include <QMutexLocker>
#include <QRunnable>

LinkageEngine* LinkageEngine::instance_ = NULL;

LinkageEngine& LinkageEngine::instance() { return *instance_; }
void LinkageEngine::setInstance(LinkageEngine* eng) { instance_ = eng; }

namespace {

void appendUnique(const std::vector<std::string>& source,
                  std::unordered_set<std::string>& seen,
                  std::vector<std::string>& target) {
    for (std::vector<std::string>::const_iterator it = source.begin();
         it != source.end(); ++it) {
        if (seen.insert(*it).second) target.push_back(*it);
    }
}

class ActionTask : public QRunnable {
    LinkageEngine::ActionCallback callback_;
public:
    explicit ActionTask(LinkageEngine::ActionCallback cb) : callback_(cb) {
        setAutoDelete(true);
    }
    void run() { callback_(); }
};

} // namespace

LinkageEngine::ActionInfo::ActionInfo()
    : name(), displayName(), enabled(false) {}

LinkageEngine::ActionInfo::ActionInfo(
        const std::string& actionName,
        const std::string& actionDisplayName,
        bool actionEnabled)
    : name(actionName),
      displayName(actionDisplayName),
      enabled(actionEnabled) {}

LinkageEngine::ActionInfo::~ActionInfo() {}

LinkageEngine::EventActionGroups::EventActionGroups()
    : activeActions(), clearActions() {}

LinkageEngine::EventActionGroups::~EventActionGroups() {}

LinkageEngine::LevelActionConfig::LevelActionConfig()
    : activeActions(), clearActions() {}

LinkageEngine::LevelActionConfig::LevelActionConfig(
        const std::vector<std::string>& levelActiveActions,
        const std::vector<std::string>& levelClearActions)
    : activeActions(levelActiveActions),
      clearActions(levelClearActions) {}

LinkageEngine::LevelActionConfig::~LevelActionConfig() {}

LinkageEngine::LinkageEngine() {
    linkagePool_.setMaxThreadCount(4);
}

LinkageEngine::~LinkageEngine() {}

std::string LinkageEngine::makeDisableKey(const EventId& id, const std::string& name) {
    return id + "|" + name;
}

// ============================================================
// 能力注册
// ============================================================

void LinkageEngine::registerAction(const std::string& name,
                                    const std::string& displayName,
                                    ActionCallback callback) {
    actionTable_[name] = callback;
    displayNames_[name] = displayName;
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
                                     const std::vector<std::string>& activeActions,
                                     const std::vector<std::string>& clearActions) {
    levelDefaults_[static_cast<int>(level)] =
        LevelActionConfig(activeActions, clearActions);
}

// ============================================================
// 名称解析（使用 originalLevel 做等级默认匹配，不受降级影响）
// ============================================================

std::vector<std::string> LinkageEngine::resolveNamesLocked(
        const Event& event, bool isActive) const {
    std::vector<std::string> result;
    if (event.originalLevel == EventLevel::Info) return result;

    std::unordered_set<std::string> seen;

    // 等级默认动作优先，重复名称保留首次出现的位置。
    std::unordered_map<int, LevelActionConfig>::const_iterator levelIt =
        levelDefaults_.find(static_cast<int>(event.originalLevel));
    if (levelIt != levelDefaults_.end()) {
        appendUnique(isActive ? levelIt->second.activeActions
                              : levelIt->second.clearActions,
                     seen, result);
    }

    std::unordered_map<EventId,
        std::pair<std::vector<std::string>, std::vector<std::string> > >::const_iterator
        it = eventConfig_.find(event.id);
    const std::vector<std::string>& phaseNames =
        (it != eventConfig_.end())
            ? (isActive ? it->second.first : it->second.second)
            : (isActive ? event.activeActions : event.clearActions);
    appendUnique(phaseNames, seen, result);

    return result;
}

std::vector<std::string> LinkageEngine::effectiveConfiguredNamesLocked(
        const EventId& eventId, EventLevel originalLevel, bool isActive) const {
    std::vector<std::string> result;
    if (originalLevel == EventLevel::Info) return result;

    std::unordered_set<std::string> seen;
    std::unordered_map<int, LevelActionConfig>::const_iterator levelIt =
        levelDefaults_.find(static_cast<int>(originalLevel));
    if (levelIt != levelDefaults_.end()) {
        appendUnique(isActive ? levelIt->second.activeActions
                              : levelIt->second.clearActions,
                     seen, result);
    }

    std::unordered_map<EventId,
        std::pair<std::vector<std::string>, std::vector<std::string> > >::const_iterator
        it = eventConfig_.find(eventId);
    if (it != eventConfig_.end()) {
        appendUnique(isActive ? it->second.first : it->second.second,
                     seen, result);
    }

    return result;
}

// ============================================================
// 联动执行（用 originalLevel 判断，降级不影响联动）
// ============================================================

void LinkageEngine::executeActive(const Event& event) {
    if (event.originalLevel == EventLevel::Info) return;
    std::vector<std::string> names;
    {
        QMutexLocker locker(&configMutex_);
        names = resolveNamesLocked(event, true);
    }
    executeNames(names, event.id, true);
    if (fallback_) fallback_(event.id, true);
}

void LinkageEngine::executeCleared(const Event& event) {
    if (event.originalLevel == EventLevel::Info) return;
    std::vector<std::string> names;
    {
        QMutexLocker locker(&configMutex_);
        names = resolveNamesLocked(event, false);
    }
    executeNames(names, event.id, false);
    if (fallback_) fallback_(event.id, false);
}

void LinkageEngine::executeNames(const std::vector<std::string>& names,
                                  const EventId& eventId, bool isActive) {
    for (std::vector<std::string>::const_iterator it = names.begin();
         it != names.end(); ++it) {
        if (isActionDisabled(eventId, *it, isActive)) continue;
        std::unordered_map<std::string, ActionCallback>::iterator
            found = actionTable_.find(*it);
        if (found != actionTable_.end()) {
            linkagePool_.start(new ActionTask(found->second));
        }
    }
}

// ============================================================
// 联动禁用（分产生/消除两侧，不持久化）
// ============================================================

void LinkageEngine::disableAction(const EventId& eventId,
                                   const std::string& name, bool isActive) {
    std::string key = makeDisableKey(eventId, name);
    if (isActive) disabledActive_.insert(key);
    else          disabledClear_.insert(key);
}

void LinkageEngine::enableAction(const EventId& eventId,
                                  const std::string& name, bool isActive) {
    std::string key = makeDisableKey(eventId, name);
    if (isActive) disabledActive_.erase(key);
    else          disabledClear_.erase(key);
}

bool LinkageEngine::isActionDisabled(const EventId& eventId,
                                      const std::string& name, bool isActive) const {
    QMutexLocker locker(&configMutex_);
    return isActionDisabledLocked(eventId, name, isActive);
}

bool LinkageEngine::isActionDisabledLocked(
        const EventId& eventId, const std::string& name, bool isActive) const {
    std::string key = makeDisableKey(eventId, name);
    if (isActive) return disabledActive_.find(key) != disabledActive_.end();
    else          return disabledClear_.find(key) != disabledClear_.end();
}

// ============================================================
// 事件联动信息查询
// ============================================================

std::vector<LinkageEngine::ActionInfo> LinkageEngine::actionInfosLocked(
        const EventId& eventId, const std::vector<std::string>& names,
        bool isActive) const {
    std::vector<ActionInfo> result;
    for (std::vector<std::string>::const_iterator it = names.begin();
         it != names.end(); ++it) {
        std::unordered_map<std::string, std::string>::const_iterator displayIt =
            displayNames_.find(*it);
        const std::string& displayName =
            (displayIt != displayNames_.end()) ? displayIt->second : *it;
        result.push_back(ActionInfo(
            *it, displayName,
            !isActionDisabledLocked(eventId, *it, isActive)));
    }
    return result;
}

LinkageEngine::EventActionGroups LinkageEngine::getEventActionGroups(
        const EventId& eventId, EventLevel originalLevel) const {
    QMutexLocker locker(&configMutex_);

    EventActionGroups result;
    const std::vector<std::string> activeNames =
        effectiveConfiguredNamesLocked(eventId, originalLevel, true);
    const std::vector<std::string> clearNames =
        effectiveConfiguredNamesLocked(eventId, originalLevel, false);
    result.activeActions = actionInfosLocked(eventId, activeNames, true);
    result.clearActions = actionInfosLocked(eventId, clearNames, false);
    return result;
}

// ============================================================
// 清理
// ============================================================

void LinkageEngine::clearAll() {
    linkagePool_.waitForDone();
    actionTable_.clear();
    displayNames_.clear();
    eventConfig_.clear();
    levelDefaults_.clear();
    disabledActive_.clear();
    disabledClear_.clear();
}
