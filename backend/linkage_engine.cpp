#include "linkage_engine.h"
#include <QRunnable>

LinkageEngine* LinkageEngine::instance_ = NULL;

LinkageEngine& LinkageEngine::instance() { return *instance_; }
void LinkageEngine::setInstance(LinkageEngine* eng) { instance_ = eng; }

namespace {

class ActionTask : public QRunnable {
    LinkageEngine::ActionCallback callback_;
public:
    explicit ActionTask(LinkageEngine::ActionCallback cb) : callback_(cb) {
        setAutoDelete(true);
    }
    void run() { callback_(); }
};

} // namespace

LinkageEngine::LinkageEngine() {
    linkagePool_.setMaxThreadCount(4);
}

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
                                     const std::vector<std::string>& names) {
    levelDefaults_[static_cast<int>(level)] = names;
}

// ============================================================
// 名称解析（使用 originalLevel 做等级默认匹配，不受降级影响）
// ============================================================

std::vector<std::string> LinkageEngine::resolveActiveNames(const Event& event) {
    std::vector<std::string> result;

    std::unordered_map<EventId,
        std::pair<std::vector<std::string>, std::vector<std::string> > >::const_iterator
        it = eventConfig_.find(event.id);
    const std::vector<std::string>& base =
        (it != eventConfig_.end()) ? it->second.first : event.activeActions;
    result.insert(result.end(), base.begin(), base.end());

    // 用 originalLevel 匹配等级默认，降级不影响联动
    std::unordered_map<int, std::vector<std::string> >::const_iterator
        levelIt = levelDefaults_.find(static_cast<int>(event.originalLevel));
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

// ============================================================
// 联动执行（用 originalLevel 判断，降级不影响联动）
// ============================================================

void LinkageEngine::executeActive(const Event& event) {
    if (event.originalLevel == EventLevel::Info) return;
    executeNames(resolveActiveNames(event), event.id, true);
    if (fallback_) fallback_(event.id, true);
}

void LinkageEngine::executeCleared(const Event& event) {
    if (event.originalLevel == EventLevel::Info) return;
    executeNames(resolveClearNames(event), event.id, false);
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
    std::string key = makeDisableKey(eventId, name);
    if (isActive) return disabledActive_.find(key) != disabledActive_.end();
    else          return disabledClear_.find(key) != disabledClear_.end();
}

// ============================================================
// 事件联动信息查询
// ============================================================

std::vector<LinkageEngine::ActionInfo> LinkageEngine::getEventActions(
        const EventId& eventId) const {
    std::vector<ActionInfo> result;

    // 从事件配置表取所有名称（active + clear 去重）
    std::unordered_map<EventId,
        std::pair<std::vector<std::string>, std::vector<std::string> > >::const_iterator
        it = eventConfig_.find(eventId);
    if (it == eventConfig_.end()) return result;

    std::unordered_set<std::string> seen;
    const std::vector<std::string>* lists[2] = {&it->second.first, &it->second.second};

    for (int i = 0; i < 2; ++i) {
        for (std::vector<std::string>::const_iterator nit = lists[i]->begin();
             nit != lists[i]->end(); ++nit) {
            if (seen.find(*nit) != seen.end()) continue;
            seen.insert(*nit);

            ActionInfo info;
            info.name          = *nit;
            info.disabledActive = isActionDisabled(eventId, *nit, true);
            info.disabledClear  = isActionDisabled(eventId, *nit, false);

            // 查找中文名
            std::unordered_map<std::string, std::string>::const_iterator dit =
                displayNames_.find(*nit);
            info.displayName = (dit != displayNames_.end()) ? dit->second : *nit;

            result.push_back(info);
        }
    }

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
