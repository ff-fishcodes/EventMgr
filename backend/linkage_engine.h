#ifndef LINKAGE_ENGINE_H
#define LINKAGE_ENGINE_H

#include "event_types.h"
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <QMutex>
#include <QThreadPool>

// ============================================================
// LinkageEngine — 联动引擎（命名回调制）
//
// registerAction(name, displayName, callback) — 注册能力
// configureEvent(id, {names}, {names})         — 绑定能力到事件
// setLevelDefault(level, {active}, {clear})     — 分阶段设置等级默认动作
//
// 联动禁用：按事件+动作+产生/消除侧单独禁用，不持久化
//
// 运行时: addEvent → resolveNames → 查 actionTable_ → 过滤禁用 → 线程池异步执行
// ============================================================
class LinkageEngine {
public:
    using ActionCallback   = std::function<void()>;
    using FallbackCallback = std::function<void(const std::string& eventId, bool isActive)>;

    // 前端查询：单个阶段的联动能力及启用状态
    struct ActionInfo {
        std::string name;          // 内部名
        std::string displayName;   // 中文显示名
        bool enabled;              // 当前阶段是否启用

        ActionInfo();
        ActionInfo(const std::string& actionName,
                   const std::string& actionDisplayName,
                   bool actionEnabled);
        ~ActionInfo();
    };

    // 前端查询：产生阶段和消除阶段分别返回，避免混淆阶段状态
    struct EventActionGroups {
        std::vector<ActionInfo> activeActions;
        std::vector<ActionInfo> clearActions;

        EventActionGroups();
        ~EventActionGroups();
    };

    // 单例
    static LinkageEngine& instance();
    static void setInstance(LinkageEngine* eng);

    LinkageEngine();
    ~LinkageEngine();

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
                         const std::vector<std::string>& activeActions,
                         const std::vector<std::string>& clearActions);

    void executeActive(const Event& event);
    void executeCleared(const Event& event);
    void clearAll();

    // 联动禁用（isActive=true 产生侧, isActive=false 消除侧）
    void disableAction(const EventId& eventId, const std::string& name, bool isActive);
    void enableAction(const EventId& eventId, const std::string& name, bool isActive);
    bool isActionDisabled(const EventId& eventId, const std::string& name, bool isActive) const;

    // 按阶段查询某事件的有效联动配置（供前端配置页使用）
    EventActionGroups getEventActionGroups(const EventId& eventId,
                                           EventLevel originalLevel) const;

private:
    struct LevelActionConfig {
        std::vector<std::string> activeActions;
        std::vector<std::string> clearActions;

        LevelActionConfig();
        LevelActionConfig(const std::vector<std::string>& levelActiveActions,
                          const std::vector<std::string>& levelClearActions);
        ~LevelActionConfig();
    };

    // 以下 Locked 辅助函数要求调用者已经持有 configMutex_，不得再次加锁。
    // 运行时解析在无显式事件配置时还会合并 Event 自带的阶段动作。
    std::vector<std::string> resolveNamesLocked(const Event& event,
                                                bool isActive) const;
    // 查询解析只合并等级默认配置和已登记的事件配置，不使用运行时 Event 回退值。
    std::vector<std::string> effectiveConfiguredNamesLocked(
        const EventId& eventId, EventLevel originalLevel, bool isActive) const;
    std::vector<ActionInfo> actionInfosLocked(
        const EventId& eventId, const std::vector<std::string>& names,
        bool isActive) const;
    bool isActionDisabledLocked(const EventId& eventId,
                                const std::string& name, bool isActive) const;
    void executeNames(const std::vector<std::string>& names,
                      const EventId& eventId, bool isActive);

    // 联动动作禁用键: "eventId|actionName"
    static std::string makeDisableKey(const EventId& id, const std::string& name);

    std::unordered_map<std::string, ActionCallback> actionTable_;
    std::unordered_map<std::string, std::string> displayNames_;   // name → 中文名
    std::unordered_map<EventId,
        std::pair<std::vector<std::string>, std::vector<std::string> > > eventConfig_;
    std::unordered_map<int, LevelActionConfig> levelDefaults_;
    FallbackCallback fallback_;

    // 产生侧/消除侧禁用的联动动作（不持久化，重启清空）
    std::unordered_set<std::string> disabledActive_;
    std::unordered_set<std::string> disabledClear_;

    // 配置互斥锁保护动作表、显示名、事件/等级配置、回退函数和两侧禁用集合。
    // linkagePool_ 的生命周期与任务调度不由此锁保护。
    mutable QMutex configMutex_;
    QThreadPool linkagePool_;

    static LinkageEngine* instance_;
};

#endif // LINKAGE_ENGINE_H
