#ifndef LINKAGE_ENGINE_H
#define LINKAGE_ENGINE_H

#include "event_types.h"
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <QThreadPool>

// ============================================================
// LinkageEngine — 联动引擎（命名回调制）
//
// registerAction(name, displayName, callback) — 注册能力
// configureEvent(id, {names}, {names})         — 绑定能力到事件
// setLevelDefault(level, {names})              — 等级默认动作
//
// 联动禁用：按事件+动作+产生/消除侧单独禁用，不持久化
//
// 运行时: addEvent → resolveNames → 查 actionTable_ → 过滤禁用 → 线程池异步执行
// ============================================================
class LinkageEngine {
public:
    using ActionCallback   = std::function<void()>;
    using FallbackCallback = std::function<void(const std::string& eventId, bool isActive)>;

    // 前端查询：某个事件的联动能力列表及禁用状态
    struct ActionInfo {
        std::string name;          // 内部名
        std::string displayName;   // 中文显示名
        bool disabledActive;       // 产生侧是否被禁用
        bool disabledClear;        // 消除侧是否被禁用
    };

    // 单例
    static LinkageEngine& instance();
    static void setInstance(LinkageEngine* eng);

    LinkageEngine();
    ~LinkageEngine() {}

    LinkageEngine(const LinkageEngine&) = delete;
    LinkageEngine& operator=(const LinkageEngine&) = delete;

    // 注册能力（displayName 供前端中文显示）
    void registerAction(const std::string& name, const std::string& displayName,
                        ActionCallback callback);

    void setFallback(FallbackCallback callback);
    void configureEvent(const EventId& eventId,
                        const std::vector<std::string>& activeNames,
                        const std::vector<std::string>& clearNames);
    void setLevelDefault(EventLevel level,
                         const std::vector<std::string>& names);

    void executeActive(const Event& event);
    void executeCleared(const Event& event);
    void clearAll();

    // 联动禁用（isActive=true 产生侧, isActive=false 消除侧）
    void disableAction(const EventId& eventId, const std::string& name, bool isActive);
    void enableAction(const EventId& eventId, const std::string& name, bool isActive);
    bool isActionDisabled(const EventId& eventId, const std::string& name, bool isActive) const;

    // 查询某事件配置的所有联动能力及禁用状态（供前端配置页使用）
    std::vector<ActionInfo> getEventActions(const EventId& eventId) const;

private:
    std::vector<std::string> resolveActiveNames(const Event& event);
    std::vector<std::string> resolveClearNames(const Event& event);
    void executeNames(const std::vector<std::string>& names,
                      const EventId& eventId, bool isActive);

    // 联动动作禁用键: "eventId|actionName"
    static std::string makeDisableKey(const EventId& id, const std::string& name);

    std::unordered_map<std::string, ActionCallback> actionTable_;
    std::unordered_map<std::string, std::string> displayNames_;   // name → 中文名
    std::unordered_map<EventId,
        std::pair<std::vector<std::string>, std::vector<std::string> > > eventConfig_;
    std::unordered_map<int, std::vector<std::string> > levelDefaults_;
    FallbackCallback fallback_;

    // 产生侧/消除侧禁用的联动动作（不持久化，重启清空）
    std::unordered_set<std::string> disabledActive_;
    std::unordered_set<std::string> disabledClear_;

    QThreadPool linkagePool_;

    static LinkageEngine* instance_;
};

#endif // LINKAGE_ENGINE_H
